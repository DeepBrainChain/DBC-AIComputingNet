#include "log.h"

const char * get_short_file_name(const char * file_path)
{
    if (nullptr == file_path || '\0' == *file_path)
    {
        return "";
    }

    const char * p = file_path;
    const char *short_file_name = file_path;

    while (*p)
    {
        if ('\\' == *p || '/' == *p)
        {
            short_file_name = p + 1;
        }

        p++;
    }

    return short_file_name;
}

const char* get_short_func_name(const char* func_name)
{
    std::string str_fn = func_name;
    size_t pos = str_fn.rfind(":");
    if (pos != std::string::npos)
    {
        return func_name + pos + 1;
    }
    
    return func_name;
}
