#include "utils.h"

std::string run_shell(const std::string& cmd, const char* modes) {
    if (cmd.empty()) return "";

    std::string ret;
    std::string strcmd = cmd + " 2>&1";
    FILE * fp = nullptr;
    char buffer[1024] = {0};
    fp = popen(strcmd.c_str(), modes);
    if (fp != nullptr) {
        while(fgets(buffer, sizeof(buffer), fp)) {
            ret += std::string(buffer);
        }
        pclose(fp);
    } else {
        ret = std::string("run_shell failed! ") + strcmd + std::string(" ") + std::to_string(errno) + ":" + strerror(errno);
    }

    return util::rtrim(ret, '\n');
}

static uint64_t power(unsigned int base, unsigned int expo)
{
    return (expo == 0) ? 1 : base * power(base, expo - 1);
}

std::string scale_size(uint64_t size, char* up)
{
    char buf[128] = {0};
    uint64_t bytes = size * 1024LU;

    for (int i = 1; up[i] != 0; i++) {
        uint64_t n = power(1024, i);
        uint64_t a = bytes / n;

        int32_t k = bytes % n;
        if (k == 0) {
            snprintf(buf, sizeof(buf), "%lu%c", a, up[i]);
        } else {
            snprintf(buf, sizeof(buf), "%.2f", bytes * (double)1.0 / (double)n);
            std::stringstream ss;
            ss << atof(buf) << up[i];
            snprintf(buf, sizeof(buf), "%s", ss.str().c_str());
        }

        if (a < 1024)
            break;
    }

    return std::move(std::string(buf));
}

std::string get_public_ip() {
    int try_count = 0;
    std::string public_ip = run_shell("curl -s myip.ipip.net | awk -F ' ' '{print $2}' | awk -F '：' '{print $2}'");
    public_ip = util::rtrim(public_ip, '\n');
    while (public_ip.empty() && try_count < 10) {
        public_ip = run_shell("curl -s myip.ipip.net | awk -F ' ' '{print $2}' | awk -F '：' '{print $2}'");
        public_ip = util::rtrim(public_ip, '\n');
        if (!public_ip.empty()) {
            break;
        }
        try_count++;
        sleep(1);
    }

    return public_ip;
}

std::string hide_ip_addr(std::string ip) {
    std::string value = "*";
    std::vector<std::string> nums;
    util::split(ip, ".", nums);
    for (int i = 1; i < nums.size(); ++i) {
        if (i == 2)
            value.append(".*");
        else
            value.append(".").append(nums[i]);
    }
    return value;
}

