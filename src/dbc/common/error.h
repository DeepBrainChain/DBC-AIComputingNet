#ifndef DBC_ERROR_H
#define DBC_ERROR_H

#include <iostream>
#include <string>

typedef int32_t ERRCODE;
enum {
    ERR_SUCCESS,
    ERR_ERROR,

};

typedef std::tuple<int32_t, std::string> FResult;
#define FResultOK { ERR_SUCCESS, "ok" }




//#define E_SUCCESS                                            0                   //success
#define E_EXIT_FAILURE                                      (-1)                  //exit because of failure
#define E_EXIT_PARSE_COMMAND_LINE                           (-2)                  //exit because of failure
#define E_DEFAULT                                           (-3)                  //default error, common use
#define E_BAD_PARAM                                         (-4)                  //bad param
#define E_NULL_POINTER                                      (-5)                  //null pointer
#define E_MSG_QUEUE_FULL                                    (-6)                  //message queue full
#define E_FILE_FAILURE                                      (-7)                  //file op failure
#define E_NOT_FOUND                                         (-8)                  //not found specified object
#define E_EXISTED                                           (-9)                  //dest already exist
#define E_INACTIVE_CHANNEL                                  (-10)                 //broadcast error, no active channel
#define E_NONCE                                             (-11)                  // nonce error
#define E_IMAGE_NOT_FOUND                                   (-12)                  // docker image not found
#define E_PULLING_IMAGE                                     (-13)
#define E_NO_DISK_SPACE                                     (-14)
#define E_NETWORK_FAILURE                                   (-15)                 //exit because network failure
#define E_BILL_DISABLE                                      (-16)                 //exit because network failure

#define E_NOT_COMPUTING_NODE                                (-17)
#define E_NO_START_CONTAINER                                (-18)                 //create new container from  new images ,but no start container
#define E_CONTAINER_NOT_FOUND                               (-19)                  // docker container not found

#define E_VIRT_CONNECT_ERROR                                (-20)                  // libvirt  conn error
#define E_VIRT_DOMAIN_NOT_FOUND                             (-21)                  // libvirt  domain not found
#define E_VIRT_INTERNAL_ERROR                               (-22)                  // vm get system info error
#define E_VIRT_DOMAIN_EXIST                                 (-23)                  // libvirt  domain alread exist

enum rpc_error_code
{
    RPC_RESPONSERR_SUCCESS = 0,

    //Error code before a request is processed
    RPC_INVALID_REQUEST = -32600,//The inspection request is illegal.
    RPC_METHOD_NOT_FOUND = -32601,//Check that the requested method does not exist
    RPC_REQUEST_INTERRUPTED = -32602,//Request is interrupted.Try again later.

    //Error code during waiting for response
    RPC_RESPONSE_TIMEOUT = -32700, //call timeout
    RPC_RESPONSE_ERROR = -32701,// response error

    RPC_MISC_ERROR = -1,  //!< std::exception thrown in command handling
    RPC_TYPE_ERROR = -3,  //!< Unexpected type was passed as parameter
    RPC_INVALID_ADDRESS_OR_KEY = -5,  //!< Invalid address or key
    RPC_OUT_OF_MEMORY = -7,  //!< Ran out of memory during operation
    RPC_DATABASE_ERROR = -9, //!< Database error
    RPC_DESERIALIZATION_ERROR = -11, //!< Error parsing or validating structure in raw format
    RPC_VERIFY_ERROR = -13, //!< General error during transaction or block submission
    RPC_VERIFY_REJECTED = -15, //!< Transaction or block was rejected by network rules
    RPC_VERIFY_ALREADY_IN_CHAIN = -17, //!< Transaction already in chain
    RPC_IN_WARMUP = -19, //!< Client still warming up
    RPC_SYSTEM_BUSYING = -21,//!<Work queue depth exceeded
    RPC_METHOD_DEPRECATED = -23, //!< RPC method is deprecated
    RPC_INVALID_PARAMS = -25, //!<
};



#endif // DBC_ERROR_H
