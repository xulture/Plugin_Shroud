//////////////////////////////////////////////////////////////////////////
// JobManager.cpp
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

#include <StdAfx.h>
#include <JobManager.h>
#include <assert.h>

namespace ShroudPlugin
{

    //////////////////////////////////////////////////////////////////////////
    Mutex::Mutex()
    {
        InitializeCriticalSection ( &m_critSection );
    }

    Mutex::~Mutex()
    {
        DeleteCriticalSection ( &m_critSection );
    }


    void Mutex::Acquire()
    {
        EnterCriticalSection ( &m_critSection );
    }


    void Mutex::Release()
    {
        LeaveCriticalSection ( &m_critSection );
    }


    //////////////////////////////////////////////////////////////////////////
    class Lock
    {
        public:
            Lock ( Mutex& mutex ) : m_mutex( mutex )
            {
                m_mutex.Acquire();
            }

            ~Lock ()
            {
                m_mutex.Release();
            }

        private:
            Lock& operator=( const Lock& rhs );

            Mutex& m_mutex;
    };


    //////////////////////////////////////////////////////////////////////////
    JobContext::JobContext()
    {
        m_hJobSubmittedEvent = CreateEvent(
                                   NULL,                   // default security attributes
                                   FALSE,                  // auto-reset after waking a thread
                                   FALSE,                  // initial state is nonsignaled
                                   NULL                    // no object name
                               );

        m_hJobFinishedEvent = CreateEvent(
                                  NULL,                   // default security attributes
                                  FALSE,                  // auto-reset after waking a thread
                                  FALSE,                  // initial state is nonsignaled
                                  NULL                    // no object name
                              );
    }

    JobContext::~JobContext()
    {
        CloseHandle( m_hJobSubmittedEvent );
        CloseHandle( m_hJobFinishedEvent );
    }

    JobDesc JobContext::PopJobFromQueue()
    {
        Lock queueLock( m_queuedJobMutex );

        JobDesc desc;

        if ( !m_queuedJobs.empty() )
        {
            desc = m_queuedJobs.front();
            m_queuedJobs.pop();
        }

        return desc;
    }

    void JobContext::PushJobOnQueue( const JobDesc& job )
    {
        Lock queueLock( m_queuedJobMutex );
        m_queuedJobs.push( job );
    }

    void JobContext::AddFinishedJob( const JobDesc& job )
    {
        Lock finishedLock( m_finishedJobMutex );
        m_finishedJobs.push_back( job );
    }

    bool JobContext::ClearCompletedJob( DWORD jobID )
    {
        Lock finishedLock( m_finishedJobMutex );

        for ( size_t i = 0; i < m_finishedJobs.size(); ++i )
        {
            if ( m_finishedJobs[i].m_jobID == jobID )
            {
                m_finishedJobs.erase( m_finishedJobs.begin() + i );
                return true;
            }
        }

        return false;
    }


    //////////////////////////////////////////////////////////////////////////
    WorkerThread::WorkerThread ( JobContext* jobContext )
        : m_threadContext( jobContext )
    {
        m_threadContext.m_hExitThreadEvent = CreateEvent(
                NULL,                   // default security attributes
                TRUE,                   // manual-reset event
                FALSE,                  // initial state is nonsignaled
                NULL                    // no object name
                                             );

        m_handle = CreateThread(
                       0,                  // No Security attributes
                       0,                  // Default Stack size
                       &ThreadFunc,        // Pointer to thread entry function
                       &m_threadContext,   // Pointer to thread argument
                       0,                  // No creation flags; start running immediately
                       &m_threadId );      // Store the ID for the created thread
    }

    WorkerThread::~WorkerThread ()
    {
        // Signal the thread to exit
        SetEvent( m_threadContext.m_hExitThreadEvent );

        // Give the thread a chance to exit naturally within the next 2 seconds
        WaitForSingleObject( m_handle, 2000 );

        // Close the thread and the event
        CloseHandle( m_handle );
        CloseHandle( m_threadContext.m_hExitThreadEvent );
    }


    HANDLE WorkerThread::GetThreadHandle() const
    {
        return m_handle;
    }


