#ifndef LUNASVG_SVGPARSERUTILS_H
#define LUNASVG_SVGPARSERUTILS_H

#include <cmath>
#include <limits>

namespace lunasvg {

 bool IS_NUM(int cc) { return cc >= '0' && cc <= '9'; }
 bool IS_ALPHA(int cc) { return (cc >= 'a' && cc <= 'z') || (cc >= 'A' && cc <= 'Z'); }
 bool IS_WS(int cc) { return cc == ' ' || cc == '\t' || cc == '\n' || cc == '\r'; }

 void stripLeadingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front())) {
        input.substr(1);
    }
}

 void stripTrailingSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.back())) {
        input.erase(input.size() - 1);
    }
}

 void stripLeadingAndTrailingSpaces(std::string& input)
{
    stripLeadingSpaces(input);
    stripTrailingSpaces(input);
}

 bool skipOptionalSpaces(std::string& input)
{
    while(!input.empty() && IS_WS(input.front()))
        input.substr(1);
    return !input.empty();
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

 bool skipOptionalSpacesOrComma(std::string& input)
{
    return skipOptionalSpacesOrDelimiter(input, ',');
}

 bool skipDelimiter(std::string& input, char delimiter)
{
    if(!input.empty() && input.front() == delimiter) {
        input.substr(1);
        return true;
    }

    return false;
}

 bool skipString(std::string& input, const std::string& value)
{
    if(input.size() >= value.size() && value == input.substr(0, value.size())) {
        input.substr(value.size());
        return true;
    }

    return false;
}

 bool isIntegralDigit(char ch, int base)
{
    if(IS_NUM(ch))
        return ch - '0' < base;
    if(IS_ALPHA(ch))
        return (ch >= 'a' && ch < 'a' + std::min(base, 36) - 10) || (ch >= 'A' && ch < 'A' + std::min(base, 36) - 10);
    return false;
}

template<typename T>
inline bool parseInteger(std::string& input, T& integer, int base = 10)
{
    static const T intMax = std::numeric_limits<T>::max();
    static const bool isSigned = std::numeric_limits<T>::is_signed;
    const T maxMultiplier = intMax / static_cast<T>(base);

    bool isNegative = false;
    if(!input.empty() && input.front() == '+') {
        input.substr(1);
    } else if(!input.empty() && isSigned && input.front() == '-') {
        input.substr(1);
        isNegative = true;
    }

    T value = 0;
    if(input.empty() || !isIntegralDigit(input.front(), base))
        return false;
    do {
        const char ch = input.front();
        int digitValue;
        if(IS_NUM(ch))
            digitValue = ch - '0';
        else if(ch >= 'a')
            digitValue = ch - 'a' + 10;
        else
            digitValue = ch - 'A' + 10;
        if(value > maxMultiplier || (value == maxMultiplier && static_cast<T>(digitValue) > (intMax % static_cast<T>(base)) + isNegative))
            return false;
        value = static_cast<T>(base) * value + static_cast<T>(digitValue);
        input.substr(1);
    } while(!input.empty() && isIntegralDigit(input.front(), base));

    using signed_t = typename std::make_signed<T>::type;
    if(isNegative)
        integer = -static_cast<signed_t>(value);
    else
        integer = value;
    return true;
}

template<typename T>
inline bool parseNumber(std::string& input, T& number)
{
    T integer, fraction;
    int sign, expsign, exponent;

     T maxValue = std::numeric_limits<T>::max();
    fraction = 0;
    integer = 0;
    exponent = 0;
    sign = 1;
    expsign = 1;

    if(!input.empty() && input.front() == '+') {
        input.substr(1);
    } else if(!input.empty() && input.front() == '-') {
        input.substr(1);
        sign = -1;
    }

    if(input.empty() || (!IS_NUM(input.front()) && input.front() != '.'))
        return false;
    if(IS_NUM(input.front())) {
        do {
            integer = static_cast<T>(10) * integer + (input.front() - '0');
            input.substr(1);
        } while(!input.empty() && IS_NUM(input.front()));
    }

    if(!input.empty() && input.front() == '.') {
        input.substr(1);
        if(input.empty() || !IS_NUM(input.front()))
            return false;
        T divisor = 1;
        do {
            fraction = static_cast<T>(10) * fraction + (input.front() - '0');
            divisor *= static_cast<T>(10);
            input.substr(1);
        } while(!input.empty() && IS_NUM(input.front()));
        fraction /= divisor;
    }

    if(input.size() > 1 && (input[0] == 'e' || input[0] == 'E')
        && (input[1] != 'x' && input[1] != 'm'))
    {
        input.substr(1);
        if(!input.empty() && input.front() == '+')
            input.substr(1);
        else if(!input.empty() && input.front() == '-') {
            input.substr(1);
            expsign = -1;
        }

        if(input.empty() || !IS_NUM(input.front()))
            return false;
        do {
            exponent = 10 * exponent + (input.front() - '0');
            input.substr(1);
        } while(!input.empty() && IS_NUM(input.front()));
    }

    number = sign * (integer + fraction);
    if(exponent)
        number *= static_cast<T>(std::pow(10.0, expsign * exponent));
    return number >= -maxValue && number <= maxValue;
}

} // namespace lunasvg

#endif // LUNASVG_SVGPARSERUTILS_H