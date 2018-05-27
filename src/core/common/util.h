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

namespace bf = boost::filesystem;


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

        };

        class file_util
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
                    if (!bf::portable_directory_name(dir_name))
                        return false;

                    bf::path pt(dir_name);
                    if (!bf::is_directory(pt))
                        return false;

                    if (bf::exists(pt))
                    {
                        if (bf::is_other(pt))
                        {
                            return false;
                        }
                        return true;
                    }
                
                    bf::create_directory(pt);
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

            //u can provide a std::string to file_name directly
            static bool write_file(const bf::path file_name, const std::string &str, std::ios_base::openmode mode = std::ios_base::out)
            {
                try
                {
                    if (!file_name.has_filename())
                        return false;

                    if (bf::exists(file_name))
                    {
                        if (bf::is_other(file_name))
                            return false;
                    }

                    //create or open
                    bf::ofstream ofs;
                    ofs.open(file_name, mode);
                    ofs.write(str.c_str(), str.size());
                    ofs.close();

                    return true;
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

            static bool read_file(const bf::path file_name, std::string &str, std::ios_base::openmode mode = std::ios_base::in)
            {
                if (!bf::exists(file_name))
                {
                    return false;
                }
                //clean str
                str.clear();

                try
                {
                    bf::ifstream ifs(file_name, mode);
                    if (ifs)
                    {
                        ifs.seekg(0, ifs.end);
                        int filesize = (int) ifs.tellg();                        
                        ifs.seekg(0, ifs.beg);
                        if (filesize > 0)
                        {
                            str.reserve(filesize);
                            while (!ifs.eof())
                            {
                                str += ifs.get();//todo ...
                            }
                        }
                    }
                    ifs.close();

                    return true;
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
        };

    }

}

#ifdef _MSC_VER
#pragma warning(pop)
#endif