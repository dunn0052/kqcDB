#include <common/Retcode.hh>
#include <common/Logger.hh>
#include <common/CLI.hh>
#include <common/Constants.hh>

#include <dbGenerator/inc/Schema.hh>

int main(int argc, char* argv[])
{
    CLI_StringArgument schemaArg("-s", "Path to schema JSON file", true);
    CLI_StringArgument headerPathArg("-h", "Path to the header file");
    CLI_StringArgument databasePathArg("-d", "Path to the database file");
    CLI_FlagArgument strictArg("--strict", "Enforce byte boundaries for compact databases");

    Parser parser("dbGenerator", "Generates a qcDB file");

    parser
        .AddArg(schemaArg)
        .AddArg(headerPathArg)
        .AddArg(strictArg)
        .AddArg(databasePathArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    std::string schemaPath = schemaArg.GetValue();

    std::string headerOutputPath;
    if(headerPathArg.IsInUse())
    {
        headerOutputPath = headerPathArg.GetValue();
    }
    else
    {
        headerOutputPath = CONSTANTS::CURRENT_DIRECTORY;
    }

    std::string databaseOutputPath;
    if(databasePathArg.IsInUse())
    {
        databaseOutputPath = databasePathArg.GetValue();
    }
    else
    {
        databaseOutputPath = CONSTANTS::CURRENT_DIRECTORY;
    }

    retcode = GenerateDatabase(schemaPath,
        headerOutputPath,
        databaseOutputPath,
        strictArg.IsInUse());

    return retcode;
}