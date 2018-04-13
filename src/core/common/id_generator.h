
#pragma once

#include<string>

namespace matrix
{
	namespace core
	{
		class id_generator
		{
		public:
			id_generator();
			~id_generator();

			std::string generate_id();

            std::string generate_check_sum();

            std::string generate_session_id();

            uint64_t  generate_nounce();

		private:

		};
	}
}

