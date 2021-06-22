/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        :   function_traits.h
* description    :   function traits
* date                  :   2018.01.20
* author            :   Bruce Feng
**********************************************************************************/
#pragma once

#include <functional>
#include <tuple>

using namespace std;

namespace matrix
{
    namespace core
    {

        template<typename T>
        struct function_traits;

        //normal function
        template<typename ret_type, typename... args_type>
        struct function_traits<ret_type (args_type ...)>
        {
        public:

            enum { arity = sizeof...(args_type) };

            typedef ret_type function_type(args_type...);
            typedef ret_type (*funtion_pointer_type)(args_type...);

            using std_function_type = std::function<function_type>;

            template<size_t I>
            class args
            {
                static_assert((I < arity), "args index out of range");
                using type = typename std::tuple_element<I, std::tuple<args_type...>>::type;
            };
        };
      

        ////////////////////////////////////////////////////////////////////////////


        //function pointer
        template<typename ret_type, typename... args_type>
        struct function_traits<ret_type (*)(args_type...)> : function_traits<ret_type (args_type...)> {};

        //std::function
        template<typename ret_type, typename... args_type>
        struct function_traits<std::function<ret_type (args_type...)>> : function_traits<ret_type (args_type...)> {};        

        //class member function
        #define FUNCTION_TRAITS(...)    \
        template<typename ret_type, typename class_type, typename... args_type> \
        struct function_traits<ret_type (class_type::*)(args_type...) __VA_ARGS__> : function_traits<ret_type (args_type...)> {}; \

        FUNCTION_TRAITS()
        FUNCTION_TRAITS(const)
        FUNCTION_TRAITS(volatile)
        FUNCTION_TRAITS(const volatile)

        //function object
        template<typename function_object>
        struct function_traits : function_traits<decltype (&function_object::operator())> {};


        ////////////////////////////////////////////////////////////////////////////


        //to function
        template<typename Function>
        typename function_traits<Function>::std_function_type to_function(const Function &func)
        {
            return static_cast<typename function_traits<Function>::std_function_type>(func);
        }

        template<typename Function>
        typename function_traits<Function>::std_function_type to_function(Function &&func)
        {
            return static_cast<typename function_traits<Function>::std_function_type>(std::forward<Function>(func));
        }

        //to function pointer
        template<typename Function>
        typename function_traits<Function>::funtion_pointer_type to_function_pointer(const Function &func)
        {
            return static_cast<typename function_traits<Function>::funtion_pointer_type>(func);
        }

    }

}
