/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : rpc_error.h
* description       : dbc general rpc error code
* date              : 2018.11.9
* author            : tower
**********************************************************************************/

#ifndef RPC_ERROR_H
#define RPC_ERROR_H

namespace matrix
{
    namespace core
    {
        
        //DBC RPC error codes
        enum rpc_error_code
        {
            
            //没有任何错误发生
            
            RPC_RESPONSE_SUCCESS=0,
            
            //发生请求被处理前的错误码
            
            RPC_INVALID_REQUEST = -32600,//The inspection request is illegal.
            RPC_METHOD_NOT_FOUND = -32601,//Check that the requested method does not exist
            RPC_REQUEST_INTERRUPTED= -32602,//Request is interrupted.Try again later.
            
            
            //等待响应过程中出现的错误码
            
            RPC_RESPONSE_TIMEOUT=-32700, //call timeout
            RPC_RESPONSE_ERROR = -32701,// response error
            
            
            //发生在请求被处理时的具体错误项目
            //响应错误若可指定具体错误项目，则用以下错误码；否则直接使用RPC_RESPONSE_ERROR
            
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
            RPC_SYSTEM_BUSYING=-21,//!<Work queue depth exceeded
            RPC_METHOD_DEPRECATED = -23, //!< RPC method is deprecated
            RPC_INVALID_PARAMS = -25, //!<
            
            
        };
        
    }  // namespce core
}  // namespace matrix

#endif
