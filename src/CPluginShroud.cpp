/* Shroud_Plugin - for licensing and copyright see license.txt */

#include <StdAfx.h>
#include <CPluginShroud.h>
#include <CShroudSystem.h>

namespace ShroudPlugin
{
    CPluginShroud* gPlugin = NULL;
    CShroudWrapper* gSWrapper = NULL;

    CPluginShroud::CPluginShroud()
    {
        gPlugin = this;
    }

    CPluginShroud::~CPluginShroud()
    {
        Release( true );

        gPlugin = NULL;
    }

    bool CPluginShroud::Release( bool bForce )
    {
        bool bRet = true;
        bool bWasInitialized = m_bIsFullyInitialized; // Will be reset by base class so backup

        if ( !m_bCanUnload )
        {
            // Note: Type Unregistration will be automatically done by the Base class (Through RegisterTypes)
            // Should be called while Game is still active otherwise there might be leaks/problems
            bRet = CPluginBase::Release( bForce );

            if ( bRet )
            {
                if ( bWasInitialized )
                {
                    gSWrapper->Release();
                }

                // Cleanup like this always (since the class is static its cleaned up when the dll is unloaded)
                gPluginManager->UnloadPlugin( GetName() );

                // Allow Plugin Manager garbage collector to unload this plugin
                AllowDllUnload();
            }
        }

        return bRet;
    };

    bool CPluginShroud::Init( SSystemGlobalEnvironment& env, SSystemInitParams& startupParams, IPluginBase* pPluginManager, const char* sPluginDirectory )
    {
        gPluginManager = ( PluginManager::IPluginManager* )pPluginManager->GetConcreteInterface( NULL );
        CPluginBase::Init( env, startupParams, pPluginManager, sPluginDirectory );

        if ( gSWrapper = new CShroudWrapper() )
        {
            return true;
        }

        return false;
    }

    bool CPluginShroud::RegisterTypes( int nFactoryType, bool bUnregister )
    {
        // Note: Autoregister Flownodes will be automatically registered by the Base class
        bool bRet = CPluginBase::RegisterTypes( nFactoryType, bUnregister );

        using namespace PluginManager;
        eFactoryType enFactoryType = eFactoryType( nFactoryType );

        if ( bRet )
        {
            if ( gEnv && gEnv->pSystem && !gEnv->pSystem->IsQuitting() )
            {
                // CVars
                if ( gEnv->pConsole && ( enFactoryType == FT_All || enFactoryType == FT_CVar ) )
                {
                    if ( !bUnregister )
                    {
                        // TODO: Register CVars here if you have some
                        // ...
                    }

                    else
                    {
                        // TODO: Unregister CVars here if you have some
                        // ...
                    }
                }

                // CVars Commands
                if ( gEnv->pConsole && ( enFactoryType == FT_All || enFactoryType == FT_CVarCommand ) )
                {
                    if ( !bUnregister )
                    {
                        // TODO: Register CVar Commands here if you have some
                        // ...
                    }

                    else
                    {
                        // TODO: Unregister CVar Commands here if you have some
                        // ...
                    }
                }

                // Game Objects
                if ( gEnv->pGame && gEnv->pGame->GetIGameFramework() && ( enFactoryType == FT_All || enFactoryType == FT_GameObjectExtension ) )
                {
                    if ( !bUnregister )
                    {
                        // TODO: Register Game Object Extensions here if you have some
                        // ...
                    }
                }
            }
        }

        return bRet;
    }

    const char* CPluginShroud::ListCVars() const
    {
        return "..."; // TODO: Enter CVARs/Commands here if you have some
    }

    const char* CPluginShroud::GetStatus() const
    {
        return "OK";
    }

    bool CPluginShroud::CheckDependencies() const
    {
        bool bRet = CPluginBase::CheckDependencies();

        return bRet;
    }

    bool CPluginShroud::InitDependencies()
    {
        if ( gEnv && gEnv->pSystem && !gEnv->pSystem->IsQuitting() && gPlugin )
        {

            // TODO: Do initializations depending on dependencies here
        }

        return CPluginBase::InitDependencies();
    }

}
