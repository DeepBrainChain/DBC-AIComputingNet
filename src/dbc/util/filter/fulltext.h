#ifndef DBC_FULLTEXT_H
#define DBC_FULLTEXT_H

#include <string>
#include <vector>

class fulltext
{
public:
    static bool search(std::string text, std::vector<std::string> keywords);
};

#endif
