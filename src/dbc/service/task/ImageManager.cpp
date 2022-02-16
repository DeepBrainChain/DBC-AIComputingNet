#include "ImageManager.h"
#include "util/threadpool.h"
#include "log/log.h"
#include "config/conf_manager.h"
#include "WalletRentTaskManager.h"
#include "vm/vm_client.h"

class ImageDownloader {
public:
    ImageDownloader() {

    }

    ~ImageDownloader() {
        delete m_threadpool;
    }

    void add(const DownloadImageEvent &ev, const std::function<void()>& after_callback) {
        if (m_threadpool == nullptr) {
            m_threadpool = new threadpool(5);
        }

        m_taskid_total[ev.task_id] = ev.images_name.size();
        m_taskid_cur[ev.task_id] = 0;
        m_taskid_after_callback[ev.task_id] = after_callback;

        struct timeval tv{};
        gettimeofday(&tv, 0);
        int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        m_taskid_begin[ev.task_id] = t;

        for (auto& it : ev.images_name) {
            std::string task_id = ev.task_id;
            ImageServer svr = ev.svr;
            std::string image = it;
            m_threadpool->commit([this, task_id, svr, image]() {
                struct timeval tv1{};
                gettimeofday(&tv1, 0);
                int64_t t1 = tv1.tv_sec * 1000 + tv1.tv_usec / 1000;

                std::string dbc_dir = util::get_exe_dir().string();
                std::string cmd = dbc_dir + "/shell/image/rsync_download.sh " + svr.ip + " " + svr.port + " " + svr.username + " " +
                          svr.passwd + " " + svr.image_dir + "/" + image + " /data/";
                LOG_INFO << "begin download, cmd: " << cmd;
                std::string ret = run_shell(cmd.c_str());

                struct timeval tv2{};
                gettimeofday(&tv2, 0);
                int64_t t2 = tv2.tv_sec * 1000 + tv2.tv_usec / 1000;
                LOG_INFO << "ret:" << ret << ", download " << image << " ok! use: " << (t2 - t1) << "ms";

                m_taskid_cur[task_id]++;
                if (m_taskid_cur[task_id] == m_taskid_total[task_id]) {
                    struct timeval tv{};
                    gettimeofday(&tv, 0);
                    int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                    int64_t msec = t - m_taskid_begin[task_id];
                    LOG_INFO << "download all file ok! use: " << msec << "ms";

                    m_taskid_total.erase(task_id);
                    m_taskid_cur.erase(task_id);
                    m_taskid_begin.erase(task_id);
                    auto it = m_taskid_after_callback.find(task_id);
                    if (it != m_taskid_after_callback.end()) {
                        if (it->second != nullptr) {
                            it->second();
                        }
                    }
                    m_taskid_after_callback.erase(task_id);
                }
            });
        }
    }

private:
    threadpool *m_threadpool = nullptr;
    std::map<std::string, std::atomic<int>> m_taskid_total;
    std::map<std::string, std::atomic<int>> m_taskid_cur;
    std::map<std::string, int64_t> m_taskid_begin;
    std::map<std::string, std::function<void()>> m_taskid_after_callback;
};

class ImageUploader {
public:
    ImageUploader() {

    }

    ~ImageUploader() {
        delete m_threadpool;
    }

    void add(const UploadImageEvent &ev) {
        if (m_threadpool == nullptr) {
            m_threadpool = new threadpool(5);
        }

        m_taskid_total[ev.task_id] = 1;
        m_taskid_cur[ev.task_id] = 0;

        struct timeval tv{};
        gettimeofday(&tv, 0);
        int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        m_taskid_begin[ev.task_id] = t;

        std::string task_id = ev.task_id;
        ImageServer svr = ev.svr;
        std::string image = ev.image_name;
        m_threadpool->commit([this, task_id, svr, image] () {
            struct timeval tv1{};
            gettimeofday(&tv1, 0);
            int64_t t1 = tv1.tv_sec * 1000 + tv1.tv_usec / 1000;

            std::string dbc_dir = util::get_exe_dir().string();
            std::string cmd = dbc_dir + "/shell/image/rsync_upload.sh " + svr.ip + " " + svr.port + " " + svr.username + " " +
                      svr.passwd + " " + "/data/" + image + " " + svr.image_dir + "/";
            LOG_INFO << "begin upload, cmd: " << cmd;
            std::string ret = run_shell(cmd.c_str());

            struct timeval tv2{};
            gettimeofday(&tv2, 0);
            int64_t t2 = tv2.tv_sec * 1000 + tv2.tv_usec / 1000;
            LOG_INFO << "ret:" << ret << ", upload " << image << " ok! use: " << (t2 - t1) << "ms";

            m_taskid_cur[task_id]++;
            if (m_taskid_cur[task_id] == m_taskid_total[task_id]) {
                struct timeval tv{};
                gettimeofday(&tv, 0);
                int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                int64_t msec = t - m_taskid_begin[task_id];
                LOG_INFO << "upload all file ok! use: " << msec << "ms";

                m_taskid_total.erase(task_id);
                m_taskid_cur.erase(task_id);
                m_taskid_begin.erase(task_id);
            }
        });
    }

private:
    threadpool *m_threadpool = nullptr;
    std::map<std::string, std::atomic<int>> m_taskid_total;
    std::map<std::string, std::atomic<int>> m_taskid_cur;
    std::map<std::string, int64_t> m_taskid_begin;
};

ImageManager::ImageManager() {
    m_downloader = new ImageDownloader();
    m_uploader = new ImageUploader();
}

ImageManager::~ImageManager() {
    delete m_downloader;
    delete m_uploader;
}

