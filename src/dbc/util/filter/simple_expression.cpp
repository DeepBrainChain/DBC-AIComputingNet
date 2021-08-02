#include "simple_expression.h"
#include "fulltext.h"
#include "../utils/string_util.h"

expression::expression(std::string s)
{
    m_conditions.clear();
    m_keywords.clear();

    //std::cout << "expression: " << s << std::endl;

    std::string delimiter = " and ";

    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos)
    {
        token = s.substr(0, pos);

        add(token);

        s.erase(0, pos + delimiter.length());
    }

    add(s);
}

bool expression::add(std::string t)
{
    condition c(t);

    if (!c.is_valid())
    {
        // not a valid condition express, put to fulltext keywords list
        //return false;
        std::string s = util::rtrim(t,' ');
        if (s.length())
            m_keywords.push_back(s);
    }
    else
    {
        m_conditions.push_back(c);
        //std::cout << "condition: " << t << std::endl;
    }

    return true;
}

uint32_t expression::size()
{
    return m_conditions.size() + m_keywords.size();
}

/**
 * check if expression matched for the input kvs and text
 * @param kvs
 * @param text
 * @return
 */
bool expression::evaluate(std::map<std::string, std::string> kvs, std::string text)
{
    if(!is_valid())
    {
        return false;
    }

    bool is_matched = false;

    for(auto& c: m_conditions)
    {
        auto k = c.get_attribute();

        if (kvs.count(k) ==0 )
        {
            // bypass unknown condition
            continue;
        }

        auto v = kvs[k];

        if(!c.evaluate(k,v))
        {
            return false;
        }

        is_matched = true;
    }

    if (text.length() && m_keywords.size())
    {
        if (!fulltext::search(text, m_keywords))
        {
            return false;
        }
        else
        {
            is_matched = true;
        }
    }

    return is_matched;
}

bool expression::is_valid()
{
    return m_conditions.size()>0 || m_keywords.size()>0 ;
}


condition::condition()
{
    m_op = UNKNOWN_OP;
    //m_type = UNKNOWN_TYPE;
    m_l = "";
    m_r = "NULL";

}

condition::condition(std::string s)
{
    m_op = UNKNOWN_OP;
    m_l = "";
    m_r = "NULL";
    set(s);
}

std::string condition::get_attribute()
{
    return m_l;
}

std::string condition::to_string()
{
    std::string op;
    if (m_op == EQ) op = "=";
    else if (m_op == GT) op = ">";
    else if (m_op == LT) op = "<";
    else op = "?";

    return m_l+op+m_r;
}

bool condition::set(std::string s)
{
    std::string a = "";
    std::string b = "";

    util::trim(s);
    for(uint32_t i=0; i<s.length(); i++)
    {
        auto c = s[i];

        if (m_op == UNKNOWN_OP)
        {
            if (c == '=')
            {
                m_op = EQ;
                continue;
            }
            else if (c == '>')
            {
                m_op = GT;
                continue;
            }
            else if (c == '<')
            {
                m_op = LT;
                continue;
            }
        }

        if (m_op != UNKNOWN_OP)
        {
            b += c;
        }
        else
        {
            a += c;
        }

    }
    //m_type = STRING;
    m_l = a;
    m_r = b;
    //std::cout << m_op <<std::endl;
    //std::cout << m_l <<std::endl;
    //std::cout << m_r <<std::endl;
    return true;
}

bool condition::is_valid()
{
    if (m_op == UNKNOWN_OP || m_l == "" || m_r == "")
    {
        return false;
    }

    return true;
}

/**
 * check if input (k,v) can pass the predefined condition: e.g v = condition.m_r
 * @param k
 * @param v
 * @return
 */
bool condition::evaluate(std::string k, std::string v)
{
    if (k != m_l)
    {
        return false;
    }

    util::trim(v);
    if (m_op == EQ)
    {
        return v == m_r;
    }
    else if (m_op == GT)
    {
        int32_t a = 0;
        int32_t b = 0;
        try
        {
            a = (int32_t) std::stoi(v);
            b = (int32_t) std::stoi(m_r);
        }
        catch (...)
        {
            return false;
        }

        return a>b;

    }
    else if (m_op == LT)
    {
        int32_t a = 0;
        int32_t b = 0;
        try
        {
            a = (int32_t) std::stoi(v);
            b = (int32_t) std::stoi(m_r);
        }
        catch (...)
        {
            return false;
        }

        return a<b;

    }
    else
    {

    }

    return true;
}
