/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : service_bus.h
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年1月20日
  最近修改   :
  功能描述   : 定义服务总线层的对外接口定义
  函数列表   :
  修改历史   :
  1.日    期   : 2017年1月20日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _SERVICE_BUS_H_
#define _SERVICE_BUS_H_

#ifdef __RTX

#include <string.h>
#include <stdlib.h>

#include "common.h"


/**
 * pointer to subscriber handler.
 */
typedef void * SUBSCRIBER_HANDLE;

/**
 * pointer to publiser handler.
 */
typedef void * PUBLISHER_HANDLE;

typedef void * TOPIC_ENTRY_HANDLE;



/**
 * topic name.
 */
#define TOPIC_NAME(name)   name##_TOPIC


/**
 * topic id: pointer to topic variable.
 */
#define TOPIC_ID(name)     &name##_TOPIC



/**
 * topic metadata definition: define topic name, topic size length, padding, extra fields informations.
 */
struct topic_meta_data 
{
    char *topic_path;                         /**< unique topic path */
    char *topic_name;                         /**< unique topic name */
    uint32_t topic_size;                          /**< topic size */
    char *topic_fields;                         /**< semicolon separated list of fields (with type) */
};


struct subscriber_meta_data 
{
    uint32_t max_mail_count_in_box;
};


/**
 * topic declaration: constant meta data declaration to extern.
 */
#if defined(__cplusplus)
# define TOPIC_DECLARE(name)       extern "C" const topic_meta_data TOPIC_NAME(name) __EXPORT
#else
# define TOPIC_DECLARE(name)       extern        const topic_meta_data TOPIC_NAME(name) __EXPORT
#endif

/**
 * topic macro definition: name, size, fields.
 */
#define TOPIC_DEFINE(path, name, _struct, _fields)     const struct topic_meta_data TOPIC_NAME(name) = {#path, #name, sizeof(_struct), _fields};



__BEGIN_DECLS


/*****************************************************************************
 *
 *
                                                     topic publisher api
 *
 *
 *****************************************************************************/


extern __EXPORT PUBLISHER_HANDLE register_topic(const topic_meta_data *meta);

extern __EXPORT int32_t publish_topic_data(PUBLISHER_HANDLE handle, const void *topic_data);

extern __EXPORT int32_t unregister_topic(PUBLISHER_HANDLE handle);


/*****************************************************************************
 *
 *
                                                     topic subscriber api
 *
 *
 *****************************************************************************/


extern __EXPORT SUBSCRIBER_HANDLE subscribe_topic(const topic_meta_data *meta);

extern __EXPORT int32_t unsubscribe_topic(SUBSCRIBER_HANDLE handle);

extern __EXPORT int32_t poll_topic(SUBSCRIBER_HANDLE handle, uint16_t timeout);

extern __EXPORT int32_t check_topic(SUBSCRIBER_HANDLE handle, bool &updated);

extern __EXPORT bool exists_topic(const topic_meta_data *meta);

extern __EXPORT int32_t copy_topic_data(const topic_meta_data *meta, SUBSCRIBER_HANDLE handle, void *buffer);

extern __EXPORT int32_t set_subscriber(SUBSCRIBER_HANDLE handle, const subscriber_meta_data *meta);


__END_DECLS;


#endif

#endif

/******************* (C) COPYRIGHT 2017 Robot OS -- Bruce Feng *****END OF FILE****/

