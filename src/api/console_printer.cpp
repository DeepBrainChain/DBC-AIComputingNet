/*********************************************************************************
*  Copyright (c) 2017-2018 DeepBrainChain core team
*  Distributed under the MIT software license, see the accompanying
*  file COPYING or http://www.opensource.org/licenses/mit-license.php
* file name        console_printer.h
* description    console printer for command line
* date                  :   2018.04.01
* author            Bruce Feng
**********************************************************************************/

#include "console_printer.h"


namespace matrix
{
    namespace core
    {

        console_printer& console_printer::operator()(ALIGN_TYPE align, int16_t field_len)
        {
            m_formatter.push_back(field_formatter(align, field_len));
            return *this;
        }

        std::ios_base::fmtflags console_printer::get_fmt_flags(ALIGN_TYPE type)
        {
            if(LEFT_ALIGN == type)
            {
                return std::ios::left;
            }
            else if(RIGHT_ALIGN == type)
            {
                return std::ios::right;
            }
            else
            {
                return std::ios::left;
            }
        }

        console_printer& console_printer::operator<<(console_printer& (*manipulator)(console_printer&))
        {
            return manipulator(*this);
        }

        console_printer& init(console_printer& printer)
        {
            printer.reset_it();
            return printer;
        }

        console_printer& endl(console_printer& printer)
        {
            cout << std::endl;
            return printer;
        }

    }

}