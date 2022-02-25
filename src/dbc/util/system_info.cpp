#include "system_info.h"
#include "service_module/service_name.h"

typedef struct mem_table_struct {
    const char *name;     /* memory type name */
    unsigned long *slot; /* slot in return struct */
} mem_table_struct;

static int compare_mem_table_structs(const void *a, const void *b) {
    return strcmp(((const mem_table_struct *) a)->name, ((const mem_table_struct *) b)->name);
}

std::string gpu_info::parse_bus(const std::string &id) {
    auto pos = id.find(':');
    if (pos != std::string::npos) {
        return id.substr(0, pos);
    } else {
        return "";
    }
}

std::string gpu_info::parse_slot(const std::string &id) {
    auto pos1 = id.find(':');
    auto pos2 = id.find('.');
    if (pos1 != std::string::npos && pos2 != std::string::npos) {
        return id.substr(pos1 + 1, pos2 - pos1 - 1);
    } else {
        return "";
    }
}

std::string gpu_info::parse_function(const std::string &id) {
    auto pos = id.find('.');
    if (pos != std::string::npos) {
        return id.substr(pos + 1);
    } else {
        return "";
    }
}

SystemInfo::SystemInfo() {

}

SystemInfo::~SystemInfo() {
    stop();
}

void SystemInfo::init(bpo::variables_map &options, int32_t reserved_cpu_cores, int32_t reserved_memory) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_compute_node = true;
    }

    m_reserved_cpu_cores = reserved_cpu_cores;
    m_reserved_memory = reserved_memory;

    init_os_type();
    update_mem_info(m_meminfo);
    init_cpu_info(m_cpuinfo);
    init_gpu();
    if (m_is_compute_node)
        update_disk_info("/data", m_diskinfo);
    else
        update_disk_info("/", m_diskinfo);
    update_load_average(m_loadaverage);

    m_public_ip = get_public_ip();
    m_default_route_ip = get_default_route_ip();
}

void SystemInfo::start() {
    m_running = true;

    if (m_thread == nullptr) {
        m_thread = new std::thread(&SystemInfo::update_func, this);
    }
}

void SystemInfo::stop() {
    m_running = false;
    if (m_thread != nullptr && m_thread->joinable()) {
        m_thread->join();
    }
    delete m_thread;
}

// os_type
void SystemInfo::init_os_type() {
    std::string os_ID = run_shell("cat /etc/os-release | grep -w NAME | awk -F '=' '{print $2}'| tr -d '\"'");
    std::string os_VERSION = run_shell("cat /etc/os-release | grep -w VERSION | awk -F '=' '{print $2}'| tr -d '\"'");
    std::string os_RELEASE = run_shell("uname -o -r");

    m_os_name = os_ID + " " + os_VERSION + " " + os_RELEASE;
    if (os_VERSION.find("18.04") != std::string::npos)
        m_os_type = OS_Ubuntu_1804;
    else if (os_VERSION.find("20.04") != std::string::npos) {
        m_os_type = OS_Ubuntu_2004;
    }
}

// mem info
// 预留了一定的系统内存： g_reserved_memory
void SystemInfo::update_mem_info(mem_info &info) {
    unsigned long kb_main_total;
    unsigned long kb_main_free;
    unsigned long kb_main_available;
    unsigned long kb_main_buffers;
    unsigned long kb_page_cache;
    unsigned long kb_swap_total;
    unsigned long kb_swap_free;
    unsigned long kb_main_shared;
    unsigned long kb_slab_reclaimable;

    const mem_table_struct mem_table[] = {
            {"Buffers",      &kb_main_buffers},
            {"Cached",       &kb_page_cache},
            {"MemAvailable", &kb_main_available},
            {"MemFree",      &kb_main_free},
            {"MemTotal",     &kb_main_total},
            {"SReclaimable", &kb_slab_reclaimable},
            {"Shmem",        &kb_main_shared},
            {"SwapFree",     &kb_swap_free},
            {"SwapTotal",    &kb_swap_total}
    };

    char buf[8192];
    do {
        int local_n;
        int meminfo_fd = open("/proc/meminfo", O_RDONLY);
        if (meminfo_fd == -1) {
            fputs("BAD_OPEN_MESSAGE", stderr);
            fflush(NULL);
            return;
        }
        lseek(meminfo_fd, 0L, SEEK_SET);
        if ((local_n = read(meminfo_fd, buf, sizeof buf - 1)) < 0) {
            perror("/proc/meminfo");
            fflush(NULL);
            close(meminfo_fd);
            return;
        }
        buf[local_n] = '\0';
        close(meminfo_fd);
    } while (0);

    const int mem_table_count = sizeof(mem_table) / sizeof(mem_table_struct);
    char namebuf[32]; /* big enough to hold any row name */
    mem_table_struct findme = {namebuf, NULL};
    mem_table_struct *found;
    char *head;
    char *tail;

    head = buf;
    for (;;) {
        tail = strchr(head, ':');
        if (!tail) break;
        *tail = '\0';
        if (strlen(head) >= sizeof(namebuf)) {
            head = tail + 1;
            goto nextline;
        }
        strcpy(namebuf, head);
        found = (mem_table_struct *) bsearch(&findme, mem_table, mem_table_count,
                                             sizeof(mem_table_struct), compare_mem_table_structs);
        head = tail + 1;
        if (!found) goto nextline;
        *(found->slot) = (unsigned long) strtoull(head, &tail, 10);
        nextline:
        tail = strchr(head, '\n');
        if (!tail) break;
        head = tail + 1;
    }

    info.mem_total = kb_main_total;
    info.mem_free = kb_main_free;

    if (kb_main_free < m_reserved_memory * 1024L * 1024L)
        info.mem_free = 0LU;
    else
        info.mem_free = kb_main_free - m_reserved_memory * 1024L * 1024L;

    if (kb_main_total < m_reserved_memory * 1024L * 1024L) {
        info.mem_total = 0LU;
        info.mem_usage = 0;
    } else {
        info.mem_total = kb_main_total - m_reserved_memory * 1024L * 1024L;
        info.mem_usage = (info.mem_total - info.mem_free) * 1.0 / info.mem_total;
    }

    unsigned long mem_used = kb_main_total - kb_main_free - (kb_page_cache + kb_slab_reclaimable) - kb_main_buffers;
    if (mem_used < 0)
        mem_used = kb_main_total - kb_main_free;
    info.mem_used = mem_used;

    info.mem_shared = kb_main_shared;
    info.mem_buffers = kb_main_buffers;
    info.mem_cached = kb_page_cache + kb_slab_reclaimable;

    unsigned long mem_available = kb_main_available;
    if (mem_available > kb_main_total)
        mem_available = kb_main_free;
    info.mem_available = mem_available - m_reserved_memory * 1024L * 1024L;

    info.mem_swap_total = kb_swap_total;
    info.mem_swap_free = kb_swap_free;
}

