//
// Created by Jianmin Kuang on 2018/9/27.
// mkdir release; cd release; cmake -DCMAKE_BUILD_TYPE=Release ../ && make
// llvm-g++ -I .. -o ./myzip ./myzip.cpp ../release/libsnappy.a
//

#include <stdio.h>
#include <iostream>
//#include <fstream>
//#include <sstream>

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "snappy-sinksource.h"
#include "snappy.h"
#include <assert.h>


//#define trace(...) printf(__VA_ARGS__)

#define trace(...)

using namespace std;

int write_full(int fd, const void *buf, size_t count)
{
    const char *ptr = (const char *)buf;

    while (count > 0) {
        int rv = write(fd, ptr, count);
        if (rv == -1) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        ptr += rv;
        count -= rv;
    }
    return (ptr - (const char *)buf);
}

namespace myzip {
    class FileSink : public snappy::Sink {
    public:

        FileSink(int fd) : fd_(fd), err_(false) {
            trace("Initialize FileSink\n");
        }

        ~FileSink() {
            trace("Uninitialize FileSink\n");
        }

        virtual void Append(const char* bytes, size_t n) {
            if (err_) {
                return;
            }
            trace("write %lu bytes\n", n);
            if (write_full(fd_, bytes, n) != (int)n) {
                trace("Failed to write a file: %s\n", strerror(errno));
                err_ = true;
            }
        }

        bool err() {
            return err_;
        }
    private:
        int fd_;
        bool err_;
    };

    class FileSource : public snappy::Source {
    public:

        FileSource(int fd, int64_t filesize, size_t bufsiz) : fd_(fd), restsize_(filesize), err_(false) {
            ptr_ = end_ = base_ = new char[bufsiz];
            limit_ = base_ + bufsiz;
        }

        virtual ~FileSource() {
            delete[] base_;
        }

        virtual size_t Available() const {
            trace("Available %lu bytes\n", restsize_);
            return restsize_;
        }

        virtual const char* Peek(size_t* len) {
            if (restsize_ == 0) {
                *len = 0;
                trace("Peek 0 bytes\n");
                return NULL;
            }
            if (ptr_ == end_) {
                int read_len;
                do {
                    read_len = read(fd_, base_, limit_ - base_);
                } while (read_len == -1 && errno == EINTR);
                if (read_len == -1) {
                    trace("Failed to read a file: %s\n", strerror(errno));
                    restsize_ = 0;
                    err_ = true;
                    *len = 0;
                    return NULL;
                }
                trace("Read %d bytes\n", read_len);
                ptr_ = base_;
                end_ = base_ + read_len;
                *len = read_len;
            } else {
                *len = end_ - ptr_;
            }
            if (restsize_ > 0 && (int64_t)*len > restsize_) {
                *len = restsize_;
            }
            trace("Peek %lu bytes\n", *len);
            return ptr_;
        }

        virtual void Skip(size_t n) {
            ptr_ += n;
            if (restsize_ > 0) {
                restsize_ -= n;
            }
            trace("Skip %lu bytes\n", n);
        }

        bool err() {
            return err_;
        }

    private:
        int fd_;
        char *ptr_;
        char *end_;
        char *base_;
        char *limit_;
        int64_t restsize_;
        bool err_;
    };
}

namespace file {
    int Defaults() { return 0; }

    class DummyStatus {
    public:
        void CheckSuccess() { }
    };

    DummyStatus GetContents(
            const std::string& filename, std::string* data, int unused) {
        FILE* fp = fopen(filename.c_str(), "rb");
        if (fp == NULL) {
            perror(filename.c_str());
            exit(1);
        }

        uint32_t buf_size = 4096*64 ;
        data->clear();
        while (!feof(fp)) {
            char buf[buf_size];
            size_t ret = fread(buf, 1, buf_size, fp);
            if (ret == 0 && ferror(fp)) {
                perror("fread");
                exit(1);
            }
            data->append(std::string(buf, ret));
        }

        fclose(fp);

        return DummyStatus();
    }

    inline DummyStatus SetContents(
            const std::string& filename, const std::string& str, int unused) {
        FILE* fp = fopen(filename.c_str(), "wb");
        if (fp == NULL) {
            perror(filename.c_str());
            exit(1);
        }

        int ret = fwrite(str.data(), str.size(), 1, fp);
        if (ret != 1) {
            perror("fwrite");
            exit(1);
        }

        fclose(fp);

        return DummyStatus();
    }
}  // namespace file




static int raw_compress(FILE *infp, FILE *outfp)
{
    int64_t filesize = -1;

    int64_t block_size = snappy::kBlockSize;

    if (filesize == -1) {
        struct stat sbuf;

        if (fstat(fileno(infp), &sbuf) == 0) {
            filesize = sbuf.st_size;
        } else {
            perror("ERROR: Cannot get file size\n");
            return 1;
        }
    }

    myzip::FileSource src(fileno(infp), filesize, block_size);
    myzip::FileSink dst(fileno(outfp));
    if (!snappy::Compress(&src, &dst, false)) {
        perror("Invalid data: snappy::Compress failed\n");
        return 1;
    }
    return src.err() || dst.err();
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        cout<< "myzip [file]\n" ;
        return 1;
    }

    string fn= string(argv[1]);  //"/Users/jianminkuang/CLionProjects/snappy/example/srv_broadcast2";
    string input;

//    ifstream t(fn.c_str(), ifstream::in);
//    stringstream buffer;
//    buffer << t.rdbuf();



//    file::GetContents(fn,&input,file::Defaults());

//    for (int i = 0; i < 5; ++i) {
//        input += input;
//    }


    FILE* fp = fopen(fn.c_str(), "rb");
    if (fp == NULL) {
        perror(fn.c_str());
        exit(1);
    }

    string fn2=fn+".comp";
    FILE* fp2 = fopen(fn2.c_str(), "wb");
    if (fp2 == NULL) {
        perror(fn2.c_str());
        exit(1);
    }

#if 0
    uint32_t buf_size = 1024*64 ;
//    output->clear();
    while (!feof(fp)) {

        string output;

        char buf[buf_size];
        size_t ret = fread(buf, 1, buf_size, fp);
        if (ret == 0 && ferror(fp)) {
            perror("fread");
            exit(1);
        }
//            data->append(std::string(buf, ret));

        bool skip_more_bytes = true;
        size_t n = snappy::Compress(buf, ret, &output);

        assert ( n == output.size());
        int ret2 = fwrite(output.data(), output.size(), 1, fp2);
        if (ret2 != 1) {
            perror("fwrite");
            exit(1);
        }
    }
#endif

    raw_compress(fp,fp2);

    fclose(fp);
    fclose(fp2);

    return 0;

//    std::cout<<"snappy::output " << output << "\n";


    string input2;
//    file::SetContents(string(fn).append(".comp"), output2,
//                      file::Defaults());

    file::GetContents(string(fn).append(".comp"),&input2,file::Defaults());

    string uncompressed;
    snappy::Uncompress(input2.data(), input2.size(), &uncompressed);

    file::SetContents(string(fn).append(".uncomp"), uncompressed,
                      file::Defaults());

    return 0;
}