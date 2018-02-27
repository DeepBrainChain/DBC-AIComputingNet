/******************************************************************************

                  版权所有 (C), , 

 ******************************************************************************
  文 件 名   : usb_hid.cpp
  版 本 号   : 初稿
  作    者   : Bruce Feng
  生成日期   : 2017年8月4日
  最近修改   :
  功能描述   : 本文件负责USB HID接口
  函数列表   :
  修改历史   :
  1.日    期   : 2017年8月4日
    作    者   : Bruce Feng
    修改内容   : 创建文件

******************************************************************************/

#ifndef _USB_HID_H_
#define _USB_HID_H_

#include "common.h"

class USBHID
{
public:

    USBHID() {}

    virtual int32_t init();

    void add_data(uint8_t *data_to_send, uint8_t length);

    void send();

    
};

#endif

