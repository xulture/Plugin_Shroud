//////////////////////////////////////////////////////////////////////////
// JobManager.h
//
// This source file is part of the Shroud(TM) Example Projects.  This code,
// but not the code of the Shroud SDK, is subject to the following terms:
//
// Copyright (C) 2009 - 2012 CloakWorks Inc.
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Materials.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY DAMAGES OR OTHER
// LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
// IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////

#ifndef __JOBMANAGER_H_
#define __JOBMANAGER_H_

#include <windows.h>
#include <queue>
#include <vector>

namespace ShroudPlugin
{

    class Lock;

    class Mutex
    {
        public:
            Mutex();
            ~Mutex();
        private:
            friend class Lock;
            void Acquire();
            void Release();
            CRITICAL_SECTION m_critSection;
    };

    struct JobDesc
    {
        JobDesc() : m_jobFunc( NULL ), m_jobData( NULL ), m_jobID( 0 ) {}
        bool IsValid() const
        {
            return ( m_jobFunc != NULL );
        }

        void    ( *m_jobFunc )( void* );
        void*   m_jobData;
        DWORD   m_jobID;
    };

    class JobContext
    {
        public:
            JobContext();
            ~JobContext();

            JobDesc PopJobFromQueue();
            void    PushJobOnQueue( const JobDesc& job );

            void    AddFinishedJob( const JobDesc& job );

            // Returns true if a job with the given ID has been found and successfully removed
            // from the completed list, false otherwise
            bool    ClearCompletedJob( DWORD jobID );

            HANDLE  GetJobSubmittedHandle() const
            {
                return m_hJobSubmittedEvent;
            }
            HANDLE  GetJobFinishedHandle() const
            {
                return m_hJobFinishedEvent;
            }

            size_t  GetNumQueuedJobs() const
            {
                return m_queuedJobs.size();
            }
            size_t  GetNumFinishedJobs() const
            {
                return m_finishedJobs.size();
            }

        private:
            std::queue<JobDesc>     m_queuedJobs;
            std::vector<JobDesc>    m_finishedJobs;

            Mutex                   m_queuedJobMutex;
            Mutex                   m_finishedJobMutex;

            HANDLE                  m_hJobSubmittedEvent;
            HANDLE                  m_hJobFinishedEvent;
    };


    class WorkerThread
    {
        public:
            WorkerThread( JobContext* jobContext );
            ~WorkerThread ();

            HANDLE GetThreadHandle() const;

        private:
            WorkerThread( const WorkerThread& rhs );
            WorkerThread& operator=( const WorkerThread& rhs );

            static DWORD WINAPI ThreadFunc( void* arg );

            struct ThreadContext
            {
                ThreadContext( JobContext* jobContext ) : m_jobContext( jobContext ) {}
                JobContext*     m_jobContext;
                HANDLE          m_hExitThreadEvent;
            };

            HANDLE          m_handle;
            DWORD           m_threadId;
            ThreadContext   m_threadContext;
    };


    // An reference implementation of a job manager that can be used with Shroud
    class JobManager : public CloakWorks::RefCounted, public CloakWorks::IJobMgr
    {
        public:
            JobManager( int numThreads );
            ~JobManager();

            static int GetNumProcessors();

            // IJobMgr Functions:
            virtual CloakWorks::JobHandle      LaunchJob( CloakWorks::JobEntryFunction funcPtr, void* data );
            virtual void                       WaitForJob( CloakWorks::JobHandle handle );
            virtual void                       WaitForAllJobs();

            JobContext                         m_jobContext;

        private:
            std::vector<WorkerThread*>         m_threads;
            DWORD                              m_jobIDCounter;
    };

    typedef CloakWorks::ref_ptr<JobManager> JobManagerPtr;

#endif // __JOBMANAGER_H_

}
