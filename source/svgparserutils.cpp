#include "svgparserutils.h"
namespace lunasvg {

void stripLeadingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front())) {
        input = input.substr(1);
    }
}

void stripTrailingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.back())) {
        input = input.substr(0, input.size() - 1);
    }
}

bool isIntegralDigit(char ch, int base)
{
    if(IS_NUM(ch))
        return ch - '0' < base;
    if(IS_ALPHA(ch))
        return (ch >= 'a' && ch < 'a' + std::min(base, 36) - 10) || (ch >= 'A' && ch < 'A' + std::min(base, 36) - 10);
    return false;
}

bool skipDelimiter(std::string& input, char delimiter)
{
    if(!input.empty() && input.front() == delimiter) {
        input = input.substr(1);
        return true;
    }

    return false;
}

bool skipOptionalSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front()))
        input = input.substr(1);
    return !input.empty();
}

bool skipOptionalSpacesOrComma(std::string& input)
{
    return skipOptionalSpacesOrDelimiter(input, ',');
}

bool skipOptionalSpacesOrDelimiter(std::string& input, char delimiter)
{
    if(!input.empty() && !IS_WS(input.front()) && delimiter != input.front())
        return false;
    if(skipOptionalSpaces(input)) {
        if(delimiter == input.front()) {
            input = input.substr(1);
            skipOptionalSpaces(input);
        }
    }

    return !input.empty();
}

bool skipString(std::string& input, const std::string& value)
{
    if(input.size() >= value.size() && value == input.substr(0, value.size())) {
        input = input.substr(value.size());
        return true;
    }

    return false;
}

void stripLeadingAndTrailingSpaces(std::string& input)
{
    stripLeadingSpaces(input);
    stripTrailingSpaces(input);
}

}
