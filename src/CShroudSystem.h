/* Videoplayer_Plugin - for licensing and copyright see license.txt */

#pragma once

#include <StdAfx.h>
#include <CPluginShroud.h>
#include <JobManager.h>
#include <Shroud/ICollisionFilter.h>
#include <Shroud/ICollider.h>
#include <Shroud/Sphere.h>
#include <Shroud/Plane.h>
#include <Shroud/Capsule.h>

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
            unsigned int ceVertCount; // vertex count in Cryengine
            unsigned int shVertCount; // vertex count in Shroud
            unsigned int loadedCeVertCount;
            std::vector< std::list< int > > uVertMap;
            IStatObj* pStatObj;
            IStatObj* pOrigStatObj;
            IEntity* pEntity;
            EntityId entityId;
            IEntity* pOriginalEntity;
            IAttachment* pAttachment;
            IRenderMesh* pRenderMesh;
            IRenderNode* pRenderNode;
            const char* sFile;
            CloakWorks::Plane* colGround;
            CloakWorks::Capsule* colPlayer;

            bool bIsCharacter;
            bool bIsDisabled;
            float m_fSimTime;

            CShroudSimulation()
            {
                Init();
            }

            CShroudSimulation( IEntity* pEntity, const char* sFile );

            ~CShroudSimulation();

        private:
            void Init()
            {
                pShroudObject = NULL; // deletes object if no more instances exist
                pShroudInstance = NULL; // deletes instance if no more references exist
                ceVertCount = 0;
                shVertCount = 0;
                pStatObj = NULL;
                pOrigStatObj = NULL;
                pEntity = NULL;
                pOriginalEntity = NULL;
                pAttachment = NULL;
                pRenderNode = NULL;
                bIsCharacter = false;
                bIsDisabled = false;
                m_fSimTime = 0.01f;
                uVertMap.empty();
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
            bool StartActivation( CShroudSimulation* pCurSim );
            bool FinishActivation( CShroudSimulation* pCurSim );
            bool AlreadyActivated( IEntity* pEntity, const char* sAttName );
            void EntityRemoved( IEntity* pEntity );
            static void StartUpdate( CShroudSimulation* pCurSim );
            static void FinishUpdate( CShroudSimulation* pCurSim );

            CShroudSimulation* FindLoadedSim( const char* sFile );

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
