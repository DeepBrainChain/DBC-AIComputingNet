#pragma once

#include <string>

namespace matrix
{
    namespace core
    {
        class matrix_capacity
        {
        public:
            matrix_capacity();
            bool thrift_binary() const;
            bool thrift_compact() const;
            bool snappy_raw() const;

            void add(std::string c);

            std::string to_string() const;

        public:
            static std::string THRIFT_BINARY_C_NAME;
            static std::string THRIFT_COMPACT_C_NAME;
            static std::string SNAPPY_RAW_C_NAME;

        private:
            bool m_thrift_binary;
            bool m_thrift_compact;
            bool m_snappy_raw;

        };

        class matrix_capacity_helper
        {
        public:
            static int get_thrift_proto(matrix_capacity& a, matrix_capacity& b);
            static bool get_compress_enabled(matrix_capacity& a, matrix_capacity& b);
        };
    }
}
