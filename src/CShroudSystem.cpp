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
        gPlugin->LogWarning( "%s", message );
    }

    CShroudSimulation::CShroudSimulation( IEntity* pEntity )
    {
        Init();
        this->pEntity = pEntity;
        entityId = pEntity->GetId();
    }

    CShroudSimulation::~CShroudSimulation()
    {
        if ( bIsCharacter && pAttachment )
        {
            gEnv->pEntitySystem->RemoveEntity( entityId );
        }

        Init();
    }

    CShroudWrapper::CShroudWrapper()
        : m_pShroudMgr( NULL )
        , m_bInitialized( false )
        , m_bUpdateSkipped( false )
        , m_iNextFreeSim( 1 )
    {
        m_pJobMgr = new JobManager( JobManager::GetNumProcessors() - 1 );
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
                existingID = pCurSim->pCharEntity->GetId();
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

            if ( pCurSim->bIsCharacter )
            {
                existingID = pCurSim->pCharEntity->GetId();
            }

            else
            {
                existingID = pCurSim->pEntity->GetId();
            }

            if (
                existingID == newID
            )
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
        bool bRet = false;

        if ( pEntity == NULL )
        {
            return( bRet );
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
            return( bRet );
        }

        CShroudSimulation* pCurSim = new CShroudSimulation( pEntity );

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
                params.nFlags = ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_RECVWIND | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SPAWNED | ENTITY_FLAG_SEND_NOT_SEEN_TIMEOUT | ENTITY_FLAG_SEND_RENDER_EVENT;
                params.pClass = pEntityClass;
                IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity( params );

                pNewEntity->SetStatObj( pNewStatObj, 0, false );
                pNewEntity->SetWorldTM( pEntity->GetWorldTM() );
                pNewEntity->SetMaterial( pCharacter->GetIAttachmentManager()->GetInterfaceByName( sAttName )->GetIAttachmentObject()->GetReplacementMaterial() );

                pCurSim->pStatObj = pNewStatObj;
                pCurSim->pCharEntity = pEntity; // for access to location and skeleton
                pCurSim->pEntity = pNewEntity;  // for access to mesh
                pCurSim->entityId = pNewEntity->GetId();

                gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem()->CreateGameObjectForEntity( pCurSim->entityId );

                pCurSim->bIsCharacter = true;
            }
        }

        else
        {
            pCurSim->bIsCharacter = false;
            pCurSim->pEntity ? pCurSim->pEntity->UnphysicalizeSlot( 0 ) : 0;
        }

        if ( !pCurSim->pStatObj )
        {
            gPlugin->LogError( "Object invalid or no attachment %s present in character, no simulation possible", sAttName );
            delete pCurSim;
            return( bRet );
        }

        // Create the Shroud Object.
        pCurSim->pShroudObject = m_pShroudMgr->CreateObject();

        char* buffer = NULL;
        size_t length = 0;
        FILE* m_infile = fopen( sFile, "rb" );

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
            gPlugin->LogError( "File not found %s", sFile );
            delete pCurSim;
            return( bRet );
        }

        if ( buffer && length > 0 )
        {
            bool loadResult = pCurSim->pShroudObject->Load( buffer, length );
            delete [] buffer;

            if ( loadResult )
            {
                pCurSim->pShroudObject->GenerateMissingMeshes();
                pCurSim->pShroudObject->Initialize();
                pCurSim->pShroudInstance = pCurSim->pShroudObject->CreateInstance();

            }

            else
            {
                pCurSim->pShroudInstance = NULL;
            }
        }

        if ( !pCurSim->pShroudInstance )
        {
            gPlugin->LogError( "Unable to create ShroudInstance" );
            delete pCurSim;
            return( bRet );
        }

        if ( pCurSim->bIsCharacter )
        {
            // only applies if simulation is on a character
            pCurSim->m_transformToBoneMap.resize( pCurSim->pShroudInstance->GetNumTransforms(), -1 );

            for ( size_t i = 0; i < pCurSim->pShroudInstance->GetNumTransforms(); ++i )
            {
                const CloakWorks::ITransform* transform = pCurSim->pShroudInstance->GetTransform( i );

                if ( transform->GetBoneName() )
                {
                    pCurSim->m_transformToBoneMap[i] = pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetJointIDByName( transform->GetBoneName() );
                }
            }

            CloakWorks::ICollisionMgr* collisionMgr = pCurSim->pShroudInstance->GetCollisionMgr();
            pCurSim->m_colliderToBoneMap.resize( collisionMgr->GetNumColliders(), -1 );

            for ( size_t i = 0; i < collisionMgr->GetNumColliders(); ++i )
            {
                const CloakWorks::ICollider* collider = collisionMgr->GetCollider( i );

                if ( collider->GetBoneName() )
                {
                    pCurSim->m_colliderToBoneMap[i] = pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetJointIDByName( collider->GetBoneName() );
                }
            }
        }

        CloakWorks::uint32 iNumMeshes = pCurSim->pShroudInstance->GetNumMeshes();
        assert( iNumMeshes == 1 );

        for ( CloakWorks::uint32 i = 0; i < iNumMeshes; ++i )
        {
            CloakWorks::IMeshInstance* meshInstance = pCurSim->pShroudInstance->GetMeshInstance( i );

            const CloakWorks::IMeshLODInstance* meshLODInstance = meshInstance->GetCurrentMeshLOD();
            const CloakWorks::IMeshLODObject* meshLODObject     = meshLODInstance->GetSourceObject();

            CloakWorks::uint32 vertCount = meshLODInstance->GetNumVerts();
            CloakWorks::uint32 indexCount = meshLODInstance->GetNumIndices();

            const float* positions = meshLODInstance->GetPositions();
            const float* normals = meshLODInstance->GetNormals();
            const float* tangents = meshLODInstance->GetTangents();
            CloakWorks::Vector2* uvws;
            uvws = new CloakWorks::Vector2 [vertCount];
            meshLODObject->GetTexCoords( uvws, vertCount, 0 );

            pCurSim->pRenderMesh = pCurSim->pStatObj ? pCurSim->pStatObj->GetRenderMesh() : NULL;
            IIndexedMesh* pIdxMesh = pCurSim->pRenderMesh ? pCurSim->pRenderMesh->GetIndexedMesh() : NULL;
            CMesh* pMesh = pIdxMesh->GetMesh();

            if ( pIdxMesh && pMesh )
            {
                pCurSim->vtxCount = pMesh->GetVertexCount();
                pCurSim->idxCount = pMesh->GetIndexCount();

                if ( pCurSim->vtxCount != vertCount )
                {
                    // reinit ce3 mesh
                    pMesh->SetVertexCount( vertCount );
                    pMesh->SetTexCoordsAndTangentsCount( vertCount );
                    pCurSim->vtxCount = vertCount;
                }

                if ( pCurSim->idxCount != indexCount )
                {
                    pMesh->SetIndexCount( indexCount );
                    pMesh->SetFaceCount( int( indexCount / 3 ) );
                    pCurSim->idxCount = indexCount;
                }

                pCurSim->spVtx = strided_pointer<Vec3>( pMesh->m_pPositions );
                pCurSim->spNrm = strided_pointer<Vec3>( pMesh->m_pNorms );
                pCurSim->spIdx = strided_pointer<uint16>( pMesh->m_pIndices );
                pCurSim->spUVs = strided_pointer<SMeshTexCoord>( pMesh->m_pTexCoord );
                pCurSim->spTangents = strided_pointer<SMeshTangents>( pMesh->m_pTangents );

            }

            for ( int i = 0; i < pCurSim->vtxCount; ++i )
            {
                pCurSim->spVtx[i].x = positions[i * 4];
                pCurSim->spVtx[i].y = positions[i * 4 + 1];
                pCurSim->spVtx[i].z = positions[i * 4 + 2];

                Vec3 ceTangents( tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2] );
                Vec3 ceNormals( normals[i * 4], normals[i * 4 + 1], normals[i * 4 + 2] );
                Vec3 ceBinormals = ceTangents.Cross( ceNormals );

                pCurSim->spTangents[i].Binormal.x = ( short ) int( ceBinormals.x * 32768 );
                pCurSim->spTangents[i].Binormal.y = ( short ) int( ceBinormals.y * 32768 );
                pCurSim->spTangents[i].Binormal.z = ( short ) int( ceBinormals.z * 32768 );
                pCurSim->spTangents[i].Binormal.w = -32767; //normals[i * 4 + 3];

                pCurSim->spTangents[i].Tangent.x = ( short ) int( ceTangents.x * 32768 );
                pCurSim->spTangents[i].Tangent.y = ( short ) int( ceTangents.y * 32768 );
                pCurSim->spTangents[i].Tangent.z = ( short ) int( ceTangents.z * 32768 );
                pCurSim->spTangents[i].Tangent.w = -32767; //tangents[i * 4 + 3];

                pCurSim->spNrm[i].x = normals[i * 4];// * normals[i * 4 + 3];
                pCurSim->spNrm[i].y = normals[i * 4 + 1];// * normals[i * 4 + 3];
                pCurSim->spNrm[i].z = normals[i * 4 + 2];// * normals[i * 4 + 3];

                pCurSim->spUVs[i].s = uvws[i].x;
                pCurSim->spUVs[i].t = uvws[i].y;
            }

            CloakWorks::uint16* indices = new CloakWorks::uint16 [pCurSim->idxCount];
            meshLODObject->GetIndices( indices, pCurSim->idxCount );
            uint16* newIdx = new uint16 [pCurSim->idxCount];

            for ( int i = 0; i < pCurSim->idxCount; ++i )
            {
                newIdx[i] = indices[i];
            }

            if ( pIdxMesh && pCurSim->pRenderMesh )
            {
                // convoluted method with apparent pointless calls but the only way I discovered to force reload of UVs
                CMesh* newMesh = pIdxMesh->GetMesh();
                pCurSim->pRenderMesh->LockForThreadAccess();
                pCurSim->pRenderMesh->SetMesh( *newMesh, 0, 0, 0, true );  // <-- set to self to reload UVs and tangents, but doesn't seem to reload indices
                pCurSim->pRenderMesh->UpdateIndices( newIdx, pCurSim->idxCount, 0, 0 );
                pCurSim->pEntity->SetStatObj( pCurSim->pStatObj, 0, false );

                pCurSim->pRenderMesh->UnLockForThreadAccess();

                pCurSim->pRenderMesh = pCurSim->pStatObj ? pCurSim->pStatObj->GetRenderMesh() : NULL;
            }
        }

        IEntityRenderProxy* pRenderProxy = ( IEntityRenderProxy* )pCurSim->pCharEntity->GetProxy( ENTITY_PROXY_RENDER ); // need to check if main char entity is drawn
        pCurSim->pRenderNode = pRenderProxy->GetRenderNode();

        gPlugin->LogAlways( "Simulation [%s] created", pCurSim->pEntity->GetName() );

        m_pSimulations[m_iNextFreeSim] = pCurSim;
        m_iNextFreeSim++;

        return bRet;
    }

    bool CShroudWrapper::ActivateStatObjCloth( IEntity* pEntity, const char* sFile )
    {
        bool bRet = false;

        if ( pEntity == NULL )
        {
            return( bRet );
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
            return( bRet );
        }

        CShroudSimulation* pCurSim = new CShroudSimulation( pEntity );

        // find stuff ce3 side
        pCurSim->pOrigStatObj = pCurSim->pEntity ? pCurSim->pEntity->GetStatObj( 0 ) : NULL;

        pCurSim->bIsCharacter = false;
        pCurSim->pEntity ? pCurSim->pEntity->UnphysicalizeSlot( 0 ) : 0;

        if ( !pCurSim->pOrigStatObj )
        {
            gPlugin->LogError( "Object invalid, no simulation possible" );
            delete pCurSim;
            return( bRet );
        }

        IStatObj* pNewStatObj = pCurSim->pOrigStatObj->Clone( true, false, true );
        pNewStatObj->SetFlags( pNewStatObj->GetFlags() & 0xFFFFFFFE ); // unhide it

        IEntityClassRegistry* pClassRegistry = gEnv->pEntitySystem->GetClassRegistry();
        pClassRegistry->IteratorMoveFirst();
        IEntityClass* pEntityClass = pClassRegistry->FindClass( "Default" );

        SEntitySpawnParams params;
        params.sName = "shroud_static_sim";
        params.nFlags = ENTITY_FLAG_CASTSHADOW | ENTITY_FLAG_RECVWIND | ENTITY_FLAG_CLIENT_ONLY | ENTITY_FLAG_SPAWNED;
        params.pClass = pEntityClass;
        IEntity* pNewEntity = gEnv->pEntitySystem->SpawnEntity( params );

        pNewEntity->SetStatObj( pNewStatObj, 0, false );
        pNewEntity->SetWorldTM( pEntity->GetWorldTM() );
        pNewEntity->SetMaterial( pEntity->GetMaterial() );

        pCurSim->pStatObj = pNewStatObj;
        pCurSim->pCharEntity = pEntity; // for access to location and skeleton
        pCurSim->pEntity = pNewEntity;  // for access to mesh
        pCurSim->entityId = pNewEntity->GetId();

        pCurSim->pOrigStatObj->SetFlags( pCurSim->pOrigStatObj->GetFlags() || STATIC_OBJECT_HIDDEN );
        gEnv->pGame->GetIGameFramework()->GetIGameObjectSystem()->CreateGameObjectForEntity( pCurSim->entityId );

        // Create the Shroud Object.
        pCurSim->pShroudObject = m_pShroudMgr->CreateObject();

        char* buffer = NULL;
        size_t length = 0;
        FILE* m_infile = fopen( sFile, "rb" );

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
            gPlugin->LogError( "File not found %s", sFile );
            delete pCurSim;
            return( bRet );
        }

        if ( buffer && length > 0 )
        {
            bool loadResult = pCurSim->pShroudObject->Load( buffer, length );
            delete [] buffer;

            if ( loadResult )
            {
                pCurSim->pShroudObject->GenerateMissingMeshes();
                pCurSim->pShroudObject->Initialize();
                pCurSim->pShroudInstance = pCurSim->pShroudObject->CreateInstance();

            }

            else
            {
                pCurSim->pShroudInstance = NULL;
            }
        }

        if ( !pCurSim->pShroudInstance )
        {
            gPlugin->LogError( "Unable to create ShroudInstance" );
            delete pCurSim;
            return( bRet );
        }

        CloakWorks::uint32 iNumMeshes = pCurSim->pShroudInstance->GetNumMeshes();
        assert( iNumMeshes == 1 );

        for ( CloakWorks::uint32 i = 0; i < iNumMeshes; ++i )
        {
            CloakWorks::IMeshInstance* meshInstance = pCurSim->pShroudInstance->GetMeshInstance( i );

            const CloakWorks::IMeshLODInstance* meshLODInstance = meshInstance->GetCurrentMeshLOD();
            const CloakWorks::IMeshLODObject* meshLODObject     = meshLODInstance->GetSourceObject();

            CloakWorks::uint32 vertCount = meshLODInstance->GetNumVerts();
            CloakWorks::uint32 indexCount = meshLODInstance->GetNumIndices();

            const float* positions = meshLODInstance->GetPositions();
            const float* normals = meshLODInstance->GetNormals();
            const float* tangents = meshLODInstance->GetTangents();
            CloakWorks::Vector2* uvws;
            uvws = new CloakWorks::Vector2 [vertCount];
            meshLODObject->GetTexCoords( uvws, vertCount, 0 );

            pCurSim->pRenderMesh = pCurSim->pStatObj ? pCurSim->pStatObj->GetRenderMesh() : NULL;
            IIndexedMesh* pIdxMesh = pCurSim->pRenderMesh ? pCurSim->pRenderMesh->GetIndexedMesh() : NULL;
            CMesh* pMesh = pIdxMesh->GetMesh();

            if ( pIdxMesh && pMesh )
            {
                pCurSim->vtxCount = pMesh->GetVertexCount();
                pCurSim->idxCount = pMesh->GetIndexCount();

                if ( pCurSim->vtxCount != vertCount )
                {
                    // reinit ce3 mesh
                    pMesh->SetVertexCount( vertCount );
                    pMesh->SetTexCoordsAndTangentsCount( vertCount );
                    pCurSim->vtxCount = vertCount;
                }

                if ( pCurSim->idxCount != indexCount )
                {
                    pMesh->SetIndexCount( indexCount );
                    pMesh->SetFaceCount( int( indexCount / 3 ) );
                    pCurSim->idxCount = indexCount;
                }

                pCurSim->spVtx = strided_pointer<Vec3>( pMesh->m_pPositions );
                pCurSim->spNrm = strided_pointer<Vec3>( pMesh->m_pNorms );
                pCurSim->spIdx = strided_pointer<uint16>( pMesh->m_pIndices );
                pCurSim->spUVs = strided_pointer<SMeshTexCoord>( pMesh->m_pTexCoord );
                pCurSim->spTangents = strided_pointer<SMeshTangents>( pMesh->m_pTangents );

            }

            for ( int i = 0; i < pCurSim->vtxCount; ++i )
            {
                pCurSim->spVtx[i].x = positions[i * 4];
                pCurSim->spVtx[i].y = positions[i * 4 + 1];
                pCurSim->spVtx[i].z = positions[i * 4 + 2];

                Vec3 ceTangents( tangents[i * 4], tangents[i * 4 + 1], tangents[i * 4 + 2] );
                Vec3 ceNormals( normals[i * 4], normals[i * 4 + 1], normals[i * 4 + 2] );
                Vec3 ceBinormals = ceTangents.Cross( ceNormals );

                pCurSim->spTangents[i].Binormal.x = ( short ) int( ceBinormals.x * 32768 );
                pCurSim->spTangents[i].Binormal.y = ( short ) int( ceBinormals.y * 32768 );
                pCurSim->spTangents[i].Binormal.z = ( short ) int( ceBinormals.z * 32768 );
                pCurSim->spTangents[i].Binormal.w = -32767; //normals[i * 4 + 3];

                pCurSim->spTangents[i].Tangent.x = ( short ) int( ceTangents.x * 32768 );
                pCurSim->spTangents[i].Tangent.y = ( short ) int( ceTangents.y * 32768 );
                pCurSim->spTangents[i].Tangent.z = ( short ) int( ceTangents.z * 32768 );
                pCurSim->spTangents[i].Tangent.w = -32767; //tangents[i * 4 + 3];

                pCurSim->spNrm[i].x = normals[i * 4];// * normals[i * 4 + 3];
                pCurSim->spNrm[i].y = normals[i * 4 + 1];// * normals[i * 4 + 3];
                pCurSim->spNrm[i].z = normals[i * 4 + 2];// * normals[i * 4 + 3];

                pCurSim->spUVs[i].s = uvws[i].x;
                pCurSim->spUVs[i].t = uvws[i].y;
            }

            CloakWorks::uint16* indices = new CloakWorks::uint16 [pCurSim->idxCount];
            meshLODObject->GetIndices( indices, pCurSim->idxCount );
            uint16* newIdx = new uint16 [pCurSim->idxCount];

            for ( int i = 0; i < pCurSim->idxCount; ++i )
            {
                newIdx[i] = indices[i];
            }

            if ( pIdxMesh && pCurSim->pRenderMesh )
            {
                // convoluted method with apparent pointless calls but the only way I discovered to force reload of UVs
                CMesh* newMesh = pIdxMesh->GetMesh();
                pCurSim->pRenderMesh->LockForThreadAccess();
                pCurSim->pRenderMesh->SetMesh( *newMesh, 0, 0, 0, true );  // <-- set to self to reload UVs and tangents, but doesn't seem to reload indices
                pCurSim->pRenderMesh->UpdateIndices( newIdx, pCurSim->idxCount, 0, 0 );
                pCurSim->pEntity->SetStatObj( pCurSim->pStatObj, 0, false );

                pCurSim->pRenderMesh->UnLockForThreadAccess();

                pCurSim->pRenderMesh = pCurSim->pStatObj ? pCurSim->pStatObj->GetRenderMesh() : NULL;

                pCurSim->pStatObj->SetMaterial( pCurSim->pOrigStatObj->GetMaterial() );
            }
        }

        IEntityRenderProxy* pRenderProxy = ( IEntityRenderProxy* )pCurSim->pEntity->GetProxy( ENTITY_PROXY_RENDER );
        pCurSim->pRenderNode = pRenderProxy->GetRenderNode();

        gPlugin->LogAlways( "Simulation [%s] created", pCurSim->pEntity->GetName() );
        m_pSimulations[m_iNextFreeSim] = pCurSim;
        m_iNextFreeSim++;

        return bRet;
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
        //
        // start updates via Job Manager (copy from CE)
        //
        m_bIsUpdating = true;

        int i = 0;

        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            ( ( CShroudSimulation* )( *iter ).second )->m_fSimTime = fFrameTime;
            StartUpdate( ( CShroudSimulation* )( *iter ).second );
            i++;
        }

        //
        // wait for all updates to complete
        //
        while ( size_t leftovers = m_pJobMgr->m_jobContext.GetNumQueuedJobs() > 0 )
        {
            Sleep( leftovers < 10 ? leftovers : 10 ); // that many milliseconds but not more than 10
        }

        //
        // finish updates via Job Manager (copy to CE)
        //
        i = 0;

        for ( tSimHolder::const_iterator iter = m_pSimulations.begin(); iter != m_pSimulations.end(); ++iter )
        {
            FinishUpdate( ( CShroudSimulation* )( *iter ).second );
            i++;
        }


        m_bIsUpdating = false;
    }

    void CShroudWrapper::StartUpdate( CShroudSimulation* pCurSim )
    {
        if ( pCurSim->bIsDisabled )
        {
            return;
        }

        IViewSystem* iv = gEnv->pGame->GetIGameFramework()->GetIViewSystem();
        const SViewParams* vp = iv->GetView( iv->GetActiveViewId() )->GetCurrentParams();
        CloakWorks::Vector3 cameraDir( CloakWorks::Vector3( vp->rotation.GetFwdX(), vp->rotation.GetFwdY(), vp->rotation.GetFwdZ() )  );

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

        Matrix34 tm;

        if ( pCurSim->bIsCharacter )
        {
            tm = pCurSim->pCharEntity->GetSlotWorldTM( 0 );
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

                    Vec3 ce_pos = pCurSim->pCharEntity->GetSlotWorldTM( 0 ) * pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_transformToBoneMap[i] ).t;
                    Quat ce_rot = pCurSim->pCharEntity->GetRotation() * pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_transformToBoneMap[i] ).q;

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
                    Vec3 ce_pos = pCurSim->pCharEntity->GetSlotWorldTM( 0 ) * pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_colliderToBoneMap[i] ).t;
                    Quat ce_rot = pCurSim->pCharEntity->GetRotation() * pCurSim->pCharEntity->GetCharacter( 0 )->GetISkeletonPose()->GetAbsJointByID( pCurSim->m_colliderToBoneMap[i] ).q;

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
                const float* tangents  = meshLODInstance->GetTangents();

                pCurSim->pRenderMesh->LockForThreadAccess();

                for ( int i = 0; i < pCurSim->vtxCount; ++i )
                {
                    pCurSim->spVtx[i].x = positions[i * 4];
                    pCurSim->spVtx[i].y = positions[i * 4 + 1];
                    pCurSim->spVtx[i].z = positions[i * 4 + 2];

                    pCurSim->spNrm[i].x = normals[i * 4];
                    pCurSim->spNrm[i].y = normals[i * 4 + 1];
                    pCurSim->spNrm[i].z = normals[i * 4 + 2];
                }

                pCurSim->pStatObj = pCurSim->pStatObj->UpdateVertices( pCurSim->spVtx, pCurSim->spNrm, 0, pCurSim->vtxCount );
                pCurSim->pEntity->SetStatObj( pCurSim->pStatObj, 0, false );

                pCurSim->pRenderMesh->UnLockForThreadAccess();
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
