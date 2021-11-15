#include "path_util.h"
#include <climits>

namespace util {
    bool create_dir(const std::string& dir_name)
    {
        if(dir_name.empty())
        {
            return false;
        }

        try
        {
            boost::filesystem::path pt(dir_name);
            if(boost::filesystem::exists(pt))
            {
                if(boost::filesystem::is_other(pt))
                {
                    return false;
                }
                return true;
            }

            boost::filesystem::create_directories(pt);
            return boost::filesystem::exists(pt);
        }
        catch(boost::filesystem::filesystem_error& e)
        {
            //cout << e.what() << endl;
            return false;
        }
        catch(...)
        {
            return false;
        }
    }

    boost::filesystem::path get_exe_dir()
    {
        boost::filesystem::path exe_dir;// = boost::filesystem::current_path();

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
        char szFilePath[PROC_PIDPATHINFO_MAXSIZE] = {0};

                pid_t pid = getpid();
                int ret = proc_pidpath(pid, szFilePath, sizeof(szFilePath));
                if(ret > 0)
                {
                    exe_dir = szFilePath;
                    exe_dir.remove_filename();
                }
#endif
        return exe_dir;
    }

    boost::filesystem::path get_user_appdata_path()
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
                            boost::filesystem::path pt(s_strAppPath);
                            create_dir(pt.string());
                        }
                        else
                        {
                            boost::filesystem::path pt = get_exe_dir();//xp and below
                            s_strAppPath = pt.wstring();
                        }
                    }

                    return boost::filesystem::path(s_strAppPath);
#else
            return get_exe_dir();
#endif
        }
        catch(boost::filesystem::filesystem_error& e)
        {
            //cout << e.what() << endl;
            return boost::filesystem::path();
        }
        catch(...)
        {
            return boost::filesystem::path();
        }
    }

    boost::filesystem::path get_user_appdata_dbc_path()
    {
        boost::filesystem::path pt = get_user_appdata_path() /= "DBC";
        create_dir(pt.string());
        return pt;
    }

    std::string GetFileNameFromPath(const std::string& szPath)
    {
        std::string::size_type pos = szPath.find_last_of("\\/");
        std::string szName;
        if(pos != std::string::npos){
            szName = szPath.substr(pos + 1);
        }else
            szName = szPath;
        return szName;
    }

    std::string GetFileNameWithoutExt(const std::string& szFile)
    {
        std::string::size_type pos1 = szFile.find_last_of("/\\");
        std::string::size_type pos2 = szFile.rfind('.');
        pos1 = pos1 == std::string::npos ? 0 : pos1 + 1;
        if(pos2 == std::string::npos)
            pos2 = szFile.size();
        if(pos2 < pos1)
            pos2 = pos1;
        return szFile.substr(pos1, pos2 - pos1);
    }

    std::string GetFileExt(const std::string& szFile)
    {
        std::string szName = GetFileNameFromPath(szFile);
        std::string::size_type nPos = szName.rfind('.');
        if(nPos != std::string::npos)
            ++nPos;
        else
            nPos = szName.size();

        return szName.substr(nPos);
    }

    std::string GetBasePath(const std::string& szFullPath)
    {
        std::string::size_type pos = szFullPath.find_last_of("/\\");
        std::string szName;
        if(pos != std::string::npos){
            szName = szFullPath.substr(0, pos);
        }else
            szName = szFullPath;
        return szName;
    }
}