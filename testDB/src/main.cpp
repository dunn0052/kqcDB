#include <common/CLI.hh>
#include <common/Logger.hh>

#include <qcDB/qcDB.hh>
#include <dbHeaders/CHARACTER.hh>

#include <chrono>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <random>

int64_t g_TIME = 1;
int g_NUM_PROCESSES = 4;

static void DBWriters(const std::string& dbPath, long long totals[], int index)
{
    RETCODE retcode = RTN_OK;
    qcDB::dbInterface<CHARACTER> database(dbPath);

    CHARACTER character = { 0 };
    character.AGE = index;


    // Set up RNG
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution(0, database.NumberOfRecords() - 1);

    bool running = true;
    int randomRecord = 0;
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();
    while(running)
    {
        randomRecord = distribution(gen);
        retcode = database.WriteObject(randomRecord, character);
        if(RTN_OK != retcode)
        {
            LOG_WARN("Error writing record: ", randomRecord, " with error: ", retcode);
            return;
        }

        (*totals)++;
        if(std::chrono::steady_clock::now() - start > std::chrono::seconds(g_TIME))
        {
            running = false;
        }
    }

    LOG_DEBUG("Wrote: ", *totals, " records");
}

static void DBReader(const std::string& dbPath, long long totals[], int index)
{
    RETCODE retcode = RTN_OK;

    qcDB::dbInterface<CHARACTER> database(dbPath);

    CHARACTER character = { 0 };

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> distribution(0, database.NumberOfRecords() - 1);

    int randomRecord = 0;

    bool running = true;
    std::chrono::_V2::steady_clock::time_point start = std::chrono::steady_clock::now();
    while(running)
    {
        randomRecord = distribution(gen);
        retcode = database.ReadObject(randomRecord, character);
        if(RTN_OK != retcode)
        {
            LOG_WARN("Error reading record: ", randomRecord, " with error: ", retcode);
            return;
        }

        (*totals)++;
        if(std::chrono::steady_clock::now() - start > std::chrono::seconds(g_TIME))
        {
            running = false;
        }
    }

    LOG_DEBUG("Read: ", *totals, " records");

}

int main(int argc, char* argv[])
{
    CLI_StringArgument dbPathArg("-d", "The path to the CHARACTER database file", true);
    CLI_IntArgument timeArg("-s", "Number of seconds to run the test");
    CLI_IntArgument numProcessArg("-p", "Number of each read and write processes");

    Parser parser("charTEST", "Test CHARACTER database");

    parser
        .AddArg(dbPathArg)
        .AddArg(timeArg)
        .AddArg(numProcessArg);

    RETCODE retcode = parser.ParseCommandLineArguments(argc, argv);

    if(RTN_OK != retcode)
    {
        parser.Usage();
        return retcode;
    }

    qcDB::dbInterface<CHARACTER> database(dbPathArg.GetValue());

    if(timeArg.IsInUse())
    {
        g_TIME = timeArg.GetValue();
    }

    if(numProcessArg.IsInUse())
    {
        g_NUM_PROCESSES = numProcessArg.GetValue();
    }

    // One process for a read and one for write
    g_NUM_PROCESSES *= 2;

    /// Set up shared memory key on a file
    key_t key = ftok("/home/osboxes/.bashrc", 1);
    if(-1 == key)
    {
        LOG_FATAL("Could not get shared memory key due to error: ", ErrorString(errno));
        return RTN_NULL_OBJ;
    }

    // Create an array of long long for totals of each write/read process
    int shm_id = shmget(key, g_NUM_PROCESSES * sizeof(long long), 0666 | IPC_CREAT);;
    if(-1 == shm_id)
    {
        LOG_FATAL("Could not create shared memory array due to error: ", ErrorString(errno));
        return RTN_MALLOC_FAIL;
    }

    // Get a pointer to the shared memory array
    long long* totals = static_cast<long long*>(shmat(shm_id, NULL, 0));
    if(nullptr == totals)
    {
        LOG_FATAL("Could not get shared memory array due to error: ", ErrorString(errno));
        return RTN_NULL_OBJ;
    }

    // Ensure that all entries start at 0 (probably not actually needed)
    memset(totals, 0, g_NUM_PROCESSES * sizeof(long long));

    int* pids = new int[g_NUM_PROCESSES];

    for(int processIndex = 0; processIndex < g_NUM_PROCESSES; processIndex++)
    {
        if(processIndex % 2)
        {
            pids[processIndex] = fork();
            if(pids[processIndex] == 0)
            {
                DBReader(dbPathArg.GetValue(), &totals[processIndex], processIndex);
                return 0;
            }
            else
            {
                continue;
            }
        }
        else
        {
            pids[processIndex] = fork();
            if(pids[processIndex] == 0)
            {
                DBWriters(dbPathArg.GetValue(), &totals[processIndex], processIndex);
                return 0;
            }
            else
            {
                continue;
            }
        }
    }

    for (int processIndex = 0; processIndex < g_NUM_PROCESSES; ++processIndex)
    {
        int status;
        while (-1 == waitpid(pids[processIndex], &status, 0));
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            std::cerr << "Process " << processIndex << " (pid " << pids[processIndex] << ") failed\n";
            exit(1);
        }
    }

    long long totalReads = 0;
    long long totalWrites = 0;

    for(int processIndex = 0; processIndex < g_NUM_PROCESSES; processIndex++)
    {
        if(processIndex % 2)
        {
            totalReads += totals[processIndex];
        }
        else
        {
            totalWrites += totals[processIndex];

        }
    }

    LOG_INFO("Total reads: ", totalReads, " total writes: ", totalWrites);
    LOG_INFO("Total reads + writes: ", totalReads + totalWrites, " in: ", g_TIME, " second(s)");

    shmctl(shm_id, IPC_RMID, 0);

    return retcode;
}