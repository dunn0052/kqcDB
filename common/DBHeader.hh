#ifndef __DB_HEADER_HH
#define __DB_HEADER_HH


#include <common/OSdefines.hh>
#ifdef WINDOWS_PLATFORM
using pthread_rwlock_t = bool;
#else
#include <pthread.h>
#endif
/*
 * NOTE: DBHeader values should be accessed before fields
 *       to remain cache friendly
 */
struct DBHeader
{
    pthread_rwlock_t m_DBLock;
    char m_ObjectName[24];
    size_t m_NumRecords;
    size_t m_LastWritten;
};

#endif