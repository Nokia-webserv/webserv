#include "utils.hpp"

// Changes all chars of a string to lowercase
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

// Converts string to type (int or long) and checks for overflow (-1: invalid number, 0: overflow, 1: ok)
IntLongOverflow strToIntLongOverflow(const std::string & type, const std::string & s_number)
{
    // struct IntLongOverflow
    // {
    //     long number;
    //     int overflow;
    // };
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
    if (s_number.size() == 1 && s_number.find_first_not_of("+-") == std::string::npos) // only sign
    {
        result.overflow = -1;
        return result;
    }
    size_t pos = s_number.find_last_of("+-");
    if (pos > 0 && pos != std::string::npos) // sign not in the very beginning
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

    // ignore leading zeroes (but not the only 0)
    while (i + 1 < s_number.size() && s_number[i] == '0')
        i++; 

    for (; i < s_number.size(); i++)
    {
        size_t extent = s_number.size() - i - 1; // extent of 10 (i = 0; s_number = 123 -> extent = 2)
        long multiplier = 1;
        for (size_t m = 0; m < extent; m++)
        {
            if (multiplier * sign > limit_a / 10 || multiplier * sign < limit_b / 10) // check overflow
            {
                result.overflow = 0;
                return result;
            }
            multiplier *= 10; // extent = 2 -> multiplier = 100; 
        }
        int digit = s_number[i] - '0'; // s_number = 123; i = 0 -> digit = 1;
        if (digit && (multiplier * sign > limit_a / digit || multiplier * sign < limit_b / digit)) // check overflow + exclude division by 0
        {
            result.overflow = 0;
            return result;
        }
        long temp = multiplier * sign * digit; // 100 * 1 * 1 = 100
        if ((sign == 1 && l_number > limit_a -  temp) || (sign == -1 && l_number < limit_b - temp)) // check overflow
        {
            result.overflow = 0;
            return result;
        }
        l_number += temp; // 0 += 100 += 20 += 3 => 123;
    }
    
    result.overflow = 1;
    result.number = l_number;
    return result;
}

// Trimms all leading and trailing spaces and tabs
void trimStr(std::string & value)
{
    while (!value.empty() && value.find_first_of(" \t") == 0)
        value.erase(0, 1);
    while (!value.empty() && (value[value.size()-1] == ' ' || value[value.size()-1] == '\t'))
        value.erase(value.size() - 1);
}