#include "ImageManager.h"
#include "util/threadpool.h"
#include "log/log.h"
#include "config/conf_manager.h"
#include "WalletRentTaskManager.h"
#include "../vm/vm_client.h"

ImageManager::ImageManager() {

}

ImageManager::~ImageManager() {

}

ERRCODE ImageManager::Init() {
	m_running = true;
	if (m_thread_check == nullptr) {
		m_thread_check = new std::thread(&ImageManager::thread_check_handle, this);
	}

    return ERR_SUCCESS;
}

void ImageManager::Exit() {
	m_running = false;
	if (m_thread_check != nullptr && m_thread_check->joinable()) {
		m_thread_check->join();
	}
	delete m_thread_check;
	m_thread_check = nullptr;

	do {
		RwMutex::WriteLock wlock(m_download_mtx);
		for (auto iter = m_download_images.begin(); iter != m_download_images.end(); ) {
            if (iter->second.process_ptr->running()) {
                iter->second.process_ptr->terminate();
            }

			m_download_finish_callback.erase(iter->first);
			iter = m_download_images.erase(iter);
		}
	} while (0);

	do {
		RwMutex::WriteLock wlock(m_upload_mtx);
		for (auto iter = m_upload_images.begin(); iter != m_upload_images.end(); ) {
			if (iter->second.process_ptr->running())
				iter->second.process_ptr->terminate();

			m_upload_finish_callback.erase(iter->first);
			iter = m_upload_images.erase(iter);
		}
	} while (0);
}

std::string ImageManager::CommandListImage(const std::string& host, const std::string& port /* = "873" */, 
    const std::string& modulename /* = "images" */) {
    std::string _modulename = modulename;
    if (!_modulename.empty() && (*_modulename.end()) != '/')
        _modulename += "/";

    return "rsync -zlptgoD --timeout=600  --list-only --port " + port + " " + host + "::" + _modulename +
        " | grep ^- | awk -F' ' '{print $5,$2}' |paste -s -d#";
}

std::string ImageManager::CommandQueryImageSize(const std::string& image_filename, const std::string& host, const std::string& port /* = "873" */,
    const std::string& modulename /* = "images" */) {
    std::string _modulename = modulename;
    if (!_modulename.empty() && (*_modulename.end()) != '/')
        _modulename += "/";

    return "rsync -zlptgoD --timeout=600  --list-only --port " + port + " " + host + "::" + _modulename +
        " | grep ^- | grep '" + image_filename + "' | head -1 | awk -F' ' '{print $5,$2}'";
}

std::string ImageManager::CommandDownloadImage(const std::string& filename, const std::string& local_dir, const std::string& host,
    const std::string& port /* = "873" */, const std::string& modulename /* = "images" */) {
	std::string _modulename = modulename;
	if (!_modulename.empty() && (*_modulename.end()) != '/')
		_modulename += "/";

    std::string _local_dir = local_dir;
	if (!_local_dir.empty() && (*_local_dir.end()) != '/')
        _local_dir += "/";

    return "rsync -za --delete --timeout=600 --partial --progress --port " + port + " " + host +
        "::" + _modulename + filename + " " + _local_dir;
}

std::string ImageManager::CommandUploadImage(const std::string& local_file, const std::string& host, 
    const std::string& port /* = "873" */, const std::string& modulename /* = "images" */) {
	std::string _modulename = modulename;
	if (!_modulename.empty() && (*_modulename.end()) != '/')
		_modulename += "/";
    
    return "rsync -za --delete --timeout=600 --partial --progress --port " + port + " " + local_file
        + " " + host + "::" + _modulename;
}

void ImageManager::ListShareImages(const ImageServer& image_server, std::vector<ImageFile> &images) {
    if (image_server.ip.empty() || image_server.port.empty() || image_server.modulename.empty()) 
        return;
    
    std::string cmd = CommandListImage(image_server.ip, image_server.port, image_server.modulename);
    std::string ret_default = run_shell(cmd);
    std::string str = util::rtrim(ret_default, '\n');
    std::vector<std::string> v_images = util::split(str, "#");
    for (int i = 0; i < v_images.size(); i++) {
        std::string str = v_images[i];
        std::vector<std::string> vec = util::split(str, " ");
        if (vec.size() < 2) continue;
        std::string fname = vec[0];
        std::string fsize = vec[1];
        if (boost::filesystem::path(fname).extension().string() != ".qcow2") continue;
        util::replace(fsize, ",", "");

        ImageFile image_file;
        image_file.name = fname;
        image_file.size = atoll(fsize.c_str());
        
        images.push_back(image_file);
    }
}

