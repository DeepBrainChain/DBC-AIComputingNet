#include "ImageManager.h"
#include "util/threadpool.h"
#include "log/log.h"
#include "config/conf_manager.h"
#include "WalletRentTaskManager.h"
#include "vm/vm_client.h"

ImageManager::ImageManager() {
    m_pthread = new std::thread(&ImageManager::thread_check_handle, this);
}

ImageManager::~ImageManager() {
    m_running = false;
    if (m_pthread != nullptr && m_pthread->joinable()) {
        m_pthread->join();
    }
    delete m_pthread;
}

std::string ImageManager::CommandListImage(const std::string& host, const std::string& port /* = "873" */, 
    const std::string& modulename /* = "images" */) {
    std::string _modulename = modulename;
    if (!_modulename.empty() && (*_modulename.end()) != '/')
        _modulename += "/";

    return "rsync -az --timeout=600  --list-only --port " + port + " " + host + "::" + _modulename +
        " | grep ^- | awk -F' ' '{print $5}' |paste -s -d, |tr -d '[[:space:]]'";
}

std::string ImageManager::CommandDownloadImage(const std::string& filename, const std::string& local_dir, const std::string& host,
    const std::string& port /* = "873" */, const std::string& modulename /* = "images" */) {
	std::string _modulename = modulename;
	if (!_modulename.empty() && (*_modulename.end()) != '/')
		_modulename += "/";

    std::string _local_dir = local_dir;
	if (!_local_dir.empty() && (*_local_dir.end()) != '/')
        _local_dir += "/";

    return "rsync -az --delete --timeout=600 --partial --progress --port " + port + " " + host +
        "::" + _modulename + filename + " " + local_dir;
}

std::string ImageManager::CommandUploadImage(const std::string& local_file, const std::string& host, 
    const std::string& port /* = "873" */, const std::string& modulename /* = "images" */) {
	std::string _modulename = modulename;
	if (!_modulename.empty() && (*_modulename.end()) != '/')
		_modulename += "/";
    
    return "rsync -az --delete --timeout=600 --partial --progress --port " + port + " " + local_file
        + " " + host + "::" + _modulename;
}

void ImageManager::ListShareImages(const ImageServer& image_server, std::vector<std::string> &images) {
    if (image_server.ip.empty() || image_server.port.empty() || image_server.modulename.empty()) 
        return;
    
    std::string cmd = CommandListImage(image_server.ip, image_server.port, image_server.modulename);
    std::string ret_default = run_shell(cmd);
    std::string str = util::rtrim(ret_default, '\n');
    std::vector<std::string> v_images = util::split(str, ",");
    for (int i = 0; i < v_images.size(); i++) {
        std::string fname = v_images[i];
        if (boost::filesystem::path(fname).extension().string() == ".qcow2")
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

void ImageManager::ListLocalShareImages(const ImageServer &image_server, std::vector<std::string> &images) {
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

void ImageManager::ListWalletLocalShareImages(const std::string &wallet, const ImageServer &image_server,
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

void ImageManager::Download(const std::string& image_name, const std::string& local_dir, const ImageServer& from_server,
                            const std::function<void()>& finish_callback) {
	if (from_server.ip.empty() || from_server.port.empty() || from_server.modulename.empty()) 
        return;
 
    std::string cmd = CommandDownloadImage(image_name, local_dir, from_server.ip, from_server.port, from_server.modulename);
    std::shared_ptr<boost::process::child> pull_image =
            std::make_shared<boost::process::child>(cmd, boost::process::std_out > boost::process::null,
                                                    boost::process::std_err > boost::process::null);
    pull_image->running();

    RwMutex::WriteLock wlock(m_download_mtx);
    m_download_images[image_name] = pull_image;
    if (finish_callback != nullptr) {
        m_download_finish_callback[image_name] = finish_callback;
    }
}

void ImageManager::TerminateDownload(const std::string &image_name) {
    RwMutex::WriteLock wlock(m_download_mtx);
    auto pull_image = m_download_images.find(image_name);
    if (pull_image->second != nullptr) {
        if (pull_image->second->running()) {
            pull_image->second->terminate();
        }

        m_download_images.erase(image_name);
        m_download_finish_callback.erase(image_name);
    }
}

bool ImageManager::IsDownloading(const std::string &image_name) {
    RwMutex::ReadLock rlock(m_download_mtx);
    auto iter = m_download_images.find(image_name);
    return iter != m_download_images.end();
}

void ImageManager::Upload(const std::string& imagefile_name, const ImageServer& to_server,
                          const std::function<void()>& finish_callback) {
	if (to_server.ip.empty() || to_server.port.empty() || to_server.modulename.empty()) 
        return;

    std::string cmd = CommandUploadImage(imagefile_name, to_server.ip, to_server.port, to_server.modulename);
    std::shared_ptr<boost::process::child> push_image =
            std::make_shared<boost::process::child>(cmd, boost::process::std_out > boost::process::null,
                                                    boost::process::std_err > boost::process::null);
    push_image->running();

    RwMutex::WriteLock wlock(m_upload_mtx);
    m_upload_images[imagefile_name] = push_image;
    if (finish_callback != nullptr) {
        m_upload_finish_callback[imagefile_name] = finish_callback;
    }
}

void ImageManager::TerminateUpload(const std::string &image_name) {
    RwMutex::WriteLock wlock(m_upload_mtx);
    auto push_image = m_upload_images.find(image_name);
    if (push_image->second != nullptr) {
        if (push_image->second->running()) {
            push_image->second->terminate();
        }

        m_upload_images.erase(image_name);
        m_upload_finish_callback.erase(image_name);
    }
}

bool ImageManager::IsUploading(const std::string &image_name) {
    RwMutex::ReadLock rlock(m_upload_mtx);
    auto iter = m_upload_images.find(image_name);
    return iter != m_upload_images.end();
}

void ImageManager::thread_check_handle() {
    while (m_running) {
        {
            RwMutex::WriteLock wlock(m_download_mtx);
            for (auto iter = m_download_images.begin(); iter != m_download_images.end(); iter++) {
                if (0 == iter->second->exit_code()) {
                    auto callback = m_download_finish_callback.find(iter->first);
                    if (callback->second != nullptr) {
                        callback->second();
                    }

                    m_download_finish_callback.erase(iter->first);
                    iter = m_download_images.erase(iter);
                }
            }
        }

        {
            RwMutex::WriteLock wlock(m_upload_mtx);
            for (auto iter = m_upload_images.begin(); iter != m_upload_images.end(); iter++) {
                if (0 == iter->second->exit_code()) {
                    auto callback = m_upload_finish_callback.find(iter->first);
                    if (callback->second != nullptr) {
                        callback->second();
                    }

                    m_upload_finish_callback.erase(iter->first);
                    iter = m_upload_images.erase(iter);
                }
            }
        }

        sleep(1);
    }
}