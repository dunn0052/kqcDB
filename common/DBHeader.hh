#ifndef __DB_HEADER_HH
#define __DB_HEADER_HH

#include <pthread.h>

/*
 * NOTE: DBHeader values should be accessed before fields
 *       to remain cache friendly
 */
struct DBHeader
{
    pthread_mutex_t m_DBLock;
    char m_ObjectName[20];
    size_t m_NumRecords;
    size_t m_LastWritten;
};

#endif