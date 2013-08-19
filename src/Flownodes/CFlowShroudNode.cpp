/* Plugin_Shroud - for licensing and copyright see license.txt */

#include <StdAfx.h>

#include <CPluginShroud.h>
#include <CShroudSystem.h>
#include <Nodes/G2FlowBaseNode.h>

namespace ShroudPlugin
{
    class CCharacterClothFlowNode :
        public CFlowBaseNode<eNCT_Instanced>
    {
        private:
            IEntity* m_pEntity;

            enum EInputPorts
            {
                EIP_ACTIVATE = 0,
                EIP_SHROUD_FILE,
                EIP_ATTACHMENT_NAME,
            };

        public:
            virtual void GetMemoryUsage( ICrySizer* s ) const
            {
                s->Add( *this );
            }

            virtual IFlowNodePtr Clone( SActivationInfo* pActInfo )
            {
                return new CCharacterClothFlowNode( pActInfo );
            }

            CCharacterClothFlowNode( SActivationInfo* pActInfo )
            {

            }

            virtual void GetConfiguration( SFlowNodeConfig& config )
            {
                static const SInputPortConfig inputs[] =
                {
                    InputPortConfig_Void   ( "Activate",                _HELP( "Activate Character Cloth" ) ),
                    InputPortConfig<string>( "file_ShroudFile", "",     _HELP( "Shroud File (set on Activate)" ),        "sShroudFile", _UICONFIG( "" ) ),
                    InputPortConfig<string>( "AttachmentName",  "cape", _HELP( "Name of Attachment (set on Activate)" ), "sAttName", _UICONFIG( "" ) ),
                    InputPortConfig_Null(),
                };

                config.pInputPorts = inputs;
                config.pOutputPorts = NULL;
                config.sDescription = _HELP( " Activate" );

                config.nFlags |= EFLN_TARGET_ENTITY;
                config.SetCategory( EFLN_APPROVED );
            }

            virtual void ProcessEvent( EFlowEvent evt, SActivationInfo* pActInfo )
            {
                switch ( evt )
                {
                    case eFE_SetEntityId:

                        m_pEntity = pActInfo->pEntity;
                        break;

                    case eFE_Activate:

                        if ( IsPortActive( pActInfo, EIP_ACTIVATE ) && m_pEntity )
                        {
                            gSWrapper->ActivateCharacterCloth(
                                m_pEntity,
                                GetPortString( pActInfo, EIP_SHROUD_FILE ).c_str(),
                                GetPortString( pActInfo, EIP_ATTACHMENT_NAME ).c_str()
                            );
                        }

                        break;
                }
            }
    };

    class CStatObjClothFlowNode :
        public CFlowBaseNode<eNCT_Instanced>
    {
        private:
            IEntity* m_pEntity;

            enum EInputPorts
            {
                EIP_ACTIVATE = 0,
                EIP_SHROUD_FILE,
            };

        public:
            virtual void GetMemoryUsage( ICrySizer* s ) const
            {
                s->Add( *this );
            }

            virtual IFlowNodePtr Clone( SActivationInfo* pActInfo )
            {
                return new CStatObjClothFlowNode( pActInfo );
            }

            CStatObjClothFlowNode( SActivationInfo* pActInfo )
            {

            }

            virtual void GetConfiguration( SFlowNodeConfig& config )
            {
                static const SInputPortConfig inputs[] =
                {
                    InputPortConfig_Void   ( "Activate",                _HELP( "Activate Static Object Cloth" ) ),
                    InputPortConfig<string>( "file_ShroudFile", "",     _HELP( "Shroud File (set on Activate)" ),        "sShroudFile", _UICONFIG( "" ) ),
                    InputPortConfig_Null(),
                };

                config.pInputPorts = inputs;
                config.pOutputPorts = NULL;
                config.sDescription = _HELP( " Activate" );

                config.nFlags |= EFLN_TARGET_ENTITY;
                config.SetCategory( EFLN_APPROVED );
            }

            virtual void ProcessEvent( EFlowEvent evt, SActivationInfo* pActInfo )
            {
                switch ( evt )
                {
                    case eFE_SetEntityId:

                        m_pEntity = pActInfo->pEntity;
                        break;

                    case eFE_Activate:

                        if ( IsPortActive( pActInfo, EIP_ACTIVATE ) && m_pEntity )
                        {
                            gSWrapper->ActivateStatObjCloth(
                                m_pEntity,
                                GetPortString( pActInfo, EIP_SHROUD_FILE ).c_str()
                            );
                        }

                        break;
                }
            }
    };

}

REGISTER_FLOW_NODE_EX( "ShroudPlugin:StaticObjectCloth",  ShroudPlugin::CStatObjClothFlowNode, CStatObjClothFlowNode );
REGISTER_FLOW_NODE_EX( "ShroudPlugin:CharacterCloth",  ShroudPlugin::CCharacterClothFlowNode, CCharacterClothFlowNode );
