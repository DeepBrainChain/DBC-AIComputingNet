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
            // Standard JSON-RPC errors
            // RPC_INVALID_REQUEST is internally mapped to HTTP_BAD_REQUEST (400).
            // It should not be used for application-layer errors.
            RPC_INVALID_REQUEST  = -32600,
            // RPC_METHOD_NOT_FOUND is internally mapped to HTTP_NOT_FOUND (404).
            // It should not be used for application-layer errors.
            RPC_METHOD_NOT_FOUND = -32601,
            RPC_INVALID_PARAMS   = -32602,
            // RPC_INTERNAL_ERROR should only be used for genuine errors in dbc
            // (for example datadir corruption).
            RPC_INTERNAL_ERROR   = -32603,
            RPC_PARSE_ERROR      = -32604,
            RPC_OUT_OF_MEMORY    = -32605,

            // General application defined errors
            RPC_TYPE_ERROR                  = -1,   // Unexpected type was passed as parameter
            RPC_INVALID_PARAMETER           = -2,   // Invalid, missing or duplicate parameter
            RPC_METHOD_DEPRECATED           = -3,   // RPC method is deprecated

            // P2P client errors
            RPC_CLIENT_NOT_CONNECTED        = -10,   // client is not connected

            // Wallet errors
        };

    }  // namespce core
}  // namespace matrix

#endif
