#include <common/Retcode.hh>
#include <common/Logger.hh>
#include <common/CLI.hh>
#include <common/Constants.hh>

#include <dbGenerator/inc/Schema.hh>

int main(int argc, char* argv[])
{
    CLI_StringArgument schemaArg("-s", "Path to schema JSON file", true);
    CLI_StringArgument outputPathArg("-o", "Path to the output files");
    CLI_FlagArgument strictArg("--strict", "Enforce byte boundaries for compact databases");

    Parser parser("dbGenerator", "Generates a qcDB file");

    parser.AddArg(schemaArg).AddArg(outputPathArg).AddArg(strictArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);
    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    std::string schemaPath = schemaArg.GetValue();

    std::string outputPath;
    if(outputPathArg.IsInUse())
    {
        outputPath = outputPathArg.GetValue();
    }
    else
    {
        outputPath = CONSTANTS::CURRENT_DIRECTORY;
    }

    retcode = GenerateDatabase(schemaPath, outputPath, strictArg.IsInUse());

    return retcode;
}