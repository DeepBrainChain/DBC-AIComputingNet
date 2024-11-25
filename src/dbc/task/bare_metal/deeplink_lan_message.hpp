#ifndef DBC_DEEPLINK_LAN_MESSAGE_H
#define DBC_DEEPLINK_LAN_MESSAGE_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

class deeplink_lan_message {
public:
    enum { header_length = 4 };
    enum { max_body_length = 512 };

    deeplink_lan_message() : body_length_(0) {
        memset(data_, 0, header_length + max_body_length);
    }

    const char* data() const { return data_; }

    char* data() { return data_; }

    unsigned int length() const { return header_length + body_length_; }

    const char* body() const { return data_ + header_length; }

    char* body() { return data_ + header_length; }

    unsigned int body_length() const { return body_length_; }

    void body_length(unsigned int new_length) {
        body_length_ = new_length;
        if (body_length_ > max_body_length) body_length_ = max_body_length;
    }

    bool decode_header() {
        char header[header_length + 1] = "";
        std::strncat(header, data_, header_length);
        // body_length_ = std::atoi(header);
        // body_length_ = *reinterpret_cast<unsigned long long*>(header);
        body_length_ = *reinterpret_cast<unsigned int*>(header);
        if (body_length_ > max_body_length) {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header() {
        // char header[header_length + 1] = "";
        // std::sprintf(header, "%4d", static_cast<int>(body_length_));
        // std::memcpy(data_, header, header_length);
        unsigned int length = body_length_;
        char* arrLen = reinterpret_cast<char*>(&length);
        std::memcpy(data_, arrLen, header_length);
    }

private:
    char data_[header_length + max_body_length];
    unsigned int body_length_;
};

#endif  // DBC_DEEPLINK_LAN_MESSAGE_H