    //static
    DWORD WINAPI WorkerThread::ThreadFunc( void* arg )
    {
        ThreadContext* context = reinterpret_cast<ThreadContext*>( arg );

        HANDLE waitEvents[2];
        waitEvents[0] = context->m_jobContext->GetJobSubmittedHandle();
        waitEvents[1] = context->m_hExitThreadEvent;

        for ( ;; )
        {
            DWORD waitResult = WaitForMultipleObjects(
                                   2,              // Number of events to wait for
                                   waitEvents,     // Pointer to list of handles of events to wait for
                                   false,          // Do not wait for all events, just one will do
                                   INFINITE        // Wait forever for the events to be signaled
                               );

            // If the Job Submitted event was signaled...
            if ( waitResult == WAIT_OBJECT_0 )
            {
                // Pop a queued job off the list
                JobDesc job = context->m_jobContext->PopJobFromQueue();

                // If the job is valid...
                while ( job.IsValid() )
                {
                    // Run the job
                    job.m_jobFunc( job.m_jobData );

                    // Add the job to the completed list
                    context->m_jobContext->AddFinishedJob( job );

                    // Signal the event that a job has been finished
                    SetEvent( context->m_jobContext->GetJobFinishedHandle() );

                    job = context->m_jobContext->PopJobFromQueue();
                }
            }

            else
            {
                // If the exit event was signaled or there was some kind of error
                // then exit the loop and return from the thread
                break;
            }
        }

        return 0;
    }


    //////////////////////////////////////////////////////////////////////////
    JobManager::JobManager( int numThreads )
        : m_jobIDCounter( 0 )
    {
        m_threads.reserve( numThreads );

        for ( int i = 0; i < numThreads; ++i )
        {
            m_threads.push_back( new WorkerThread( &m_jobContext ) );
        }
    }


    JobManager::~JobManager()
    {
        for ( size_t i = 0; i < m_threads.size(); ++i )
        {
            delete m_threads[i];
        }

        m_threads.clear();

        assert( m_jobContext.GetNumQueuedJobs() == 0 );
        assert( m_jobContext.GetNumFinishedJobs() == 0 );
    }


    //static
    int JobManager::GetNumProcessors()
    {
        _SYSTEM_INFO sysInfo;
        GetSystemInfo( &sysInfo );
        return sysInfo.dwNumberOfProcessors;
    }


    CloakWorks::JobHandle JobManager::LaunchJob( CloakWorks::JobEntryFunction funcPtr, void* data )
    {
        if ( m_threads.empty() )
        {
            funcPtr( data );
            return 0;
        }

        else
        {
            JobDesc newJob;
            newJob.m_jobFunc = funcPtr;
            newJob.m_jobData = data;
            newJob.m_jobID = ++m_jobIDCounter;

            m_jobContext.PushJobOnQueue( newJob );
            SetEvent( m_jobContext.GetJobSubmittedHandle() );

            return newJob.m_jobID;
        }
    }

    void JobManager::WaitForAllJobs()
    {
        // we don't actually wait, just grind through the queue
        if ( !m_threads.empty() )
        {
            JobDesc job = m_jobContext.PopJobFromQueue();

            while ( job.IsValid() )
            {
                // Run the job
                job.m_jobFunc( job.m_jobData );

                // Add the job to the completed list
                m_jobContext.AddFinishedJob( job );

                // get next
                job = m_jobContext.PopJobFromQueue();
            }
        }

    }

    void JobManager::WaitForJob( CloakWorks::JobHandle handle )
    {
        if ( !m_threads.empty() )
        {
            while ( !m_jobContext.ClearCompletedJob( handle ) )
            {
                // Check to see if a job finished event has been signaled
                DWORD result = WaitForSingleObject( m_jobContext.GetJobFinishedHandle(), 0 );

                // If no job finished event has been signaled, we have to wait for the job to finish.
                // Try to use the time productively by processing a job
                if ( result == WAIT_TIMEOUT )
                {
                    // Pop a queued job off the list
                    JobDesc job = m_jobContext.PopJobFromQueue();

                    // If the job is valid...
                    if ( job.IsValid() )
                    {
                        // Run the job
                        job.m_jobFunc( job.m_jobData );

                        // Add the job to the completed list
                        m_jobContext.AddFinishedJob( job );
                    }

                    // If the job isn't valid, then the queue must be empty.  We have no choice but to
                    // wait for a job finished signal
                    else
                    {
                        WaitForSingleObject( m_jobContext.GetJobFinishedHandle(), 0 );
                    }
                }
            }
        }
    }

}
