/* Videoplayer_Plugin - for licensing and copyright see license.txt */

#pragma once

#include <StdAfx.h>
#include <CPluginShroud.h>
#include <JobManager.h>

namespace ShroudPlugin
{
    class CLogger :
        public CloakWorks::IWarningListener
    {
        public:
            CLogger();
            virtual ~CLogger() {}

            bool Initialize( )
            {
                return true;
            }

            virtual void HandleWarning ( const CloakWorks::String& message );

    };

    class CAllocator :
        public CloakWorks::IAllocator
    {
        private:
            size_t m_alignment;
        public:
            CAllocator() {};
            virtual ~CAllocator() {};

            virtual void* Alloc ( size_t  size, size_t alignment, const char* debugName )
            {
                return _aligned_malloc( size, alignment );
            }
            virtual void Free ( void* mem )
            {
                _aligned_free( mem );
            }
    };

    class CShroudSimulation
    {
        public:
            CloakWorks::IShroudObjectPtr pShroudObject;
            CloakWorks::IShroudInstancePtr pShroudInstance;

            std::vector<int> m_transformToBoneMap;
            std::vector<int> m_colliderToBoneMap;

            strided_pointer<Vec3> spVtx;
            strided_pointer<Vec3> spNrm;
            strided_pointer<uint16> spIdx;
            strided_pointer<SMeshTexCoord> spUVs;
            strided_pointer<SMeshTangents> spTangents;
            int vtxCount;
            int idxCount;
            IStatObj* pStatObj;
            IStatObj* pOrigStatObj;
            IEntity* pEntity;
            EntityId entityId;
            IEntity* pCharEntity;
            IAttachment* pAttachment;
            IRenderMesh* pRenderMesh;
            IRenderNode* pRenderNode;

            bool bIsCharacter;
            bool bIsDisabled;
            float m_fSimTime;

            CShroudSimulation()
            {
                Init();
            }

            CShroudSimulation( IEntity* pEntity );

            ~CShroudSimulation();

        private:
            void Init()
            {
                pShroudObject = NULL;
                pShroudInstance = NULL;
                spVtx = NULL;
                spNrm = NULL;
                spIdx = NULL;
                spUVs = NULL;
                vtxCount = 0;
                idxCount = 0;
                pStatObj = NULL;
                pOrigStatObj = NULL;
                pEntity = NULL;
                pCharEntity = NULL;
                pAttachment = NULL;
                pRenderNode = NULL;
                bIsCharacter = false;
                bIsDisabled = false;
                m_fSimTime = 0.01f;
            }
    };

    typedef std::map<int, CShroudSimulation*> tSimHolder;

    class CShroudWrapper :
        public IGameFrameworkListener,
        public IEntityEventListener,
        public ISystemEventListener
    {
        public:
            CShroudWrapper();
            virtual ~CShroudWrapper();

            virtual bool Initialize();
            virtual bool Release();

            bool ActivateCharacterCloth( IEntity* pEntity, const char* sFile, const char* sAttName );
            bool ActivateStatObjCloth( IEntity* pEntity, const char* sFile );
            bool AlreadyActivated( IEntity* pEntity, const char* sAttName );
            void EntityRemoved( IEntity* pEntity );
            void StartUpdate( CShroudSimulation* pCurSim );
            void FinishUpdate( CShroudSimulation* pCurSim );

            // see IGameFrameworkListener
            virtual void OnPostUpdate( float fDeltaTime ) {};
            virtual void OnPreRender();
            virtual void OnSaveGame( ISaveGame* pSaveGame ) {};
            virtual void OnLoadGame( ILoadGame* pLoadGame ) {};
            virtual void OnLevelEnd( const char* nextLevel ) {};
            virtual void OnActionEvent( const SActionEvent& event ) {};

            // see ISystemEventListener
            virtual void OnSystemEvent( ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam );

            // see IEntityEventListener
            virtual void OnEntityEvent( IEntity* pEntity, SEntityEvent& event );

        private:
            JobManagerPtr m_pJobMgr;
            CLogger* m_pLogger;
            bool m_bUpdateSkipped;

            CloakWorks::IShroudMgrPtr m_pShroudMgr;

            bool m_bInitialized;
            int m_iNextFreeSim;
            tSimHolder m_pSimulations;

            bool m_bIsUpdating;
    };

    extern CShroudWrapper* gSWrapper;

}
