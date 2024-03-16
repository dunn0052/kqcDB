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

#include <common/Retcode.hh>
#include <common/DBHeader.hh>

namespace qcDB
{
    template <class object>
    class dbInterface
    {

public:

        RETCODE ReadObject(size_t record, object& out_object)
        {
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

        RETCODE ReadObjects(std::vector<std::tuple<size_t, object>>& objects)
        {
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
                memcpy(&std::get<1>(readObject), p_object, sizeof(readObject));
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

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

            reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = record;
            memcpy(p_object, &objectWrite, sizeof(object));

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        RETCODE WriteObjects(std::vector<std::tuple<size_t, object>>& objects)
        {

            RETCODE retcode = RTN_OK;
            std::sort(objects.begin(), objects.end(),
                [](const std::tuple<size_t, object>& a, const std::tuple<size_t, object>& b) {
                return std::get<0>(a) < std::get<0>(b);
                });

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
                memcpy(p_object, &std::get<1>(writeObject), sizeof(writeObject));
            }

            reinterpret_cast<DBHeader*>(m_DBAddress)->m_LastWritten = std::get<0>(objects.back());
            
            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        using Predicate = std::function<bool(const object* currentObject)>;
        RETCODE FindObjects(Predicate predicate, std::vector<size_t>& out_MatchingObjects)
        {
            retcode = LockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }


            const object* currentObject = reinterpret_cast<const object*>(m_DBAddress + sizeof(DBHeader));
            for(size_t record = 0; record < NumberOfRecords(); record++)
            {
                if(predicate(currentObject))
                {
                    out_MatchingObjects.push_back(record);
                }

                currentObject++;
            }

            retcode = UnlockDB();
            if (RTN_OK != retcode)
            {
                return retcode;
            }

            return RTN_OK;
        }

        inline size_t NumberOfRecords(void)
        {
            if(m_IsOpen)
            {
                return m_NumRecords;
            }

            return 0;
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
                FILE_SHARE_READ,                     // Share mode
                NULL,                                // Security attributes
                OPEN_ALWAYS,                         // How to create
                FILE_ATTRIBUTE_NORMAL,               // File attributes
                NULL                                 // Handle to template file
            );

            if (hFile == INVALID_HANDLE_VALUE) {
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