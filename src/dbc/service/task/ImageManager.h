#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"
#include "config/conf_manager.h"

struct DownloadImageEvent {
    std::string task_id;
    ImageServer svr;
    std::vector<std::string> images_name;
};

struct UploadImageEvent {
    std::string task_id;
    ImageServer svr;
    std::string image_name;
};

class ImageDownloader;
class ImageUploader;

class ImageManager : public Singleton<ImageManager> {
public:
    ImageManager();

    virtual ~ImageManager();

    void ListShareImages(const std::vector<std::string>& image_server, std::vector<std::string> &images);

    void ListLocalBaseImages(std::vector<std::string>& images);

    void ListLocalShareImages(const std::vector<std::string>& image_server, std::vector<std::string> &images);

    void ListWalletLocalShareImages(const std::string& wallet, const std::vector<std::string>& image_server, std::vector<std::string> &images);

    void PushDownloadEvent(const DownloadImageEvent &ev, const std::function<void()>& after_callback = nullptr);

    void PushUploadEvent(const UploadImageEvent &ev);

private:
    ImageDownloader* m_downloader = nullptr;
    ImageUploader* m_uploader = nullptr;
};

typedef ImageManager ImageMgr;

#endif
