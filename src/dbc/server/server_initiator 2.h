#ifndef DBC_SERVER_INITIATOR_H
#define DBC_SERVER_INITIATOR_H

class server_initiator
{
public:
    virtual int32_t init(int argc, char* argv[]) = 0;

    virtual int32_t exit() = 0;
};

#endif



