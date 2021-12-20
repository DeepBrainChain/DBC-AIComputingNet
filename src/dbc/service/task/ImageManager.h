#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"

struct DownloadEvent {
    std::string task_id;
    std::vector<std::string> images;
};

struct UploadEvent {
    std::string image;
};

class ImageManager : Singleton<ImageManager> {
public:
    ImageManager();

    virtual ~ImageManager();

    void PushDownloadEvent(const DownloadEvent &ev);

    void PushUploadEvent(const UploadEvent &ev);

private:
    std::list<DownloadEvent> m_download_events;
    std::list<UploadEvent> m_upload_events;
};


#endif
