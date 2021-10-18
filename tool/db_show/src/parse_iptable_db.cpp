#include "comm_def.h"
#include <iosfwd>
#include "protocol.h"
#include "TToString.h"
using namespace matrix::core;
namespace matrix {
namespace service_core {
class db_iptable;
class db_iptable : public virtual ::apache::thrift::TBase {
public:
    db_iptable(const db_iptable& other0) {
        task_id = other0.task_id;
        host_ip = other0.host_ip;
        vm_local_ip = other0.vm_local_ip;
        ssh_port = other0.ssh_port;
    }
    db_iptable& operator=(const db_iptable& other1) {
        task_id = other1.task_id;
        host_ip = other1.host_ip;
        vm_local_ip = other1.vm_local_ip;
        ssh_port = other1.ssh_port;
        return *this;
    }
    db_iptable() : task_id(), host_ip(), vm_local_ip(), ssh_port() {}

    virtual ~db_iptable() throw() {}

    std::string task_id;
    std::string host_ip;
    std::string vm_local_ip;
    std::string ssh_port;

    void __set_task_id(const std::string& val) {task_id = val;}

    void __set_host_ip(const std::string& val) {host_ip = val;}

    void __set_vm_local_ip(const std::string& val) {vm_local_ip = val;}

    void __set_ssh_port(const std::string& val) {ssh_port = val;}

    bool operator == (const db_iptable & rhs) const
    {
        if (!(task_id == rhs.task_id))
            return false;
        if (!(host_ip == rhs.host_ip))
            return false;
        if (!(vm_local_ip == rhs.vm_local_ip))
            return false;
        if (!(ssh_port == rhs.ssh_port))
            return false;
        return true;
    }
    bool operator != (const db_iptable &rhs) const {
        return !(*this == rhs);
    }

    bool operator < (const db_iptable & ) const;

    uint32_t read(::apache::thrift::protocol::TProtocol* iprot) {
        ::apache::thrift::protocol::TInputRecursionTracker tracker(*iprot);
        uint32_t xfer = 0;
        std::string fname;
        ::apache::thrift::protocol::TType ftype;
        int16_t fid;

        xfer += iprot->readStructBegin(fname);

        using ::apache::thrift::protocol::TProtocolException;

        bool isset_task_id = false;
        bool isset_host_ip = false;
        bool isset_vm_local_ip = false;
        bool isset_ssh_port = false;

        while (true)
        {
            xfer += iprot->readFieldBegin(fname, ftype, fid);
            if (ftype == ::apache::thrift::protocol::T_STOP) {
                break;
            }
            switch (fid)
            {
                case 1:
                    if (ftype == ::apache::thrift::protocol::T_STRING) {
                        xfer += iprot->readString(this->task_id);
                        isset_task_id = true;
                    } else {
                        xfer += iprot->skip(ftype);
                    }
                    break;
                    case 2:
                        if (ftype == ::apache::thrift::protocol::T_STRING) {
                            xfer += iprot->readString(this->host_ip);
                            isset_host_ip = true;
                        } else {
                            xfer += iprot->skip(ftype);
                        }
                        break;
                        case 3:
                            if (ftype == ::apache::thrift::protocol::T_STRING) {
                                xfer += iprot->readString(this->vm_local_ip);
                                isset_vm_local_ip = true;
                            } else {
                                xfer += iprot->skip(ftype);
                            }
                            break;
                            case 4:
                                if (ftype == ::apache::thrift::protocol::T_STRING) {
                                    xfer += iprot->readString(this->ssh_port);
                                    isset_ssh_port = true;
                                } else {
                                    xfer += iprot->skip(ftype);
                                }
                                break;
                                default:
                                    xfer += iprot->skip(ftype);
                                    break;
            }
            xfer += iprot->readFieldEnd();
        }

        xfer += iprot->readStructEnd();

        if (!isset_task_id)
            throw TProtocolException(TProtocolException::INVALID_DATA);
        if (!isset_host_ip)
            throw TProtocolException(TProtocolException::INVALID_DATA);
        if (!isset_vm_local_ip)
            throw TProtocolException(TProtocolException::INVALID_DATA);
        if (!isset_ssh_port)
            throw TProtocolException(TProtocolException::INVALID_DATA);
        return xfer;
    }
    uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const {
        uint32_t xfer = 0;
        ::apache::thrift::protocol::TOutputRecursionTracker tracker(*oprot);
        xfer += oprot->writeStructBegin("task_iptable");

        xfer += oprot->writeFieldBegin("task_id", ::apache::thrift::protocol::T_STRING, 1);
        xfer += oprot->writeString(this->task_id);
        xfer += oprot->writeFieldEnd();

        xfer += oprot->writeFieldBegin("host_ip", ::apache::thrift::protocol::T_STRING, 2);
        xfer += oprot->writeString(this->host_ip);
        xfer += oprot->writeFieldEnd();

        xfer += oprot->writeFieldBegin("vm_local_ip", ::apache::thrift::protocol::T_STRING, 3);
        xfer += oprot->writeString(this->vm_local_ip);
        xfer += oprot->writeFieldEnd();

        xfer += oprot->writeFieldBegin("ssh_port", ::apache::thrift::protocol::T_STRING, 4);
        xfer += oprot->writeString(this->ssh_port);
        xfer += oprot->writeFieldEnd();

        xfer += oprot->writeFieldStop();
        xfer += oprot->writeStructEnd();
        return xfer;
    }

    virtual void printTo(std::ostream& out) const {
        using ::apache::thrift::to_string;
        out << "task_iptable(";
        out << "task_id=" << to_string(task_id);
        out << ", " << "host_ip=" << to_string(host_ip);
        out << ", " << "vm_local_ip=" << to_string(vm_local_ip);
        out << ", " << "ssh_port=" << to_string(ssh_port);
        out << ")";
    }
};

void swap(db_iptable &a, db_iptable &b) {
    using ::std::swap;
    swap(a.task_id, b.task_id);
    swap(a.host_ip, b.host_ip);
    swap(a.vm_local_ip, b.vm_local_ip);
    swap(a.ssh_port, b.ssh_port);
}

std::ostream& operator<<(std::ostream& out, const db_iptable& obj) {
    obj.printTo(out);
    return out;
}

}
}

void parse_iptable_db(const char *path)
{
    leveldb::DB      *db;
    leveldb::Options  options;

    options.create_if_missing = false;
    options.reuse_logs = false;
    std::shared_ptr<leveldb::DB> iptable_db;

    try {
        // open
        leveldb::Status status = leveldb::DB::Open(options, path, &db);
        if (db == nullptr) {
            cout << "open db failed: " << status.ToString() << endl;
            exit(0);
        }
        iptable_db.reset(db);
        leveldb::ReadOptions r_options;
        r_options.snapshot = db->GetSnapshot();

        std::unique_ptr<leveldb::Iterator> it;
        it.reset(iptable_db->NewIterator(r_options));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
            // cout << "key: " << it->key().ToString() << ",value: " << it->value().ToString() << endl;
            std::shared_ptr<byte_buf> buf(new byte_buf);
            buf->write_to_byte_buf(it->value().data(), (uint32_t)it->value().size());
            binary_protocol proto(buf.get());
            matrix::service_core::db_iptable db_iptable;
            db_iptable.read(&proto);
            cout << db_iptable << endl;
        }

        db->ReleaseSnapshot(r_options.snapshot);
    }
    catch (const std::exception &e) {
        cout << e.what() << endl;
    }
    catch (...) {
        cout << "unknowned error" << endl;
    }
}
