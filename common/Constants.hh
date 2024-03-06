#ifndef __CONSTANTS_HH
#define __CONSTANTS_HH

#include <string>

namespace CONSTANTS
{
    constexpr size_t WORD_SIZE = 8;

    const std::string CURRENT_DIRECTORY = "./";

    const char SCHEMA_COMMENT = '#';

    const std::string SCHEMA_EXT = ".skm";
    const std::string HEADER_EXT = ".hh";
    const std::string DB_EXT = ".qcdb";

    constexpr int RW = 0666;
}


#endif