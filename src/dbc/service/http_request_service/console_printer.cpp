#include "console_printer.h"

console_printer &console_printer::operator()(ALIGN_TYPE align, int16_t field_len) {
    m_formatter.push_back(field_formatter(align, field_len));
    return *this;
}

std::ios_base::fmtflags console_printer::get_fmt_flags(ALIGN_TYPE type) {
    if (LEFT_ALIGN == type) {
        return std::ios::left;
    } else if (RIGHT_ALIGN == type) {
        return std::ios::right;
    } else {
        return std::ios::left;
    }
}

console_printer &console_printer::operator<<(console_printer &(*manipulator)(console_printer &)) {
    return manipulator(*this);
}

console_printer &init(console_printer &printer) {
    printer.reset_it();
    return printer;
}

console_printer &endl(console_printer &printer) {
    std::cout << std::endl;
    return printer;
}

