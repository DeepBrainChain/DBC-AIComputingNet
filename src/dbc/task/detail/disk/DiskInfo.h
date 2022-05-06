#ifndef DBC_DISK_INFO_H
#define DBC_DISK_INFO_H

#include "util/utils.h"

class DiskInfo {
public:
    friend class TaskDiskManager;

    DiskInfo() = default;

    virtual ~DiskInfo() = default;

    std::string getName() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_name;
    }

    void setName(const std::string& name) {
        RwMutex::WriteLock wlock(m_mtx);
        m_name = name;
    }

    int64_t getVirtualSize() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_virtual_size;
    }

    void setVirtualSize(int64_t n) {
        RwMutex::WriteLock wlock(m_mtx);
        m_virtual_size = n;
    }

    std::string getSourceFile() const {
        RwMutex::ReadLock rlock(m_mtx);
        return m_source_file;
    }

    void setSourceFile(const std::string& file) {
        RwMutex::WriteLock wlock(m_mtx);
        m_source_file = file;
    }

private:
    mutable RwMutex m_mtx;
    std::string m_name;		    //磁盘名称
    int64_t m_virtual_size = 0;	//虚拟大小（字节）
    std::string m_source_file;	//文件绝对路径
};

#endif