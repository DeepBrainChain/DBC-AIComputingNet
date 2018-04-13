#include "id_generator.h"

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

		std::string id_generator::generate_id()
		{
			//ref to btc
			//todo ...
			return std::string();
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
	}
}


