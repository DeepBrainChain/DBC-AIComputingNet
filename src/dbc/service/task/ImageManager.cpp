#include "ImageManager.h"
#include "util/threadpool.h"

struct ImageEvent {
    std::string task_id;
    std::string image;
};

class ImageDownloader {
public:
    void add(const DownloadImageEvent &ev) {
        m_taskid_total[ev.task_id] = ev.images.size();
        m_taskid_cur[ev.task_id] = 0;

        struct timeval tv{};
        gettimeofday(&tv, 0);
        int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        m_taskid_begin[ev.task_id] = t;

        for (auto& it : ev.images) {
            std::string task_id = ev.task_id;
            std::string image = it;
            m_threadpool.commit([this, task_id, image] () {
                struct timeval tv1{};
                gettimeofday(&tv1, 0);
                int64_t t1 = tv1.tv_sec * 1000 + tv1.tv_usec / 1000;

                std::string cmd = "cp nfs_dbc_images/" + image + " /data";
                std::string ret = run_shell(cmd.c_str());

                struct timeval tv2{};
                gettimeofday(&tv2, 0);
                int64_t t2 = tv2.tv_sec * 1000 + tv2.tv_usec / 1000;
                std::cout << "ret:" << ret << ", download " << image << " ok! use: " << (t2 - t1) << "ms" << std::endl;

                m_taskid_cur[task_id]++;
                if (m_taskid_cur[task_id] == m_taskid_total[task_id]) {
                    struct timeval tv{};
                    gettimeofday(&tv, 0);
                    int64_t t = tv.tv_sec * 1000 + tv.tv_usec / 1000;
                    int64_t msec = t - m_taskid_begin[task_id];
                    std::cout << "download all file ok! use: " << msec << "ms" << std::endl;
                }
            });
        }
    }

private:
    threadpool m_threadpool{10};
    std::map<std::string, std::atomic<int>> m_taskid_total;
    std::map<std::string, std::atomic<int>> m_taskid_cur;
    std::map<std::string, int64_t> m_taskid_begin;
};

class ImageUploader {
public:
    void add(const UploadImageEvent &ev) {

    }

private:
    std::list<UploadImageEvent> m_upload_events;
};

ImageManager::ImageManager() {
    m_downloader = new ImageDownloader();
    m_uploader = new ImageUploader();
}

ImageManager::~ImageManager() {
    delete m_downloader;
    delete m_uploader;
}

void ImageManager::PushDownloadEvent(const DownloadImageEvent &ev) {
    if (m_downloader != nullptr)
        m_downloader->add(ev);
}

void ImageManager::PushUploadEvent(const UploadImageEvent &ev) {
    if (m_uploader != nullptr)
        m_uploader->add(ev);
}

