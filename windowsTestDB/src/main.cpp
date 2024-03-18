#include <common/CLI.hh>
#include <qcDB/qcDB.hh>
#include <dbHeaders/INDEX.hh>

std::string g_NAME = "KEVIN";

int main(int argc, char* argv[])
{
    CLI_StringArgument dbPathArg("-d", "The path to the INDEX database file", true);
    CLI_IntArgument timeArg("-s", "Number of seconds to run the test");
    CLI_IntArgument numProcessArg("-p", "Number of each read and write processes");
    CLI_IntArgument numRecordAltsArg("-r", "Number of altered records");
    CLI_StringArgument nameArg("-n", "Character name");

    Parser parser("charTEST", "Test CHARACTER database");

    parser
        .AddArg(dbPathArg)
        .AddArg(timeArg)
        .AddArg(numProcessArg)
        .AddArg(numRecordAltsArg)
        .AddArg(nameArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);

    if (RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    if (nameArg.IsInUse())
    {
        g_NAME = nameArg.GetValue();
    }

    qcDB::dbInterface<INDEX> database(dbPathArg.GetValue());

    INDEX entry = { 0 };
    errno_t error = strncpy_s(entry.PATH, g_NAME.c_str(), g_NAME.length());
    if(error)
    {
        LOG_FATAL("Could not write name to DB object due to error:", ErrorString(error));
        return error;
    }

    retcode = database.WriteObject(0, entry);
    if(RTN_OK != retcode)
    {
        LOG_FATAL("Could not write: ", g_NAME, " to database");
    }

    LOG_INFO("Wrote: \"", g_NAME, "\" to: ", dbPathArg.GetValue());

    return retcode;
}