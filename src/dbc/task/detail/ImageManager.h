#ifndef DBCPROJ_IMAGEMANAGER_H
#define DBCPROJ_IMAGEMANAGER_H

#include "util/utils.h"
#include "config/conf_manager.h"
#include <boost/process.hpp>

class ImageManager : public Singleton<ImageManager> {
public:
    ImageManager();

    virtual ~ImageManager();

    static std::string CommandListImage(const std::string& host, const std::string& port = "873", const std::string& modulename = "images");
    
    static std::string CommandDownloadImage(const std::string& filename, const std::string& local_dir, const std::string& host, const std::string& port = "873", const std::string& modulename = "images");
	
    static std::string CommandUploadImage(const std::string& local_file, const std::string& host, const std::string& port = "873", const std::string& modulename = "images");

	ERRCODE Init();

	void Exit();

    void ListShareImages(const ImageServer& image_server, std::vector<std::string> &images);

    void ListLocalBaseImages(std::vector<std::string>& images);

    void ListLocalShareImages(const ImageServer& image_server, std::vector<std::string> &images);

    void ListWalletLocalShareImages(const std::string& wallet, const ImageServer& image_server, std::vector<std::string> &images);

    void Download(const std::string& image_name, const std::string& local_dir, 
        const ImageServer& from_server, const std::function<void()>& finish_callback = nullptr);

    void TerminateDownload(const std::string& image_name);

    bool IsDownloading(const std::string& image_name);

    void Upload(const std::string& imagefile_name, const ImageServer& to_server, const std::function<void()>& finish_callback = nullptr);

    void TerminateUpload(const std::string& image_name);

    bool IsUploading(const std::string& image_name);

protected:
    void thread_check_handle();

private:
    std::map<std::string, std::shared_ptr<boost::process::child> > m_download_images;
    std::map<std::string, std::function<void()> > m_download_finish_callback;
    RwMutex m_download_mtx;
    std::map<std::string, std::shared_ptr<boost::process::child> > m_upload_images;
    std::map<std::string, std::function<void()> > m_upload_finish_callback;
    RwMutex m_upload_mtx;
	std::thread* m_thread_check = nullptr;
	std::atomic<bool> m_running{ false };
};

typedef ImageManager ImageMgr;

#endif
