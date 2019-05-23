// db_show.cpp : Defines the entry point for the console application.
//
#include "comm_def.h"

//#include <boost/filesystem/convenience.hpp>
#include <stdio.h>
#ifdef WIN32
#include <io.h>
#endif
#ifdef __linux__
#include <unistd.h>
#endif

#include <fcntl.h>
using namespace std;
using namespace matrix::core;

extern void parse_req_db(const char *path);
extern void parse_peer_db(const char *path);
extern void parse_provide_db(const char *path);
extern void repair_db(const char *path);

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        cout << "parms error" << endl;
        cout << "dbc_show req|pro xxxxx.db" << endl;
        exit(0);
    }
    std::string mode = argv[1];
   
    int result = 0;
#ifdef WIN32
    result = _access(argv[2], 0);
#endif

#ifdef __linux__
    result = access(argv[2], R_OK);
#endif
    if (result != 0)
    {
        cout << "path is not exist";
        exit(0);
    }

    if (mode == "req")
    {
        parse_req_db(argv[2]);
    }
    else if (mode == "pro")
    {
        parse_provide_db(argv[2]);
    }
    else if (mode == "peer")
    {
        parse_peer_db(argv[2]);
    }
    else if (mode == "repair")
    {
        repair_db(argv[2]);
    }

    else
    {
        cout << "mode error";
    }
    
    //delete db;    

    return 0;
}

