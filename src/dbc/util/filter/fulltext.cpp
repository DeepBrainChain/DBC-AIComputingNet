#include "fulltext.h"
#include <algorithm>

bool fulltext::search(std::string text, std::vector<std::string> keywords)
{
    transform(text.begin(), text.end(), text.begin(), ::tolower);

    for (std::string &k: keywords)
    {
        transform(k.begin(), k.end(), k.begin(), ::tolower);
        if (text.find(k) == std::string::npos)
        {
            return false;
        }
    }

    return true;
}
