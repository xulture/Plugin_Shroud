/* Shroud_Plugin - for licensing and copyright see license.txt */

#include <StdAfx.h>
#include <CPluginShroud.h>
#include <CShroudSystem.h>
#include <Shroud/ISkinnedBlendControlInstance.h>
#include <Shroud/IMeshSkinnedBlendInstance.h>

namespace ShroudPlugin
{
    CLogger::CLogger()
    {
        gPlugin->LogWarning( "Logger Created" );
    }

    void CLogger::HandleWarning ( const CloakWorks::String& message )
    {
        gPlugin->LogWarning( "%s", message.GetStr() );
    }

    CShroudSimulation::CShroudSimulation( IEntity* pEntity, const char* sFile )
    {
        Init();
        this->pEntity = pEntity;
        this->entityId = pEntity->GetId();
        this->sFile = sFile;
    }

    CShroudSimulation::~CShroudSimulation()
    {
        if ( bIsCharacter && pAttachment )
        {
            gEnv->pEntitySystem->RemoveEntity( entityId );
        }

        this->uVertMap.empty();

        Init();
    }

    CShroudWrapper::CShroudWrapper()
        : m_pShroudMgr( NULL )
        , m_bInitialized( false )
        , m_bUpdateSkipped( false )
        , m_iNextFreeSim( 1 )
    {
        m_pJobMgr = new JobManager( JobManager::GetNumProcessors() * 4 );
        m_pLogger = new CLogger();
    }

    CShroudWrapper::~CShroudWrapper()
    {
        m_pJobMgr = NULL;
    }

    bool CShroudWrapper::Initialize()
    {

        CloakWorks::ShroudMgrSettings settings;

        settings.m_allocator = new CAllocator();
        //settings.m_parity = CloakWorks::kRightHanded; // Specify which coordinate system the library will be running in.  Default is kRightHanded
        settings.m_jobMgr = m_pJobMgr.get();
        settings.m_warningListener = m_pLogger;
        //settings.m_profiler = NULL;                   // Hook in your profiler

        // Create the Shroud Manager.
        m_pShroudMgr = CloakWorks::CreateShroudMgr( settings );

        // register listeners
        if ( gEnv && gEnv->pGame && gEnv->pGame->GetIGameFramework() )
        {
            gEnv->pGame->GetIGameFramework()->RegisterListener( this, "Shroud", FRAMEWORKLISTENERPRIORITY_DEFAULT );
            ISystemEventDispatcher* pDispatcher = gEnv->pSystem->GetISystemEventDispatcher();

            if ( pDispatcher )
            {
                pDispatcher->RegisterListener( this );
            }

            else
            {
                gPlugin->LogError( "ISystemEventDispatcher == NULL" );
            }

        }

        else
        {
            gPlugin->LogError( "IGameFramework == NULL" );
        }

        ICVar* p_es_not_seen_timeout = gEnv->pConsole->GetCVar( "es_not_seen_timeout" );
        p_es_not_seen_timeout->Set( 1 );

        m_bInitialized = true;
        m_bIsUpdating = false;
        return true;
    }

    bool CShroudWrapper::AlreadyActivated( IEntity* pEntity, const char* sAttName )
    {
        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            CShroudSimulation* pCurSim = ( *iter ).second;

            EntityId newID = pEntity->GetId();
            EntityId existingID;

            if ( pCurSim->bIsCharacter )
            {
                existingID = pCurSim->pOriginalEntity->GetId();
            }

            else
            {
                existingID = pCurSim->pEntity->GetId();
            }

            if (
                existingID == newID &&
                pCurSim->pAttachment &&
                strcmp( pCurSim->pAttachment->GetName(), sAttName ) == 0
            )
            {
                pCurSim->bIsDisabled = false;
                gPlugin->LogError( "Entity '%s' attachment '%s' already initialized", pEntity->GetName(), sAttName );
                return true;
            }
        }

