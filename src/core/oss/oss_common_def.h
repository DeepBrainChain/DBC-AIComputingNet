#pragma once
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "error/en.h"
enum OSS_ERROR
{
    OSS_NET_ERROR = -1,
    OSS_SUCCESS = 0
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