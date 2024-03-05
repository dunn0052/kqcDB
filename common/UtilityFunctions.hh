#ifndef __UTILITY_FUNCTIONS_HH
#define __UTILITY_FUNCTIONS_HH

#include <string>
#include <iostream>

static std::string ErrorString(int errornum)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
        char errmsg[94] = { 0 };
        errno_t error = strerror_s(errmsg, 94 - 1, errornum);
        if(error)
        {
            std::cerr << "Error printing errno: " << errornum << "/n";
        }
        return errmsg;
#else
        return strerror(errornum);
#endif
}

#endif