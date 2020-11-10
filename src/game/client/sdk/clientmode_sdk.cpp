//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//
#include "cbase.h"
#include "hud.h"
#include "clientmode_sdk.h"
#include "cdll_client_int.h"
#include "iinput.h"
#include "vgui/isurface.h"
#include "vgui/ipanel.h"
#include <vgui_controls/AnimationController.h>
#include "BuyMenu.h"
#include "filesystem.h"
#include "vgui/ivgui.h"
#include "hud_basechat.h"
#include "view_shared.h"
#include "view.h"
#include "ivrenderview.h"
#include "model_types.h"
#include "iefx.h"
#include "dlight.h"
#include <imapoverview.h>
#include "c_playerresource.h"
#include <keyvalues.h>
#include "text_message.h"
#include "panelmetaclassmgr.h"
#include "weapon_sdkbase.h"
#include "c_sdk_player.h"
#include "c_weapon__stubs.h"		//Tony; add stubs
#include "viewpostprocess.h"
#include "shaderapi/ishaderapi.h"
#include "tier2/renderutils.h"
#include "materialsystem/imaterial.h"
#include "materialsystem/imaterialvar.h"
#include "engine/IEngineSound.h"
#include "object_motion_blur_effect.h"
#include "ivieweffects.h"
#include "glow_outline_effect.h"
#include "BackgroundVideo.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHudChat;

#define SCREEN_FILE		"scripts/vgui_screens.txt"

//Tony; add stubs for cycler weapon and cubemap.
STUB_WEAPON_CLASS( cycler_weapon,   WeaponCycler,   C_BaseCombatWeapon );
STUB_WEAPON_CLASS( weapon_cubemap,  WeaponCubemap,  C_BaseCombatWeapon );

//-----------------------------------------------------------------------------
// HACK: the detail sway convars are archive, and default to 0.  Existing CS:S players thus have no detail
// prop sway.  We'll force them to DoD's default values for now.  What we really need in the long run is
// a system to apply changes to archived convars' defaults to existing players.
extern ConVar cl_detail_max_sway;
extern ConVar cl_detail_avoid_radius;
extern ConVar cl_detail_avoid_force;
extern ConVar cl_detail_avoid_recover_speed;

extern ConVar mat_object_motion_blur_enable;

ConVar default_fov( "default_fov", "90", FCVAR_CHEAT );
ConVar fov_desired( "fov_desired", "90", FCVAR_USERINFO, "Sets the base field-of-view.", true, 1.0, true, 90.0 );

// Voice data
void VoxCallback( IConVar *var, const char *oldString, float oldFloat )
{
	if ( engine && engine->IsConnected() )
	{
		ConVarRef voice_vox( var->GetName() );
		if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
			engine->ClientCmd( "voicerecord_toggle on\n" );
		else
			engine->ClientCmd( "voicerecord_toggle off\n" );
	}
}
ConVar voice_vox( "voice_vox", "0", FCVAR_ARCHIVE, "Voice chat uses a vox-style always on", false, 0, true, 1, VoxCallback );

