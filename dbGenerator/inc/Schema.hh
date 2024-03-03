#ifndef __SCHEMA_HH
#define __SCHEMA_HH

#include <string>
#include <common/Retcode.hh>

RETCODE GenerateDatabase(const std::string& schemaPath, const std::string& outputPath, bool isStrict);

#endif