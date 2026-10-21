#pragma once

#include <string>
#include <climits>

struct IntLongOverflow
{
    long number;
    int overflow;
};

IntLongOverflow strToIntLongOverflow(const std::string & type, const std::string & s_number);
std::string strTolower(std::string str);