// cpu info
void SystemInfo::init_cpu_info(cpu_info &info) {
    std::set<int32_t> cpus;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[1024] = {0};
    while (fgets(line, sizeof(line), fp) != nullptr) {
        //printf("%s", line);

        // key
        char *p1 = strchr(line, ':');
        if (!p1) {
            continue;
        }
        *p1 = '\0';

        char *p2 = strchr(line, '\t');
        if (p2) *p2 = '\0';

        // value
        char *p3 = strchr(p1 + 1, '\n');
        if (p3) *p3 = '\0';
        p3 = strchr(p1 + 1, ' ');

        if (strcmp(line, "model name") == 0) {
            if (p3) {
                info.cpu_name = std::string(p3 + 1);

                auto pos1 = info.cpu_name.find("Hz");
                auto pos2 = info.cpu_name.rfind(' ', pos1);
                std::string mhz = info.cpu_name.substr(pos2 + 1, pos1 - pos2 - 1);
                char ch = mhz[mhz.size() - 1];
                if (ch == 'G') {
                    mhz = std::to_string(int32_t(atof(mhz.substr(0, mhz.size() - 1).c_str()) * 1000)) + "MHz";
                } else if (ch == 'M') {
                    mhz = std::to_string(atof(mhz.substr(0, mhz.size() - 1).c_str()) * 1) + "MHz";
                }
                info.mhz = mhz;
            }
        } else if (strcmp(line, "physical id") == 0) {
            if (p3) {
                cpus.insert(atoi(p3 + 1));
            }
        } else if (strcmp(line, "cpu cores") == 0) {
            if (p3) {
                info.physical_cores_per_cpu = atoi(p3 + 1);
            }
        } else if (strcmp(line, "siblings") == 0) {
            if (p3) {
                info.logical_cores_per_cpu = atoi(p3 + 1);
            }
        }
    }
    fclose(fp);

    info.physical_cores = cpus.size();
    info.threads_per_cpu = info.logical_cores_per_cpu / info.physical_cores_per_cpu;
    info.physical_cores_per_cpu -= m_reserved_cpu_cores;
    info.logical_cores_per_cpu = info.physical_cores_per_cpu * info.threads_per_cpu;
    info.total_cores = info.physical_cores * info.physical_cores_per_cpu * info.threads_per_cpu;
}

// gpu info
void SystemInfo::init_gpu() {
    std::string cmd = "lspci -nnv |grep NVIDIA |awk '{print $2\",\"$1}' |tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    if (!str.empty()) {
        std::vector<std::string> vGpus;
        util::split(str, "|", vGpus);
        std::string cur_id;
        for (int i = 0; i < vGpus.size(); i++) {
            std::vector<std::string> vIDs;
            util::split(vGpus[i], ",", vIDs);
            std::string id = vIDs[1];
            std::string idx;
            auto pos = id.find_last_of('.');
            if (pos != std::string::npos) {
                idx = id.substr(pos + 1);
            }
            if (idx.empty()) continue;

            if (idx == "0") {
                cur_id = id;
                m_gpuinfo[cur_id].id = cur_id;
            }

            m_gpuinfo[cur_id].devices.push_back(vIDs[1]);
        }
    }
}