//--------------------------------------------------------------------------------------------------------
static IClientMode *g_pClientMode[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientMode()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return g_pClientMode[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

//--------------------------------------------------------------------------------------------------------
ClientModeSDKNormal g_ClientModeNormal[ MAX_SPLITSCREEN_PLAYERS ];
IClientMode *GetClientModeNormal()
{
	ASSERT_LOCAL_PLAYER_RESOLVABLE();
	return &g_ClientModeNormal[ GET_ACTIVE_SPLITSCREEN_SLOT() ];
}

//--------------------------------------------------------------------------------------------------------
ClientModeSDKNormal* GetClientModeSDKNormal()
{
	Assert( engine->IsLocalPlayerResolvable() );
	return &g_ClientModeNormal[ engine->GetActiveSplitScreenPlayerSlot() ];
}

//--------------------------------------------------------------------------------------------------------
ClientModeSDKNormalFullscreen g_FullscreenClientMode;
IClientMode *GetFullscreenClientMode( void )
{
	return &g_FullscreenClientMode;
}

// --------------------------------------------------------------------------------- //
// CVoxManager implementation.
// --------------------------------------------------------------------------------- //
static CVoxManager s_VoxManager;

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVoxManager::LevelInitPostEntity( void )
{
	if ( voice_vox.GetBool() /*&& voice_modenable.GetBool()*/ )
		engine->ClientCmd( "voicerecord_toggle on\n" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CVoxManager::LevelShutdownPreEntity( void )
{
	if ( voice_vox.GetBool() )
		engine->ClientCmd( "voicerecord_toggle off\n" );
}

// --------------------------------------------------------------------------------- //
// CSDKModeManager implementation.
// --------------------------------------------------------------------------------- //
static CSDKModeManager g_ModeManager;
IVModeManager *modemanager = ( IVModeManager * )&g_ModeManager;

void CSDKModeManager::Init()
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ] = GetClientModeNormal();
	}

	PanelMetaClassMgr()->LoadMetaClassDefinitionFile( SCREEN_FILE );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDKModeManager::LevelInit( const char *newmap )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		g_pClientMode[ i ]->LevelInit( newmap );
	}

	// HACK: the detail sway convars are archive, and default to 0.  Existing CS:S players thus have no detail
	// prop sway.  We'll force them to DoD's default values for now.
	if ( !cl_detail_max_sway.GetFloat() &&
		!cl_detail_avoid_radius.GetFloat() &&
		!cl_detail_avoid_force.GetFloat() &&
		!cl_detail_avoid_recover_speed.GetFloat() )
	{
		cl_detail_max_sway.SetValue( "5" );
		cl_detail_avoid_radius.SetValue( "64" );
		cl_detail_avoid_force.SetValue( "0.4" );
		cl_detail_avoid_recover_speed.SetValue( "0.25" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDKModeManager::LevelShutdown( void )
{
	for( int i = 0; i < MAX_SPLITSCREEN_PLAYERS; ++i )
	{
		ACTIVE_SPLITSCREEN_PLAYER_GUARD( i );
		GetClientMode()->LevelShutdown();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
ClientModeSDKNormal::ClientModeSDKNormal()
{
}

//-----------------------------------------------------------------------------
// Purpose: If you don't know what a destructor is by now, you are probably going to get fired
//-----------------------------------------------------------------------------
ClientModeSDKNormal::~ClientModeSDKNormal()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::Init()
{
	BaseClass::Init();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::InitViewport()
{
	m_pViewport = new SDKViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
float ClientModeSDKNormal::GetViewModelFOV( void )
{
	//Tony; retrieve the fov from the view model script, if it overrides it.
	float viewFov = 74.0;

	C_WeaponSDKBase *pWeapon = (C_WeaponSDKBase*)GetActiveWeapon();
	if ( pWeapon )
	{
		viewFov = pWeapon->GetWeaponFOV();
	}
	return viewFov;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int ClientModeSDKNormal::GetDeathMessageStartHeight( void )
{
	return m_pViewport->GetDeathMessageStartHeight();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::PostRenderVGui()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool ClientModeSDKNormal::CanRecordDemo( char *errorMsg, int length ) const
{
	C_SDKPlayer *player = C_SDKPlayer::GetLocalSDKPlayer();
	if ( !player )
		return true;

	if ( !player->IsAlive() )
		return true;

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pSetup - 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::OverrideView( CViewSetup *pSetup )
{
	QAngle camAngles;

	// Let the player override the view.
	C_SDKPlayer *pPlayer = (C_SDKPlayer*)C_BasePlayer::GetLocalPlayer();
	if(!pPlayer)
		return;

	pPlayer->OverrideView( pSetup );

	if( ::input->CAM_IsThirdPerson() )
	{
		Vector cam_ofs;

		::input->CAM_GetCameraOffset( cam_ofs );

		camAngles[ PITCH ] = cam_ofs[ PITCH ];
		camAngles[ YAW ] = cam_ofs[ YAW ];
		camAngles[ ROLL ] = 0;

		Vector camForward, camRight, camUp;
		AngleVectors( camAngles, &camForward, &camRight, &camUp );

		VectorMA( pSetup->origin, -cam_ofs[ ROLL ], camForward, pSetup->origin );

		static ConVarRef c_thirdpersonshoulder( "c_thirdpersonshoulder" );
		if ( c_thirdpersonshoulder.GetBool() )
		{
			static ConVarRef c_thirdpersonshoulderoffset( "c_thirdpersonshoulderoffset" );
			static ConVarRef c_thirdpersonshoulderheight( "c_thirdpersonshoulderheight" );
			static ConVarRef c_thirdpersonshoulderaimdist( "c_thirdpersonshoulderaimdist" );

			// add the shoulder offset to the origin in the cameras right vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderoffset.GetFloat(), camRight, pSetup->origin );

			// add the shoulder height to the origin in the cameras up vector
			VectorMA( pSetup->origin, c_thirdpersonshoulderheight.GetFloat(), camUp, pSetup->origin );

			// adjust the yaw to the aim-point
			camAngles[ YAW ] += RAD2DEG( atan(c_thirdpersonshoulderoffset.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );

			// adjust the pitch to the aim-point
			camAngles[ PITCH ] += RAD2DEG( atan(c_thirdpersonshoulderheight.GetFloat() / (c_thirdpersonshoulderaimdist.GetFloat() + cam_ofs[ ROLL ])) );
		}

		// Override angles from third person camera
		VectorCopy( camAngles, pSetup->angles );
	}
	else if (::input->CAM_IsOrthographic())
	{
		pSetup->m_bOrtho = true;
		float w, h;
		::input->CAM_OrthographicSize( w, h );
		w *= 0.5f;
		h *= 0.5f;
		pSetup->m_OrthoLeft   = -w;
		pSetup->m_OrthoTop    = -h;
		pSetup->m_OrthoRight  = w;
		pSetup->m_OrthoBottom = h;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::DoPostScreenSpaceEffects( const CViewSetup *pSetup )
{
	CMatRenderContextPtr pRenderContext( materials );
	
	if ( mat_object_motion_blur_enable.GetBool() )
		DoObjectMotionBlur( pSetup );
	
	// Render object glows and selectively-bloomed objects
	g_GlowObjectManager.RenderGlowEffects( pSetup, GetSplitScreenPlayerSlot() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::Update( void )
{
	UpdatePostProcessingEffects();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::OnColorCorrectionWeightsReset( void )
{
	C_ColorCorrection *pNewColorCorrection = NULL;
	C_ColorCorrection *pOldColorCorrection = m_pCurrentColorCorrection;
	C_SDKPlayer *pPlayer = C_SDKPlayer::GetLocalSDKPlayer();
	if ( pPlayer )
		pNewColorCorrection = pPlayer->GetActiveColorCorrection();

	// Only blend between environmental color corrections if there is no failure/infested-induced color correction
	if ( pNewColorCorrection != pOldColorCorrection )
	{
		if ( pOldColorCorrection )
			pOldColorCorrection->EnableOnClient( false );

		if ( pNewColorCorrection )
			pNewColorCorrection->EnableOnClient( true, pOldColorCorrection == NULL );

		m_pCurrentColorCorrection = pNewColorCorrection;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::DoObjectMotionBlur( const CViewSetup *pSetup )
{
	if ( g_ObjectMotionBlurManager.GetDrawableObjectCount() <= 0 )
		return;

	CMatRenderContextPtr pRenderContext( materials );

	ITexture *pFullFrameFB1 = materials->FindTexture( "_rt_FullFrameFB1", TEXTURE_GROUP_RENDER_TARGET );

	//
	// Render Velocities into a full-frame FB1
	//
	IMaterial *pGlowColorMaterial = materials->FindMaterial( "dev/glow_color", TEXTURE_GROUP_OTHER, true );
	
	pRenderContext->PushRenderTargetAndViewport();
	pRenderContext->SetRenderTarget( pFullFrameFB1 );
	pRenderContext->Viewport( 0, 0, pSetup->width, pSetup->height );

	// Red and Green are x- and y- screen-space velocities biased and packed into the [0,1] range.
	// A value of 127 gets mapped to 0, a value of 0 gets mapped to -1, and a value of 255 gets mapped to 1.
	//
	// Blue is set to 1 within the object's bounds and 0 outside, and is used as a mask to ensure that
	// motion blur samples only pull from the core object itself and not surrounding pixels (even though
	// the area being blurred is larger than the core object).
	//
	// Alpha is not used
	pRenderContext->ClearColor4ub( 127, 127, 0, 0 );
	// Clear only color, not depth & stencil
	pRenderContext->ClearBuffers( true, false, false );

	// Save off state
	Vector vOrigColor;
	render->GetColorModulation( vOrigColor.Base() );

	// Use a solid-color unlit material to render velocity into the buffer
	g_pStudioRender->ForcedMaterialOverride( pGlowColorMaterial );
	g_ObjectMotionBlurManager.DrawObjects();	
	g_pStudioRender->ForcedMaterialOverride( NULL );

	render->SetColorModulation( vOrigColor.Base() );
	
	pRenderContext->PopRenderTargetAndViewport();

	//
	// Render full-screen pass
	//
	IMaterial *pMotionBlurMaterial;
	IMaterialVar *pFBTextureVariable;
	IMaterialVar *pVelocityTextureVariable;
	bool bFound1 = false, bFound2 = false;

	// Make sure our render target of choice has the results of the engine post-process pass
	ITexture *pFullFrameFB = materials->FindTexture( "_rt_FullFrameFB", TEXTURE_GROUP_RENDER_TARGET );
	pRenderContext->CopyRenderTargetToTexture( pFullFrameFB );

	pMotionBlurMaterial = materials->FindMaterial( "effects/object_motion_blur", TEXTURE_GROUP_OTHER, true );
	pFBTextureVariable = pMotionBlurMaterial->FindVar( "$fb_texture", &bFound1, true );
	pVelocityTextureVariable = pMotionBlurMaterial->FindVar( "$velocity_texture", &bFound2, true );
	if ( bFound1 && bFound2 )
	{
		pFBTextureVariable->SetTextureValue( pFullFrameFB );
		
		pVelocityTextureVariable->SetTextureValue( pFullFrameFB1 );

		int nWidth, nHeight;
		pRenderContext->GetRenderTargetDimensions( nWidth, nHeight );

		pRenderContext->DrawScreenSpaceRectangle( pMotionBlurMaterial, 0, 0, nWidth, nHeight, 0.0f, 0.0f, nWidth - 1, nHeight - 1, nWidth, nHeight );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::UpdatePostProcessingEffects()
{
	C_PostProcessController *pNewPostProcessController = NULL;
	C_SDKPlayer *pPlayer = C_SDKPlayer::GetLocalSDKPlayer();
	if ( pPlayer )
		pNewPostProcessController = pPlayer->GetActivePostProcessController();

	// Figure out new endpoints for parameter lerping
	if ( pNewPostProcessController != m_pCurrentPostProcessController )
	{
		m_LerpStartPostProcessParameters = m_CurrentPostProcessParameters;
		m_LerpEndPostProcessParameters = pNewPostProcessController ? pNewPostProcessController->m_PostProcessParameters : PostProcessParameters_t();
		m_pCurrentPostProcessController = pNewPostProcessController;

		float flFadeTime = pNewPostProcessController ? pNewPostProcessController->m_PostProcessParameters.m_flParameters[ PPPN_FADE_TIME ] : 0.0f;
		if ( flFadeTime <= 0.0f )
		{
			flFadeTime = 0.001f;
		}
		m_PostProcessLerpTimer.Start( flFadeTime );
	}

	// Lerp between start and end
	float flLerpFactor = 1.0f - m_PostProcessLerpTimer.GetRemainingRatio();
	for ( int nParameter = 0; nParameter < POST_PROCESS_PARAMETER_COUNT; ++ nParameter )
	{
		m_CurrentPostProcessParameters.m_flParameters[ nParameter ] = 
			Lerp( 
				flLerpFactor, 
				m_LerpStartPostProcessParameters.m_flParameters[ nParameter ], 
				m_LerpEndPostProcessParameters.m_flParameters[ nParameter ] );
	}
	SetPostProcessParams( &m_CurrentPostProcessParameters );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::Shutdown()
{
	if ( BackgroundVideo() )
		BackgroundVideo()->ClearCurrentMovie();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void ClientModeSDKNormal::LevelInit( const char *newmap )
{
	// reset ambient light
	static ConVarRef mat_ambient_light_r( "mat_ambient_light_r" );
	static ConVarRef mat_ambient_light_g( "mat_ambient_light_g" );
	static ConVarRef mat_ambient_light_b( "mat_ambient_light_b" );

	if ( mat_ambient_light_r.IsValid() )
		mat_ambient_light_r.SetValue( "0" );

	if ( mat_ambient_light_g.IsValid() )
		mat_ambient_light_g.SetValue( "0" );

	if ( mat_ambient_light_b.IsValid() )
		mat_ambient_light_b.SetValue( "0" );

	BaseClass::LevelInit( newmap );

	// clear any DSP effects
	CLocalPlayerFilter filter;
	enginesound->SetRoomType( filter, 0 );
	enginesound->SetPlayerDSP( filter, 0, true );
}

// --------------------------------------------------------------------------------- //
// FullscreenSDKViewport implementation.
// --------------------------------------------------------------------------------- //
void FullscreenSDKViewport::InitViewportSingletons( void )
{
	SetAsFullscreenViewportInterface();
}

// --------------------------------------------------------------------------------- //
// ClientModeSDKNormalFullscreen implementation.
// --------------------------------------------------------------------------------- //
void ClientModeSDKNormalFullscreen::InitViewport()
{
	// Skip over BaseClass!!!
	BaseClass::InitViewport();
	m_pViewport = new FullscreenSDKViewport();
	m_pViewport->Start( gameuifuncs, gameeventmanager );
}






