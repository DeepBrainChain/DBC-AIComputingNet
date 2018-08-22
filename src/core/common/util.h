/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   util.h
* description    :   common util tool 
* date                  :   2018.03.21
* author            :   Bruce Feng
**********************************************************************************/

#pragma once
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4101)
#endif

#include <string>
#include <vector>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <assert.h>
#include <boost/asio.hpp>
//#include "log.h"

#if defined(WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sysinfoapi.h>

#undef WIN32_LEAN_AND_MEAN
#elif defined(__linux__)
#include <limits.h>
#include <sys/sysinfo.h>
#include <sys/vfs.h> 
#ifdef _POSIX_C_SOURCE
#undef _POSIX_C_SOURCE
#endif
#define _POSIX_C_SOURCE 200112L
#include <sys/stat.h>
#include <sys/resource.h>
#elif defined(MAC_OSX)
#include <libproc.h>
#endif

#include <algorithm>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#if ! defined(WIN32)
#include <unistd.h>
#endif


namespace bf = boost::filesystem;

using namespace boost::asio::ip;


namespace matrix
{
    namespace core
    {
        class string_util
        {
        public:

            static void split(const std::string & str, const std::string& delim, std::vector<std::string> &vec)
            {
                size_t pos = 0;
                size_t index = str.find_first_of(delim, pos);
                while (index != std::string::npos)
                {
                    //push back
                    vec.push_back(str.substr(pos, index - pos));

                    pos = index + 1;
                    index = str.find_first_of(delim, pos);
                }

                //index is npos, and left string to end
                std::string left = str.substr(pos, std::string::npos);
                if (left.size() > 0)
                {
                    vec.push_back(left);
                }
            }

            static void split(char *str, char delim, int &argc, char* argv[])
            {
                if (nullptr == str || nullptr == argv || argc <= 0)
                {
                    argc = 0;
                    return;
                }

                int i = 0;
                char *p = str;

                while (*p)
                {
                    if (delim == *p)
                    {
                        *p++ = '\0';
                        continue;
                    }

                    argv[i++] = p;
                    if (i >= argc)
                    {
                        return;         //argc is unchanged
                    }

                    while (*p && delim != *p)
                    {
                        p++;
                    }
                }

                argc = i;
            }

            static void trim(std::string & str)
            {
                if (str.empty())
                {
                    return;
                }

                //header
                if (str[0] == ' ')
                {
                    str.erase(0, str.find_first_not_of(" "));
                }

                //tail
                if (str[str.length() - 1] == ' ')
                {
                    str.erase(str.find_last_not_of(" ") + 1);
                }
            }

            static std::string fuzz_ip(const std::string &ip)
            {
                try
                {
                    boost::asio::ip::address addr = boost::asio::ip::make_address(ip);
                    if (addr.is_v4())
                    {
                        boost::asio::ip::address_v4::bytes_type bytes = addr.to_v4().to_bytes();
                        return "*.*." + std::to_string(bytes[2]) + "." + std::to_string(bytes[3]);
                    }
                    else if (addr.is_v6())
                    {
                        //boost::asio::ip::address_v6::bytes_type bytes = addr.to_v6().to_bytes();
                        return "*:*:*:*:*:*:*:*";
                    }
                    else
                    {
                        return "*.*.*.*";
                    }
                }
                catch (const boost::exception & e)
                {
                    return "*.*.*.*";
                }
            }
            /**
             * trim indicated character from string's end
             * @param s, the string
             * @param c, the character
             * @return the new string
             */
            static std::string rtrim(std::string& s, char c)
            {
                if (s.size() == 0)
                {
                    return "";
                }

                int i = ((int) s.length()) - 1;

                for(; i>=0; i--)
                {
                    if(s[i] != c)
                        break;
                }

                return s.substr(0,i+1);
            }

            static std::string remove_leading_zero(std::string str)
            {
                bool is_input_str_empty = str.empty();
                if (is_input_str_empty)
                {
                    return str;
                }

                str.erase(0, str.find_first_not_of('0'));

                return str.empty() ? "0" : str;
            }

        };

        class file_util
        {
        public:
            //u can provide a std::string to file_name directly
            static bool write_file(const bf::path file_name, const std::string &str, std::ios_base::openmode mode = std::ios_base::out)
            {
                //create or open
                bf::ofstream ofs;
                try
                {
                    if (!file_name.has_filename())
                        return false;

                    if (bf::exists(file_name))
                    {
                        if (bf::is_other(file_name))
                            return false;
                    }
                    
                    ofs.open(file_name, mode);
                    ofs.write(str.c_str(), str.size());
                    ofs.close();

                    return true;
                }
                catch (...)
                {
                    try
                    {
                        ofs.close();
                    }
                    catch (...)
                    {
                    }
                    return false;
                }
            }

            static bool read_file(const bf::path file_name, std::string &str, std::ios_base::openmode mode = std::ios_base::in)
            {
                if (!bf::exists(file_name))
                {
                    return false;
                }
                //clean str
                str.clear();
                bf::ifstream ifs(file_name, mode);
                try
                {
                    if (ifs)
                    {
                        ifs.seekg(0, ifs.end);
                        int filesize = (int) ifs.tellg();
                        
                        if (filesize > 0)
                        { 
                            ifs.seekg(0, ifs.beg);
                            str.resize(filesize, 0x00);
                            ifs.read(&str[0], filesize);
                        }

                        ifs.close();
                        return true;
                    }
                    ifs.close();
                    return false;
                }
                catch (...)
                {
                    //cout << e.what() << endl;
                    try
                    {
                        ifs.close();
                    }
                    catch (...)
                    {
                    }
                    return false;
                }
            }
        };

