#ifndef __QC_DB_HH
#define __QC_DB_HH

#include <common/OSdefines.hh>

#include <string>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef WINDOWS_PLATFORM
#include <Windows.h>
#else
#include <sys/mman.h>
#endif
#include <iostream>
#include <algorithm>
#include <functional>
#include <thread>

#include <common/Retcode.hh>
#include <common/DBHeader.hh>

namespace qcDB
{
    template <class object>
    class dbInterface
    {

public:

        /*
         * Read object at given record.
         */
        RETCODE ReadObject(size_t record, object& out_object)
        {
            RETCODE retcode = RTN_OK;
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            memcpy(&out_object, p_object, sizeof(object));

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        /*
         * Read several objects given a vector of tuples <record, empty object>.
         * The empty object will have the data of the object of the given record
         * copied into it.
         */
        RETCODE ReadObjects(std::vector<std::tuple<size_t, object>>& objects)
        {
            RETCODE retcode = RTN_OK;
            // Sort here to be cache friendly
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            for(std::tuple<size_t, object> readObject : objects)
            {
                char* p_object = Get(std::get<0>(readObject));
                if(nullptr == p_object)
                {
                    return RTN_NULL_OBJ;
                }
                memcpy(&std::get<1>(readObject), p_object, sizeof(object));
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        /*
         * Overwrite object data at given record.
         */
        RETCODE WriteObject(size_t record, object& objectWrite)
        {
            RETCODE retcode = RTN_OK;
            char* p_object = Get(record);
            if(nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
            header->m_LastWritten = record;
            if (header->m_Size < record)
            {
                header->m_Size = record;
            }

            memcpy(p_object, &objectWrite, sizeof(object));

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        /*
         * Write an object at the next available (empty) record.
         */
        RETCODE WriteObject(object& objectWrite)
        {
            RETCODE retcode = RTN_OK;
            object emptyObject = { 0 };
            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
            size_t record = header->m_LastWritten;
            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader) + record * sizeof(object));
            for (; record < NumberOfRecords(); record++)
            {
                if (std::memcmp(currentObject, &emptyObject, sizeof(object)) == 0)
                {
                    header->m_LastWritten = record;
                    memcpy(currentObject, &objectWrite, sizeof(object));

                    if (header->m_Size < record)
                    {
                        header->m_Size = record;
                    }

                    break;
                }

                currentObject++;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if(NumberOfRecords() == record)
            {
                return RTN_NOT_FOUND;
            }

            return RTN_OK;
        }

        /*
         * Write multiple objects by record. The vector is a list of <record, object data> to
         * be overwritten at the corresponding record.
         */
        RETCODE WriteObjects(std::vector<std::tuple<size_t, object>>& objects)
        {

            RETCODE retcode = RTN_OK;
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& first, const std::tuple<size_t, object>& second)
                {
                    return std::get<0>(first) < std::get<0>(second);
                }
            );

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            for(std::tuple<size_t, object> writeObject : objects)
            {
                char* p_object = Get(std::get<0>(writeObject));
                if(nullptr == p_object)
                {
                    return RTN_NULL_OBJ;
                }
                memcpy(p_object, &std::get<1>(writeObject), sizeof(object));
            }

            DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
            header->m_LastWritten = std::get<0>(objects.back());

            if (header->m_Size < std::get<0>(objects.back()))
            {
                header->m_Size = std::get<0>(objects.back());
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        /*
         * Write multiple objects at the next available (empty) records.
         */
        RETCODE WriteObjects(std::vector<object>& objects)
        {
            RETCODE retcode = RTN_OK;
            const object emptyObject = { 0 };
            auto objectsIterator = objects.begin();

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
            size_t record = header->m_LastWritten;
            object* currentObject = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader) + record * sizeof(object));
            for (; record < NumberOfRecords(); record++)
            {
                if (0 == std::memcmp(currentObject, &emptyObject, sizeof(object)))
                {
                    if (objectsIterator == objects.end())
                    {
                        header->m_LastWritten = record;
                        break;
                    }

                    memcpy(currentObject, &(*objectsIterator), sizeof(object));

                    ++objectsIterator;
                }

                currentObject++;
            }

            if (header->m_Size < record)
            {
                header->m_Size = record;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if (objectsIterator != objects.end())
            {
                return RTN_EOF;
            }

            return RTN_OK;
        }

        /*
         * Clear out the data at a given record.
         */
        RETCODE DeleteObject(size_t record)
        {
            RETCODE retcode = RTN_OK;
            object deletedObject = { 0 };
            char* p_object = Get(record);
            if (nullptr == p_object)
            {
                return RTN_NULL_OBJ;
            }

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }
            DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
            size_t size = header->m_Size;

            memset(p_object, 0, sizeof(object));

            // If the deleted record was the last one, find the next last record
            if (size == record)
            {
                for (; p_object != m_DBAddress; p_object -= sizeof(object))
                {
                    --record;
                    if (std::memcmp(p_object, &deletedObject, sizeof(object)) != 0)
                    {
                        break;
                    }
                }

                header->m_Size = record;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        /*
         * Zero out all data from the database.
         */
        RETCODE Clear(void)
        {
            RETCODE retcode = RTN_OK;
            if (m_IsOpen)
            {
                retcode = LockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                DBHeader* header = reinterpret_cast<DBHeader*>(m_DBAddress);
                size_t dbSize = header->m_NumRecords * sizeof(object);
                object* start = reinterpret_cast<object*>(m_DBAddress + sizeof(DBHeader));

                memset(start, 0, dbSize);

                header->m_LastWritten = 0;
                header->m_Size = 0;

                retcode = UnlockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                return RTN_OK;
            }

            return RTN_MALLOC_FAIL;
        }

        /*
         * A user defined search function.
         * Example lambda function
         * [](const CHARACTER* character) -> bool
         * {
         *    return !strcmp(character->NAME, "KEVIN");
         * }
         */
        using Predicate = std::function<bool(const object* currentObject)>;

        /*
         * If multiple records would match the predicate,
         * return the record of the first one found.
         */
        RETCODE FindFirstOf(Predicate predicate, size_t& out_Record)
        {
            RETCODE retcode = RTN_OK;
            bool found = false;
            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            size_t size = reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size;
            size_t record = 0;
            for (record = 0; record < size; record++)
            {
                if (predicate(currentObject))
                {
                    out_Record = record;
                    found = true;
                    break;
                }

                currentObject++;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            if(!found)
            {
                return RTN_NOT_FOUND;
            }

            return RTN_OK;
        }

        /*
         * Find objects using the predicate by sharding the database and searching
         * in parallel.
         */
        RETCODE FindObjects(Predicate predicate, std::vector<object>& out_MatchingObjects)
        {
            RETCODE retcode = RTN_OK;
            // Number of threads /2 so we don't completely lock up the CPU
#ifdef WINDOWS_PLATFORM
            size_t numThreads = std::thread::hardware_concurrency() / 2;
#else
            size_t numThreads = sysconf(_SC_NPROCESSORS_ONLN) / 2;
#endif
            std::vector<std::thread> threads;
            std::vector<std::vector<object>> results(numThreads);

            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            size_t size = reinterpret_cast<DBHeader*>(m_DBAddress)->m_Size;
            size_t segmentSize = size / numThreads;

            for (size_t threadIndex = 0; threadIndex < numThreads - 1; threadIndex++)
            {
                threads.emplace_back(FinderThread, predicate, currentObject, segmentSize, std::ref(results[threadIndex]));
                currentObject += segmentSize;
            }

            threads.emplace_back(FinderThread, predicate, currentObject, size - ((numThreads - 1) * segmentSize), std::ref(results[numThreads - 1]));

            for (std::thread& thread : threads)
            {
                thread.join();
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            for (std::vector<object>& matches : results)
            {
                for (object& match : matches)
                {
                    out_MatchingObjects.push_back(match);
                }
            }

            return RTN_OK;
        }

        /*
         * Total number of records to be accessed by users.
         */
        inline size_t NumberOfRecords(void)
        {
            if (m_IsOpen)
            {
                return m_NumRecords;
            }

            return 0;
        }

        /*
         * Gather last written record for users.
         */
        inline size_t LastWrittenRecord(size_t& record)
        {
            RETCODE retcode = RTN_OK;
            size_t lastWrittenRecord = 0;

            if (m_IsOpen)
            {
                retcode = LockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

                record = reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten;

                retcode = UnlockDB();
                if (RTN_OK != retcode)
                {
                    return retcode;
                }

            }
            else
            {
                return RTN_NULL_OBJ;
            }

            return retcode;
        }

        dbInterface(const std::string& dbPath) :
            m_IsOpen(false), m_Size(0),
            m_DBAddress(nullptr), m_NumRecords(0)
#ifdef WINDOWS_PLATFORM
            , m_Mutex(INVALID_HANDLE_VALUE)
#endif
        {
#ifdef WINDOWS_PLATFORM
            HANDLE hFile = CreateFileA(
                static_cast<LPCSTR>(dbPath.c_str()), // File name
                GENERIC_READ | GENERIC_WRITE,        // Access mode
                FILE_SHARE_READ | FILE_SHARE_WRITE,  // Share mode
                NULL,                                // Security attributes
                OPEN_EXISTING,                         // How to create
                FILE_ATTRIBUTE_NORMAL,               // File attributes
                NULL                                 // Handle to template file
            );

            if (hFile == INVALID_HANDLE_VALUE)
            {
                return;
            }

            // Get the file size
            m_Size = static_cast<size_t>(GetFileSize(hFile, NULL));
            if (m_Size == INVALID_FILE_SIZE) {
                CloseHandle(hFile);
                return;
            }

            HANDLE hMapFile = CreateFileMappingA(
                hFile,                          // File handle
                NULL,                           // Security attributes
                PAGE_READWRITE,                 // Protection
                0,                              // High-order 32 bits of file size
                0,                              // Low-order 32 bits of file size
                NULL                            // Name of file-mapping object
            );

            if (hMapFile == NULL) {
                CloseHandle(hFile);
                return;
            }

            // Close the file handle, as it's not needed anymore
            CloseHandle(hFile);


            // Map the file to memory
            m_DBAddress = static_cast<char*>(MapViewOfFile(
                hMapFile,                       // Handle to file mapping object
                FILE_MAP_ALL_ACCESS,            // Access mode
                0,                              // High-order 32 bits of file offset
                0,                              // Low-order 32 bits of file offset
                0                               // Number of bytes to map (0 for all)
            ));

            // Close the file mapping handle
            CloseHandle(hMapFile);

            if (nullptr == m_DBAddress)
            {
                return;
            }
#else
            int fd = open(dbPath.c_str(), O_RDWR);
            if(INVALID_FD > fd)
            {
                return;
            }

            struct stat statbuf;
            int error = fstat(fd, &statbuf);
            if(0 > error)
            {
                return;
            }

            m_Size = statbuf.st_size;
            m_DBAddress = static_cast<char*>(mmap(nullptr, m_Size,
                    PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE,
                    fd, 0));
            if(MAP_FAILED == m_DBAddress)
            {
                return;
            }

#endif
            m_NumRecords = reinterpret_cast<DBHeader*>(m_DBAddress)->m_NumRecords;

            m_IsOpen = true;
        }

        ~dbInterface(void)
        {
#ifdef WINDOWS_PLATFORM
            // Unmap the file view
            UnmapViewOfFile(m_DBAddress);

    #else
            int error = munmap(m_DBAddress, m_Size);
            if(0 == error)
            {
                // Nothing much you can do in this case..
                m_IsOpen = false;
            }
#endif
        }

protected:

    /*
     * Lock the DB. Several ways to do this depending on OS.
     */
    RETCODE LockDB(void)
    {
#ifdef WINDOWS_PLATFORM
        m_Mutex = CreateMutexA(NULL, FALSE, "MutexForFileLock");
        if (nullptr == m_Mutex)
        {
            return RTN_LOCK_ERROR;
        }
#else

        int lockError = pthread_rwlock_rdlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
        if (0 != lockError)
        {
            return RTN_LOCK_ERROR;
        }
#endif

        return RTN_OK;
    }

    /*
     * Unlock the DB. Several ways to do this depending on OS.
     */
    RETCODE UnlockDB(void)
    {
#ifdef WINDOWS_PLATFORM
        if (ReleaseMutex(m_Mutex))
        {
            return RTN_LOCK_ERROR;
        }
#else
        lockError = pthread_rwlock_unlock(&reinterpret_cast<DBHeader*>(m_DBAddress)->m_DBLock);
        if (0 != lockError)
        {
            return RTN_LOCK_ERROR;
        }
#endif

    return RTN_OK;
    }

    /*
     * Get a pointer into the database according to the record number.
     * Returns a nullptr on error.
     */
    char* Get(const size_t record)
    {
        if(NumberOfRecords() - 1 < record)
        {
            return nullptr;
        }

        if(!m_IsOpen || nullptr == m_DBAddress)
        {
            return nullptr;
        }

        // Record off by 1 adjustment
        size_t byte_index = sizeof(DBHeader) + sizeof(object) * record;
        if(m_Size < byte_index)
        {
            return nullptr;
        }

        return m_DBAddress + byte_index;
    }

    /*
     * Internal thread function that is used to run the predicate
     * in parallel in the sharded database.
     */
    static void FinderThread(Predicate predicate, const object* currentObject, size_t numRecords, std::vector<object>& results)
    {
        for (size_t record = 0; record < numRecords; record++)
        {
            if (predicate(++currentObject))
            {
                results.push_back(*currentObject);
            }
        }
    }

    bool m_IsOpen;
    size_t m_Size;
    size_t m_NumRecords;
    char* m_DBAddress;

#ifdef WINDOWS_PLATFORM
    HANDLE m_Mutex;
#else
#endif

    static constexpr int INVALID_FD = 0;

    };
}

#endif