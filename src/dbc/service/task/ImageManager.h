#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"

struct DownloadImageEvent {
    std::string task_id;
    std::vector<std::string> images;
};

struct UploadImageEvent {
    std::string task_id;
    std::string image;
};

class ImageDownloader;
class ImageUploader;

class ImageManager : public Singleton<ImageManager> {
public:
    ImageManager();

    virtual ~ImageManager();

    void PushDownloadEvent(const DownloadImageEvent &ev, const std::function<void()>& after_callback = nullptr);

    void PushUploadEvent(const UploadImageEvent &ev);

private:
    ImageDownloader* m_downloader = nullptr;
    ImageUploader* m_uploader = nullptr;
};

typedef ImageManager ImageMgr;

#endif
