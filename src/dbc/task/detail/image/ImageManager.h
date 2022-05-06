#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include <boost/process.hpp>

struct DownloadImageFile {
	ImageFile file;
	std::string local_dir;
	ImageServer from_server;
	std::shared_ptr<boost::process::child> process_ptr;
};

struct UploadImageFile {
	ImageFile file;
	ImageServer to_server;
	std::shared_ptr<boost::process::child> process_ptr;
};

class ImageManager : public Singleton<ImageManager> {
public:
    friend class TaskManager;

    ImageManager();

    virtual ~ImageManager();

    static std::string CommandListImage(const std::string& host, const std::string& port = "873", const std::string& modulename = "images");
    
    static std::string CommandQueryImageSize(const std::string& image_filename, const std::string& host, const std::string& port = "873", const std::string& modulename = "images");

    static std::string CommandDownloadImage(const std::string& filename, const std::string& local_dir, const std::string& host, const std::string& port = "873", const std::string& modulename = "images");
	
    static std::string CommandUploadImage(const std::string& local_file, const std::string& host, const std::string& port = "873", const std::string& modulename = "images");

	FResult init();

	void exit();

    void listShareImages(const ImageServer& image_server, std::vector<ImageFile>& imagefiles);

    void listLocalBaseImages(std::vector<ImageFile>& imagefiles);

    void listLocalShareImages(const ImageServer& image_server, std::vector<ImageFile>& imagefiles);

    void listWalletLocalShareImages(const std::string& wallet, const ImageServer& image_server, std::vector<ImageFile>& imagefiles);

    FResult download(const std::string& imagefile_name, const std::string& local_dir,
        const ImageServer& from_server, const std::function<void()>& finish_callback = nullptr);

    float downloadProgress(const std::string& imagefile_name);

    void terminateDownload(const std::string& imagefile_name);

    bool isDownloading(const std::string& imagefile_name);

    FResult upload(const std::string& imagefile_name, const ImageServer& to_server, const std::function<void()>& finish_callback = nullptr);

    float uploadProgress(const std::string& imagefile_name);

    void terminateUpload(const std::string& imagefile_name);

    bool isUploading(const std::string& imagefile_name);

    void deleteImage(const std::string& imagefile_name);

protected:
    void thread_check_handle();

private:
    RwMutex m_download_mtx;
    std::map<std::string, DownloadImageFile> m_download_images;
    std::map<std::string, std::function<void()> > m_download_finish_callback;
    
	RwMutex m_upload_mtx;
    std::map<std::string, UploadImageFile> m_upload_images;
    std::map<std::string, std::function<void()> > m_upload_finish_callback;
    
    std::thread* m_thread_check = nullptr;
	std::atomic<bool> m_running{ false };
};

typedef ImageManager ImageMgr;

#endif
