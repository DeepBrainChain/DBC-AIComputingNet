#ifndef DBC_VERSION_H
#define DBC_VERSION_H

//major version . minor version . revision version . build version
#define CORE_VERSION                        0x00030704
#define PROTOCO_VERSION                     0x00000001

#define STR_CONV(v)  #v
#define STR_VER(v)  STR_CONV(v)

inline std::string dbcversion() {
    std::string ver = STR_VER(CORE_VERSION);
    // major version
    std::string major_ver = ver.substr(2, 2);
    major_ver.erase(0, major_ver.find_first_not_of('0'));
    if (major_ver.empty()) major_ver = "0";
    // minor version
    std::string minor_ver = ver.substr(4, 2);
    minor_ver.erase(0, minor_ver.find_first_not_of('0'));
    if (minor_ver.empty()) minor_ver = "0";
    // revision version
    std::string revision_ver = ver.substr(6, 2);
    revision_ver.erase(0, revision_ver.find_first_not_of('0'));
    if (revision_ver.empty()) revision_ver = "0";
    // build version
    std::string build_ver = ver.substr(8, 2);
    build_ver.erase(0, build_ver.find_first_not_of('0'));
    if (build_ver.empty()) build_ver = "0";

    return major_ver + "." + minor_ver + "." + revision_ver + "." + build_ver;
}

#endif //DBC_VERSION_H
