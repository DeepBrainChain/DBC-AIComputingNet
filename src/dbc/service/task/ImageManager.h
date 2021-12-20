#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"

struct DownloadImageEvent {
    std::string task_id;
    std::vector<std::string> images;
};

struct UploadImageEvent {
    std::string image;
};

class ImageManager : Singleton<ImageManager> {
public:
    ImageManager();

    virtual ~ImageManager();

    void PushDownloadEvent(const DownloadImageEvent &ev);

    void PushUploadEvent(const UploadImageEvent &ev);

private:
    std::list<DownloadImageEvent> m_download_events;
    std::list<UploadImageEvent> m_upload_events;
};


#endif
