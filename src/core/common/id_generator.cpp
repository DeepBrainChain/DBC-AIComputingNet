#include "id_generator.h"
#include  "key.h"
#include "base58.h"
#include "common.h"

namespace matrix
{
    namespace core
    {
        id_generator::id_generator()
        {

        }


        id_generator::~id_generator()
        {

        }

        int32_t id_generator::generate_node_info(node_info &info)
        {
            CKey secret;

            //private key
            bool fCompressed = true;
            secret.MakeNewKey(fCompressed);

            //public key
            CPubKey pubkey = secret.GetPubKey();

            //create compressed public key
            CKeyID keyID = pubkey.GetID();

            //encode node id
            CPrivKey private_key = secret.GetPrivKey();

            std::vector<unsigned char> id_data;
            std::vector<unsigned char> id_prefix = { 'n', 'o', 'd', 'e', '.', NODE_ID_VERSION, '.' };
            id_data.reserve(id_prefix.size() + keyID.size());
            id_data.insert(id_data.end(), id_prefix.begin(), id_prefix.end());
            id_data.insert(id_data.end(), keyID.begin(), keyID.end());
            info.node_id = EncodeBase58Check(id_data);

            //encode node private key
            std::vector<unsigned char> private_key_data;
            std::vector<unsigned char> private_key_prefix = { PRIVATE_KEY_VERSION, '.' };
            private_key_data.reserve(private_key_prefix.size() + private_key.size());
            private_key_data.insert(private_key_data.end(), private_key_prefix.begin(), private_key_prefix.end());
            private_key_data.insert(private_key_data.end(), private_key.begin(), private_key.end());
            info.node_private_key = EncodeBase58Check(private_key_data);

            return E_SUCCESS;
        }

        std::string id_generator::generate_check_sum()
        {
            //check_sum in protocol header
            return std::string();
        }

        std::string id_generator::generate_session_id()
        {
            //same to check_sum
            return std::string();
        }

        uint64_t id_generator::generate_nounce()
        {
            uint64_t nounce = 0;
            return nounce;
        }

        std::string id_generator::generate_task_id()
        {
            CKey secret;

            //private key
            bool fCompressed = true;
            secret.MakeNewKey(fCompressed);

            //public key
            CPubKey pubkey = secret.GetPubKey();

            //create compressed public key
            CKeyID keyID = pubkey.GetID();

            //encode node id
            CPrivKey private_key = secret.GetPrivKey();

            std::vector<unsigned char> id_data;
            std::vector<unsigned char> id_prefix = { 't', 'a', 's', 'k', '.', NODE_ID_VERSION, '.' };
            id_data.reserve(id_prefix.size() + keyID.size());
            id_data.insert(id_data.end(), id_prefix.begin(), id_prefix.end());
            id_data.insert(id_data.end(), keyID.begin(), keyID.end());
            std::string task_id = EncodeBase58Check(id_data);

            return task_id;
        }

    }

}


