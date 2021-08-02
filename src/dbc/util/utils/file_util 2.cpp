#include "file_util.h"

namespace util {
    bool write_file(const boost::filesystem::path file_name, const std::string& str, std::ios_base::openmode mode)
    {
        //create or open
        boost::filesystem::ofstream ofs;
        try
        {
            if(!file_name.has_filename())
            {
                return false;
            }

            if(boost::filesystem::exists(file_name))
            {
                if(boost::filesystem::is_other(file_name))
                {
                    return false;
                }
            }

            ofs.open(file_name, mode);
            ofs.write(str.c_str(), str.size());
            ofs.close();

            return true;
        }
        catch(...)
        {
            try
            {
                ofs.close();
            }
            catch(...)
            {
            }
            return false;
        }
    }

    bool read_file(const boost::filesystem::path file_name, std::string& str, std::ios_base::openmode mode)
    {
        if(!boost::filesystem::exists(file_name))
        {
            return false;
        }
        //clean str
        str.clear();
        boost::filesystem::ifstream ifs(file_name, mode);
        try
        {
            if(ifs)
            {
                ifs.seekg(0, ifs.end);
                int filesize = (int) ifs.tellg();

                if(filesize > 0)
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
        catch(...)
        {
            //cout << e.what() << endl;
            try
            {
                ifs.close();
            }
            catch(...)
            {
            }
            return false;
        }
    }
}
