#include "svgparserutils.h"
namespace lunasvg {

void stripLeadingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front())) {
        input.substr(1);
    }
}

void stripTrailingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.back())) {
        input.substr(1);
    }
}

bool skipDelimiter(std::string& input, char delimiter)
{
    if(!input.empty() && input.front() == delimiter) {
        input.substr(1);
        return true;
    }

    return false;
}

bool skipOptionalSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front()))
        input.substr(1);
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
            input.substr(1);
            skipOptionalSpaces(input);
        }
    }

    return !input.empty();
}

bool skipString(std::string& input, const std::string& value)
{
    if(input.size() >= value.size() && value == input.substr(0, value.size())) {
        input.substr(value.size());
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