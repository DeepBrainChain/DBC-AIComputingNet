/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : container_resource_mng.cpp
* description       : 
* date              : 2019/1/10
* author            : Regulus
**********************************************************************************/
#ifdef _WIN64
#include "ansi_win_conv.h"
#include "error.h"
#include <assert.h>
#include <thread>
using namespace ai::dbc;

ansi_win_conv::~ansi_win_conv()
{
    release_dll();
}

int32_t ansi_win_conv::load_dll()
{
    typedef void(*RemoteLoad64)(LPPROCESS_INFORMATION);
    typedef int(*ProcessType)(LPPROCESS_INFORMATION, PBYTE*, BOOL*);
    typedef void(*InjectDLL) (LPPROCESS_INFORMATION ppi, PBYTE pBase);

    RemoteLoad64 remote_Load = nullptr;
    ProcessType Process_Type = nullptr;
    InjectDLL inject_dll = nullptr;

    m_ansi_mod = boost::winapi::LoadLibraryA("ANSI64.dll");
    assert(m_ansi_mod);
    remote_Load = (RemoteLoad64)GetProcAddress(m_ansi_mod, "RemoteLoad64");
    Process_Type = (ProcessType)GetProcAddress(m_ansi_mod, "ProcessType");
    inject_dll = (InjectDLL)GetProcAddress(m_ansi_mod, "InjectDLL");

    if (nullptr == remote_Load || nullptr == Process_Type || nullptr == inject_dll)
    {
        return E_DEFAULT;
    }
    PROCESS_INFORMATION pi;
    pi.dwProcessId = GetCurrentProcessId();
    pi.hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pi.dwProcessId);
    if (pi.hProcess == nullptr)
    {
        return E_DEFAULT;
    }

    PBYTE base;
    int32_t pro_ret = Process_Type(&pi, &base, nullptr);
    inject_dll(&pi, base);
    CloseHandle(pi.hProcess);
    return E_SUCCESS;
}

void ansi_win_conv::release_dll()
{   
    if (m_ansi_mod)
    {
        boost::winapi::FreeLibrary(m_ansi_mod);
        //if don't sleep, system will crash after freelibrary.  waiting for resource release. 
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
    
    return;
}
#endif