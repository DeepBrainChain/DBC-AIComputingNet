/*********************************************************************************

*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name         : simple expression.h
* description       : 
* date              : 2018/7/1
* author            : Jimmy Kuang
**********************************************************************************/
#pragma once

#include <string>
#include <map>
#include <vector>

namespace matrix
{
    namespace core
    {

        class condition
        {
        public:
            condition();
            condition(std::string s);
            bool set(std::string s);
            bool evaluate(std::string k, std::string v);

            std::string get_attribute();
            std::string to_string();

            bool is_valid();

        public:
            //enum v_type
            //{
            //    STRING = 0,
            //    INT =1,
            //    UNKNOWN_TYPE
            //};

            enum op
            {
                EQ = 0,
                GT = 1,
                LT = 2,
                UNKNOWN_OP
            };

        private:
            op m_op;
            //v_type m_type;
            std::string m_l;
            std::string m_r;
        };

        /**
         * simple expression
         *  relation operators: and
         *  logic operators: =, >, <
         *  attribute:
         */
        class expression
        {
        public:
            expression (std::string s);
            uint32_t size();
            bool evaluate(std::map<std::string,std::string> kvs, std::string text="");
            bool is_valid();

        private:
            bool add(std::string);

        private:
            std::vector<condition> m_conditions;
            std::vector<std::string> m_keywords;
        };

    }

}
