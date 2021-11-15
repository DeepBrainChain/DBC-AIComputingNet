#ifndef DBC_PATH_UTIL_H
#define DBC_PATH_UTIL_H

#include <iostream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace util {
    bool create_dir(const std::string& dir_name);

    boost::filesystem::path get_exe_dir();

    boost::filesystem::path get_user_appdata_path();

    boost::filesystem::path get_user_appdata_dbc_path();

    std::string GetFileNameFromPath(const std::string& szPath);

    std::string GetFileNameWithoutExt(const std::string& szFile);

    std::string GetFileExt(const std::string& szFile);

    std::string GetBasePath(const std::string& szFullPath);
}

#endif //DBC_PATH_UTIL_H
