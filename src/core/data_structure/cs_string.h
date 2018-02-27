/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : cs_string.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月25日
  最近修改   :
  功能描述   : simple string class for use
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月25日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _CS_STRING_H_
#define _CS_STRING_H_


#include <string.h>
#include <stdlib.h>

#include "common.h"

#pragma warning(disable : 4996)


class string
{
public:

    string() : m_psz(NULL) {}

    string(const char * psz)
    {
        copy(psz);
    }

    ~string()
    {
        clear();
    }

    void clear()
    {
        if (NULL != m_psz)
        {
            delete [] m_psz;
            m_psz = NULL;
         }
    }
    
    void copy(const char * psz)
    {
        size_t str_len = strlen(psz) + 1;
        
        m_psz = (char *)malloc(str_len);
        strncpy(m_psz, psz, str_len - 1);
        m_psz[str_len - 1] = 0;
    }

    void clear_and_copy(const char * psz)
    {
        clear();
        copy(psz);
    }

    char *get() const {return m_psz;}

    int compare(const char *psz) const
    {
        return strcmp(m_psz, psz);
    }

    string &operator=(const char *psz)
    {
        clear_and_copy(psz);
        return *this;        
    }

    friend bool operator==(const string &str, const char *psz)
    {
        return 0 == str.compare(psz);
    }

    friend bool operator==(const char *psz, const string &str)
    {
        return 0 == str.compare(psz);
    }

    friend bool operator==(const string &str1, const string &str2)
    {
        return 0 == str1.compare(str2.get());
    }    

private:

    char *m_psz;
};

#endif


