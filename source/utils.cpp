#include "utils.hpp"

std::string strTolower(std::string str)
{
    std::string strLowerCase;

    for (size_t i = 0; i < str.size(); i++)
    {
        if (str[i] >= 'A' && str[i] <= 'Z')
            strLowerCase.append(1, str[i] - 'A' + 'a');
        else
            strLowerCase.append(1, str[i]);
    }
    
    return strLowerCase;
}

IntLongOverflow strToIntLongOverflow(const std::string & type, const std::string & s_number)
{
    IntLongOverflow result = {0, 0};
    long limit_a;
    long limit_b;
    int sign;
    long l_number = 0;

    if (type == "int")
    {
        limit_a = INT_MAX;
        limit_b = INT_MIN;
    }
    else if (type == "long")
    {
        limit_a = LONG_MAX;
        limit_b = LONG_MIN;
    }
    else
    {
        result.overflow = -1;
        return result;
    }

    if (s_number.empty() || s_number.find_first_not_of("+-0123456789") != std::string::npos)
    {
        result.overflow = -1;
        return result;
    }
    if (s_number.size() == 1 && s_number.find_first_not_of("+-") == std::string::npos)
    {
        result.overflow = -1;
        return result;
    }
    size_t pos = s_number.find_last_of("+-");
    if (pos > 0 && pos != std::string::npos)
    {    
        result.overflow = -1;
        return result;
    }

    size_t i = 0;

    if (s_number[i] == '-')
    {
        sign = -1;
        i++;
    }
    else
    {
        sign = 1;
        if (s_number[i] == '+')
            i++;
    }

    while (i + 1 < s_number.size() && s_number[i] == '0')
        i++;

    for (; i < s_number.size(); i++)
    {
        size_t extent = s_number.size() - i - 1;
        long multiplier = 1;
        for (size_t m = 0; m < extent; m++)
        {
            if (multiplier * sign > limit_a / 10 || multiplier * sign < limit_b / 10)
            {
                result.overflow = 0;
                return result;
            }
            multiplier *= 10;
        }
        int digit = s_number[i] - '0';
        if (digit && (multiplier * sign > limit_a / digit || multiplier * sign < limit_b / digit))
        {
            result.overflow = 0;
            return result;
        }
        long temp = multiplier * sign * digit;
        if ((sign == 1 && l_number > limit_a -  temp) || (sign == -1 && l_number < limit_b - temp))
        {
            result.overflow = 0;
            return result;
        }
        l_number += temp;
    }
    
    result.overflow = 1;
    result.number = l_number;
    return result;
}