// disk info
void SystemInfo::update_disk_info(const std::string &path, disk_info &info) {
    char dpath[256] = "/"; //设置默认位置

    if (!path.empty()) {
        strcpy(dpath, path.c_str());
    }

    struct statfs diskInfo;
    if (-1 == statfs(dpath, &diskInfo)) {
        return;
    }

    uint64_t block_size = diskInfo.f_bsize; //每块包含字节大小
    info.disk_total = (diskInfo.f_blocks * block_size) >> 10; //磁盘总空间
    info.disk_available = (diskInfo.f_bavail * block_size) >> 10; //非超级用户可用空间
    info.disk_free = (diskInfo.f_bfree * block_size) >> 10; //磁盘所有剩余空间
    info.disk_usage = (info.disk_total - info.disk_available) * 1.0 / info.disk_total;

    std::string cmd = "df -l " + std::string(dpath) + " | tail -1";
    std::string tmp = run_shell(cmd.c_str());
    cmd = "echo " + tmp + " | awk -F' ' '{print $1}' | awk -F\"/\" '{print $3}'";
    tmp = run_shell(cmd.c_str());
    if (tmp.find("sda") != std::string::npos)
        tmp = "sda";
    std::string tmppath = "/sys/block/" + tmp + "/queue/rotational";
    int fd = open(tmppath.c_str(), O_RDONLY);
    if (fd == -1) {
        return;
    }
    lseek(fd, 0L, SEEK_SET);
    char buf[8] = {0};
    int len = 0;
    if ((len = read(fd, buf, sizeof buf - 1)) < 0) {
        close(fd);
        return;
    }
    buf[len] = '\0';
    close(fd);

    int t = atoi(buf);
    if (t == 1)
        info.disk_type = DISK_HDD;
    else
        info.disk_type = DISK_SSD;
}

void SystemInfo::update_load_average(std::vector<float> &average) {
    std::string tmp = run_shell("uptime | awk '{print $10,$11,$12}'");
    std::vector<std::string> vecSplit;
    util::split(tmp, ", ", vecSplit);
    for (const auto& str : vecSplit) {
        average.push_back(atof(str.c_str()));
    }
}

// cpu usage
struct occupy {
    char name[20];
    unsigned int user;
    unsigned int nice;
    unsigned int system;
    unsigned int idle;
};

// read cpu info from /proc/stat这里读取文件得到一些数值，这些数值可用来计算cpu使用率
void get_occupy(struct occupy *p, int cpu_count) {
    FILE *fp;
    char buff[1024];

    if ((fp = fopen("/proc/stat", "r")) == NULL) {
        fprintf(stderr, "cant open /proc/stat\n");
        fclose(fp);
    }

    fgets(buff, sizeof(buff), fp);
    sscanf(buff, "%s%u%u%u%u", p[0].name, &(p[0].user), &(p[0].nice), &(p[0].system), &(p[0].idle));

    for (int i = 0; i < cpu_count; i++) {
        fgets(buff, sizeof(buff), fp);
        sscanf(buff, "%s%u%u%u%u", p[i + 1].name, &(p[i + 1].user), &(p[i + 1].nice), &(p[i + 1].system),
               &(p[i + 1].idle));
    }
    fclose(fp);
}

// calculate cpu occupation计算cpu使用率
float cal_occupy(struct occupy *p1, struct occupy *p2) {
    double od, nd;
    od = (double) (p1->user + p1->nice + p1->system + p1->idle);
    nd = (double) (p2->user + p2->nice + p2->system + p2->idle);
    float cpu_used = ((p2->user + p2->system + p2->nice) - (p1->user + p1->system + p1->nice)) / (nd - od);
    return cpu_used;
}

void SystemInfo::update_func() {
    int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    struct occupy *ocpu = new occupy[cpu_num + 1];
    struct occupy *ncpu = new occupy[cpu_num + 1];

    while (m_running) {
        // public ip
        if (m_public_ip.empty())
            m_public_ip = get_public_ip();
        
        // default route ip
        if (m_default_route_ip.empty())
            m_default_route_ip = get_default_route_ip();

        // cpu usage
        get_occupy(ocpu, cpu_num);
        sleep(3);
        get_occupy(ncpu, cpu_num);
        m_cpu_usage = cal_occupy(&ocpu[0], &ncpu[0]);

        // mem
        mem_info _mem_info;
        update_mem_info(_mem_info);
        m_meminfo = _mem_info;

        // disk
        disk_info _disk_info;
        if (m_is_compute_node)
            update_disk_info("/data", _disk_info);
        else
            update_disk_info("/", _disk_info);
        m_diskinfo = _disk_info;

        std::vector<float> vecAverage;
        update_load_average(vecAverage);
        m_loadaverage.swap(vecAverage);
    }

    delete[] ocpu;
    delete[] ncpu;
}
