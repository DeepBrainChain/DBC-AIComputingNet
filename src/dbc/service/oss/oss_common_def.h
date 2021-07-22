#ifndef DBC_OSS_COMMON_DEF_H
#define DBC_OSS_COMMON_DEF_H

#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"
#include "log/log.h"

enum OSS_ERROR
{
    OSS_NET_ERROR = -1,
    OSS_SUCCESS = 0,

    OSS_SUCCESS_TASK = 1
};

inline int32_t parse_item_string(rapidjson::Document &doc, std::string item, std::string &target)
{
    if (!doc.HasMember(item.c_str()))
    {
        LOG_ERROR << "parse error. Do not have " << item;
        return E_DEFAULT;
    }
    rapidjson::Value &value = doc[item.c_str()];
    auto ret_type = value.GetType();
    if (ret_type != rapidjson::kStringType)
    {
        return E_DEFAULT;
    }
    target = value.GetString();
    return E_SUCCESS;
}

inline int32_t parse_item_int32(rapidjson::Document &doc, std::string item, int32_t &target)
{
    if (!doc.HasMember(item.c_str()))
    {
        LOG_ERROR << "parse error. Do not have " << item;
        return E_DEFAULT;
    }
    rapidjson::Value &value = doc[item.c_str()];
    auto ret_type = value.GetType();
    if (ret_type != rapidjson::kNumberType)
    {
        return E_DEFAULT;
    }
    target = value.GetInt();
    return E_SUCCESS;
}

inline int32_t parse_item_int64(rapidjson::Document &doc, std::string item, int64_t &target)
{
    if (!doc.HasMember(item.c_str()))
    {
        LOG_ERROR << "parse error. Do not have " << item;
        return E_DEFAULT;
    }
    rapidjson::Value &value = doc[item.c_str()];
    auto ret_type = value.GetType();
    if (ret_type != rapidjson::kNumberType)
    {
        return E_DEFAULT;
    }
    target = value.GetInt64();
    return E_SUCCESS;
}

#endif
