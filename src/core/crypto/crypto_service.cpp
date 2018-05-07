/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name      crypto_service.h
* description    crypto service for initialization
* date                  : 2018.04.22
* author            Bruce Feng
**********************************************************************************/

#include "crypto_service.h"
#include <openssl/conf.h>
#include <openssl/rand.h>
#include <openssl/crypto.h>
#include "random.h"
#include "sha256.h"
#include "key.h"


static std::unique_ptr<ECCVerifyHandle> g_ecc_verify_handle;
static std::unique_ptr<std::recursive_mutex[]> g_ssl_lock;

void ssl_locking_callback(int mode, int type, const char *file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        g_ssl_lock[type].lock();
    }
    else
    {
        g_ssl_lock[type].unlock();
    }
}

namespace matrix
{
    namespace core
    {

        int32_t crypto_service::init(bpo::variables_map &options)
        {
            //openssl multithread lock
            g_ssl_lock.reset(new std::recursive_mutex[CRYPTO_num_locks()]);
            CRYPTO_set_locking_callback(ssl_locking_callback);
            OPENSSL_no_config();

            //rand seed
            RandAddSeed();                          // Seed OpenSSL PRNG with performance counter

            //elliptic curve code
            std::string sha256_algo = SHA256AutoDetect();
            RandomInit();
            ECC_Start();
            g_ecc_verify_handle.reset(new ECCVerifyHandle());

            //ecc check
            if (!ECC_InitSanityCheck())
            {
                LOG_ERROR << "Elliptic curve cryptography sanity check failure. Aborting.";
                return E_DEFAULT;
            }

            //random check
            if (!Random_SanityCheck()) 
            {
                LOG_ERROR << "OS cryptographic RNG sanity check failure. Aborting.";
                return E_DEFAULT;
            }

            return E_SUCCESS;
        }

        int32_t crypto_service::exit()
        {            
            //rand clean
            RAND_cleanup();                         // Securely erase the memory used by the PRNG           

            //openssl multithread lock
            CRYPTO_set_locking_callback(nullptr);               // Shutdown OpenSSL library multithreading support
            g_ssl_lock.reset();                         // Clear the set of locks now to maintain symmetry with the constructor.

            //ecc release
            g_ecc_verify_handle.reset();
            ECC_Stop();

            return E_SUCCESS;
        }

    }

}