        return false;

    }

    void CShroudWrapper::EntityRemoved( IEntity* pEntity )
    {
        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            CShroudSimulation* pCurSim = ( *iter ).second;

            EntityId newID = pEntity->GetId();
            EntityId existingID;

            existingID = pCurSim->pOriginalEntity->GetId();

            if ( existingID == newID )
            {
                pCurSim->bIsDisabled = true;
            }
        }

    }

    bool CShroudWrapper::Release()
    {

        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            CShroudSimulation* pCurSim = ( *iter ).second;

            if ( pCurSim->pAttachment )
            {
                pCurSim->pAttachment->HideAttachment( 0 );
            }

            if ( pCurSim->pOrigStatObj )
            {
                pCurSim->pOrigStatObj->SetFlags( ( *iter ).second->pOrigStatObj->GetFlags() & 0xFFFFFFFE ); // unhide original
            }

            delete ( CShroudSimulation* )( *iter ).second;
        }

        m_pSimulations.clear();
        m_iNextFreeSim = 1;

        m_bInitialized = false;

        return true;
    }

    bool CShroudWrapper::ActivateCharacterCloth( IEntity* pEntity, const char* sFile, const char* sAttName )
    {
        if ( pEntity == NULL )
        {
            return( false );
        }

        else
        {
            if ( !m_bInitialized )
            {
                this->Initialize();
            }
        }

        // check if Activate for this entity/attachment already exists
        // CE or a user may attempt to activate it more than once
        if ( AlreadyActivated( pEntity, sAttName ) )
        {
            return( false );
        }

        CShroudSimulation* pCurSim = new CShroudSimulation( pEntity, sFile );

        if ( !StartActivation( pCurSim ) )
        {
            delete pCurSim;
            return( false );
        }

        // find stuff ce3 side
        pCurSim->pStatObj = pCurSim->pEntity ? pCurSim->pEntity->GetStatObj( 0 ) : NULL;

        if ( !pCurSim->pStatObj )
        {
            // no static object, interrogate character
            ICharacterInstance* pCharacter = pCurSim->pEntity->GetCharacter( 0 );

            if ( pCharacter &&
                    pCharacter->GetIAttachmentManager() &&
                    ( pCurSim->pAttachment = pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttName ) ) &&
                    pCurSim->pAttachment->GetIAttachmentObject()
               )
            {
                IStatObj* pOrigStatObj = pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttName )->GetIAttachmentObject()->GetIStatObj();
                IStatObj* pNewStatObj = pOrigStatObj->Clone( true, false, true );

                IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
                pClassRegistry->IteratorMoveFirst();
                IEntityClass* pEntityClass = pClassRegistry->FindClass( "Default" );

                SEntitySpawnParams params;
                params.sName = "shroud_char_sim";
                params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SPAWNED | ENTITY_FLAG_CASTSHADOW;
                params.pClass = pEntityClass;
                IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity( params );

                pNewEntity->SetStatObj( pNewStatObj, 0, false );
                pNewEntity->SetWorldTM( pEntity->GetWorldTM() );
                pNewEntity->SetMaterial( pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttName )->GetIAttachmentObject()->GetReplacementMaterial() );

                pCurSim->pStatObj = pNewStatObj;
                pCurSim->pOriginalEntity = pEntity; // for access to location and skeleton
                pCurSim->pEntity = pNewEntity;  // for access to mesh
                pCurSim->entityId = pNewEntity->GetId();

                gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem()->CreateGameObjectForEntity( pCurSim->entityId );

                pCurSim->bIsCharacter = true;
            }
        }

        if ( !pCurSim->pStatObj )
        {
            gPlugin->LogError( "Object invalid or no attachment %s present in character, no simulation possible", sAttName );
            delete pCurSim;
            return( false );
        }

        return FinishActivation( pCurSim );
    }

    bool CShroudWrapper::ActivateStatObjCloth( IEntity* pEntity, const char* sFile )
    {
        if ( pEntity == NULL )
        {
            return( false );
        }

        else
        {
            if ( !m_bInitialized )
            {
                this->Initialize();
            }
        }

        // check if Activate for this entity/attachment already exists
        // CE or a user may attempt to activate it more than once
        if ( AlreadyActivated( pEntity, "" ) )
        {
            return( false );
        }

        CShroudSimulation* pCurSim = new CShroudSimulation( pEntity, sFile );

        if ( !StartActivation( pCurSim ) )
        {
            delete pCurSim;
            return( false );
        }

        // find stuff ce3 side
        pCurSim->pOrigStatObj = pCurSim->pEntity ? pCurSim->pEntity->GetStatObj( 0 ) : NULL;

        pCurSim->bIsCharacter = false;
        pCurSim->pEntity ? pCurSim->pEntity->UnphysicalizeSlot( 0 ) : 0;

        if ( !pCurSim->pOrigStatObj )
        {
            gPlugin->LogError( "Object invalid, no simulation possible" );
            delete pCurSim;
            return( false );
        }

        IStatObj* pNewStatObj = pCurSim->pOrigStatObj->Clone( true, false, true );
        pNewStatObj->SetFlags( pNewStatObj->GetFlags() & 0xFFFFFFFE ); // unhide it

        IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
        pClassRegistry->IteratorMoveFirst();
        IEntityClass* pEntityClass = pClassRegistry->FindClass( "Default" );

        SEntitySpawnParams params;
        params.sName = "shroud_static_sim";
        params.nFlags = ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SPAWNED | ENTITY_FLAG_CASTSHADOW;
        params.pClass = pEntityClass;
        IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity( params );

        pNewEntity->SetStatObj( pNewStatObj, 0, false );
        pNewEntity->SetWorldTM( pEntity->GetWorldTM() );
        pNewEntity->SetMaterial( pEntity->GetMaterial() );

        pCurSim->pStatObj = pNewStatObj;
        pCurSim->pOriginalEntity = pEntity; // for access to location and skeleton
        pCurSim->pEntity = pNewEntity;  // for access to mesh
        pCurSim->entityId = pNewEntity->GetId();

        pCurSim->pOrigStatObj->SetFlags( pCurSim->pOrigStatObj->GetFlags() || STATIC_OBJECT_HIDDEN );
        gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem()->CreateGameObjectForEntity( pCurSim->entityId );

        return FinishActivation( pCurSim );
    }

    bool CShroudWrapper::StartActivation( CShroudSimulation* pCurSim )
    {

        // check if we already have this sFile loaded
        if ( CShroudSimulation* pLoadedSim = FindLoadedSim( pCurSim->sFile ) )
        {
            pCurSim->pShroudObject = pLoadedSim->pShroudObject;
            pCurSim->uVertMap = pLoadedSim->uVertMap;
            pCurSim->loadedCeVertCount = pLoadedSim->ceVertCount;
        }

        else
        {
            // Create the Shroud Object.
            pCurSim->pShroudObject = m_pShroudMgr->CreateObject();

            char* buffer = NULL;
            size_t length = 0;
            FILE* m_infile = fopen( pCurSim->sFile, "rb" );

            if ( m_infile )
            {
                // find the length of the file
                fseek( m_infile, 0, SEEK_END );
                length = ftell( m_infile );
                fseek( m_infile, 0, SEEK_SET );

                // allocate memory for the file:
                buffer = new char [length + 1];

                // read data as a block:
                size_t readCount = fread ( buffer, length, 1, m_infile ) * length;

                fclose( m_infile );

                buffer[readCount - 1] = '\0';
            }

            else
            {
                gPlugin->LogError( "Entity=[%d,%s] File not found=[%s]", pCurSim->entityId, pCurSim->pEntity->GetName(), pCurSim->sFile );
                return( false );
            }

            if ( buffer && length > 0 )
            {
                bool loadResult = pCurSim->pShroudObject->Load( buffer, length );
                delete [] buffer;

                if ( loadResult )
                {
                    pCurSim->pShroudObject->GenerateMissingMeshes();
                    pCurSim->pShroudObject->Initialize();
                }

                else
                {
                    return( false );
                }
            }
        }

        pCurSim->pShroudInstance = pCurSim->pShroudObject->CreateInstance();

        if ( !pCurSim->pShroudInstance )
        {
            gPlugin->LogError( "[%s] Unable to create ShroudInstance", pCurSim->sFile );
            delete pCurSim;
            return( false );
        }

        return( true );

    }

    bool CShroudWrapper::FinishActivation( CShroudSimulation* pCurSim )
    {

        CloakWorks::ICollisionMgr* collisionMgr = pCurSim->pShroudInstance->GetCollisionMgr();

        if ( pCurSim->bIsCharacter )
        {
            // only applies if simulation is on a character
            pCurSim->m_transformToBoneMap.resize( pCurSim->pShroudInstance->GetNumTransforms(), -1 );

            for ( size_t i = 0; i < pCurSim->pShroudInstance->GetNumTransforms(); ++i )
            {
                const CloakWorks::ITransform* transform = pCurSim->pShroudInstance->GetTransform( i );

                if ( transform->GetBoneName() )
                {
                    pCurSim->m_transformToBoneMap[i] = pCurSim->pOriginalEntity->GetCharacter( 0 )->GetIDefaultSkeleton().GetJointIDByName( transform->GetBoneName() );
                }
            }

            pCurSim->m_colliderToBoneMap.resize( collisionMgr->GetNumColliders(), -1 );

            for ( size_t i = 0; i < collisionMgr->GetNumColliders(); ++i )
            {
                const CloakWorks::ICollider* collider = collisionMgr->GetCollider( i );

                if ( collider->GetBoneName() )
                {
                    pCurSim->m_colliderToBoneMap[i] = pCurSim->pOriginalEntity->GetCharacter( 0 )->GetIDefaultSkeleton().GetJointIDByName( collider->GetBoneName() );
                }
            }
        }

        // Terrain and Player colliders
        //CloakWorks::ICollider* colliderGround = collisionMgr->AddCollider( CloakWorks::Plane::MyTypeInfo()->GetName() );
        //colliderGround->SetFrictionScale( 0.2f );
        //pCurSim->colGround = CloakWorks::reflection_cast<CloakWorks::Plane>( colliderGround->GetShape() );
        //pCurSim->colGround->SetReferencePos( CloakWorks::Vector3( 0, 0, 0 ) );
        //pCurSim->colGround->SetNormal( CloakWorks::Vector3( 0, 0, 1 ) );
        //
        //CloakWorks::ICollider* colliderPlayer = collisionMgr->AddCollider( CloakWorks::Capsule::MyTypeInfo()->GetName() );
        //colliderPlayer->SetFrictionScale( 0.2f );
        //pCurSim->colPlayer = CloakWorks::reflection_cast<CloakWorks::Capsule>( colliderPlayer->GetShape() );
        //pCurSim->colPlayer->SetRadius1( 0.4f );
        //pCurSim->colPlayer->SetRadius2( 0.4f );
        //pCurSim->colPlayer->SetOffset1( CloakWorks::Vector3( 0, 0, 0.4f ) );
        //pCurSim->colPlayer->SetOffset2( CloakWorks::Vector3( 0, 0, 1.6f ) );

        CloakWorks::uint32 iNumMeshes = pCurSim->pShroudInstance->GetNumMeshes();
        assert( iNumMeshes == 1 );

        CloakWorks::Matrix44 root_mtx ( pCurSim->pShroudObject->GetMeshObject( 0 )->GetWorldMatrix() );

        if ( root_mtx != CloakWorks::Matrix44( CloakWorks::Matrix44::kInitIdentity ) )
        {
            gPlugin->LogError( "[%s] render mesh not at 0,0,0, please re-export from Shroud with pos and rot=[0,0,0]", pCurSim->sFile );
            delete pCurSim;
            return( false );
        }

        for ( CloakWorks::uint32 i = 0; i < iNumMeshes; ++i )
        {

            CloakWorks::IMeshInstance* meshInstance = pCurSim->pShroudInstance->GetMeshInstance( i );

            const CloakWorks::IMeshLODInstance* meshLODInstance = meshInstance->GetCurrentMeshLOD();
            const CloakWorks::IMeshLODObject* meshLODObject     = meshLODInstance->GetSourceObject();

            pCurSim->shVertCount = meshLODInstance->GetNumVerts();

            const float* positions = meshLODInstance->GetPositions();

            pCurSim->pRenderMesh = pCurSim->pStatObj ? pCurSim->pStatObj->GetRenderMesh() : NULL;
            IIndexedMesh* pIdxMesh = pCurSim->pRenderMesh ? pCurSim->pRenderMesh->GetIndexedMesh() : NULL;
            CMesh* pMesh = pIdxMesh->GetMesh();

            if ( pIdxMesh && pMesh )
            {
                pCurSim->ceVertCount = pMesh->GetVertexCount();
                pCurSim->spVtx = strided_pointer<Vec3>( pMesh->m_pPositions );
                pCurSim->spNrm = strided_pointer<Vec3>( pMesh->m_pNorms );

            }

            // if we havent already loaded vertex map...
            if ( pCurSim->uVertMap.size() == 0 || pCurSim->ceVertCount != pCurSim->loadedCeVertCount )
            {
                // create vertex index map between shroud and cryengine, since array is not sorted equally
                // map is an array, each of the shroud vertices is assigned one destination cryengine vertex

                pCurSim->uVertMap.resize( pCurSim->shVertCount );


                for ( int i = 0; i < pCurSim->shVertCount; ++i )
                {
                    std::list< int > ceVerts;

                    for ( int j = 0; j < pCurSim->ceVertCount; ++j )
                    {
                        f32 x = pCurSim->spVtx[j].x - positions[i * 4];
                        f32 y = pCurSim->spVtx[j].y - positions[i * 4 + 1];
                        f32 z = pCurSim->spVtx[j].z - positions[i * 4 + 2];
                        f32 dist_squared = x * x + y * y + z * z;

                        if ( dist_squared < 0.0000001 )
                        {
                            ceVerts.push_back( j );
                        }
                    }

                    pCurSim->uVertMap[i] = ceVerts;
                }

            }

            if ( ! pCurSim->bIsCharacter )
            {
                pCurSim->pStatObj->SetMaterial( pCurSim->pOrigStatObj->GetMaterial() );
            }
        }

        IEntityRenderProxy* pRenderProxy = ( IEntityRenderProxy* )pCurSim->pOriginalEntity->GetProxy( ENTITY_PROXY_RENDER ); // need to check if main char entity is drawn
        pCurSim->pRenderNode = pRenderProxy->GetRenderNode();

        for ( size_t i = 0; i < pCurSim->pShroudInstance->GetNumSimulations(); ++i )
        {
            CloakWorks::ISimulationInstance* simInstance = pCurSim->pShroudInstance->GetSimulationInstance( i );
            simInstance->SetTargetUpdateRate( CloakWorks::ISimulation::kUpdate_30fps );

            // If the simulation has a skinned blend control, adjust the blend based on the distance from the camera
            const float blendWeight = 1.0f;

            CloakWorks::IMeshSkinnedBlendInstance* meshBlendCtrl = simInstance->QueryInterface<CloakWorks::IMeshSkinnedBlendInstance>();
            CloakWorks::ISkinnedBlendControlInstance* blendCtrl = simInstance->QueryInterface<CloakWorks::ISkinnedBlendControlInstance>();

            // Prefer to blend in the Mesh skinned verts rather than the simulation's skinned verts,
            // because if the simulation is using Thick Cloth then the mesh skinned verts will have
            // weighting that matches the original imported mesh.
            if ( meshBlendCtrl )
            {
                meshBlendCtrl->SetGlobalBlendWeight( blendWeight );
            }

            else if ( blendCtrl )
            {
                blendCtrl->SetGlobalBlendWeight( blendWeight );
            }

        }

        gPlugin->LogAlways( "[%s] Simulation [%s] created", pCurSim->sFile, pCurSim->pEntity->GetName() );

        m_pSimulations[m_iNextFreeSim] = pCurSim;
        m_iNextFreeSim++;

        return ( true );
    }

    CShroudSimulation* CShroudWrapper::FindLoadedSim( const char* sFile )
    {
        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            CShroudSimulation* pCurSim = ( *iter ).second;

            if ( !strcmp( sFile, pCurSim->sFile ) )
            {
                gPlugin->LogAlways( "reusing shroud_object already loaded" );
                return pCurSim;
            }
        }

        // else
        return NULL;
    }

    void CShroudWrapper::OnPreRender()
    {
        if ( m_bIsUpdating || m_iNextFreeSim == 1 )
        {
            // no simulations created or update already started
            // gPlugin->LogError( "already in update, not starting again" );
            return;
        }

        float fFrameTime = gEnv->pTimer->GetFrameTime();
        m_bIsUpdating = true;

        std::list<CloakWorks::JobHandle> handles;

        handles.clear();

        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            ( ( CShroudSimulation* )( *iter ).second )->m_fSimTime = fFrameTime;

            //CloakWorks::JobHandle h =  m_pJobMgr->LaunchJob( ( CloakWorks::JobEntryFunction ) &StartUpdate, ( CShroudSimulation* )( *iter ).second );
            //handles.push_back( h );
            StartUpdate( ( CShroudSimulation* )( *iter ).second );
        }

        //for ( std::list<CloakWorks::JobHandle>::iterator h = handles.begin(); h != handles.end(); h++ )
        //{
        //    m_pJobMgr->WaitForJob( *h );
        //}

        // check for zombies
        while ( m_pJobMgr->m_jobContext.GetNumQueuedJobs() > 0 ) {}

        handles.clear();

        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            //CloakWorks::JobHandle h = m_pJobMgr->LaunchJob( ( CloakWorks::JobEntryFunction ) &FinishUpdate, ( CShroudSimulation* )( *iter ).second );
            //handles.push_back( h );
            FinishUpdate( ( CShroudSimulation* )( *iter ).second );
        }

        //for ( std::list<CloakWorks::JobHandle>::iterator h = handles.begin(); h != handles.end(); h++ )
        //{
        //    m_pJobMgr->WaitForJob( *h );
        //}

        m_bIsUpdating = false;
    }

    void CShroudWrapper::StartUpdate( CShroudSimulation* pCurSim )
    {
        if ( pCurSim->bIsDisabled )
        {
            return;
        }

        // Camera orientation not currently used, no need to get coords
        //IViewSystem* iv = gEnv->pGame->GetIGameFramework()->GetIViewSystem();
        //const SViewParams* vp = iv->GetView( iv->GetActiveViewId() )->GetCurrentParams();
        //CloakWorks::Vector3 cameraDir( CloakWorks::Vector3( vp->rotation.GetFwdX(), vp->rotation.GetFwdY(), vp->rotation.GetFwdZ() )  );

        int last_draw = pCurSim->pRenderNode->GetDrawFrame();
        int frame_id = gEnv->pRenderer->GetFrameID( );
        //gPlugin->LogAlways( "Last draw = %d, frame_id = %d", last_draw, frame_id );

        if ( last_draw + 1 == frame_id || last_draw + 2 == frame_id ) // it appears that ocean is drawn in a separate frame, so we need to check 2 frames ago, as well
        {
            //gPlugin->LogAlways( "Shroud %s is visible", pCurSim->pEntity->GetName() );
        }

        else
        {
            //gPlugin->LogAlways( "Shroud %s is NOT visible", pCurSim->pEntity->GetName() );
            return;
        }

        Matrix34 tm;

        if ( pCurSim->bIsCharacter )
        {
            tm = pCurSim->pOriginalEntity->GetSlotWorldTM( 0 );
            pCurSim->pEntity->SetWorldTM( tm );
        }

        else
        {
            tm = pCurSim->pEntity->GetWorldTM();
        }

        CloakWorks::Matrix44 root_mtx(
            tm.m00, tm.m01, tm.m02, tm.m03,
            tm.m10, tm.m11, tm.m12, tm.m13,
            tm.m20, tm.m21, tm.m22, tm.m23,
            0.0f,   0.0f,   0.0f,   1.0f
        );

        for ( CloakWorks::uint32 i = 0; i < pCurSim->pShroudInstance->GetNumMeshes(); ++i )
        {
            pCurSim->pShroudInstance->GetMeshInstance( i )->SetRootLocalToWorldMatrix( root_mtx );
        }

        for ( CloakWorks::uint32 i = 0; i < pCurSim->pShroudInstance->GetNumTransforms(); ++i )
        {
            CloakWorks::ITransform* transform = pCurSim->pShroudInstance->GetTransform( i );

            if ( pCurSim->bIsCharacter )
            {
                if ( pCurSim->m_transformToBoneMap[i] != -1 )
                {

                    Vec3 ce_pos = pCurSim->pOriginalEntity->GetSlotWorldTM( 0 ) * pCurSim->pOriginalEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_transformToBoneMap[i] ).t;
                    Quat ce_rot = pCurSim->pOriginalEntity->GetRotation() * pCurSim->pOriginalEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_transformToBoneMap[i] ).q;

                    CloakWorks::Matrix44 mtx (
                        ce_rot.GetColumn0().x,  ce_rot.GetColumn1().x,  ce_rot.GetColumn2().x,  ce_pos.x,
                        ce_rot.GetColumn0().y,  ce_rot.GetColumn1().y,  ce_rot.GetColumn2().y,  ce_pos.y,
                        ce_rot.GetColumn0().z,  ce_rot.GetColumn1().z,  ce_rot.GetColumn2().z,  ce_pos.z,
                        0.0f,                   0.0f,                   0.0f,                   1.0f
                    );
                    transform->SetMatrix( mtx );
                }
            }

            else
            {
                transform->SetMatrix( root_mtx );
            }
        }

        CloakWorks::ICollisionMgr* pCollMgr = pCurSim->pShroudInstance->GetCollisionMgr();

        for ( size_t i = 0; i < pCollMgr->GetNumColliders(); ++i )
        {
            CloakWorks::ICollider* collider = pCollMgr->GetCollider( i );

            if ( pCurSim->bIsCharacter )
            {
                if ( pCurSim->m_colliderToBoneMap[i] != -1 )
                {
                    Vec3 ce_pos = pCurSim->pOriginalEntity->GetSlotWorldTM( 0 ) * pCurSim->pOriginalEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_colliderToBoneMap[i] ).t;
                    Quat ce_rot = pCurSim->pOriginalEntity->GetRotation() * pCurSim->pOriginalEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_colliderToBoneMap[i] ).q;

                    CloakWorks::Matrix44 mtx (
                        ce_rot.GetColumn0().x,  ce_rot.GetColumn1().x,  ce_rot.GetColumn2().x,  ce_pos.x,
                        ce_rot.GetColumn0().y,  ce_rot.GetColumn1().y,  ce_rot.GetColumn2().y,  ce_pos.y,
                        ce_rot.GetColumn0().z,  ce_rot.GetColumn1().z,  ce_rot.GetColumn2().z,  ce_pos.z,
                        0.0f,                   0.0f,                   0.0f,                   1.0f
                    );
                    collider->SetMatrix( mtx );
                }
            }

            else
            {
                collider->SetMatrix( root_mtx );
            }
        }

        pCurSim->pShroudInstance->BeginUpdate( pCurSim->m_fSimTime );
    }

    void CShroudWrapper::FinishUpdate( CShroudSimulation* pCurSim )
    {
        if ( pCurSim->pShroudInstance->IsUpdating() )
        {
            pCurSim->pShroudInstance->EndUpdate(); // blocks if update happens on a separate thread

            if (
                pCurSim->pAttachment &&
                ! ( pCurSim->pAttachment->IsAttachmentHidden() )
            )
            {
                pCurSim->pAttachment->HideAttachment( 1 );
            }

            for ( CloakWorks::uint32 i = 0; i < pCurSim->pShroudInstance->GetNumMeshes(); ++i )
            {
                CloakWorks::IMeshInstance* meshInstance = pCurSim->pShroudInstance->GetMeshInstance( i );

                const CloakWorks::IMeshLODInstance* meshLODInstance = meshInstance->GetCurrentMeshLOD();
                const CloakWorks::IMeshLODObject* meshLODObject     = meshLODInstance->GetSourceObject();

                const float* positions = meshLODInstance->GetPositions();
                const float* normals   = meshLODInstance->GetNormals();

                for ( int i = 0; i < pCurSim->shVertCount; ++i )
                {
                    for ( std::list<int>::iterator it = pCurSim->uVertMap[i].begin(); it != pCurSim->uVertMap[i].end(); it++ )
                    {
                        pCurSim->spVtx[*it].x = positions[i * 4];
                        pCurSim->spVtx[*it].y = positions[i * 4 + 1];
                        pCurSim->spVtx[*it].z = positions[i * 4 + 2];

                        pCurSim->spNrm[*it].x = normals[i * 4];
                        pCurSim->spNrm[*it].y = normals[i * 4 + 1];
                        pCurSim->spNrm[*it].z = normals[i * 4 + 2];
                    }
                }

                pCurSim->pStatObj = pCurSim->pStatObj->UpdateVertices( pCurSim->spVtx, pCurSim->spNrm, 0, pCurSim->ceVertCount );
            }
        }
    }

    void CShroudWrapper::OnSystemEvent( ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam )
    {
        switch ( event )
        {
            case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
                if ( wparam == 0 )
                {
                    this->Release();
                }

                break;

            case ESYSTEM_EVENT_LEVEL_UNLOAD:

                this->Release();
                break;

        }

    }

    void CShroudWrapper::OnEntityEvent( IEntity* pEntity, SEntityEvent& event )
    {
        switch ( event.event )
        {
            case ENTITY_EVENT_DONE:
                gPlugin->LogAlways( "EntityEvent-DONE, [%s], %d, %f, %d", pEntity->GetName(), event.event, event.fParam, event.nParam );
                EntityRemoved( pEntity );
                break;

            case ENTITY_EVENT_VISIBLITY:
                // not sure what to do with it
                gPlugin->LogAlways( "EntityEvent-VISIBILITY, [%s], %d, %f, %d", pEntity->GetName(), event.event, event.fParam, event.nParam );
                break;

            case ENTITY_EVENT_NOT_SEEN_TIMEOUT:
                // not sure what to do with it
                gPlugin->LogAlways( "EntityEvent-NOT_SEEN_TIMEOUT, [%s], %d, %f, %d", pEntity->GetName(), event.event, event.fParam, event.nParam );
                break;

            case ENTITY_EVENT_RENDER:
                // not sure what to do with it
                gPlugin->LogAlways( "EntityEvent-RENDER, [%s], %d, %f, %d", pEntity->GetName(), event.event, event.fParam, event.nParam );
                break;
        }
    }

}
