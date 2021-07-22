#ifndef DBC_CONSOLE_PRINTER_H
#define DBC_CONSOLE_PRINTER_H

#include "util/utils.h"
#include "log/log.h"

enum ALIGN_TYPE {
    LEFT_ALIGN = 0,
    //MIDDLE_ALIGN,
    RIGHT_ALIGN
};

struct field_formatter {
    ALIGN_TYPE align;
    int16_t field_len;

    field_formatter(ALIGN_TYPE align, int16_t field_len) {
        this->align = align;
        this->field_len = field_len;
    }
};

class console_printer {
public:

    console_printer() = default;

    virtual ~console_printer() = default;

    console_printer &operator()(ALIGN_TYPE align, int16_t field_len);

    console_printer &operator<<(console_printer &(*manipulator)(console_printer &));

    template<typename arg_type>
    inline
    console_printer &operator<<(arg_type val) {
        if (m_it != m_formatter.end()) {
            field_formatter formatter = *m_it++;
            std::cout.setf(get_fmt_flags(formatter.align));
            std::cout.width(formatter.field_len);

            std::cout << val;
        } else {
            LOG_ERROR << "console_printer iterator out of range";
        }
        return *this;
    }

    void reset_it() { m_it = m_formatter.begin(); }

    std::ios_base::fmtflags get_fmt_flags(ALIGN_TYPE type);

protected:

    std::list<field_formatter> m_formatter;

    std::list<field_formatter>::iterator m_it;

};

console_printer &init(console_printer &printer);

console_printer &endl(console_printer &printer);

#endif
