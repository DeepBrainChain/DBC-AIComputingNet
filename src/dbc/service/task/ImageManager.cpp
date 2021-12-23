#include "ImageManager.h"
#include "util/threadpool.h"
#include "log/log.h"

class ImageDownloader {
public:
    ImageDownloader() {

    }

    ~ImageDownloader() {
        delete m_threadpool;
    }

    void add(const DownloadImageEvent &ev, const std::function<void()>& after_callback) {
        if (m_threadpool == nullptr) {
            m_threadpool = new threadpool(10);
        }

        m_taskid_total[ev.task_id] = ev.images.size();
        m_taskid_cur[ev.task_id] = 0;
        m_taskid_after_callback[ev.task_id] = after_callback;

        struct timeval tv{};
        gettimeofday(&tv, 0);
        int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        m_taskid_begin[ev.task_id] = t;

        for (auto& it : ev.images) {
            std::string task_id = ev.task_id;
            std::string image = it;
            m_threadpool->commit([this, task_id, image] () {
                struct timeval tv1{};
                gettimeofday(&tv1, 0);
                int64_t t1 = tv1.tv_sec * 1000 + tv1.tv_usec / 1000;

                std::string cmd = "cp /nfs_dbc_images/" + image + " /data/" + image + ".cache";
                LOG_INFO << "begin download, cmd: " << cmd;
                std::string ret = run_shell(cmd.c_str());

                struct timeval tv2{};
                gettimeofday(&tv2, 0);
                int64_t t2 = tv2.tv_sec * 1000 + tv2.tv_usec / 1000;
                LOG_INFO << "ret:" << ret << ", download " << image << " ok! use: " << (t2 - t1) << "ms";

                cmd = "mv /data/" + image + ".cache /data/" + image;
                ret = run_shell(cmd.c_str());

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
        std::string image = ev.image;
        m_threadpool->commit([this, task_id, image] () {
            struct timeval tv1{};
            gettimeofday(&tv1, 0);
            int64_t t1 = tv1.tv_sec * 1000 + tv1.tv_usec / 1000;

            std::string cmd = "cp /data/" + image + " /nfs_dbc_images/" + image + ".cache";
            LOG_INFO << "begin upload, cmd: " << cmd;
            std::string ret = run_shell(cmd.c_str());

            struct timeval tv2{};
            gettimeofday(&tv2, 0);
            int64_t t2 = tv2.tv_sec * 1000 + tv2.tv_usec / 1000;
            LOG_INFO << "ret:" << ret << ", upload " << image << " ok! use: " << (t2 - t1) << "ms";

            cmd = "mv /nfs_dbc_images/" + image + ".cache /nfs_dbc_images/" + image;
            ret = run_shell(cmd.c_str());

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

FResult ImageManager::ListAllImages(std::vector<std::string> &images) {
    try {
        boost::filesystem::path imageFolder = "/nfs_dbc_images";
        boost::filesystem::directory_iterator iterend;
        for (boost::filesystem::directory_iterator iter(imageFolder); iter != iterend; iter++) {
            if (boost::filesystem::is_regular_file(iter->path()) && iter->path().extension().string() == ".qcow2") {
                images.push_back(iter->path().filename().string());
            }
        }
        return {E_SUCCESS, "ok"};
    }
    catch (const std::exception & e) {
        return {E_DEFAULT, std::string("list images file error: ").append(e.what())};
    }
    catch (const boost::exception & e) {
        return {E_DEFAULT, "list images file error: " + diagnostic_information(e)};
    }
    catch (...) {
        return {E_DEFAULT, "unknowned list images error"};
    }
    return {E_DEFAULT, "list images error"};
}

void ImageManager::PushDownloadEvent(const DownloadImageEvent &ev, const std::function<void()>& after_callback) {
    if (m_downloader != nullptr)
        m_downloader->add(ev, after_callback);
}

void ImageManager::PushUploadEvent(const UploadImageEvent &ev) {
    if (m_uploader != nullptr)
        m_uploader->add(ev);
}

