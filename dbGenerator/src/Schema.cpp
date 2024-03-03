#include <dbGenerator/inc/Schema.hh>
#include <dbGenerator/inc/ObjectSchema.hh>
#include <common/Logger.hh>
#include <common/Constants.hh>

#include <fstream>

static RETCODE ParseField(std::istringstream& lineStream, FIELD_SCHEMA& out_field)
{
    FIELD_SCHEMA field;
    lineStream >> field;
    if(lineStream.fail())
    {
        return RTN_BAD_ARG;
    }

    LOG_INFO("FIELD NUMBER: ",
        field.fieldNumber,
        " FIELD NAME: ",
        field.fieldName,
        " FIELD TYPE: ",
        field.fieldType,
        " NUMBER OF ELEMENTS: ",
        field.numElements);

    return RTN_OK;
}

static RETCODE ParseObject(std::istringstream& lineStream, OBJECT_SCHEMA& out_object)
{
    lineStream >> out_object;
    if(lineStream.fail())
    {
        return RTN_BAD_ARG;
    }

    LOG_INFO("OBJECT NUMBER: ",
        out_object.objectNumber,
        " OBJECT NAME: ",
        out_object.objectName,
        " NUMBER OF RECORDS: ",
        out_object.numberOfRecords);

    return RTN_OK;
}

RETCODE GenerateDatabase(const std::string& schemaPath, const std::string& outputPath, bool isStrict)
{
    RETCODE retcode = RTN_OK;
    size_t currentLineNumber = 0;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    OBJECT_SCHEMA object;
    bool readObject = false;

    std::ifstream schema(schemaPath);
    if(!schema)
    {
        LOG_FATAL("Failed to open: ",
            schemaPath,
            " due to error: ",
            strerror(errno));
        return RTN_NOT_FOUND;
    }


    std::string line;
    while(std::getline(schema, line))
    {
        currentLineNumber++;
        firstNonEmptyChar = line.find_first_not_of(' ');
        firstChar = line.at(firstNonEmptyChar);

        if(CONSTANTS::SCHEMA_COMMENT == firstChar)
        {
            continue;
        }

        std::istringstream lineStream(line);
        if(readObject)
        {
            FIELD_SCHEMA field;
            retcode = ParseField(lineStream, field);
            if(RTN_OK != retcode)
            {
                LOG_FATAL("Error parsing field on line: ", currentLineNumber);
                return retcode;
            }
        }
        else
        {
            retcode = ParseObject(lineStream, object);
            if(RTN_OK != retcode)
            {
                LOG_FATAL("Error parsing object on line: ", currentLineNumber);
                return retcode;
            }

            readObject = true;
        }

    }

    return RTN_OK;
}
