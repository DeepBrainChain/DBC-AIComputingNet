#include "check_images.h"
#include <openssl/md5.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <boost/filesystem.hpp>
#include <boost/system/error_code.hpp>

#define READ_DATA_SIZE 1024
#define MD5_SIZE       16

namespace check_images
{

struct ImageItem {
    std::string image_name;
    std::string image_md5;
};

static const ImageItem images[] = {
    {"ubuntu.qcow2", "98141663e1f51209c888c2def13f2437"},
    {"ubuntu-2004.qcow2", "8de92f8383738d265bba5f2e789f19e0"}
};


int compute_file_md5(const char* file, std::string& md5) {
    FILE* fd = fopen(file, "r");
    if (fd == NULL) {
        std::cout << "open file failed." << std::endl;
        return -1;
    }
    MD5_CTX ctx;
    unsigned char md5_value[MD5_SIZE] = {0};
    unsigned char data[READ_DATA_SIZE];
    int ret;
    MD5_Init(&ctx);
    while(0 != (ret = fread(data, 1, READ_DATA_SIZE, fd))) {
        MD5_Update(&ctx, data, ret);
    }
    fclose(fd);
    if (ret < 0) {
        std::cout << "read error" << std::endl;
        return -1;
    }
    MD5_Final(md5_value, &ctx);
    std::stringstream ss;
    for (int i = 0; i < MD5_SIZE; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (unsigned int)md5_value[i];
    }
    md5 = ss.str();
    return 0;
}

void test_image(const ImageItem& item) {
    std::string image_path, md5;
    boost::system::error_code error_code;
    image_path = "/data/" + item.image_name;
    if (!boost::filesystem::exists(image_path, error_code) || error_code) {
        std::cout << item.image_name << " does not exist" << std::endl;
        return ;
    }
    if (!boost::filesystem::is_regular_file(image_path, error_code) || error_code) {
        std::cout << item.image_name << " is not a regular file" << std::endl;
        return ;
    }
    std::cout << "find image: " << item.image_name << ", begin to calculate md5." << std::endl;
    int ret = compute_file_md5(image_path.c_str(), md5);
    if (ret == 0) {
        if (md5 == item.image_md5) {
            std::cout << "image: " << item.image_name << " md5 is correct." << std::endl;
        }
        else {
            std::cout << "image: " << item.image_name << " md5 is wrong." << std::endl;
        }
    }
    else {
        std::cout << "image: " << item.image_name << " calculate md5 failed." << std::endl;
    }
}

void test_images() {
    for(const auto item : images) {
        test_image(item);
    }
}

void test_images(std::string jsonurl) {
    // TODO: do something.
}

} // namespace name
