#ifndef __UTILITY_FUNCTIONS_HH
#define __UTILITY_FUNCTIONS_HH

#include <string>
#include <iostream>

static std::string ErrorString(int errornum)
{
        char errmsg[94] = { 0 }; 
        errno_t error = strerror_s(errmsg, 94 - 1, errornum);
        if(error)
        {
            std::cerr << "Error printing errno: " << errornum << "/n";
        }
        return errmsg;
}

#endif