int32_t str_to_i32(const std::string& str) {
    try {
        int32_t n = std::stoi(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

uint32_t str_to_u32(const std::string& str) {
    try {
        uint64_t n = std::stoul(str);
        return (uint32_t)n;
    } catch (std::exception& e) {
        return 0;
    }
}

int64_t str_to_i64(const std::string& str) {
    try {
        int64_t n = std::stol(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

uint64_t str_to_u64(const std::string& str) {
    try {
        uint64_t n = std::stoul(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

float str_to_f(const std::string& str) {
    try {
        float n = std::stof(str);
        return n;
    } catch (std::exception& e) {
        return 0;
    }
}

void printhex(unsigned char *buf, int len) {
    for (int i = 0; i < len; ++i) {
        printf("%02X", buf[i]);
    }
    printf("\n");
}

void ByteToHexStr(const unsigned char* source, int sourceLen, char* dest)
{
    short i;
    unsigned char highByte, lowByte;

    for (i = 0; i < sourceLen; i++)
    {
        highByte = source[i] >> 4;
        lowByte = source[i] & 0x0f;

        highByte += 0x30;

        if (highByte > 0x39)
            dest[i * 2] = highByte + 0x07;
        else
            dest[i * 2] = highByte;

        lowByte += 0x30;
        if (lowByte > 0x39)
            dest[i * 2 + 1] = lowByte + 0x07;
        else
            dest[i * 2 + 1] = lowByte;
    }
    return;
}

void HexStrToByte(const char* source, int sourceLen, unsigned char* dest)
{
    short i;
    unsigned char highByte, lowByte;

    for (i = 0; i < sourceLen; i += 2)
    {
        highByte = toupper(source[i]);
        lowByte = toupper(source[i + 1]);

        if (highByte > 0x39)
            highByte -= 0x37;
        else
            highByte -= 0x30;

        if (lowByte > 0x39)
            lowByte -= 0x37;
        else
            lowByte -= 0x30;

        dest[i / 2] = (highByte << 4) | lowByte;
    }
    return;
}

std::string encrypt_data(unsigned char* data, int32_t len, const std::string& pub_key, const std::string& priv_key) {
    long psize = crypto_box_ZEROBYTES + len; //32
    unsigned char *padded = new unsigned char[psize];
    memset(padded, 0, crypto_box_ZEROBYTES);
    memcpy(padded + crypto_box_ZEROBYTES, data, len);

    unsigned char nonce[crypto_box_NONCEBYTES]; //24
    randombytes(nonce, sizeof(nonce));

    unsigned char* byte_pub = new unsigned char[pub_key.size()/2];
    unsigned char* byte_priv = new unsigned char[priv_key.size()/2];
    HexStrToByte(pub_key.c_str(), pub_key.size(), byte_pub);
    HexStrToByte(priv_key.c_str(), priv_key.size(), byte_priv);

    unsigned char *encrypted = new unsigned char[psize];
    crypto_box(encrypted, padded, psize, nonce, byte_pub, byte_priv);
    unsigned char *real_encrypted = new unsigned char[psize - crypto_box_BOXZEROBYTES + crypto_box_NONCEBYTES];
    memcpy(real_encrypted, nonce, crypto_box_NONCEBYTES);
    memcpy(real_encrypted + crypto_box_NONCEBYTES, encrypted + crypto_box_BOXZEROBYTES, psize - crypto_box_BOXZEROBYTES);

    std::string str_base64 = base64_encode(real_encrypted, psize - crypto_box_BOXZEROBYTES + crypto_box_NONCEBYTES);

    delete[] padded;
    delete[] encrypted;
    delete[] real_encrypted;
    delete[] byte_pub;
    delete[] byte_priv;

    return str_base64;
}

bool decrypt_data(const std::string& data, const std::string& pub_key, const std::string& priv_key, std::string& ori_message) {
    unsigned char* byte_pub = new unsigned char[pub_key.size()/2];
    unsigned char* byte_priv = new unsigned char[priv_key.size()/2];
    HexStrToByte(pub_key.c_str(), pub_key.size(), byte_pub);
    HexStrToByte(priv_key.c_str(), priv_key.size(), byte_priv);

    std::string str_data = base64_decode(data);
    unsigned char nonce[crypto_box_NONCEBYTES];
    memcpy(nonce, str_data.c_str(), crypto_box_NONCEBYTES);
    int32_t real_len = str_data.size() + crypto_box_BOXZEROBYTES - crypto_box_NONCEBYTES;
    unsigned char* encrypt = new unsigned char[real_len];
    memset(encrypt, 0, real_len);
    memcpy(encrypt + crypto_box_BOXZEROBYTES, str_data.c_str() + crypto_box_NONCEBYTES, str_data.size() - crypto_box_NONCEBYTES);
    unsigned char *message = new unsigned char[real_len];
    int ret = crypto_box_open(message, encrypt, real_len, nonce, byte_pub, byte_priv);
    if (ret == 0) {
        ori_message.assign((char*) (message + crypto_box_ZEROBYTES), real_len - crypto_box_ZEROBYTES);
    }

    delete[] encrypt;
    delete[] message;
    delete[] byte_pub;
    delete[] byte_priv;

    return ret == 0;
}
