
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

		private:

		};
	}
}

