#include "system_info.h"
#include <cstring>
#include <unistd.h>
#include <set>
#include "util/utils.h"
#include "service_module/service_name.h"

typedef struct mem_table_struct {
    const char *name;     /* memory type name */
    unsigned long *slot; /* slot in return struct */
} mem_table_struct;

static int compare_mem_table_structs(const void *a, const void *b){
    return strcmp(((const mem_table_struct*)a)->name, ((const mem_table_struct*)b)->name);
}

#define PROCFS_OSRELEASE "/proc/sys/kernel/osrelease"
#define MEMINFO_FILE "/proc/meminfo"

std::string gpu_info::parse_bus(const std::string& id) {
    auto pos = id.find(':');
    if (pos != std::string::npos) {
        return id.substr(0, pos);
    } else {
        return "";
    }
}

std::string gpu_info::parse_slot(const std::string& id) {
    auto pos1 = id.find(':');
    auto pos2 = id.find('.');
    if (pos1 != std::string::npos && pos2 != std::string::npos) {
        return id.substr(pos1 + 1, pos2 - pos1 - 1);
    } else {
        return "";
    }
}

std::string gpu_info::parse_function(const std::string& id) {
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

void SystemInfo::init(bpo::variables_map &options) {
    if (options.count(SERVICE_NAME_AI_TRAINING)) {
        m_is_compute_node = true;
    }
}

void SystemInfo::start() {
    m_running = true;
    get_mem_info(m_meminfo);
    get_cpu_info(m_cpuinfo);
    init_gpu();
    if (m_is_compute_node)
        get_disk_info("/data", m_diskinfo);
    else
        get_disk_info("/", m_diskinfo);

    std::string ver = STR_VER(CORE_VERSION);
    auto s_ver = util::remove_leading_zero(ver.substr(2, 2)) + "."
            + util::remove_leading_zero(ver.substr(4, 2)) + "."
            + util::remove_leading_zero(ver.substr(6, 2)) + "."
            + util::remove_leading_zero(ver.substr(8, 2));
    m_version = s_ver;
    m_public_ip = get_public_ip();
    get_os_type();

    if (m_thread == nullptr) {
        m_thread = new std::thread(&SystemInfo::update_cpu_usage, this);
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
void SystemInfo::get_os_type() {
    std::string os_name = run_shell("cat /etc/os-release | grep VERSION | awk -F '=' '{print $2}'| tr -d '\"'");
    os_name = util::rtrim(os_name, '\n');
    std::string linux_ver = run_shell("uname -o -r");
    linux_ver = util::rtrim(linux_ver, '\n');

    m_os_name = os_name + " " + linux_ver;
    if (os_name.find("18.04") != std::string::npos)
        m_os_type = OS_1804;
    else if (os_name.find("20.04") != std::string::npos) {
        m_os_type = OS_2004;
    }
}

// mem info
void SystemInfo::get_mem_info(mem_info &info) {
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

    const int mem_table_count = sizeof(mem_table)/sizeof(mem_table_struct);
    char namebuf[32]; /* big enough to hold any row name */
    mem_table_struct findme = { namebuf, NULL};
    mem_table_struct *found;
    char *head;
    char *tail;

    head = buf;
    for(;;) {
        tail = strchr(head, ':');
        if(!tail) break;
        *tail = '\0';
        if(strlen(head) >= sizeof(namebuf)){
            head = tail+1;
            goto nextline;
        }
        strcpy(namebuf,head);
        found = (mem_table_struct*) bsearch(&findme, mem_table, mem_table_count,
                                            sizeof(mem_table_struct), compare_mem_table_structs);
        head = tail+1;
        if(!found) goto nextline;
        *(found->slot) = (unsigned long)strtoull(head,&tail,10);
        nextline:
        tail = strchr(head, '\n');
        if(!tail) break;
        head = tail+1;
    }

    info.mem_total = kb_main_total - g_reserved_memory * 1024L * 1024L;
    info.mem_free = kb_main_free;

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
    info.mem_available = mem_available;

    info.mem_swap_total = kb_swap_total;
    info.mem_swap_free = kb_swap_free;
    info.mem_usage = info.mem_used * 1.0 / info.mem_total;
}

// cpu info
void SystemInfo::get_cpu_info(cpu_info& info) {
    std::set<int32_t> cpus;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[1024] = {0};
    while (fgets(line, sizeof(line), fp) != nullptr) {
        //printf("%s", line);

        // key
        char* p1 = strchr(line, ':');
        if (!p1) {
            continue;
        }
        *p1 = '\0';

        char* p2 = strchr(line, '\t');
        if (p2) *p2 = '\0';

        // value
        char* p3 = strchr(p1 + 1, '\n');
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
    info.physical_cores_per_cpu -= 1;
    info.logical_cores_per_cpu -= 1 * info.threads_per_cpu;
    info.total_cores = cpus.size() * info.logical_cores_per_cpu;
}

// gpu info
void SystemInfo::init_gpu() {
    std::string cmd = "lspci -nnv |grep NVIDIA |awk '{print $2\",\"$1}' |tr \"\n\" \"|\"";
    std::string str = run_shell(cmd.c_str());
    std::vector<std::string> vec;
    util::split(str, "|", vec);
    std::string cur_id;
    for (int i = 0; i < vec.size(); i++) {
        std::vector<std::string> vec2;
        util::split(vec[i], ",", vec2);
        if (vec2[0] == "VGA") {
            cur_id = vec2[1];
            m_gpuinfo[cur_id].id = cur_id;
        }
        m_gpuinfo[cur_id].devices.push_back(vec2[1]);
    }
}

// disk info
void SystemInfo::get_disk_info(const std::string &path, disk_info &info) {
    char dpath[256] = "/"; //设置默认位置

    if(!path.empty()) {
        strcpy(dpath, path.c_str());
    }

    struct statfs diskInfo;
    if(-1 == statfs(dpath, &diskInfo)) {
        return;
    }

    uint64_t block_size = diskInfo.f_bsize; //每块包含字节大小
    info.disk_total = (diskInfo.f_blocks * block_size) >> 10; //磁盘总空间
    info.disk_awalible = (diskInfo.f_bavail * block_size) >> 10; //非超级用户可用空间
    info.disk_free = (diskInfo.f_bfree * block_size) >> 10; //磁盘所有剩余空间
    info.disk_usage = (info.disk_total - info.disk_awalible) * 1.0 / info.disk_total;

    std::string cmd = "df -l " + std::string(dpath) + " | tail -1";
    std::string tmp = run_shell(cmd.c_str());
    if (tmp[tmp.size() - 1] == '\n')
        tmp.erase(tmp.size() - 1, 1);
    cmd = "echo " + tmp + " | awk -F' ' '{print $1}' | awk -F\"/\" '{print $3}'";
    tmp = run_shell(cmd.c_str());
    if (tmp[tmp.size() - 1] == '\n') {
        tmp.erase(tmp.size() - 1, 1);
    }

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

// cpu usage
float SystemInfo::get_cpu_usage() {
    return m_cpu_usage;
}

// cpu info
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
        sscanf(buff, "%s%u%u%u%u", p[i+1].name, &(p[i+1].user), &(p[i+1].nice), &(p[i+1].system), &(p[i+1].idle));
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

void SystemInfo::update_cpu_usage() {
    int cpu_num = sysconf(_SC_NPROCESSORS_CONF);
    struct occupy *ocpu = new occupy[cpu_num + 1];
    struct occupy *ncpu = new occupy[cpu_num + 1];

    while (m_running) {
        m_public_ip = get_public_ip();

        get_occupy(ocpu, cpu_num);
        sleep(3);
        get_occupy(ncpu, cpu_num);
        m_cpu_usage = cal_occupy(&ocpu[0], &ncpu[0]);

        mem_info _mem_info;
        get_mem_info(_mem_info);
        m_meminfo = _mem_info;

        disk_info _disk_info;
        if (m_is_compute_node)
            get_disk_info("/data", _disk_info);
        else
            get_disk_info("/", _disk_info);
        m_diskinfo = _disk_info;
    }

    delete[] ocpu;
    delete[] ncpu;
}


