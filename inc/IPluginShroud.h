/* Shroud_Plugin - for licensing and copyright see license.txt */

#include <IPluginBase.h>

#pragma once

/**
* @brief Shroud Plugin Namespace
*/
namespace ShroudPlugin
{
    /**
    * @brief plugin Shroud concrete interface
    */
    struct IPluginShroud
    {
        /**
        * @brief Get Plugin base interface
        */
        virtual PluginManager::IPluginBase* GetBase() = 0;

        // TODO: Add your concrete interface declaration here
    };
};