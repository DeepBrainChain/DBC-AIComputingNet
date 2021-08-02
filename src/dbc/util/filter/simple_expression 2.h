#ifndef DBC_SIMPLE_EXPRESSION_H
#define DBC_SIMPLE_EXPRESSION_H

#include <string>
#include <map>
#include <vector>

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

#endif