        class path_util
        {
        public:
            static bool create_dir(const std::string &dir_name)
            {
                if (dir_name.empty())
                {
                    return false;
                }

                try
                {
                    bf::path pt(dir_name);
                    if (bf::exists(pt))
                    {
                        if (bf::is_other(pt))
                        {
                            return false;
                        }
                        return true;
                    }

                    bf::create_directories(pt);
                    return bf::exists(pt);
                }
                catch (bf::filesystem_error &e)
                {
                    //cout << e.what() << endl;
                    return false;
                }
                catch (...)
                {
                    return false;
                }
            }

            static bf::path get_exe_dir()
            {
                bf::path exe_dir;// = bf::current_path();

#if defined(WIN32)
                char szFilePath[MAX_PATH + 1] = { 0 };
                if (GetModuleFileNameA(NULL, szFilePath, MAX_PATH))
                {
                    exe_dir = szFilePath;
                    exe_dir.remove_filename();
                }
                else
                {
                    //int err = ::GetLastError();
                }

#elif defined(__linux__)
                char szFilePath[PATH_MAX + 1] = { 0 };
                int ret = readlink("/proc/self/exe", szFilePath, PATH_MAX);
                if (ret > 0 && ret < PATH_MAX)
                {
                    exe_dir = szFilePath;
                    exe_dir.remove_filename();
                }
#elif defined(MAC_OSX)
                char szFilePath[PROC_PIDPATHINFO_MAXSIZE] = { 0 };

                pid_t pid = getpid();
                int ret = proc_pidpath (pid, szFilePath, sizeof(szFilePath));
                if ( ret > 0 )
                {
                    exe_dir = szFilePath;
                    exe_dir.remove_filename();
                }
#endif
                return exe_dir;
            }

            static bf::path get_user_appdata_path()
            {
                try
                {
#if defined(WIN32)
                    static std::wstring s_strAppPath;
                    if (s_strAppPath.empty())
                    {
                        wchar_t buf[MAX_PATH];
                        GetEnvironmentVariableW(L"USERPROFILE", buf, sizeof(buf) / sizeof(wchar_t));
                        std::wstring appDataPath = buf;

                        OSVERSIONINFO osvi;
                        ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
                        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
                        BOOL bRet = GetVersionEx(&osvi);
                        if (bRet && osvi.dwMajorVersion >= 6)
                        {
                            appDataPath += L"\\AppData\\Local";//vista and above
                            s_strAppPath = appDataPath;
                            bf::path pt(s_strAppPath);
                            create_dir(pt.string());
                        }
                        else
                        {
                            bf::path pt = get_exe_dir();//xp and below
                            s_strAppPath = pt.wstring();
                        }
                    }

                    return bf::path(s_strAppPath);
#else
                    return get_exe_dir();
#endif
                }
                catch (bf::filesystem_error &e)
                {
                    //cout << e.what() << endl;
                    return bf::path();
                }
                catch (...)
                {
                    return bf::path();
                }
            }

            static bf::path get_user_appdata_dbc_path()
            {
                bf::path pt = get_user_appdata_path() /= "DBC";
                create_dir(pt.string());
                return pt;
            }
        };

        /**
        * this function tries to raise the file descriptor limit to the requested number.
        * It returns the actual file descriptor limit (which may be more or less than nMinFD)
        */
        inline int32_t RaiseFileDescriptorLimit(int nMinFD) {
#if defined(WIN32)
            return 2048;
#else
            struct rlimit limitFD;
            if (getrlimit(RLIMIT_NOFILE, &limitFD) != -1) {
                if (limitFD.rlim_cur < (rlim_t)nMinFD) {
                    limitFD.rlim_cur = nMinFD;
                    if (limitFD.rlim_cur > limitFD.rlim_max)
                        limitFD.rlim_cur = limitFD.rlim_max;
                    setrlimit(RLIMIT_NOFILE, &limitFD);
                    getrlimit(RLIMIT_NOFILE, &limitFD);
                }
                return limitFD.rlim_cur;
            }
            return nMinFD; // getrlimit failed, assume it's fine
#endif
        }

        inline void get_sys_mem(int64_t & mem, int64_t & mem_swap)
        {
#if defined(WIN32)
            MEMORYSTATUSEX memoryInfo;
            memoryInfo.dwLength = sizeof(memoryInfo);
            GlobalMemoryStatusEx(&memoryInfo);
            mem = memoryInfo.ullTotalPhys;
            mem_swap = memoryInfo.ullTotalVirtual;
#elif defined(__linux__)
            struct sysinfo s_info;
            if (0 == sysinfo(&s_info))
            {
                mem = s_info.totalram;
                mem_swap = mem + s_info.totalswap;
            }
#endif
        }

        inline uint32_t get_disk_free(const std::string & disk_path)
        {
#if defined(__linux__)
            struct statfs diskInfo;
            statfs(disk_path.c_str(), &diskInfo);
            uint64_t b_size = diskInfo.f_bsize;

            uint64_t free = b_size * diskInfo.f_bfree;
            //MB
            free = free >> 20;
            return free;
#endif
            return 0;
        }

        inline uint32_t get_disk_total(const std::string & disk_path)
        {
#if defined(__linux__)
            struct statfs diskInfo;
            statfs(disk_path.c_str(), &diskInfo);
            uint64_t b_size = diskInfo.f_bsize;
            uint64_t totalSize = b_size * diskInfo.f_blocks;

            uint32_t total = totalSize >> 20;
            return total;
#endif
            return 0;
        }

    }

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif