#ifndef DBCPROJ_CHECK_IMAGES_H
#define DBCPROJ_CHECK_IMAGES_H

#include <string>

namespace check_images {

// 方法一: 把镜像名称和md5硬编码写死
void test_images();

// 方法二: 在镜像文件服务器上放一个json文件，格式如下，下载后在本地检验
// {
//     "images":[
//         "ubuntu.qcow2":"98141663e1f51209c888c2def13f2437",
//         "ubuntu-2004.qcow2":"8de92f8383738d265bba5f2e789f19e0"
//     ]
// }
void test_images(std::string jsonurl);

}

#endif