void ImageManager::ListLocalBaseImages(std::vector<ImageFile>& images) {
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
                    ImageFile image_file;
                    image_file.name = iter->path().filename().string();
                    image_file.size = bfs::file_size(iter->path());

                    images.push_back(image_file);
                }
            }
        }
    }
}

void ImageManager::ListLocalShareImages(const ImageServer &image_server, std::vector<ImageFile>& images) {
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
                    ImageFile image_file;
                    image_file.name = iter->path().filename().string();
                    image_file.size = bfs::file_size(iter->path());

                    images.push_back(image_file);
                } else {
                    set_images.insert(iter->path().filename().string());
                }
            }
        }
    }

    // mix local and remote
    std::vector<ImageFile> vec_share_images;
    ListShareImages(image_server, vec_share_images);
    for (int i = 0; i < vec_share_images.size(); i++) {
        if (set_images.count(vec_share_images[i].name) > 0) {
            images.push_back(vec_share_images[i]);
        }
    }
}

void ImageManager::ListWalletLocalShareImages(const std::string &wallet, const ImageServer &image_server, std::vector<ImageFile>& images) {
    std::vector<ImageFile> vec_images;
    ImageMgr::instance().ListLocalShareImages(image_server, vec_images);

    std::set<ImageFile> set_images;
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
                        ImageFile image_file;
                        image_file.name = fname;
                        image_file.size = bfs::file_size(iter.second.sourceFile);

                        set_images.insert(image_file);
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

FResult ImageManager::Download(const std::string& image_name, const std::string& local_dir, const ImageServer& from_server,
                            const std::function<void()>& finish_callback) {
	if (from_server.ip.empty() || from_server.port.empty() || from_server.modulename.empty()) 
        return FResult(ERR_ERROR, "image server is invalid");
    
    std::string cmd1 = CommandQueryImageSize(image_name, from_server.ip, from_server.port, from_server.modulename);
    std::string rsp1 = run_shell(cmd1);
    std::vector<std::string> vec_file = util::split(rsp1, " ");
    if (vec_file.size() < 2) 
        return FResult(ERR_ERROR, "query image file size failed");

    std::string filename = vec_file[0];
    std::string filesize = vec_file[1];
    util::replace(filesize, ",", "");

    ImageFile image_file;
    image_file.name = filename;
    image_file.size = atoll(filesize.c_str());
    
    std::string cmd = CommandDownloadImage(image_name, local_dir, from_server.ip, from_server.port, from_server.modulename);
    std::shared_ptr<boost::process::child> pull_image =
            std::make_shared<boost::process::child>(cmd, boost::process::std_out > boost::process::null,
                                                    boost::process::std_err > boost::process::null);
    pull_image->running();

    DownloadImageFile download_imagefile;
    download_imagefile.file = image_file;
    download_imagefile.local_dir = local_dir;
    download_imagefile.from_server = from_server;
    download_imagefile.process_ptr = pull_image;

    RwMutex::WriteLock wlock(m_download_mtx);
    m_download_images[image_name] = download_imagefile;
    if (finish_callback != nullptr) {
        m_download_finish_callback[image_name] = finish_callback;
    }

    return FResultOk;
}

float ImageManager::DownloadProgress(const std::string& image_name) {
    RwMutex::ReadLock rlock(m_download_mtx);
    auto it = m_download_images.find(image_name);
    if (it != m_download_images.end()) {
        std::string ret = run_shell("ls " + it->second.local_dir + "/ -a | grep '." + image_name + ".' | head -1");
        if (!ret.empty() && ret.find(image_name) != std::string::npos) {
            bfs::path fpath(it->second.local_dir + "/" + ret);
            int64_t fsize = bfs::file_size(fpath);
            return (fsize * 1.0 / it->second.file.size);
        }
        else {
            if (it->second.process_ptr->running())
                return 0.0f;
            else
                return 1.0f;
        }
    }
    else {
        return 1.0f;
    }
}

void ImageManager::TerminateDownload(const std::string &image_name) {
    RwMutex::WriteLock wlock(m_download_mtx);
    auto pull_image = m_download_images.find(image_name);
    if (pull_image != m_download_images.end()) {
        if (pull_image->second.process_ptr->running()) {
            pull_image->second.process_ptr->terminate();
        }
    }

	m_download_images.erase(image_name);
	m_download_finish_callback.erase(image_name);
}

bool ImageManager::IsDownloading(const std::string &image_name) {
    RwMutex::ReadLock rlock(m_download_mtx);
    auto iter = m_download_images.find(image_name);
    return iter != m_download_images.end();
}

FResult ImageManager::Upload(const std::string& imagefile_name, const ImageServer& to_server,
                          const std::function<void()>& finish_callback) {
	if (to_server.ip.empty() || to_server.port.empty() || to_server.modulename.empty()) 
        return FResult(ERR_ERROR, "image server is invalid");

    bfs::path fpath(imagefile_name);
    ImageFile image_file;
    image_file.name = fpath.filename().string();
    image_file.size = bfs::file_size(fpath);

    std::string cmd = CommandUploadImage(imagefile_name, to_server.ip, to_server.port, to_server.modulename);
    std::shared_ptr<boost::process::child> push_image =
            std::make_shared<boost::process::child>(cmd, boost::process::std_out > boost::process::null,
                                                    boost::process::std_err > boost::process::null);
    push_image->running();

    UploadImageFile upload_imagefile;
    upload_imagefile.file = image_file;
    upload_imagefile.to_server = to_server;
    upload_imagefile.process_ptr = push_image;

    RwMutex::WriteLock wlock(m_upload_mtx);
    m_upload_images[imagefile_name] = upload_imagefile;
    if (finish_callback != nullptr) {
        m_upload_finish_callback[imagefile_name] = finish_callback;
    }

    return FResultOk;
}

float ImageManager::UploadProgress(const std::string& image_name) {
    RwMutex::ReadLock rlock(m_upload_mtx);
    auto it = m_upload_images.find(image_name);
    if (it != m_upload_images.end()) {
        bfs::path fpath(image_name);
        std::string fname = "." + fpath.filename().string() + ".";
        std::string cmd1 = CommandQueryImageSize(fname, it->second.to_server.ip, it->second.to_server.port, 
            it->second.to_server.modulename);
        std::string rsp1 = run_shell(cmd1);
        std::vector<std::string> vec_file = util::split(rsp1, " ");
        if (vec_file.size() < 2) {
            if (it->second.process_ptr->running())
                return 0.0f;
            else
                return 1.0f;
        } 
        else {
            std::string filename = vec_file[0];
            std::string filesize = vec_file[1];
            util::replace(filesize, ",", "");
            int64_t fsize = atoll(filesize.c_str());
            return (fsize * 1.0 / it->second.file.size);
        }
    }
    else {
        return 1.0f;
    }
}

void ImageManager::TerminateUpload(const std::string &image_name) {
    RwMutex::WriteLock wlock(m_upload_mtx);
    auto push_image = m_upload_images.find(image_name);
    if (push_image != m_upload_images.end()) {
        if (push_image->second.process_ptr->running()) {
            push_image->second.process_ptr->terminate();
        }
    }

	m_upload_images.erase(image_name);
	m_upload_finish_callback.erase(image_name);
}

bool ImageManager::IsUploading(const std::string &image_name) {
    RwMutex::ReadLock rlock(m_upload_mtx);
    auto iter = m_upload_images.find(image_name);
    return iter != m_upload_images.end();
}

void ImageManager::DeleteImage(const std::string& image_name) {
    bfs::remove(bfs::path(image_name));
}

void ImageManager::thread_check_handle() {
    while (m_running) {
        {
            RwMutex::WriteLock wlock(m_download_mtx);
            for (auto iter = m_download_images.begin(); iter != m_download_images.end(); ) {
                if (!iter->second.process_ptr->running()) {
                    auto callback = m_download_finish_callback.find(iter->first);
                    if (callback->second != nullptr) {
                        callback->second();
                    }
                    
                    m_download_finish_callback.erase(iter->first);
                    iter = m_download_images.erase(iter);
                }
                else {
                    iter++;
                }
            }
        }

        {
            RwMutex::WriteLock wlock(m_upload_mtx);
            for (auto iter = m_upload_images.begin(); iter != m_upload_images.end(); ) {
                if (!iter->second.process_ptr->running()) {
                    auto callback = m_upload_finish_callback.find(iter->first);
                    if (callback->second != nullptr) {
                        callback->second();
                    }

                    m_upload_finish_callback.erase(iter->first);
                    iter = m_upload_images.erase(iter);
                }
                else {
                    iter++;
                }
            }
        }

        sleep(3);
    }
}