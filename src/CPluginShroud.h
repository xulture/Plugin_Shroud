/* Shroud_Plugin - for licensing and copyright see license.txt */

#pragma once

#include <StdAfx.h>

#include <Game.h>

#include <IPluginManager.h>
#include <IPluginBase.h>
#include <CPluginBase.hpp>

#include <IPluginShroud.h>

#include <CShroudSystem.h>

#define PLUGIN_NAME "Shroud"
#define PLUGIN_CONSOLE_PREFIX "[" PLUGIN_NAME " " PLUGIN_TEXT "] " //!< Prefix for Logentries by this plugin

namespace ShroudPlugin
{
    /**
    * @brief Provides information and manages the resources of this plugin.
    */
    class CPluginShroud :
        public PluginManager::CPluginBase,
        public IPluginShroud
    {
        public:
            CPluginShroud();
            ~CPluginShroud();

            // IPluginBase
            bool Release( bool bForce = false );

            int GetInitializationMode() const
            {
                return int( PluginManager::IM_Default );
            };

            bool Init( SSystemGlobalEnvironment& env, SSystemInitParams& startupParams, IPluginBase* pPluginManager, const char* sPluginDirectory );

            bool RegisterTypes( int nFactoryType, bool bUnregister );

            const char* GetVersion() const
            {
                return "1.0.0.0";
            };

            const char* GetName() const
            {
                return PLUGIN_NAME;
            };

            const char* GetCategory() const
            {
                return "Other";
            };

            const char* ListAuthors() const
            {
                return "Sasha 'xulture' Petrovich <sasha at allegragames dot com>";
            };

            const char* ListCVars() const;

            const char* GetStatus() const;

            const char* GetCurrentConcreteInterfaceVersion() const
            {
                return "1.0";
            };

            void* GetConcreteInterface( const char* sInterfaceVersion )
            {
                return static_cast < IPluginShroud* > ( this );
            };

            // IPluginShroud
            IPluginBase* GetBase()
            {
                return static_cast<IPluginBase*>( this );
            };

            bool CheckDependencies() const;
            bool InitDependencies();

    };

    extern CPluginShroud* gPlugin;
}

/**
* @brief This function is required to use the Autoregister Flownode without modification.
* Include the file "CPluginShroud.h" in front of flownode.
*/
inline void GameWarning( const char* sFormat, ... ) PRINTF_PARAMS( 1, 2 );
inline void GameWarning( const char* sFormat, ... )
{
    va_list ArgList;
    va_start( ArgList, sFormat );
    ShroudPlugin::gPlugin->LogV( ILog::eWarningAlways, sFormat, ArgList );
    va_end( ArgList );
};