void ImageManager::ListShareImages(const std::vector<std::string>& image_server, std::vector<std::string> &images) {
    if (image_server.empty()) return;

    ImageServer svr;
    svr.from_string(image_server[0]);

    std::string dbc_dir = util::get_exe_dir().string();
    std::string ret_default = run_shell(
    dbc_dir + "/shell/image/list_file.sh " + svr.ip + " " + svr.port + " " + svr.username + " " +
            svr.passwd + " " + svr.image_dir);
    auto pos = ret_default.find_last_of('\n');
    std::string str = ret_default.substr(pos + 1);
    std::vector<std::string> v_images = util::split(str, ",");
    for (int i = 0; i < v_images.size(); i++) {
        std::string fname = v_images[i];
        images.push_back(fname);
    }
}

void ImageManager::ListLocalBaseImages(std::vector<std::string> &images) {
    std::vector<std::string> vecRunningDomains;
    VmClient::instance().ListAllRunningDomains(vecRunningDomains);
    std::set<std::string> setRunningImages;
    for (int i = 0; i < vecRunningDomains.size(); i++) {
        std::map<std::string, domainDiskInfo> mp;
        VmClient::instance().ListDomainDiskInfo(vecRunningDomains[i], mp);
        for (auto& iter : mp) {
            std::string fname = boost::filesystem::path(iter.second.sourceFile).filename().string();
            setRunningImages.insert(fname);
        }
    }

    boost::filesystem::path local_image_dir = "/data";
    boost::filesystem::directory_iterator iterend;
    for (boost::filesystem::directory_iterator iter(local_image_dir); iter != iterend; iter++) {
        if (boost::filesystem::is_regular_file(iter->path()) &&
            iter->path().extension().string() == ".qcow2" &&
            iter->path().filename().string().find("data") == std::string::npos) {
            if (setRunningImages.count(iter->path().filename().string()) <= 0) {
                std::string cmd_backing_file = "qemu-img info " + iter->path().string() +
                                               " | grep -w 'backing file:' | awk -F ': ' '{print $2}'";
                std::string backing_file = run_shell(cmd_backing_file);
                if (backing_file.empty()) {
                    images.push_back(iter->path().filename().string());
                }
            }
        }
    }
}

void ImageManager::ListLocalShareImages(const std::vector<std::string> &image_server, std::vector<std::string> &images) {
    // local base images
    std::vector<std::string> vecRunningDomains;
    VmClient::instance().ListAllRunningDomains(vecRunningDomains);
    std::set<std::string> setRunningImages;
    for (int i = 0; i < vecRunningDomains.size(); i++) {
        std::map<std::string, domainDiskInfo> mp;
        VmClient::instance().ListDomainDiskInfo(vecRunningDomains[i], mp);
        for (auto& iter : mp) {
            std::string fname = boost::filesystem::path(iter.second.sourceFile).filename().string();
            setRunningImages.insert(fname);
        }
    }

    std::set<std::string> set_images;
    boost::filesystem::path local_image_dir = "/data";
    boost::filesystem::directory_iterator iterend;
    for (boost::filesystem::directory_iterator iter(local_image_dir); iter != iterend; iter++) {
        if (boost::filesystem::is_regular_file(iter->path()) &&
            iter->path().extension().string() == ".qcow2" &&
            iter->path().filename().string().find("data") == std::string::npos) {
            if (setRunningImages.count(iter->path().filename().string()) <= 0) {
                std::string cmd_backing_file = "qemu-img info " + iter->path().string() +
                                               " | grep -w 'backing file:' | awk -F ': ' '{print $2}'";
                std::string backing_file = run_shell(cmd_backing_file);
                if (backing_file.empty()) {
                    images.push_back(iter->path().filename().string());
                } else {
                    set_images.insert(iter->path().filename().string());
                }
            }
        }
    }

    // mix local and remote
    std::vector<std::string> vec_share_images;
    ListShareImages(image_server, vec_share_images);
    for (int i = 0; i < vec_share_images.size(); i++) {
        if (set_images.count(vec_share_images[i]) > 0) {
            images.push_back(vec_share_images[i]);
        }
    }
}

void ImageManager::ListWalletLocalShareImages(const std::string &wallet, const std::vector<std::string> &image_server,
                                              std::vector<std::string> &images) {
    std::vector<std::string> vec_images;
    ImageMgr::instance().ListLocalShareImages(image_server, vec_images);

    std::set<std::string> set_images;
    std::shared_ptr<dbc::rent_task> rent_task = WalletRentTaskMgr::instance().getRentTask(wallet);
    if (rent_task != nullptr) {
        for (auto& id : rent_task->task_ids) {
            virDomainState status = VmClient::instance().GetDomainStatus(id);
            if (status != VIR_DOMAIN_RUNNING) {
                std::map<std::string, domainDiskInfo> mp;
                VmClient::instance().ListDomainDiskInfo(id, mp);

                for (auto &iter: mp) {
                    std::string fname = util::GetFileNameFromPath(iter.second.sourceFile);
                    if (iter.first == "vda") {
                        set_images.insert(fname);
                    }
                }
            }
        }
    }

    for (int i = 0; i < vec_images.size(); i++) {
        if (set_images.count(vec_images[i]) <= 0) {
            images.push_back(vec_images[i]);
        }
    }

    for (auto& iter : set_images) {
        images.push_back(iter);
    }
}

void ImageManager::PushDownloadEvent(const DownloadImageEvent &ev, const std::function<void()>& after_callback) {
    if (m_downloader != nullptr)
        m_downloader->add(ev, after_callback);
}

void ImageManager::PushUploadEvent(const UploadImageEvent &ev) {
    if (m_uploader != nullptr)
        m_uploader->add(ev);
}

