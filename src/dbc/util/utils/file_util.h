#ifndef DBC_FILE_UTIL_H
#define DBC_FILE_UTIL_H

#include <iostream>
#include <string>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

namespace util {
    bool write_file(const boost::filesystem::path file_name, const std::string& str, std::ios_base::openmode mode = std::ios_base::out);

    bool read_file(const boost::filesystem::path file_name, std::string& str, std::ios_base::openmode mode = std::ios_base::in);
}

#endif //DBC_FILE_UTIL_H
