#include <dbGenerator/inc/Schema.hh>
#include <dbGenerator/inc/ObjectSchema.hh>
#include <common/Logger.hh>
#include <common/Constants.hh>
#include <common/UtilityFunctions.hh>

#include <fstream>

static RETCODE ParseField(std::istringstream& lineStream, FIELD_SCHEMA& out_field)
{
    FIELD_SCHEMA field;
    lineStream >> field;
    if(lineStream.fail())
    {
        return RTN_BAD_ARG;
    }

    switch(static_cast<FIELD_TYPE>(field.fieldType))
    {
        case FIELD_TYPE::INT:
        {
            field.fieldSize = sizeof(int);
            break;
        }
        case FIELD_TYPE::UINT:
        {
            field.fieldSize = sizeof(unsigned int);
            break;
        }
        case FIELD_TYPE::LONG:
        {
            field.fieldSize = sizeof(long);
            break;
        }
        case FIELD_TYPE::ULONG:
        {
            field.fieldSize = sizeof(unsigned long);
            break;
        }
        case FIELD_TYPE::CHAR:
        {
            field.fieldSize = sizeof(char);
            break;
        }
        case FIELD_TYPE::BYTE:
        {
            field.fieldSize = sizeof(unsigned char);
            break;
        }
        case FIELD_TYPE::BOOL:
        {
            field.fieldSize = sizeof(bool);
            break;
        }
        case FIELD_TYPE::PADDING:
        {
            field.fieldSize = sizeof(unsigned char);
            break;
        }
        default:
        {
            LOG_FATAL("field: ",
                field.fieldName,
                " type: ",
                field.fieldType,
                " is invalid");

            return RTN_BAD_ARG;
        }
    }

    field.fieldSize *= field.numElements;

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

static RETCODE GenerateObjectHeader(const OBJECT_SCHEMA& object, std::ofstream& headerFile)
{
    headerFile << "// GENERATED: DO NOT MODIFY!!\n";
    /* Header guard */
    headerFile << "#ifndef " << std::uppercase << object.objectName << "__HH\n";
    headerFile << "#define " << std::uppercase << object.objectName << "__HH\n\n";

    headerFile
        << "\nstruct "
        << std::uppercase
        << object.objectName
        << "\n{";

    if(headerFile.bad())
    {
        LOG_FATAL("Could not write object: ",
            object.objectName,
            " due to error: ",
            ErrorString(errno));

        return RTN_FAIL;
    }

    return RTN_OK;
}

static RETCODE GenerateFieldHeader(const FIELD_SCHEMA& field, std::ofstream& headerFile)
{
    std::string dataType;
    switch(static_cast<FIELD_TYPE>(field.fieldType))
    {
        case FIELD_TYPE::INT:
        {
            dataType = "int";
            break;
        }
        case FIELD_TYPE::UINT:
        {
            dataType = "unsigned int";
            break;
        }
        case FIELD_TYPE::LONG:
        {
            dataType = "long";
            break;
        }
        case FIELD_TYPE::ULONG:
        {
            dataType = "unsigned long";
            break;
        }
        case FIELD_TYPE::CHAR:
        {
            dataType = "char";
            break;
        }
        case FIELD_TYPE::BYTE:
        {
            dataType = "unsigned char";
            break;
        }
        case FIELD_TYPE::BOOL:
        {
            dataType = "bool";
            break;
        }
        case FIELD_TYPE::PADDING:
        {
            dataType = "unsigned char";
            break;
        }
        default:
        {
            LOG_FATAL("field: ",
                field.fieldName,
                " type: ",
                field.fieldType,
                " is invalid");

            return RTN_BAD_ARG;
        }
    }

    if(field.numElements > 1)
    {
        /* Array of elements */
        headerFile
            << "\n    "
            << dataType
            << " "
            << field.fieldName
            << "["
            << field.numElements
            << "];";
    }
    else
    {
        headerFile
            << "\n    "
            << dataType
            << " "
            << field.fieldName
            << ";";
    }

    if(headerFile.bad())
    {
        LOG_FATAL("Could not generate header file entry for field: ",
            field.fieldName,
            " due to error: ",
            ErrorString(errno));

        return RTN_FAIL;
    }

    return RTN_OK;
}

static RETCODE GenerateObectFooter(const OBJECT_SCHEMA& object, std::ofstream& headerFile)
{
    headerFile << "\n};\n\n";

    headerFile << "#endif";

    if(headerFile.bad())
    {
        LOG_FATAL("Could not generate footer for header file: ",
            object.objectName,
            " due to error: ",
            ErrorString(errno));
        return RTN_FAIL;
    }

    return RTN_OK;
}

static RETCODE GenerateHeader(const OBJECT_SCHEMA& object, const std::string& outputDirectory)
{
    RETCODE retcode = RTN_OK;
    std::string outputPath = outputDirectory + object.objectName + CONSTANTS::HEADER_EXT;

    std::ofstream headerFile(outputPath);
    if(!headerFile)
    {
        LOG_FATAL("Could not create: ",
            outputPath,
            " due to error: ",
            ErrorString(errno));

        return RTN_NOT_FOUND;
    }

    retcode = GenerateObjectHeader(object, headerFile);
    if(RTN_OK != retcode)
    {
        return retcode;
    }

    for(const FIELD_SCHEMA& field : object.fields)
    {
        retcode = GenerateFieldHeader(field, headerFile);
        if(RTN_OK != retcode)
        {
            return retcode;
        }
    }

    retcode = GenerateObectFooter(object, headerFile);

    LOG_INFO("Generated: ", outputPath);

    return retcode;
}

size_t CalculateByteBounds(const OBJECT_SCHEMA& object)
{
    size_t extraBytes = object.objectSize % CONSTANTS::WORD_SIZE;

    // If this field doesn't end on a word boundary then we will need to add padding
    if(extraBytes)
    {
        return false;
    }

    return true;
}

RETCODE GenerateDatabase(const std::string& schemaPath, const std::string& outputPath, bool isStrict)
{
    RETCODE retcode = RTN_OK;
    size_t currentLineNumber = 0;
    size_t firstNonEmptyChar = 0;
    char firstChar = 0;
    OBJECT_SCHEMA object = { 0 };
    bool readObject = false;

    std::ifstream schema(schemaPath);
    if(!schema)
    {
        int error = errno;
        LOG_FATAL("Failed to open: ",
            schemaPath,
            " due to error: ",
            ErrorString(error));
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

        if(std::string::npos == firstNonEmptyChar)
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

            object.objectSize += field.fieldSize;
            object.fields.push_back(field);
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

    retcode = GenerateHeader(object, outputPath);

    return retcode;
}
