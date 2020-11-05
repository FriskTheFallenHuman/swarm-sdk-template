//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#ifndef SDK_CLIENTMODE_H
#define SDK_CLIENTMODE_H
#ifdef _WIN32
#pragma once
#endif

#include "clientmode_shared.h"
#include "sdkviewport.h"
#include "ivmodemanager.h"

// --------------------------------------------------------------------------------- //
// Purpose: CSDKModeManager.
// --------------------------------------------------------------------------------- //
class CSDKModeManager : public IVModeManager
{
public:
	virtual void	Init();
	virtual void	SwitchMode( bool commander, bool force ) {}
	virtual void	LevelInit( const char *newmap );
	virtual void	LevelShutdown( void );
	virtual void	ActivateMouse( bool isactive ) {}
};

// --------------------------------------------------------------------------------- //
// Purpose: CVoxManager.
// --------------------------------------------------------------------------------- //
class CVoxManager : public CAutoGameSystem
{
public:
	CVoxManager() : CAutoGameSystem( "VoxManager" ) { }

	virtual void LevelInitPostEntity( void );
	virtual void LevelShutdownPreEntity( void );
};

// --------------------------------------------------------------------------------- //
// Purpose: ClientModeSDKNormal.
// --------------------------------------------------------------------------------- //
class ClientModeSDKNormal : public ClientModeShared 
{
	DECLARE_CLASS( ClientModeSDKNormal, ClientModeShared );

public:

					ClientModeSDKNormal();
	virtual			~ClientModeSDKNormal();

	virtual void	Init();
	virtual void	InitViewport();
	virtual float	GetViewModelFOV( void );
	virtual int		GetDeathMessageStartHeight( void );
	virtual void	PostRenderVGui();
	virtual bool	CanRecordDemo( char *errorMsg, int length ) const;
	virtual void	OverrideView( CViewSetup *pSetup );
	virtual void	Update( void );
	virtual void	Shutdown();

	virtual void	LevelInit( const char *newmap );
	virtual void	DoObjectMotionBlur( const CViewSetup *pSetup );
	virtual void	UpdatePostProcessingEffects();
	virtual void	DoPostScreenSpaceEffects( const CViewSetup *pSetup );
	virtual void	OnColorCorrectionWeightsReset( void );
	virtual float	GetColorCorrectionScale( void ) const { return 1.0f; }
	virtual void	ClearCurrentColorCorrection() { m_pCurrentColorCorrection = NULL; }

private:

	friend class ClientModeSDKNormalFullscreen;

	const C_PostProcessController *m_pCurrentPostProcessController;
	PostProcessParameters_t m_CurrentPostProcessParameters;
	PostProcessParameters_t m_LerpStartPostProcessParameters, m_LerpEndPostProcessParameters;
	CountdownTimer m_PostProcessLerpTimer;

	CHandle<C_ColorCorrection> m_pCurrentColorCorrection;
};

// --------------------------------------------------------------------------------- //
// Purpose: FullscreenSDKViewport.
// --------------------------------------------------------------------------------- //
class FullscreenSDKViewport : public SDKViewport
{
	DECLARE_CLASS_SIMPLE( FullscreenSDKViewport, SDKViewport );

private:
	virtual void InitViewportSingletons( void );
};

// --------------------------------------------------------------------------------- //
// Purpose: ClientModeSDKNormalFullscreen.
// --------------------------------------------------------------------------------- //
class ClientModeSDKNormalFullscreen : public ClientModeSDKNormal
{
	DECLARE_CLASS_SIMPLE( ClientModeSDKNormalFullscreen, ClientModeSDKNormal );

public:
	virtual void InitViewport();
};

extern IClientMode *GetClientModeNormal();
extern ClientModeSDKNormal* GetClientModeSDKNormal();

#endif // SDK_CLIENTMODE_H
