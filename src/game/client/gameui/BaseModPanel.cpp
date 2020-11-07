//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include <stdio.h>

#include "threadtools.h"

#include "BaseModPanel.h"
#include "EngineInterface.h"
#include "VGuiSystemModuleLoader.h"

#include "vgui/IInput.h"
#include "vgui/ILocalize.h"
#include "vgui/IPanel.h"
#include "vgui/ISurface.h"
#include "vgui/ISystem.h"
#include "vgui/IVGui.h"
#include "filesystem.h"
#include "GameConsole.h"
#include "GameUI_Interface.h"

using namespace vgui;

#include "GameConsole.h"
#include "ModInfo.h"
#include "gamerules.h"
#include "tier2/renderutils.h"
#include "igameuifuncs.h"
#include "ienginevgui.h"
#include "game/client/IGameClientExports.h"
#include "loadingdialog.h"
#include "backgroundmenubutton.h"
#include "BackGroundVideo.h"
#include "vgui_controls/AnimationController.h"
#include "vgui_controls/ImagePanel.h"
#include "vgui_controls/Label.h"
#include "vgui_controls/Menu.h"
#include "vgui_controls/MenuItem.h"
#include "vgui_controls/PHandle.h"
#include "vgui_controls/MessageBox.h"
#include "vgui_controls/QueryBox.h"
#include "vgui_controls/ControllerMap.h"
#include "tier0/icommandline.h"
#include "tier1/convar.h"
#include "NewGameDialog.h"
#include "BonusMapsDialog.h"
#include "LoadGameDialog.h"
#include "SaveGameDialog.h"
#include "OptionsDialog.h"
#include "CreateMultiplayerGameDialog.h"
#include "ChangeGameDialog.h"
#include "BackgroundMenuButton.h"
#include "PlayerListDialog.h"
#include "BenchmarkDialog.h"
#include "LoadCommentaryDialog.h"
#include "ControllerDialog.h"
#include "BonusMapsDatabase.h"
#include "engine/IEngineSound.h"
#include "bitbuf.h"
#include "tier1/fmtstr.h"
#include "inputsystem/iinputsystem.h"
#include "ixboxsystem.h"
#include "matchmaking/matchmakingbasepanel.h"
#include "matchmaking/achievementsdialog.h"
#include "OptionsSubAudio.h"
#include "hl2orange.spa.h"
#include "iachievementmgr.h"
#if defined( _X360 )
#include "xbox/xbox_launch.h"
#else
#include "xbox/xboxstubs.h"
#endif
#include "vguimatsurface/imatsystemsurface.h"
#include "materialsystem/imaterialsystem.h"
#include "materialsystem/imesh.h"
#include "matchmaking/imatchframework.h"
#include "tier1/UtlString.h"
#include "steam/steam_api.h"

#undef MessageBox	// Windows helpfully #define's this to MessageBoxA, we're using vgui::MessageBox

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


#define MAIN_MENU_INDENT_X360 10

ConVar vgui_message_dialog_modal( "vgui_message_dialog_modal", "1", FCVAR_ARCHIVE );

extern vgui::DHANDLE<CLoadingDialog> g_hLoadingDialog;
static CBaseModPanel	*g_pBasePanel = NULL;
static float		g_flAnimationPadding = 0.01f;

extern const char *COM_GetModDirectory( void );

extern ConVar x360_audio_english;
extern bool bSteamCommunityFriendsVersion;

ConVar gameui_debug( "gameui_debug", "1", FCVAR_RELEASE );
int UI_IsDebug() { return (*(int *)(&gameui_debug)) ? gameui_debug.GetInt() : 0; }

//-----------------------------------------------------------------------------
// Purpose: singleton accessor
//-----------------------------------------------------------------------------
CBaseModPanel *BasePanel()
{
	return g_pBasePanel;
}

static CBackgroundMenuButton* CreateMenuButton( CBaseModPanel *parent, const char *panelName, const wchar_t *panelText )
{
	CBackgroundMenuButton *pButton = new CBackgroundMenuButton( parent, panelName );
	pButton->SetProportional(true);
	pButton->SetCommand("OpenGameMenu");
	pButton->SetText(panelText);

	return pButton;
}

bool g_bIsCreatingNewGameMenuForPreFetching = false;

//-----------------------------------------------------------------------------
// Purpose: Constructor
//-----------------------------------------------------------------------------
CBaseModPanel::CBaseModPanel() : Panel(NULL, "BaseGameUIPanel")
{	
	g_pBasePanel = this;
	m_bLevelLoading = false;
	m_eBackgroundState = BACKGROUND_INITIAL;
	m_flTransitionStartTime = 0.0f;
	m_flTransitionEndTime = 0.0f;
	m_flFrameFadeInTime = 0.5f;
	m_bRenderingBackgroundTransition = false;
	m_bFadingInMenus = false;
	m_bEverActivated = false;
	m_iGameMenuInset = 24;
	m_bPlatformMenuInitialized = false;
	m_bHaveDarkenedBackground = false;
	m_bHaveDarkenedTitleText = true;
	m_bForceTitleTextUpdate = true;
	m_BackdropColor = Color(0, 0, 0, 128);
	m_pConsoleAnimationController = NULL;
	m_pConsoleControlSettings = NULL;
	m_bCopyFrameBuffer = false;
	m_bUseRenderTargetImage = false;
	m_ExitingFrameCount = 0;
	m_bXUIVisible = false;
	m_bUseMatchmaking = false;
	m_bRestartFromInvite = false;
	m_bRestartSameGame = false;
	m_bUserRefusedSignIn = false;
	m_bUserRefusedStorageDevice = false;
	m_bWaitingForUserSignIn = false;
	m_bWaitingForStorageDeviceHandle = false;
	m_bNeedStorageDeviceHandle = false;
	m_bStorageBladeShown = false;
	m_iStorageID = XBX_INVALID_STORAGE_ID;
	m_pAsyncJob = NULL;
	m_pStorageDeviceValidatedNotify = NULL;
	m_flMovieFadeInTime = 0.0f;
	m_flBlurScale = 0;
	m_flLastBlurTime = 0;
	m_pBackgroundMaterial = NULL;
	m_pBackgroundTexture = NULL;
	// delay 3 frames before doing activation on initialization
	// needed to allow engine to exec startup commands (background map signal is 1 frame behind) 
	m_DelayActivation = 3;

	if ( GameUI().IsConsoleUI() )
	{
		m_pConsoleAnimationController = new AnimationController( this );
		m_pConsoleAnimationController->SetScriptFile( GetVPanel(), "scripts/GameUIAnimations.txt" );
		m_pConsoleAnimationController->SetAutoReloadScript( IsDebug() );

		m_pConsoleControlSettings = new KeyValues( "XboxDialogs.res" );
		if ( !m_pConsoleControlSettings->LoadFromFile( g_pFullFileSystem, "resource/UI/XboxDialogs.res", "GAME" ) )
		{
			Error( "Failed to load UI control settings!\n" );
		}
		else
		{
			m_pConsoleControlSettings->ProcessResolutionKeys( surface()->GetResolutionKey() );
		}

#ifdef _X360
		x360_audio_english.SetValue( XboxLaunch()->GetForceEnglish() );
#endif
	}

	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton", ModInfo().GetGameTitle() ) );
	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton2", ModInfo().GetGameTitle2() ) );
	m_pGameMenuButtons.AddToTail( CreateMenuButton( this, "GameMenuButton3", ModInfo().GetGameTitle3() ) );

	m_pGameMenu = NULL;
	m_pGameLogo = NULL;

#ifndef NO_STEAM
	// Set Steam overlay position
	if ( steamapicontext && steamapicontext->SteamUtils() )
		steamapicontext->SteamUtils()->SetOverlayNotificationPosition( k_EPositionTopRight );

	if ( SteamClient() )
	{
		HSteamPipe steamPipe = SteamClient()->CreateSteamPipe();
		ISteamUtils *pUtils = SteamClient()->GetISteamUtils( steamPipe, "SteamUtils002" );
		if ( pUtils )
			bSteamCommunityFriendsVersion = true;

		SteamClient()->BReleaseSteamPipe( steamPipe );
	}
#endif

	CreateGameMenu();
	CreateGameLogo();

	// Bonus maps menu blinks if something has been unlocked since the player last opened the menu
	// This is saved as persistant data, and here is where we check for that
	CheckBonusBlinkState();

	// start the menus fully transparent
	SetMenuAlpha( 0 );

	if ( GameUI().IsConsoleUI() )
	{
		// do any costly resource prefetching now....
		// force the new dialog to get all of its chapter pics
		g_bIsCreatingNewGameMenuForPreFetching = true;
		m_hNewGameDialog = new CNewGameDialog( this, false );
		m_hNewGameDialog->MarkForDeletion();
		g_bIsCreatingNewGameMenuForPreFetching = false;

		m_hOptionsDialog_Xbox = new COptionsDialogXbox( this );
		m_hOptionsDialog_Xbox->MarkForDeletion();

		m_hControllerDialog = new CControllerDialog( this );
		m_hControllerDialog->MarkForDeletion();
		
		ArmFirstMenuItem();
		m_pConsoleAnimationController->StartAnimationSequence( "InitializeUILayout" );
	}

	// Record data used for rich presence updates
	if ( IsX360() )
	{
		// Get our active mod directory name
		const char *pGameName = CommandLine()->ParmValue( "-game", "hl2" );;

		// Set the game we're playing
		m_iGameID = CONTEXT_GAME_GAME_HALF_LIFE_2;
		m_bSinglePlayer = true;
		if ( Q_stristr( pGameName, "episodic" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_EPISODE_ONE;
		}
		else if ( Q_stristr( pGameName, "ep2" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_EPISODE_TWO;
		}
		else if ( Q_stristr( pGameName, "portal" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_PORTAL;
		}
		else if ( Q_stristr( pGameName, "tf" ) )
		{
			m_iGameID = CONTEXT_GAME_GAME_TEAM_FORTRESS;
			m_bSinglePlayer = false;
		}

	}

	// Subscribe to event notifications
	g_pMatchFramework->GetEventsSubscription()->Subscribe( this );
}

//-----------------------------------------------------------------------------
// Purpose: Xbox 360 - Get the console UI keyvalues to pass to LoadControlSettings()
//-----------------------------------------------------------------------------
KeyValues *CBaseModPanel::GetConsoleControlSettings( void )
{
	return m_pConsoleControlSettings;
}

//-----------------------------------------------------------------------------
// Purpose: Causes the first menu item to be armed
//-----------------------------------------------------------------------------
void CBaseModPanel::ArmFirstMenuItem( void )
{
	UpdateGameMenus();

	// Arm the first item in the menu
	for ( int i = 0; i < m_pGameMenu->GetItemCount(); ++i )
	{
		if ( m_pGameMenu->GetMenuItem( i )->IsVisible() )
		{
			m_pGameMenu->SetCurrentlyHighlightedItem( i );
			break;
		}
	}
}

CBaseModPanel::~CBaseModPanel()
{
	g_pBasePanel = NULL;

	ReleaseStartupGraphic();

	// Unsubscribe from event notifications
	g_pMatchFramework->GetEventsSubscription()->Unsubscribe( this );
}

void CBaseModPanel::OnEvent( KeyValues *pEvent )
{
	/* Do Nothing for now */
}

static char *g_rgValidCommands[] =
{
	"OpenGameMenu",
	"OpenPlayerListDialog",
	"OpenNewGameDialog",
	"OpenLoadGameDialog",
	"OpenSaveGameDialog",
	"OpenBonusMapsDialog",
	"OpenOptionsDialog",
	"OpenBenchmarkDialog",
	"OpenServerBrowser",
	"OpenFriendsDialog",
	"OpenLoadDemoDialog",
	"OpenCreateMultiplayerGameDialog",
	"OpenChangeGameDialog",
	"OpenLoadCommentaryDialog",
	"Quit",
	"QuitNoConfirm",
	"ResumeGame",
	"Disconnect",
};

static void CC_GameMenuCommand( const CCommand &args )
{
	int c = args.ArgC();
	if ( c < 2 )
	{
		Msg( "Usage:  gamemenucommand <commandname>\n" );
		return;
	}

	if ( !g_pBasePanel )
	{
		return;
	}

	vgui::ivgui()->PostMessage( g_pBasePanel->GetVPanel(), new KeyValues("Command", "command", args[1] ), NULL);
}

static bool UtlStringLessFunc( const CUtlString &lhs, const CUtlString &rhs )
{
	return Q_stricmp( lhs.String(), rhs.String() ) < 0;
}
static int CC_GameMenuCompletionFunc( char const *partial, char commands[ COMMAND_COMPLETION_MAXITEMS ][ COMMAND_COMPLETION_ITEM_LENGTH ] )
{
	char const *cmdname = "gamemenucommand";

	char *substring = (char *)partial;
	if ( Q_strstr( partial, cmdname ) )
	{
		substring = (char *)partial + strlen( cmdname ) + 1;
	}

	int checklen = Q_strlen( substring );

	CUtlRBTree< CUtlString > symbols( 0, 0, UtlStringLessFunc );

	int i;
	int c = ARRAYSIZE( g_rgValidCommands );
	for ( i = 0; i < c; ++i )
	{
		if ( Q_strnicmp( g_rgValidCommands[ i ], substring, checklen ) )
			continue;

		CUtlString str;
		str = g_rgValidCommands[ i ];

		symbols.Insert( str );

		// Too many
		if ( symbols.Count() >= COMMAND_COMPLETION_MAXITEMS )
			break;
	}

	// Now fill in the results
	int slot = 0;
	for ( i = symbols.FirstInorder(); i != symbols.InvalidIndex(); i = symbols.NextInorder( i ) )
	{
		char const *name = symbols[ i ].String();

		char buf[ 512 ];
		Q_strncpy( buf, name, sizeof( buf ) );
		Q_strlower( buf );

		Q_snprintf( commands[ slot++ ], COMMAND_COMPLETION_ITEM_LENGTH, "%s %s",
			cmdname, buf );
	}

	return slot;
}

static ConCommand gamemenucommand( "gamemenucommand", CC_GameMenuCommand, "Issue game menu command.", 0, CC_GameMenuCompletionFunc );

//-----------------------------------------------------------------------------
// Purpose: paints the main background image
//-----------------------------------------------------------------------------
void CBaseModPanel::PaintBackground()
{
	if ( !GameUI().IsInLevel() || g_hLoadingDialog.Get() || m_ExitingFrameCount )
	{
		// not in the game or loading dialog active or exiting, draw the ui background
		DrawBackgroundImage();
	}
	else if ( IsX360() )
	{
		// only valid during loading from level to level
		m_bUseRenderTargetImage = false;
	}

	if ( !m_bLevelLoading &&
		!GameUI().IsInLevel() &&
		!GameUI().IsInBackgroundLevel() )
	{
		int wide, tall;
		GetSize( wide, tall );

		if ( engine->IsTransitioningToLoad() )
		{
			// not in the game or loading dialog active or exiting, draw the ui background
			DrawBackgroundImage();
		}
		else
		{
			if ( BackgroundVideo() )
			{
				BackgroundVideo()->Update();
				if ( BackgroundVideo()->SetTextureMaterial() != -1 )
				{
					surface()->DrawSetColor( 255, 255, 255, 255 );
					int x, y, w, h;
					GetBounds( x, y, w, h );

					// center, 16:9 aspect ratio
					int width_at_ratio = h * (16.0f / 9.0f);
					x = ( w * 0.5f ) - ( width_at_ratio * 0.5f );

					surface()->DrawTexturedRect( x, y, x + width_at_ratio, y + h );

					if ( !m_flMovieFadeInTime )
					{
						// do the fade a little bit after the movie starts (needs to be stable)
						// the product overlay will fade out
						m_flMovieFadeInTime	= Plat_FloatTime() + TRANSITION_TO_MOVIE_DELAY_TIME;
					}

					float flFadeDelta = RemapValClamped( Plat_FloatTime(), m_flMovieFadeInTime, m_flMovieFadeInTime + TRANSITION_TO_MOVIE_FADE_TIME, 1.0f, 0.0f );
					if ( flFadeDelta > 0.0f )
					{
						if ( !m_pBackgroundMaterial )
							PrepareStartupGraphic();

						DrawStartupGraphic( flFadeDelta );
					}
				}
			}
		}
	}

	if ( m_flBackgroundFillAlpha )
	{
		int swide, stall;
		surface()->GetScreenSize(swide, stall);
		surface()->DrawSetColor(0, 0, 0, m_flBackgroundFillAlpha);
		surface()->DrawFilledRect(0, 0, swide, stall);
	}
}

//-----------------------------------------------------------------------------
// Updates which background state we should be in.
//
// NOTE: These states change at funny times and overlap. They CANNOT be
// used to demarcate exact transitions.
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateBackgroundState()
{
	if ( m_ExitingFrameCount )
	{
		// trumps all, an exiting state must own the screen image
		// cannot be stopped
		SetBackgroundRenderState( BACKGROUND_EXITING );
	}
	else if ( GameUI().IsInLevel() )
	{
		SetBackgroundRenderState( BACKGROUND_LEVEL );
	}
	else if ( GameUI().IsInBackgroundLevel() && !m_bLevelLoading )
	{
		// 360 guarantees a progress bar
		// level loading is truly completed when the progress bar is gone, then transition to main menu
		if ( IsPC() || ( IsX360() && !g_hLoadingDialog.Get() ) )
		{
			SetBackgroundRenderState( BACKGROUND_MAINMENU );
		}
	}
	else if ( m_bLevelLoading )
	{
		SetBackgroundRenderState( BACKGROUND_LOADING );
	}
	else if ( m_bEverActivated && m_bPlatformMenuInitialized )
	{
		SetBackgroundRenderState( BACKGROUND_DISCONNECTED );
	}

	if ( GameUI().IsConsoleUI() )
	{
		if ( !m_ExitingFrameCount && !m_bLevelLoading && !g_hLoadingDialog.Get() && GameUI().IsInLevel() )
		{
			// paused
			if ( m_flBackgroundFillAlpha == 0.0f )
				m_flBackgroundFillAlpha = 120.0f;
		}
		else
		{
			m_flBackgroundFillAlpha = 0;
		}

		// console ui has completely different menu/dialog/fill/fading behavior
		return;
	}

	// don't evaluate the rest until we've initialized the menus
	if ( !m_bPlatformMenuInitialized )
		return;

	// check for background fill
	// fill over the top if we have any dialogs up
	int i;
	bool bHaveActiveDialogs = false;
	bool bIsInLevel = GameUI().IsInLevel();
	for ( i = 0; i < GetChildCount(); ++i )
	{
		VPANEL child = ipanel()->GetChild( GetVPanel(), i );
		if ( child 
			&& ipanel()->IsVisible( child ) 
			&& ipanel()->IsPopup( child )
			&& child != m_pGameMenu->GetVPanel() )
		{
			bHaveActiveDialogs = true;
		}
	}
	// see if the base gameui panel has dialogs hanging off it (engine stuff, console, bug reporter)
	VPANEL parent = GetVParent();
	for ( i = 0; i < ipanel()->GetChildCount( parent ); ++i )
	{
		VPANEL child = ipanel()->GetChild( parent, i );
		if ( child 
			&& ipanel()->IsVisible( child ) 
			&& ipanel()->IsPopup( child )
			&& child != GetVPanel() )
		{
			bHaveActiveDialogs = true;
		}
	}

	// check to see if we need to fade in the background fill
	bool bNeedDarkenedBackground = (bHaveActiveDialogs || bIsInLevel);
	if ( m_bHaveDarkenedBackground != bNeedDarkenedBackground )
	{
		// fade in faster than we fade out
		float targetAlpha, duration;
		if ( bNeedDarkenedBackground )
		{
			// fade in background tint
			targetAlpha = m_BackdropColor[3];
			duration = m_flFrameFadeInTime;
		}
		else
		{
			// fade out background tint
			targetAlpha = 0.0f;
			duration = 2.0f;
		}

		m_bHaveDarkenedBackground = bNeedDarkenedBackground;
		vgui::GetAnimationController()->RunAnimationCommand( this, "m_flBackgroundFillAlpha", targetAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
	}

	// check to see if the game title should be dimmed
	// don't transition on level change
	if ( m_bLevelLoading )
		return;

	bool bNeedDarkenedTitleText = bHaveActiveDialogs;
	if (m_bHaveDarkenedTitleText != bNeedDarkenedTitleText || m_bForceTitleTextUpdate)
	{
		float targetTitleAlpha, duration;
		if (bHaveActiveDialogs)
		{
			// fade out title text
			duration = m_flFrameFadeInTime;
			targetTitleAlpha = 32.0f;
		}
		else
		{
			// fade in title text
			duration = 2.0f;
			targetTitleAlpha = 255.0f;
		}

		if ( m_pGameLogo )
		{
			vgui::GetAnimationController()->RunAnimationCommand( m_pGameLogo, "alpha", targetTitleAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
		}

		// Msg( "animating title (%d => %d at time %.2f)\n", m_pGameMenuButton->GetAlpha(), (int)targetTitleAlpha, Plat_FloatTime());
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			vgui::GetAnimationController()->RunAnimationCommand( m_pGameMenuButtons[i], "alpha", targetTitleAlpha, 0.0f, duration, AnimationController::INTERPOLATOR_LINEAR );
		}
		m_bHaveDarkenedTitleText = bNeedDarkenedTitleText;
		m_bForceTitleTextUpdate = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: sets how the game background should render
//-----------------------------------------------------------------------------
void CBaseModPanel::SetBackgroundRenderState(EBackgroundState state)
{
	if ( state == m_eBackgroundState )
	{
		return;
	}

	// apply state change transition
	float frametime = Plat_FloatTime();

	m_bRenderingBackgroundTransition = false;
	m_bFadingInMenus = false;

	CMatchmakingBasePanel *pPanel = GetMatchmakingBasePanel();

	if ( pPanel )
	{
		if ( state == BACKGROUND_LOADING )
		{
			pPanel->SetVisible( false );
		}
		else
		{
			pPanel->SetVisible( true );
		}
	}

	if ( state == BACKGROUND_EXITING )
	{
		// hide the menus
		m_bCopyFrameBuffer = false;
	}
	else if ( state == BACKGROUND_DISCONNECTED || state == BACKGROUND_MAINMENU )
	{
		// menu fading
		// make the menus visible
		m_bFadingInMenus = true;
		m_flFadeMenuStartTime = frametime;
		m_flFadeMenuEndTime = frametime + 3.0f;

		if ( state == BACKGROUND_MAINMENU )
		{
			// fade background into main menu
			m_bRenderingBackgroundTransition = true;
			m_flTransitionStartTime = frametime;
			m_flTransitionEndTime = frametime + 3.0f;
		}
	}
	else if ( state == BACKGROUND_LOADING )
	{
		if ( GameUI().IsConsoleUI() )
		{
			RunAnimationWithCallback( this, "InstantHideMainMenu", new KeyValues( "LoadMap" ) );
		}

		// hide the menus
		SetMenuAlpha( 0 );
	}
	else if ( state == BACKGROUND_LEVEL )
	{
		// show the menus
		SetMenuAlpha( 255 );
	}

	m_eBackgroundState = state;
}

void CBaseModPanel::StartExitingProcess()
{
	// must let a non trivial number of screen swaps occur to stabilize image
	// ui runs in a constrained state, while shutdown is occurring
	m_flTransitionStartTime = Plat_FloatTime();
	m_flTransitionEndTime = m_flTransitionStartTime + 0.5f;
	m_ExitingFrameCount = 30;
	g_pInputSystem->DetachFromWindow();

	CMatchmakingBasePanel *pPanel = GetMatchmakingBasePanel();
	if ( pPanel )
	{
		pPanel->CloseAllDialogs( false );
	}

	engine->StartXboxExitingProcess();
}

//-----------------------------------------------------------------------------
// Purpose: Size should only change on first vgui frame after startup
//-----------------------------------------------------------------------------
void CBaseModPanel::OnSizeChanged( int newWide, int newTall )
{
	// Recenter message dialogs
	m_MessageDialogHandler.PositionDialogs( newWide, newTall );

	if ( m_hMatchmakingBasePanel.Get() )
	{
		m_hMatchmakingBasePanel->SetBounds( 0, 0, newWide, newTall );
	}
}

//-----------------------------------------------------------------------------
// Purpose: notifications
//-----------------------------------------------------------------------------
void CBaseModPanel::OnLevelLoadingStarted( char const *levelName, bool bShowProgressDialog )
{
	m_bLevelLoading = true;

	m_pGameMenu->ShowFooter( false );

	if ( UI_IsDebug() )
	{
		ConColorMsg( Color( 48, 127, 255, 255 ), "[GAMEUI] OnLevelLoadingStarted - opening loading progress (%s)...\n",
			levelName ? levelName : "<< no level specified >>" );
	}

	if ( m_hMatchmakingBasePanel.Get() )
	{
		m_hMatchmakingBasePanel->OnCommand( "LevelLoadingStarted" );
	}

	if ( IsX360() && m_eBackgroundState == BACKGROUND_LEVEL )
	{
		// already in a level going to another level
		// frame buffer is about to be cleared, copy it off for ui backing purposes
		m_bCopyFrameBuffer = true;
	}

	if ( GameUI().IsInLevel() )
		GameUI().ActivateGameUI();
}

//-----------------------------------------------------------------------------
// Purpose: notification
//-----------------------------------------------------------------------------
void CBaseModPanel::OnLevelLoadingFinished( bool bError, const char *failureReason, const char *extendedReason )
{
	m_bLevelLoading = false;

	if ( UI_IsDebug() )
		ConColorMsg( Color( 48, 127, 255, 255 ), "[GAMEUI] CBaseModPanel::OnLevelLoadingFinished( %s, %s, %s )\n", 
			bError ? "Had Error" : "No Error", failureReason, extendedReason );

	if ( m_hMatchmakingBasePanel.Get() )
		m_hMatchmakingBasePanel->OnCommand( "LevelLoadingFinished" );

	// hide the UI
	GameUI().HideGameUI();
}

//-----------------------------------------------------------------------------
// Draws the background image.
//-----------------------------------------------------------------------------
void CBaseModPanel::DrawBackgroundImage()
{
	if ( IsX360() && m_bCopyFrameBuffer )
	{
		// force the engine to do an image capture ONCE into this image's render target
		char filename[MAX_PATH];
		surface()->DrawGetTextureFile( m_iRenderTargetImageID, filename, sizeof( filename ) );
		engine->CopyFrameBufferToMaterial( filename );
		m_bCopyFrameBuffer = false;
		m_bUseRenderTargetImage = true;
	}

	int wide, tall;
	GetSize( wide, tall );

	float frametime = Plat_FloatTime();

	// a background transition has a running map underneath it, so fade image out
	// otherwise, there is no map and the background image stays opaque
	int alpha = 255;
	if ( m_bRenderingBackgroundTransition )
	{
		// goes from [255..0]
		alpha = (m_flTransitionEndTime - frametime) / (m_flTransitionEndTime - m_flTransitionStartTime) * 255;
		alpha = clamp( alpha, 0, 255 );
	}

	// an exiting process needs to opaquely cover everything
	if ( m_ExitingFrameCount )
	{
		// goes from [0..255]
		alpha = (m_flTransitionEndTime - frametime) / (m_flTransitionEndTime - m_flTransitionStartTime) * 255;
		alpha = 255 - clamp( alpha, 0, 255 );
	}

	int iImageID = m_iBackgroundImageID;
	if ( IsX360() )
	{
		if ( m_ExitingFrameCount )
		{
			if ( !m_bRestartSameGame )
			{
				iImageID = m_iProductImageID;
			}
		}
		else if ( m_bUseRenderTargetImage )
		{
			// the render target image must be opaque, the alpha channel contents are unknown
			// it is strictly an opaque background image and never used as an overlay
			iImageID = m_iRenderTargetImageID;
			alpha = 255;
		}
	}

	surface()->DrawSetColor( 255, 255, 255, alpha );
	surface()->DrawSetTexture( iImageID );
	surface()->DrawTexturedRect( 0, 0, wide, tall );

	if ( IsX360() && m_ExitingFrameCount )
	{
		// Make invisible when going back to appchooser
		m_pGameMenu->SetVisible( false );

		IScheme *pScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "SourceScheme" ) );
		HFont hFont = pScheme->GetFont( "ChapterTitle" );
		wchar_t *pString = g_pVGuiLocalize->Find( "#GameUI_Loading" );
		int textWide, textTall;
		surface()->GetTextSize( hFont, pString, textWide, textTall );
		surface()->DrawSetTextPos( ( wide - textWide )/2, tall * 0.50f );
		surface()->DrawSetTextFont( hFont );
		surface()->DrawSetTextColor( 255, 255, 255, alpha );
		surface()->DrawPrintText( pString, wcslen( pString ) );
	}

	// 360 always use the progress bar, TCR Requirement, and never this loading plaque
	if ( IsPC() && ( m_bRenderingBackgroundTransition || m_eBackgroundState == BACKGROUND_LOADING ) )
	{
		// draw the loading image over the top
		surface()->DrawSetColor(255, 255, 255, alpha);
		surface()->DrawSetTexture(m_iLoadingImageID);
		int twide, ttall;
		surface()->DrawGetTextureSize(m_iLoadingImageID, twide, ttall);
		surface()->DrawTexturedRect(wide - twide, tall - ttall, wide, tall);
	}

	// update the menu alpha
	if ( m_bFadingInMenus )
	{
		if ( GameUI().IsConsoleUI() )
		{
			m_pConsoleAnimationController->StartAnimationSequence( "OpenMainMenu" );
			m_bFadingInMenus = false;
		}
		else
		{
			// goes from [0..255]
			alpha = (frametime - m_flFadeMenuStartTime) / (m_flFadeMenuEndTime - m_flFadeMenuStartTime) * 255;
			alpha = clamp( alpha, 0, 255 );
			m_pGameMenu->SetAlpha( alpha );
			if ( alpha == 255 )
			{
				m_bFadingInMenus = false;
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameMenu()
{
	// load settings from config file
	KeyValues *datafile = new KeyValues("GameMenu");
	datafile->UsesEscapeSequences( true );	// VGUI uses escape sequences
	if (datafile->LoadFromFile( g_pFullFileSystem, "Resource/GameMenu.res" ) )
	{
		m_pGameMenu = RecursiveLoadGameMenu(datafile);
	}

	if ( !m_pGameMenu )
	{
		Error( "Could not load file Resource/GameMenu.res" );
	}
	else
	{
		// start invisible
		SETUP_PANEL( m_pGameMenu );
		m_pGameMenu->SetAlpha( 0 );
	}

	datafile->deleteThis();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CreateGameLogo()
{
	if ( ModInfo().UseGameLogo() )
	{
		m_pGameLogo = new CMainMenuGameLogo( this, "GameLogo" );

		if ( m_pGameLogo )
		{
			SETUP_PANEL( m_pGameLogo );
			m_pGameLogo->InvalidateLayout( true, true );

			// start invisible
			m_pGameLogo->SetAlpha( 0 );
		}
	}
	else
	{
		m_pGameLogo = NULL;
	}
}

void CBaseModPanel::CheckBonusBlinkState()
{
#ifdef _X360
	// On 360 if we have a storage device at this point and try to read the bonus data it can't find the bonus file!
	return;
#endif

	if ( BonusMapsDatabase()->GetBlink() )
	{
		if ( GameUI().IsConsoleUI() )
			SetMenuItemBlinkingState( "OpenNewGameDialog", true );	// Consoles integrate bonus maps menu into the new game menu
		else
			SetMenuItemBlinkingState( "OpenBonusMapsDialog", true );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Checks to see if menu items need to be enabled/disabled
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateGameMenus()
{
	// check our current state
	bool isInGame = GameUI().IsInLevel();
	bool isMulti = isInGame && (engine->GetMaxClients() > 1);

	// iterate all the menu items
	m_pGameMenu->UpdateMenuItemState( isInGame, isMulti );

	// position the menu
	InvalidateLayout();
	m_pGameMenu->SetVisible( true );
}

//-----------------------------------------------------------------------------
// Purpose: sets up the game menu from the keyvalues
//			the game menu is hierarchial, so this is recursive
//-----------------------------------------------------------------------------
CGameMenu *CBaseModPanel::RecursiveLoadGameMenu(KeyValues *datafile)
{
	CGameMenu *menu = new CGameMenu(this, datafile->GetName());

	// loop through all the data adding items to the menu
	for (KeyValues *dat = datafile->GetFirstSubKey(); dat != NULL; dat = dat->GetNextKey())
	{
		const char *label = dat->GetString("label", "<unknown>");
		const char *cmd = dat->GetString("command", NULL);
		const char *name = dat->GetString("name", label);

//		if ( !Q_stricmp( cmd, "OpenFriendsDialog" ) && bSteamCommunityFriendsVersion )
//			continue;

		menu->AddMenuItem(name, label, cmd, this, dat);
	}

	return menu;
}

//-----------------------------------------------------------------------------
// Purpose: update the taskbar a frame
//-----------------------------------------------------------------------------
void CBaseModPanel::RunFrame()
{
	InvalidateLayout();

	vgui::GetAnimationController()->UpdateAnimations( Plat_FloatTime() );

	if ( m_DelayActivation )
	{
		m_DelayActivation--;
		if ( !m_bLevelLoading && !m_DelayActivation )
		{
			if ( UI_IsDebug() )
				ConColorMsg( Color( 48, 127, 255, 255 ), "[GAMEUI] Executing delayed UI activation\n");

			OnGameUIActivated();
		}
	}

	bool bDoBlur = true;

	if ( enginevguifuncs && !enginevguifuncs->IsGameUIVisible() && !GameUI().IsInBackgroundLevel() )
		bDoBlur = false;

	if ( !bDoBlur )
		bDoBlur = GameClientExports()->ClientWantsBlurEffect();

	float nowTime = Plat_FloatTime();
	float deltaTime = nowTime - m_flLastBlurTime;
	if ( deltaTime > 0 )
	{
		m_flLastBlurTime = nowTime;
		m_flBlurScale += deltaTime * bDoBlur ? 0.05f : -0.05f;
		m_flBlurScale = clamp( m_flBlurScale, 0, 0.85f );
		engine->SetBlurFade( m_flBlurScale );
	}

	if ( GameUI().IsConsoleUI() )
	{
		// run the console ui animations
		m_pConsoleAnimationController->UpdateAnimations( Plat_FloatTime() );

		if ( IsX360() && m_ExitingFrameCount && Plat_FloatTime() >= m_flTransitionEndTime )
		{
			if ( m_ExitingFrameCount > 1 )
			{
				m_ExitingFrameCount--;
				if ( m_ExitingFrameCount == 1 )
				{
					// enough frames have transpired, send the single shot quit command
					// If we kicked off this event from an invite, we need to properly setup the restart to account for that
					if ( m_bRestartFromInvite )
					{
						engine->ClientCmd_Unrestricted( "quit_x360 invite" );
					}
					else if ( m_bRestartSameGame )
					{
						engine->ClientCmd_Unrestricted( "quit_x360 restart" );
					}
					else
					{
						// quits to appchooser
						engine->ClientCmd_Unrestricted( "quit_x360\n" );
					}
				}
			}
		}
	}

	UpdateBackgroundState();

	if ( !m_bPlatformMenuInitialized )
	{
		// check to see if the platform is ready to load yet
		if ( IsX360() || g_VModuleLoader.IsPlatformReady() )
		{
			m_bPlatformMenuInitialized = true;
		}
	} 

	// Check to see if a pending async task has already finished
	if ( m_pAsyncJob && !m_pAsyncJob->m_hThreadHandle )
	{
		m_pAsyncJob->Completed();
		delete m_pAsyncJob;
		m_pAsyncJob = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: Tells XBox Live our user is in the current game's menu
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdateRichPresenceInfo()
{
#if defined( _X360 )
	// For all other users logged into this console (not primary), set to idle to satisfy cert
	for( uint i = 0; i < XUSER_MAX_COUNT; ++i )
	{
		XUSER_SIGNIN_STATE State = XUserGetSigninState( i );

		if( State != eXUserSigninState_NotSignedIn )
		{
			if ( i != XBX_GetPrimaryUserId() )
			{
				// Set rich presence as 'idle' for users logged in that can't participate in orange box.
				if ( !xboxsystem->UserSetContext( i, X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_IDLE, true ) )
				{
					Warning( "BasePanel: UserSetContext failed.\n" );
				}
			}
		}
	}

	if ( !GameUI().IsInLevel() )
	{
		if ( !xboxsystem->UserSetContext( XBX_GetPrimaryUserId(), CONTEXT_GAME, m_iGameID, true ) )
		{
			Warning( "BasePanel: UserSetContext failed.\n" );
		}
		if ( !xboxsystem->UserSetContext( XBX_GetPrimaryUserId(), X_CONTEXT_PRESENCE, CONTEXT_PRESENCE_MENU, true ) )
		{
			Warning( "BasePanel: UserSetContext failed.\n" );
		}
		if ( m_bSinglePlayer )
		{
			if ( !xboxsystem->UserSetContext( XBX_GetPrimaryUserId(), X_CONTEXT_GAME_MODE, CONTEXT_GAME_MODE_SINGLEPLAYER, true ) )
			{
				Warning( "BasePanel: UserSetContext failed.\n" );
			}
		}
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: Lays out the position of the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PerformLayout()
{
	BaseClass::PerformLayout();

	// Get the screen size
	int wide, tall;
	vgui::surface()->GetScreenSize(wide, tall);

	// Get the size of the menu
	int menuWide, menuTall;
	m_pGameMenu->GetSize( menuWide, menuTall );

	int idealMenuY = m_iGameMenuPos.y;
	if ( idealMenuY + menuTall + m_iGameMenuInset > tall )
	{
		idealMenuY = tall - menuTall - m_iGameMenuInset;
	}

	int yDiff = idealMenuY - m_iGameMenuPos.y;

	for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		// Get the size of the logo text
		//int textWide, textTall;
		m_pGameMenuButtons[i]->SizeToContents();
		//vgui::surface()->GetTextSize( m_pGameMenuButtons[i]->GetFont(), ModInfo().GetGameTitle(), textWide, textTall );

		// place menu buttons above middle of screen
		m_pGameMenuButtons[i]->SetPos(m_iGameTitlePos[i].x, m_iGameTitlePos[i].y + yDiff);
		//m_pGameMenuButtons[i]->SetSize(textWide + 4, textTall + 4);
	}

	if ( m_pGameLogo )
	{
		// move the logo to sit right on top of the menu
		m_pGameLogo->SetPos( m_iGameMenuPos.x + m_pGameLogo->GetOffsetX(), idealMenuY - m_pGameLogo->GetTall() + m_pGameLogo->GetOffsetY() );
	}

	// position self along middle of screen
	if ( GameUI().IsConsoleUI() )
	{
		int posx, posy;
		m_pGameMenu->GetPos( posx, posy );
		m_iGameMenuPos.x = posx;
	}
	m_pGameMenu->SetPos(m_iGameMenuPos.x, idealMenuY);

	UpdateGameMenus();
}

//-----------------------------------------------------------------------------
// Purpose: Loads scheme information
//-----------------------------------------------------------------------------
void CBaseModPanel::ApplySchemeSettings(IScheme *pScheme)
{
	int i;
	BaseClass::ApplySchemeSettings(pScheme);

	m_iGameMenuInset = atoi(pScheme->GetResourceString("MainMenu.Inset"));
	m_iGameMenuInset *= 2;

	IScheme *pClientScheme = vgui::scheme()->GetIScheme( vgui::scheme()->GetScheme( "ClientScheme" ) );
	CUtlVector< Color > buttonColor;
	if ( pClientScheme )
	{
		m_iGameTitlePos.RemoveAll();
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pClientScheme->GetFont("ClientTitleFont", true));
			m_iGameTitlePos.AddToTail( coord() );
			m_iGameTitlePos[i].x = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.X", i+1 ) ) );
			m_iGameTitlePos[i].x = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].x );
			m_iGameTitlePos[i].y = atoi(pClientScheme->GetResourceString( CFmtStr( "Main.Title%d.Y", i+1 ) ) );
			m_iGameTitlePos[i].y = scheme()->GetProportionalScaledValue( m_iGameTitlePos[i].y );

			if ( GameUI().IsConsoleUI() )
				m_iGameTitlePos[i].x += MAIN_MENU_INDENT_X360;

			buttonColor.AddToTail( pClientScheme->GetColor( CFmtStr( "Main.Title%d.Color", i+1 ), Color(255, 255, 255, 255)) );
		}

		m_iGameMenuPos.x = atoi(pClientScheme->GetResourceString("Main.Menu.X"));
		m_iGameMenuPos.x = scheme()->GetProportionalScaledValue( m_iGameMenuPos.x );
		m_iGameMenuPos.y = atoi(pClientScheme->GetResourceString("Main.Menu.Y"));
		m_iGameMenuPos.y = scheme()->GetProportionalScaledValue( m_iGameMenuPos.y );

		m_iGameMenuInset = atoi(pClientScheme->GetResourceString("Main.BottomBorder"));
		m_iGameMenuInset = scheme()->GetProportionalScaledValue( m_iGameMenuInset );
	}
	else
	{
		for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
		{
			m_pGameMenuButtons[i]->SetFont(pScheme->GetFont("TitleFont"));
			buttonColor.AddToTail( Color( 255, 255, 255, 255 ) );
		}
	}

	for ( i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetDefaultColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetArmedColor(buttonColor[i], Color(0, 0, 0, 0));
		m_pGameMenuButtons[i]->SetDepressedColor(buttonColor[i], Color(0, 0, 0, 0));
	}

	m_flFrameFadeInTime = atof(pScheme->GetResourceString("Frame.TransitionEffectTime"));

	// work out current focus - find the topmost panel
	SetBgColor(Color(0, 0, 0, 0));

	m_BackdropColor = pScheme->GetColor("mainmenu.backdrop", Color(0, 0, 0, 128));

	char filename[MAX_PATH];
	if ( IsX360() )
	{
		// 360 uses FullFrameFB1 RT for map to map transitioning
		m_iRenderTargetImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iRenderTargetImageID, "console/rt_background", false, false );
	}

	int screenWide, screenTall;
	surface()->GetScreenSize( screenWide, screenTall );
	float aspectRatio = (float)screenWide/(float)screenTall;
	bool bIsWidescreen = aspectRatio >= 1.5999f;

	// work out which background image to use
	if ( IsPC() || !IsX360() )
	{
		// pc uses blurry backgrounds based on the background level
		char background[MAX_PATH];
		engine->GetMainMenuBackgroundName( background, sizeof(background) );
		Q_snprintf( filename, sizeof( filename ), "console/%s%s", background, ( bIsWidescreen ? "_widescreen" : "" ) );

		//Faked BG Image
		Q_snprintf( m_szFadeFilename, sizeof( m_szFadeFilename ), "materials/console/%s%s.vtf", background, ( bIsWidescreen ? "_widescreen" : "" ) );
	}
	else
	{
		// 360 uses hi-res game specific backgrounds
		char gameName[MAX_PATH];
		const char *pGameDir = engine->GetGameDirectory();
		V_FileBase( pGameDir, gameName, sizeof( gameName ) );
		V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_%s%s", gameName, ( bIsWidescreen ? "_widescreen" : "" ) );
	}
	m_iBackgroundImageID = surface()->CreateNewTextureID();
	surface()->DrawSetTextureFile( m_iBackgroundImageID, filename, false, false );

	if ( IsX360() )
	{
		// 360 uses a product image during application exit
		V_snprintf( filename, sizeof( filename ), "vgui/appchooser/background_orange%s", ( bIsWidescreen ? "_widescreen" : "" ) );
		m_iProductImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iProductImageID, filename, false, false );
	}

	if ( IsPC() )
	{
		// load the loading icon
		m_iLoadingImageID = surface()->CreateNewTextureID();
		surface()->DrawSetTextureFile( m_iLoadingImageID, "console/startup_loading", false, false );
	}
}

//-----------------------------------------------------------------------------
// Purpose: message handler for platform menu; activates the selected module
//-----------------------------------------------------------------------------
void CBaseModPanel::OnActivateModule(int moduleIndex)
{
	g_VModuleLoader.ActivateModule(moduleIndex);
}

//-----------------------------------------------------------------------------
// Purpose: Animates menus on gameUI being shown
//-----------------------------------------------------------------------------
void CBaseModPanel::OnGameUIActivated()
{
	if ( UI_IsDebug() )
		ConColorMsg( Color( 48, 127, 255, 255 ), "[GAMEUI] CBaseModPanel::OnGameUIActivated( delay = %d )\n", m_DelayActivation );

	if ( m_DelayActivation )
		return;

	COM_TimestampedLog( "CBaseModPanel::OnGameUIActivated()" );

	// If the load failed, we're going to bail out here
	if ( engine->MapLoadFailed() )
	{
		// Don't display this again until it happens again
		engine->SetMapLoadFailed( false );
		ShowMessageDialog( MD_LOAD_FAILED_WARNING );
	}

	if ( !m_bEverActivated )
	{
		// Layout the first time to avoid focus issues (setting menus visible will grab focus)
		UpdateGameMenus();
		m_bEverActivated = true;

#if defined( _X360 )
		
		// Open all active containers if we have a valid storage device
		if ( XBX_GetPrimaryUserId() != XBX_INVALID_USER_ID && XBX_GetStorageDeviceId() != XBX_INVALID_STORAGE_ID && XBX_GetStorageDeviceId() != XBX_STORAGE_DECLINED )
		{
			// Open user settings and save game container here
			uint nRet = engine->OnStorageDeviceAttached();
			if ( nRet != ERROR_SUCCESS )
			{
				// Invalidate the device
				XBX_SetStorageDeviceId( XBX_INVALID_STORAGE_ID, 0 );

				// FIXME: We don't know which device failed!
				// Pop a dialog explaining that the user's data is corrupt
				BasePanel()->ShowMessageDialog( MD_STORAGE_DEVICES_CORRUPT );
			}
		}

		// determine if we're starting up because of a cross-game invite
		int fLaunchFlags = XboxLaunch()->GetLaunchFlags();
		if ( fLaunchFlags & LF_INVITERESTART )
		{
			XNKID nSessionID;
			XboxLaunch()->GetInviteSessionID( &nSessionID );
			matchmaking->JoinInviteSessionByID( nSessionID );
		}
#endif

		// Brute force check to open tf matchmaking ui.
		if ( GameUI().IsConsoleUI() )
		{
			const char *pGame = engine->GetGameDirectory();
			if ( !Q_stricmp( Q_UnqualifiedFileName( pGame ), "tf" ) )
			{
				m_bUseMatchmaking = true;
				RunMenuCommand( "OpenMatchmakingBasePanel" );
			}
		}
	}

	if ( GameUI().IsConsoleUI() )
	{
		ArmFirstMenuItem();
	}
	if ( GameUI().IsInLevel() )
	{
		if ( !m_bUseMatchmaking )
		{
			OnCommand( "OpenPauseMenu" );
		}
 		else
		{
			RunMenuCommand( "OpenMatchmakingBasePanel" );
 		}

		if ( m_hAchievementsDialog.Get() )
		{
			// Achievement dialog refreshes it's data if the player looks at the pause menu
			m_hAchievementsDialog->OnCommand( "OnGameUIActivated" );
		}
	}
	else // not the pause menu, update presence
	{
		if ( IsX360() )
		{
			UpdateRichPresenceInfo();
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: executes a menu command
//-----------------------------------------------------------------------------
void CBaseModPanel::RunMenuCommand(const char *command)
{
	if ( !Q_stricmp( command, "OpenGameMenu" ) )
	{
		if ( m_pGameMenu )
		{
			PostMessage( m_pGameMenu, new KeyValues("Command", "command", "Open") );
		}
	}
	else if ( !Q_stricmp( command, "OpenPlayerListDialog" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		OnOpenPlayerListDialog();
	}
	else if ( !Q_stricmp( command, "OpenNewGameDialog" ) )
	{
		OnOpenNewGameDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadGameDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenLoadGameDialog();
		}
		else
		{
			OnOpenLoadGameDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenSaveGameDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenSaveGameDialog();
		}
		else
		{
			OnOpenSaveGameDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenBonusMapsDialog" ) )
	{
		OnOpenBonusMapsDialog();
	}
	else if ( !Q_stricmp( command, "OpenOptionsDialog" ) )
	{
		if ( !GameUI().IsConsoleUI() )
		{
			OnOpenOptionsDialog();
		}
		else
		{
			OnOpenOptionsDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "OpenControllerDialog" ) )
	{
		if ( GameUI().IsConsoleUI() )
		{
			OnOpenControllerDialog();
		}
	}
	else if ( !Q_stricmp( command, "OpenBenchmarkDialog" ) )
	{
		OnOpenBenchmarkDialog();
	}
	else if ( !Q_stricmp( command, "OpenServerBrowser" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		OnOpenServerBrowser();
	}
	else if ( !Q_stricmp( command, "OpenFriendsDialog" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		OnOpenFriendsDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadDemoDialog" ) )
	{
		OnOpenDemoDialog();
	}
	else if ( !Q_stricmp( command, "OpenCreateMultiplayerGameDialog" ) )
	{
		OnOpenCreateMultiplayerGameDialog();
	}
	else if ( !Q_stricmp( command, "OpenChangeGameDialog" ) )
	{
		OnOpenChangeGameDialog();
	}
	else if ( !Q_stricmp( command, "OpenLoadCommentaryDialog" ) )
	{
		OnOpenLoadCommentaryDialog();	
	}
	else if ( !Q_stricmp( command, "OpenLoadSingleplayerCommentaryDialog" ) )
	{
		OpenLoadSingleplayerCommentaryDialog();	
	}
	else if ( !Q_stricmp( command, "OpenMatchmakingBasePanel" ) )
	{
		OnOpenMatchmakingBasePanel();
	}
	else if ( !Q_stricmp( command, "OpenAchievementsDialog" ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		if ( IsPC() )
		{
#ifndef NO_STEAM
			OnOpenAchievementsDialog();
#else
			return;
#endif
		}
		else
		{
			OnOpenAchievementsDialog_Xbox();
		}
	}
	else if ( !Q_stricmp( command, "AchievementsDialogClosing" ) )
	{
		if ( IsX360() )
		{
			if ( m_hAchievementsDialog.Get() )
			{
				m_hAchievementsDialog->Close();
			}
		}
	}
	else if ( !Q_stricmp( command, "Quit" ) )
	{
		OnOpenQuitConfirmationDialog();
	}
	else if ( !Q_stricmp( command, "QuitNoConfirm" ) )
	{
		if ( IsX360() )
		{
			// start the shutdown process
			StartExitingProcess();
		}
		else
		{		
			// hide everything while we quit
			SetVisible( false );
			vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
			engine->ClientCmd_Unrestricted( "quit\n" );
		}
	}
	else if ( !Q_stricmp( command, "QuitRestartNoConfirm" ) )
	{
		if ( IsX360() )
		{
			// start the shutdown process
			m_bRestartSameGame = true;
			StartExitingProcess();
		}
	}
	else if ( !Q_stricmp( command, "ResumeGame" ) )
	{
		GameUI().HideGameUI();
	}
	else if ( !Q_stricmp( command, "Disconnect" ) )
	{
		OnOpenDisconnectConfirmationDialog();
	}
	else if ( !Q_stricmp( command, "DisconnectNoConfirm" ) )
	{
		ConVarRef commentary( "commentary" );
		if ( commentary.IsValid() && commentary.GetBool() )
		{
			engine->ClientCmd_Unrestricted( "disconnect" );

			CMatchmakingBasePanel *pBase = GetMatchmakingBasePanel();
			if ( pBase )
			{
				pBase->CloseAllDialogs( false );
				pBase->OnCommand( "OpenWelcomeDialog" );
			}
		}
		else
		{
			// Leave our current session, if we have one
			//matchmaking->KickPlayerFromSession( 0 );

			engine->ClientCmd_Unrestricted( "disconnect" );
		}
	}
	else if ( !Q_stricmp( command, "ReleaseModalWindow" ) )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
	}
	else if ( Q_stristr( command, "engine " ) )
	{
		const char *engineCMD = strstr( command, "engine " ) + strlen( "engine " );
		if ( strlen( engineCMD ) > 0 )
		{
			engine->ClientCmd_Unrestricted( const_cast<char *>( engineCMD ) );
		}
	}
	else if ( Q_stristr( command, "steam " ) )
	{
		// If PC make sure that the Steam user is logged in
		if ( CheckAndDisplayErrorIfNotLoggedIn() )
			return;

		const char *SteamOverlayCMD = strstr( command, "steam " ) + strlen( "steam " );
		if ( strlen( SteamOverlayCMD ) > 0 )
		{
			engine->ClientCmd_Unrestricted( const_cast<char *>( SteamOverlayCMD ) );
			if ( steamapicontext && steamapicontext->SteamFriends() &&
				 steamapicontext->SteamUtils() && steamapicontext->SteamUtils()->IsOverlayEnabled() )
				steamapicontext->SteamFriends()->ActivateGameOverlay( SteamOverlayCMD );
			else
				MakeGenericDialog( "#GameUI_SteamOverlay_Title", "#GameUI_SteamOverlay_Text" );
		}
	}
	else if ( !Q_stricmp( command, "ShowSigninUI" ) )
	{
		m_bWaitingForUserSignIn = true;
		xboxsystem->ShowSigninUI( 1, 0 ); // One user, no special flags
	}
	else if ( !Q_stricmp( command, "ShowDeviceSelector" ) )
	{
		OnChangeStorageDevice();
	}
	else if ( !Q_stricmp( command, "SignInDenied" ) )
	{
		// The user doesn't care, so re-send the command they wanted and mark that we want to skip checking
		m_bUserRefusedSignIn = true;
		if ( m_strPostPromptCommand.IsEmpty() == false )
		{
			OnCommand( m_strPostPromptCommand );		
		}
	}
	else if ( !Q_stricmp( command, "RequiredSignInDenied" ) )
	{
		m_strPostPromptCommand = "";
	}
	else if ( !Q_stricmp( command, "RequiredStorageDenied" ) )
	{
		m_strPostPromptCommand = "";
	}
	else if ( !Q_stricmp( command, "StorageDeviceDenied" ) )
	{
		// The user doesn't care, so re-send the command they wanted and mark that we want to skip checking
		m_bUserRefusedStorageDevice = true;
		IssuePostPromptCommand();

		// Set us as declined
		XBX_SetStorageDeviceId( XBX_STORAGE_DECLINED, 0 );
		m_iStorageID = XBX_INVALID_STORAGE_ID;

		if ( m_pStorageDeviceValidatedNotify )
		{
			*m_pStorageDeviceValidatedNotify = 2;
			m_pStorageDeviceValidatedNotify = NULL;
		}
	}
	else if ( !Q_stricmp( command, "clear_storage_deviceID" ) )
	{
		XBX_SetStorageDeviceId( XBX_STORAGE_DECLINED, 0 );
	}
	else if ( !Q_stricmp( command, "RestartWithNewLanguage" ) )
	{
		if ( !IsX360() )
		{
			const char *pUpdatedAudioLanguage = COptionsSubAudio::GetUpdatedAudioLanguage();

			if ( pUpdatedAudioLanguage[ 0 ] != '\0' )
			{
				char szSteamURL[50];
				char szAppId[50];

				// hide everything while we quit
				SetVisible( false );
				vgui::surface()->RestrictPaintToSinglePanel( GetVPanel() );
				engine->ClientCmd_Unrestricted( "quit\n" );

				// Construct Steam URL. Pattern is steam://run/<appid>/<language>. (e.g. Ep1 In French ==> steam://run/380/french)
				Q_strcpy(szSteamURL, "steam://run/");
				itoa( engine->GetAppID(), szAppId, 10 );
				Q_strcat( szSteamURL, szAppId, sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, "/", sizeof( szSteamURL ) );
				Q_strcat( szSteamURL, pUpdatedAudioLanguage, sizeof( szSteamURL ) );

				// Set Steam URL for re-launch in registry. Launcher will check this registry key and exec it in order to re-load the game in the proper language
				vgui::system()->SetRegistryString("HKEY_CURRENT_USER\\Software\\Valve\\Source\\Relaunch URL", szSteamURL );
			}
		}
	}
	else
	{
		BaseClass::OnCommand( command);
	}
}

//-----------------------------------------------------------------------------
// Purpose: Queue a command to be run when XUI Closes
//-----------------------------------------------------------------------------
void CBaseModPanel::QueueCommand( const char *pCommand )
{
	if ( m_bXUIVisible )
	{
		m_CommandQueue.AddToTail( CUtlString( pCommand ) );
	}
	else
	{
		OnCommand( pCommand );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Run all the commands in the queue
//-----------------------------------------------------------------------------
void CBaseModPanel::RunQueuedCommands()
{
	for ( int i = 0; i < m_CommandQueue.Count(); ++i )
	{
		OnCommand( m_CommandQueue[i] );
	}
	ClearQueuedCommands();
}

//-----------------------------------------------------------------------------
// Purpose: Clear all queued commands
//-----------------------------------------------------------------------------
void CBaseModPanel::ClearQueuedCommands()
{
	m_CommandQueue.Purge();
}

//-----------------------------------------------------------------------------
// Purpose: Whether this command should cause us to prompt the user if they're not signed in and do not have a storage device
//-----------------------------------------------------------------------------
bool CBaseModPanel::IsPromptableCommand( const char *command )
{
	// Blech!
	if ( !Q_stricmp( command, "OpenNewGameDialog" ) ||
		 !Q_stricmp( command, "OpenLoadGameDialog" ) ||
		 !Q_stricmp( command, "OpenSaveGameDialog" ) ||
		 !Q_stricmp( command, "OpenBonusMapsDialog" ) ||
		 !Q_stricmp( command, "OpenOptionsDialog" ) ||
		 !Q_stricmp( command, "OpenControllerDialog" ) ||
		 !Q_stricmp( command, "OpenLoadCommentaryDialog" ) ||
		 !Q_stricmp( command, "OpenLoadSingleplayerCommentaryDialog" ) ||
		 !Q_stricmp( command, "OpenAchievementsDialog" ) )
	{
		 return true;
	}

	return false;
}

#ifdef _WIN32
//-------------------------
// Purpose: Job wrapper
//-------------------------
static unsigned PanelJobWrapperFn( void *pvContext )
{
	CBaseModPanel::CAsyncJobContext *pAsync = reinterpret_cast< CBaseModPanel::CAsyncJobContext * >( pvContext );

	float const flTimeStart = Plat_FloatTime();
	
	pAsync->ExecuteAsync();

	float const flElapsedTime = Plat_FloatTime() - flTimeStart;

	if ( flElapsedTime < pAsync->m_flLeastExecuteTime )
	{
		ThreadSleep( ( pAsync->m_flLeastExecuteTime - flElapsedTime ) * 1000 );
	}

	ReleaseThreadHandle( ( ThreadHandle_t ) pAsync->m_hThreadHandle );
	pAsync->m_hThreadHandle = NULL;

	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: Enqueues a job function to be called on a separate thread
//-----------------------------------------------------------------------------
void CBaseModPanel::ExecuteAsync( CAsyncJobContext *pAsync )
{
	Assert( !m_pAsyncJob );
	Assert( pAsync && !pAsync->m_hThreadHandle );
	m_pAsyncJob = pAsync;

#ifdef _WIN32
	ThreadHandle_t hHandle = CreateSimpleThread( PanelJobWrapperFn, reinterpret_cast< void * >( pAsync ) );
	pAsync->m_hThreadHandle = hHandle;

#ifdef _X360
	ThreadSetAffinity( hHandle, XBOX_PROCESSOR_3 );
#endif

#else
	pAsync->ExecuteAsync();
#endif
}



//-----------------------------------------------------------------------------
// Purpose: Whether this command requires the user be signed in
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRequiresSignIn( const char *command )
{
	// Blech again!
	if ( !Q_stricmp( command, "OpenAchievementsDialog" ) || 
		 !Q_stricmp( command, "OpenLoadGameDialog" ) ||
		 !Q_stricmp( command, "OpenSaveGameDialog" ) || 
		 !Q_stricmp( command, "OpenRankingsDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the command requires the user to have a valid storage device
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRequiresStorageDevice( const char *command )
{
	// Anything which touches the storage device must prompt
	if ( !Q_stricmp( command, "OpenSaveGameDialog" ) ||
		 !Q_stricmp( command, "OpenLoadGameDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Whether the command requires the user to have a valid profile selected
//-----------------------------------------------------------------------------
bool CBaseModPanel::CommandRespectsSignInDenied( const char *command )
{
	// Anything which touches the user profile must prompt
	if ( !Q_stricmp( command, "OpenOptionsDialog" ) ||
		 !Q_stricmp( command, "OpenControllerDialog" ) )
		return true;

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: A storage device has been connected, update our settings and anything else
//-----------------------------------------------------------------------------

class CAsyncCtxOnDeviceAttached : public CBaseModPanel::CAsyncJobContext
{
public:
	CAsyncCtxOnDeviceAttached();
	~CAsyncCtxOnDeviceAttached();
	virtual void ExecuteAsync();
	virtual void Completed();
	uint GetContainerOpenResult( void ) { return m_ContainerOpenResult; }

private:
	uint m_ContainerOpenResult;
};

CAsyncCtxOnDeviceAttached::CAsyncCtxOnDeviceAttached() :
	CBaseModPanel::CAsyncJobContext( 3.0f ),	// Storage device info for at least 3 seconds
	m_ContainerOpenResult( ERROR_SUCCESS )
{
	BasePanel()->ShowMessageDialog( MD_CHECKING_STORAGE_DEVICE );
}

CAsyncCtxOnDeviceAttached::~CAsyncCtxOnDeviceAttached()
{
	BasePanel()->CloseMessageDialog( 0 );
}

void CAsyncCtxOnDeviceAttached::ExecuteAsync()
{
	// Asynchronously do the tasks that don't interact with the command buffer

	// Open user settings and save game container here
	m_ContainerOpenResult = engine->OnStorageDeviceAttached(0);
	if ( m_ContainerOpenResult != ERROR_SUCCESS )
		return;

	// Make the QOS system initialized for multiplayer games
	if ( !ModInfo().IsSinglePlayerOnly() )
	{
#if defined( _X360 )
		( void ) matchmaking->GetQosWithLIVE();
#endif
	}
}

void CAsyncCtxOnDeviceAttached::Completed()
{
	BasePanel()->OnCompletedAsyncDeviceAttached( this );
}


void CBaseModPanel::OnDeviceAttached( void )
{
	ExecuteAsync( new CAsyncCtxOnDeviceAttached );
}

void CBaseModPanel::OnCompletedAsyncDeviceAttached( CAsyncCtxOnDeviceAttached *job )
{
	uint nRet = job->GetContainerOpenResult();
	if ( nRet != ERROR_SUCCESS )
	{
		// Invalidate the device
		XBX_SetStorageDeviceId( XBX_INVALID_STORAGE_ID, 0 );

		// FIXME: We don't know which device failed!
		// Pop a dialog explaining that the user's data is corrupt
		BasePanel()->ShowMessageDialog( MD_STORAGE_DEVICES_CORRUPT );
	}

	// First part of the device checking completed asynchronously,
	// perform the rest of duties that require to run on main thread.
	engine->ReadConfiguration(0,false);
	engine->ExecuteClientCmd( "refreshplayerstats" );

	BonusMapsDatabase()->ReadBonusMapSaveData();

	if ( m_hSaveGameDialog_Xbox.Get() )
	{
		m_hSaveGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
	}
	if ( m_hLoadGameDialog_Xbox.Get() )
	{
		m_hLoadGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
	}
	if ( m_hOptionsDialog_Xbox.Get() )
	{
		m_hOptionsDialog_Xbox->OnCommand( "RefreshOptions" );
	}
	if ( m_pStorageDeviceValidatedNotify )
	{
		*m_pStorageDeviceValidatedNotify = 1;
		m_pStorageDeviceValidatedNotify = NULL;
	}

	// Finish their command
	IssuePostPromptCommand();
}

//-----------------------------------------------------------------------------
// Purpose: FIXME: Only TF takes this path...
//-----------------------------------------------------------------------------
bool CBaseModPanel::ValidateStorageDevice( void )
{
	if ( m_bUserRefusedStorageDevice == false )
	{
#if defined( _X360 )
		if ( XBX_GetStorageDeviceId() == XBX_INVALID_STORAGE_ID )
		{
			// Try to discover content on the user's storage devices
			DWORD nFoundDevice = xboxsystem->DiscoverUserData( XBX_GetPrimaryUserId(), COM_GetModDirectory() );
			if ( nFoundDevice == XBX_INVALID_STORAGE_ID )
			{
				// They don't have a device, so ask for one
				ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE );
				return false;
			}
			else
			{
				// Take this device
				XBX_SetStorageDeviceId( nFoundDevice );
				OnDeviceAttached();
			}
			// Fall through
		}
#endif
	}
	return true;
}

bool CBaseModPanel::ValidateStorageDevice( int *pStorageDeviceValidated )
{
	if ( m_pStorageDeviceValidatedNotify )
	{
		if ( pStorageDeviceValidated != m_pStorageDeviceValidatedNotify )
		{
			*m_pStorageDeviceValidatedNotify = -1;
			m_pStorageDeviceValidatedNotify = NULL;
		}
		else
		{
			return false;
		}
	}

	if ( pStorageDeviceValidated )
	{
		if ( HandleStorageDeviceRequest( "" ) )
			return true;

		m_pStorageDeviceValidatedNotify = pStorageDeviceValidated;
		return false;
	}

	return false;
}

//-----------------------------------------------------------------------------
// Purpose: Monitor commands for certain necessary cases
// Input  : *command - What menu command we're policing
//-----------------------------------------------------------------------------
bool CBaseModPanel::HandleSignInRequest( const char *command )
{
#ifdef _X360
	// If we have a post-prompt command, we're coming back into the call from that prompt
	bool bQueuedCall = ( m_strPostPromptCommand.IsEmpty() == false );

	XUSER_SIGNIN_INFO info;
	bool bValidUser = ( XUserGetSigninInfo( XBX_GetPrimaryUserId(), 0, &info ) == ERROR_SUCCESS );

	if ( bValidUser )
		return true;

	// Queued command means we're returning from a prompt or blade
	if ( bQueuedCall )
	{
		// Blade has returned with nothing
		if ( m_bUserRefusedSignIn )
			return true;
		
		// User has not denied the storage device, so ask
		ShowMessageDialog( MD_PROMPT_SIGNIN );
		m_strPostPromptCommand = command;
		
		// Do not run command
		return false;
	}
	else
	{
		// If the user refused the sign-in and we respect that on this command, we're done
		if ( m_bUserRefusedSignIn && CommandRespectsSignInDenied( command ) )
			return true;

		// If the message is required first, then do that instead
		if ( CommandRequiresSignIn( command ) )
		{
			ShowMessageDialog( MD_PROMPT_SIGNIN_REQUIRED );
			m_strPostPromptCommand = command;
			return false;
		}

		// Pop a blade out
		xboxsystem->ShowSigninUI( 1, 0 );
		m_strPostPromptCommand = command;
		m_bWaitingForUserSignIn = true;
		m_bUserRefusedSignIn = false;
		return false;	
	}
#endif // _X360
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *command - 
//-----------------------------------------------------------------------------
bool CBaseModPanel::HandleStorageDeviceRequest( const char *command )
{
	// If we don't have a valid sign-in, then we do nothing!
	if ( m_bUserRefusedSignIn )
		return true;

	// If we have a valid storage device, there's nothing to prompt for
	if ( XBX_GetStorageDeviceId(0) != XBX_INVALID_STORAGE_ID && XBX_GetStorageDeviceId(0) != XBX_STORAGE_DECLINED )
		return true;

	// If we have a post-prompt command, we're coming back into the call from that prompt
	bool bQueuedCall = ( m_strPostPromptCommand.IsEmpty() == false );
	
	// Are we returning from a prompt?
	if ( bQueuedCall && m_bStorageBladeShown )
	{
		// User has declined
		if ( m_bUserRefusedStorageDevice )
			return true;

		// Prompt them
		ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE );
		m_strPostPromptCommand = command;
		
		// Do not run the command
		return false;
	}
	else
	{
		// If the user refused the sign-in and we respect that on this command, we're done
		if ( m_bUserRefusedStorageDevice && CommandRespectsSignInDenied( command ) )
			return true;

#if 0 // This attempts to find user data, but may not be cert-worthy even though it's a bit nicer for the user
		// Attempt to automatically find a device
		DWORD nFoundDevice = xboxsystem->DiscoverUserData( XBX_GetPrimaryUserId(), COM_GetModDirectory() );
		if ( nFoundDevice != XBX_INVALID_STORAGE_ID )
		{
			// Take this device
			XBX_SetStorageDeviceId( nFoundDevice );
			OnDeviceAttached();
			return true;
		}
#endif // 

		// If the message is required first, then do that instead
		if ( CommandRequiresStorageDevice( command ) )
		{
			ShowMessageDialog( MD_PROMPT_STORAGE_DEVICE_REQUIRED );
			m_strPostPromptCommand = command;
			return false;
		}

		// This is a misnomer of the first order!
		OnChangeStorageDevice();
		m_strPostPromptCommand = command;
		m_bStorageBladeShown = true;
		m_bUserRefusedStorageDevice = false;
		return false;
	}

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: Clear the command we've queued once it has succeeded in being called
//-----------------------------------------------------------------------------
void CBaseModPanel::ClearPostPromptCommand( const char *pCompletedCommand )
{
	if ( !Q_stricmp( m_strPostPromptCommand, pCompletedCommand ) )
	{
		// All commands are executed, so stop holding this
		m_strPostPromptCommand = "";
	}
}

//-----------------------------------------------------------------------------
// Purpose: Issue our queued command to either the base panel or the matchmaking panel
//-----------------------------------------------------------------------------
void CBaseModPanel::IssuePostPromptCommand( void )
{
	// The device is valid, so launch any pending commands
	if ( m_strPostPromptCommand.IsEmpty() == false )
	{
		if ( m_bSinglePlayer )
		{
			OnCommand( m_strPostPromptCommand );
		}
		else
		{
			CMatchmakingBasePanel *pMatchMaker = GetMatchmakingBasePanel();
			if ( pMatchMaker )
			{
				pMatchMaker->OnCommand( m_strPostPromptCommand );
			}
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: message handler for menu selections
//-----------------------------------------------------------------------------
void CBaseModPanel::OnCommand( const char *command )
{
	if ( GameUI().IsConsoleUI() )
	{
#if defined( _X360 )

		// See if this is a command we need to intercept
		if ( IsPromptableCommand( command ) )
		{
			// Handle the sign in case
			if ( HandleSignInRequest( command ) == false )
				return;
			
			// Handle storage
			if ( HandleStorageDeviceRequest( command ) == false )
				return;

			// If we fall through, we'll need to track this again
			m_bStorageBladeShown = false;

			// Fall through
		}
#endif // _X360

		RunAnimationWithCallback( this, command, new KeyValues( "RunMenuCommand", "command", command ) );
	
		// Clear our pending command if we just executed it
		ClearPostPromptCommand( command );
	}
	else
	{
		RunMenuCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: runs an animation sequence, then calls a message mapped function
//			when the animation is complete. 
//-----------------------------------------------------------------------------
void CBaseModPanel::RunAnimationWithCallback( vgui::Panel *parent, const char *animName, KeyValues *msgFunc )
{
	if ( !m_pConsoleAnimationController )
		return;

	m_pConsoleAnimationController->StartAnimationSequence( animName );
	float sequenceLength = m_pConsoleAnimationController->GetAnimationSequenceLength( animName );
	if ( sequenceLength )
	{
		sequenceLength += g_flAnimationPadding;
	}
	if ( parent && msgFunc )
	{
		PostMessage( parent, msgFunc, sequenceLength );
	}
}

//-----------------------------------------------------------------------------
// Purpose: trinary choice query "save & quit", "quit", "cancel"
//-----------------------------------------------------------------------------
class CSaveBeforeQuitQueryDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CSaveBeforeQuitQueryDialog, vgui::Frame );
public:
	CSaveBeforeQuitQueryDialog(vgui::Panel *parent, const char *name) : BaseClass(parent, name)
	{
		LoadControlSettings("resource/SaveBeforeQuitDialog.res");
		SetDeleteSelfOnClose(true);
		SetSizeable(false);
	}

	void DoModal()
	{
		BaseClass::Activate();
		input()->SetAppModalSurface(GetVPanel());
		MoveToCenterOfScreen();
		vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());

		GameUI().PreventEngineHideGameUI();
	}

	void OnKeyCodePressed(KeyCode code)
	{
		// ESC cancels
		if ( code == KEY_ESCAPE )
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

	virtual void OnCommand(const char *command)
	{
		if (!Q_stricmp(command, "Quit"))
		{
			PostMessage(GetVParent(), new KeyValues("Command", "command", "QuitNoConfirm"));
		}
		else if (!Q_stricmp(command, "SaveAndQuit"))
		{
			// find a new name to save
			char saveName[128];
			CSaveGameDialog::FindSaveSlot( saveName, sizeof(saveName) );
			if ( saveName && saveName[ 0 ] )
			{
				// save the game
				char sz[ 256 ];
				Q_snprintf(sz, sizeof( sz ), "save %s\n", saveName );
				engine->ClientCmd_Unrestricted( sz );
			}

			// quit
			PostMessage(GetVParent(), new KeyValues("Command", "command", "QuitNoConfirm"));
		}
		else if (!Q_stricmp(command, "Cancel"))
		{
			Close();
		}
		else
		{
			BaseClass::OnCommand(command);
		}
	}

	virtual void OnClose()
	{
		BaseClass::OnClose();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		GameUI().AllowEngineHideGameUI();
	}
};

//-----------------------------------------------------------------------------
// Purpose: simple querybox that accepts escape
//-----------------------------------------------------------------------------
class CQuitQueryBox : public vgui::QueryBox
{
	DECLARE_CLASS_SIMPLE( CQuitQueryBox, vgui::QueryBox );
public:
	CQuitQueryBox(const char *title, const char *info, Panel *parent) : BaseClass( title, info, parent )
	{
	}

	void DoModal( Frame* pFrameOver )
	{
		BaseClass::DoModal( pFrameOver );
		vgui::surface()->RestrictPaintToSinglePanel(GetVPanel());
		GameUI().PreventEngineHideGameUI();
	}

	void OnKeyCodePressed(KeyCode code)
	{
		// ESC cancels
		if (code == KEY_ESCAPE)
		{
			Close();
		}
		else
		{
			BaseClass::OnKeyCodePressed(code);
		}
	}

	virtual void OnClose()
	{
		BaseClass::OnClose();
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		GameUI().AllowEngineHideGameUI();
	}
};

//-----------------------------------------------------------------------------
// Purpose: asks user how they feel about quiting
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenQuitConfirmationDialog()
{
	if ( GameUI().IsConsoleUI() )
	{
		if ( !GameUI().HasSavedThisMenuSession() && GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
		{
			// single player, progress will be lost...
			ShowMessageDialog( MD_SAVE_BEFORE_QUIT ); 
		}
		else
		{
			if ( m_bUseMatchmaking )
			{
				ShowMessageDialog( MD_QUIT_CONFIRMATION_TF );
			}
			else
			{
				ShowMessageDialog( MD_QUIT_CONFIRMATION );
			}
		}
		return;
	}


	if ( GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
	{
		// prompt for saving current game before quiting
		CSaveBeforeQuitQueryDialog *box = new CSaveBeforeQuitQueryDialog(this, "SaveBeforeQuitQueryDialog");
		box->DoModal();
	}
	else
	{
		// simple ok/cancel prompt
		QueryBox *box = new CQuitQueryBox("#GameUI_QuitConfirmationTitle", "#GameUI_QuitConfirmationText", this);
		box->SetOKButtonText("#GameUI_Quit");
		box->SetOKCommand(new KeyValues("Command", "command", "QuitNoConfirm"));
		box->SetCancelCommand(new KeyValues("Command", "command", "ReleaseModalWindow"));
		box->AddActionSignalTarget(this);
		box->DoModal();
	}
}

//-----------------------------------------------------------------------------
// Purpose: asks user how they feel about disconnecting
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenDisconnectConfirmationDialog()
{
	if ( GameUI().IsConsoleUI() )
	{
		if ( GameUI().IsInLevel() && engine->GetMaxClients() == 1 )
		{
			ShowMessageDialog( MD_DISCONNECT_CONFIRMATION_HOST );
		}
		else
		{
			ShowMessageDialog( MD_DISCONNECT_CONFIRMATION );
		}
		return;
	}
	else
	{
		if ( GameUI().IsInLevel() )
		{
			const char* message = nullptr;

			if ( engine->GetMaxClients() == 1 )
				message = "#GameUI_DisconnectHostConfirmationText";
			else
				message = "#GameUI_DisconnectConfirmationText";

			// simple ok/cancel prompt
			QueryBox *box = new CQuitQueryBox( "#GameUI_Disconnect", message, this );
			box->SetOKButtonText( "#GameUI_Disconnect" );
			box->SetOKCommand( new KeyValues( "Command", "command", "DisconnectNoConfirm" ) );
			box->SetCancelCommand( new KeyValues( "Command", "command", "ReleaseModalWindow" ) );
			box->AddActionSignalTarget( this );
			box->DoModal();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenNewGameDialog(const char *chapter )
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog(this, false);
		PositionDialog( m_hNewGameDialog );
	}

	if ( chapter )
	{
		((CNewGameDialog *)m_hNewGameDialog.Get())->SetSelectedChapter(chapter);
	}

	((CNewGameDialog *)m_hNewGameDialog.Get())->SetCommentaryMode( false );
	m_hNewGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenBonusMapsDialog( void )
{
	if ( !m_hBonusMapsDialog.Get() )
	{
		m_hBonusMapsDialog = new CBonusMapsDialog(this);
		PositionDialog( m_hBonusMapsDialog );
	}

	m_hBonusMapsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadGameDialog()
{
	if ( !m_hLoadGameDialog.Get() )
	{
		m_hLoadGameDialog = new CLoadGameDialog(this);
		PositionDialog( m_hLoadGameDialog );
	}
	m_hLoadGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadGameDialog_Xbox()
{
	if ( !m_hLoadGameDialog_Xbox.Get() )
	{
		m_hLoadGameDialog_Xbox = new CLoadGameDialogXbox(this);
		PositionDialog( m_hLoadGameDialog_Xbox );
	}
	m_hLoadGameDialog_Xbox->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenSaveGameDialog()
{
	if ( !m_hSaveGameDialog.Get() )
	{
		m_hSaveGameDialog = new CSaveGameDialog(this);
		PositionDialog( m_hSaveGameDialog );
	}
	m_hSaveGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenSaveGameDialog_Xbox()
{
	if ( !m_hSaveGameDialog_Xbox.Get() )
	{
		m_hSaveGameDialog_Xbox = new CSaveGameDialogXbox(this);
		PositionDialog( m_hSaveGameDialog_Xbox );
	}
	m_hSaveGameDialog_Xbox->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenOptionsDialog()
{
	if ( !m_hOptionsDialog.Get() )
	{
		m_hOptionsDialog = new COptionsDialog(this);
		PositionDialog( m_hOptionsDialog );
	}

	m_hOptionsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenOptionsDialog_Xbox()
{
	if ( !m_hOptionsDialog_Xbox.Get() )
	{
		m_hOptionsDialog_Xbox = new COptionsDialogXbox( this );
		PositionDialog( m_hOptionsDialog_Xbox );
	}

	m_hOptionsDialog_Xbox->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: forces any changed options dialog settings to be applied immediately, if it's open
//-----------------------------------------------------------------------------
void CBaseModPanel::ApplyOptionsDialogSettings()
{
	if (m_hOptionsDialog.Get())
	{
		m_hOptionsDialog->ApplyChanges();
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenControllerDialog()
{
	if ( !m_hControllerDialog.Get() )
	{
		m_hControllerDialog = new CControllerDialog( this );
		PositionDialog( m_hControllerDialog );
	}

	m_hControllerDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenBenchmarkDialog()
{
	if (!m_hBenchmarkDialog.Get())
	{
		m_hBenchmarkDialog = new CBenchmarkDialog(this, "BenchmarkDialog");
		PositionDialog( m_hBenchmarkDialog );
	}
	m_hBenchmarkDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenServerBrowser()
{
	bool isSteam = IsPC() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils();
	if ( isSteam )
	{
		// show the server browser
		g_VModuleLoader.ActivateModule( "Servers" );

		KeyValues *pSchemeKV = new KeyValues( "SetCustomScheme" );
		pSchemeKV->SetString( "SchemeName", "SourceScheme" );
		g_VModuleLoader.PostMessageToAllModules( pSchemeKV );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenFriendsDialog()
{
	bool isSteam = IsPC() && steamapicontext->SteamFriends() && steamapicontext->SteamUtils();
	if ( isSteam )
		steamapicontext->SteamFriends()->ActivateGameOverlay( "Friends" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenDemoDialog()
{
	engine->ClientCmd_Unrestricted( "demoui2" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenCreateMultiplayerGameDialog()
{
	if (!m_hCreateMultiplayerGameDialog.Get())
	{
		m_hCreateMultiplayerGameDialog = new CCreateMultiplayerGameDialog(this);
		PositionDialog(m_hCreateMultiplayerGameDialog);
	}
	m_hCreateMultiplayerGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenChangeGameDialog()
{
	if (!m_hChangeGameDialog.Get())
	{
		m_hChangeGameDialog = new CChangeGameDialog(this);
		PositionDialog(m_hChangeGameDialog);
	}
	m_hChangeGameDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenPlayerListDialog()
{
	if (!m_hPlayerListDialog.Get())
	{
		m_hPlayerListDialog = new CPlayerListDialog(this);
		PositionDialog(m_hPlayerListDialog);
	}
	m_hPlayerListDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenLoadCommentaryDialog()
{
	if (!m_hPlayerListDialog.Get())
	{
		m_hLoadCommentaryDialog = new CLoadCommentaryDialog(this);
		PositionDialog(m_hLoadCommentaryDialog);
	}
	m_hLoadCommentaryDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OpenLoadSingleplayerCommentaryDialog()
{
	if ( !m_hNewGameDialog.Get() )
	{
		m_hNewGameDialog = new CNewGameDialog(this,true);
		PositionDialog( m_hNewGameDialog );
	}

	((CNewGameDialog *)m_hNewGameDialog.Get())->SetCommentaryMode( true );
	m_hNewGameDialog->Activate();
}

void CBaseModPanel::OnOpenAchievementsDialog()
{
	if (!m_hAchievementsDialog.Get())
	{
		m_hAchievementsDialog = new CAchievementsDialog( this );
		PositionDialog(m_hAchievementsDialog);
	}
	m_hAchievementsDialog->Activate();
}

void CBaseModPanel::OnOpenAchievementsDialog_Xbox()
{
	if (!m_hAchievementsDialog.Get())
	{
		m_hAchievementsDialog = new CAchievementsDialog_XBox( this );
		PositionDialog(m_hAchievementsDialog);
	}
	m_hAchievementsDialog->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnOpenMatchmakingBasePanel()
{
	if (!m_hMatchmakingBasePanel.Get())
	{
		m_hMatchmakingBasePanel = new CMatchmakingBasePanel( this );
		int x, y, wide, tall;
		GetBounds( x, y, wide, tall );
		m_hMatchmakingBasePanel->SetBounds( x, y, wide, tall );
	}

	if ( m_pGameLogo )
	{
		m_pGameLogo->SetVisible( false );
	}

	// Hide the standard game menu
	for ( int i = 0; i < m_pGameMenuButtons.Count(); ++i ) 
	{
		m_pGameMenuButtons[i]->SetVisible( false );
	}

	// Hide the BasePanel's button footer
	m_pGameMenu->ShowFooter( false );

	m_hMatchmakingBasePanel->SetVisible( true );

	m_hMatchmakingBasePanel->Activate();
}

//-----------------------------------------------------------------------------
// Purpose: Helper function for this common operation
//-----------------------------------------------------------------------------
CMatchmakingBasePanel *CBaseModPanel::GetMatchmakingBasePanel()
{
	CMatchmakingBasePanel *pBase = NULL;
	if ( m_bUseMatchmaking )
	{
		pBase = dynamic_cast< CMatchmakingBasePanel* >( m_hMatchmakingBasePanel.Get() );
	}
	return pBase;
}

//-----------------------------------------------------------------------------
// Purpose: moves the game menu button to the right place on the taskbar
//-----------------------------------------------------------------------------
void CBaseModPanel::PositionDialog(vgui::PHandle dlg)
{
	if (!dlg.Get())
		return;

	int x, y, ww, wt, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, ww, wt );
	dlg->GetSize(wide, tall);

	// Center it, keeping requested size
	dlg->SetPos(x + ((ww - wide) / 2), y + ((wt - tall) / 2));
}

//-----------------------------------------------------------------------------
// Purpose: If on PC and not logged into Steam, displays an error and returns true.  
//			Returns false if everything is OK.
//-----------------------------------------------------------------------------
bool CBaseModPanel::CheckAndDisplayErrorIfNotLoggedIn()
{
	// only check if PC
	if ( !IsPC() )
		return false;

#ifndef _X360
	// if we have Steam interfaces and user is logged on, everything is OK
	if ( steamapicontext && steamapicontext->SteamUser() && steamapicontext->SteamMatchmaking() )
		return false;
#endif

	// Steam is not running or user not logged on, display error
	MakeGenericDialog( "#GameUI_SteamRequired_Title", "#GameUI_SteamRequired_Message" );

	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::MakeGenericDialog( const char* title, const char* messge )
{
	vgui::MessageBox *pMessageBox = new vgui::MessageBox( title, messge );
	pMessageBox->DoModal();
}

//-----------------------------------------------------------------------------
// Purpose: Add an Xbox 360 message dialog to a dialog stack
//-----------------------------------------------------------------------------
void CBaseModPanel::ShowMessageDialog( const uint nType, vgui::Panel *pOwner )
{
	if ( pOwner == NULL )
	{
		pOwner = this;
	}

	m_MessageDialogHandler.ShowMessageDialog( nType, pOwner );
}

//-----------------------------------------------------------------------------
// Purpose: Add an Xbox 360 message dialog to a dialog stack
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseMessageDialog( const uint nType )
{
	m_MessageDialogHandler.CloseMessageDialog( nType );
}

//-----------------------------------------------------------------------------
// Purpose: Matchmaking notification from engine
//-----------------------------------------------------------------------------
void CBaseModPanel::SessionNotification( const int notification, const int param )
{
	// This is a job for the matchmaking panel
	CMatchmakingBasePanel *pBase = GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->SessionNotification( notification, param );
	}
}

//-----------------------------------------------------------------------------
// Purpose: System notification from engine
//-----------------------------------------------------------------------------
void CBaseModPanel::SystemNotification( const int notification )
{
	CMatchmakingBasePanel *pBase = GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->SystemNotification( notification );
	}

	if ( notification == SYSTEMNOTIFY_USER_SIGNEDIN )
	{
#if defined( _X360 )
		// See if it was the active user who signed in
		uint state = XUserGetSigninState( XBX_GetPrimaryUserId() );
		if ( state != eXUserSigninState_NotSignedIn )
		{
			// Reset a bunch of state
			m_bUserRefusedSignIn = false;
			m_bUserRefusedStorageDevice = false;
			m_bStorageBladeShown = false;
		}	
		UpdateRichPresenceInfo();
		engine->GetAchievementMgr()->DownloadUserData();
		engine->GetAchievementMgr()->EnsureGlobalStateLoaded();
#endif
	}
	else if ( notification == SYSTEMNOTIFY_USER_SIGNEDOUT  )
	{
#if defined( _X360 )
		// See if it was the active user who signed out
		uint state = XUserGetSigninState( XBX_GetPrimaryUserId() );
		if ( state != eXUserSigninState_NotSignedIn )
		{
			return;
		}

		// Invalidate their storage ID
		engine->OnStorageDeviceDetached();
		m_bUserRefusedStorageDevice = false;
		m_bUserRefusedSignIn = false;
		m_iStorageID = XBX_INVALID_STORAGE_ID;
		engine->GetAchievementMgr()->InitializeAchievements();
		m_MessageDialogHandler.CloseAllMessageDialogs();

#endif
		if ( GameUI().IsInLevel() )
		{
			if ( m_pGameLogo )
			{
				m_pGameLogo->SetVisible( false );
			}

			// Hide the standard game menu
			for ( int i = 0; i < m_pGameMenuButtons.Count(); ++i )
			{
				m_pGameMenuButtons[i]->SetVisible( false );
			}

			// Hide the BasePanel's button footer
			m_pGameMenu->ShowFooter( false );

			QueueCommand( "QuitNoConfirm" );
		}
		else
		{
			CloseBaseDialogs();
		}

		OnCommand( "OpenMainMenu" );
	}
	else if ( notification == SYSTEMNOTIFY_STORAGEDEVICES_CHANGED )
	{
		if ( m_hSaveGameDialog_Xbox.Get() )
			m_hSaveGameDialog_Xbox->OnCommand( "RefreshSaveGames" );
		if ( m_hLoadGameDialog_Xbox.Get() )
			m_hLoadGameDialog_Xbox->OnCommand( "RefreshSaveGames" );

		// FIXME: This code is incorrect, they do NOT need a storage device, it is only recommended that they do
		if ( GameUI().IsInLevel() )
		{
			// They wanted to use a storage device and are already playing!
			// They need a storage device now or we're quitting the game!
			m_bNeedStorageDeviceHandle = true;
			ShowMessageDialog( MD_STORAGE_DEVICES_NEEDED, this );
		}
		else
		{
			ShowMessageDialog( MD_STORAGE_DEVICES_CHANGED, this );
		}
	}
	else if ( notification == SYSTEMNOTIFY_XUIOPENING )
	{
		m_bXUIVisible = true;
	}
	else if ( notification == SYSTEMNOTIFY_XUICLOSED )
	{
		m_bXUIVisible = false;

		if ( m_bWaitingForStorageDeviceHandle )
		{
			DWORD ret = xboxsystem->GetOverlappedResult( m_hStorageDeviceChangeHandle, NULL, true );
			if ( ret != ERROR_IO_INCOMPLETE )
			{
				// Done waiting
				xboxsystem->ReleaseAsyncHandle( m_hStorageDeviceChangeHandle );
				
				m_bWaitingForStorageDeviceHandle = false;
				
				// If we selected something, validate it
				if ( m_iStorageID != XBX_INVALID_STORAGE_ID )
				{
					// Check to see if there is enough room on this storage device
					if ( xboxsystem->DeviceCapacityAdequate( m_iStorageID, 0, COM_GetModDirectory() ) == false )
					{
						ShowMessageDialog( MD_STORAGE_DEVICES_TOO_FULL, this );
						m_bStorageBladeShown = false; // Show the blade again next time
						m_strPostPromptCommand = ""; // Clear the buffer, we can't return
					}
					else
					{
						m_bNeedStorageDeviceHandle = false;

						// Set the storage device
						XBX_SetStorageDeviceId( m_iStorageID, 0 );
						OnDeviceAttached();
					}
				}
				else
				{
					if ( m_pStorageDeviceValidatedNotify )
					{
						*m_pStorageDeviceValidatedNotify = 2;
						m_pStorageDeviceValidatedNotify = NULL;
					}
					else if ( m_bNeedStorageDeviceHandle )
					{
						// They didn't select a storage device!
						// Remind them that they must pick one or the game will shut down
						ShowMessageDialog( MD_STORAGE_DEVICES_NEEDED, this );
					}
					else
					{
						// Start off the command we queued up
						IssuePostPromptCommand();
					}
				}
			}
		}
		
		// If we're waiting for the user to sign in, and check if they selected a usable profile
		if ( m_bWaitingForUserSignIn )
		{
			// Done waiting
			m_bWaitingForUserSignIn = false;
			m_bUserRefusedSignIn = false;

			// The UI has closed, so go off and revalidate the state
			if ( m_strPostPromptCommand.IsEmpty() == false )
			{
				// Run the command again
				OnCommand( m_strPostPromptCommand );
			}
		}

		RunQueuedCommands();
	}
	else if ( notification == SYSTEMNOTIFY_INVITE_SHUTDOWN )
	{
		// Quit the current game without confirmation
		m_bRestartFromInvite = true;
		m_bXUIVisible = true;
		OnCommand( "QuitNoConfirm" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Matchmaking notification that a player's info has changed
//-----------------------------------------------------------------------------
void CBaseModPanel::UpdatePlayerInfo( uint64 nPlayerId, const char *pName, int nTeam, byte cVoiceState, int nPlayersNeeded, bool bHost )
{
	CMatchmakingBasePanel *pBase = GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->UpdatePlayerInfo( nPlayerId, pName, nTeam, cVoiceState, nPlayersNeeded, bHost );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Matchmaking notification to add a session to the browser
//-----------------------------------------------------------------------------
void CBaseModPanel::SessionSearchResult( int searchIdx, void *pHostData, XSESSION_SEARCHRESULT *pResult, int ping )
{
	CMatchmakingBasePanel *pBase = GetMatchmakingBasePanel();
	if ( pBase )
	{
		pBase->SessionSearchResult( searchIdx, pHostData, pResult, ping );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnChangeStorageDevice( void )
{
	if ( m_bWaitingForStorageDeviceHandle == false )
	{
		m_bWaitingForStorageDeviceHandle = true;
		m_hStorageDeviceChangeHandle = xboxsystem->CreateAsyncHandle();
		m_iStorageID = XBX_INVALID_STORAGE_ID;
		//xboxsystem->ShowDeviceSelector( true, &m_iStorageID, &m_hStorageDeviceChangeHandle );
	}
}

void CBaseModPanel::OnCreditsFinished( void )
{
	if ( !IsX360() )
	{
		// valid for 360 only
		Assert( 0 );
		return;
	}

	bool bExitToAppChooser = false;
	if ( bExitToAppChooser )
	{
		// unknown state from engine, force to a compliant exiting state
		// causes an complete exit out of the game back to the app launcher
		SetVisible( true );
		m_pGameMenu->SetAlpha( 0 );
		StartExitingProcess();
	}
	else
	{
		// expecting to transition from the credits back to the background map
		// prevent any possibility of using the last transition image
		m_bUseRenderTargetImage = false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::OnGameUIHidden()
{
	if ( UI_IsDebug() )
		ConColorMsg( Color( 48, 127, 255, 255 ), "[GAMEUI] CBaseModPanel::OnGameUIHidden()\n" );

	if ( m_hOptionsDialog.Get() )
		PostMessage( m_hOptionsDialog.Get(), new KeyValues( "GameUIHidden" ) );

	// HACKISH: Force this dialog closed so it gets data updates upon reopening.
	vgui::Frame* pAchievementsFrame = m_hAchievementsDialog.Get();
	if ( pAchievementsFrame )
		pAchievementsFrame->Close();
}

//-----------------------------------------------------------------------------
// Purpose: Sets the alpha of the menu panels
//-----------------------------------------------------------------------------
void CBaseModPanel::SetMenuAlpha(int alpha)
{
	if ( GameUI().IsConsoleUI() )
	{
		// handled by animation, not code
		return;
	}

	m_pGameMenu->SetAlpha(alpha);

	if ( m_pGameLogo )
	{
		m_pGameLogo->SetAlpha( alpha );
	}

	for ( int i=0; i<m_pGameMenuButtons.Count(); ++i )
	{
		m_pGameMenuButtons[i]->SetAlpha(alpha);
	}
	m_bForceTitleTextUpdate = true;
}

//-----------------------------------------------------------------------------
// Purpose: starts the game
//-----------------------------------------------------------------------------
void CBaseModPanel::FadeToBlackAndRunEngineCommand( const char *engineCommand )
{
	KeyValues *pKV = new KeyValues( "RunEngineCommand", "command", engineCommand );

	// execute immediately, with no delay
	PostMessage( this, pKV, 0 );
}

void CBaseModPanel::SetMenuItemBlinkingState( const char *itemName, bool state )
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		Panel *child = GetChild(i);
		CGameMenu *pGameMenu = dynamic_cast<CGameMenu *>(child);
		if ( pGameMenu )
		{
			pGameMenu->SetMenuItemBlinkingState( itemName, state );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: runs an engine command, used for delays
//-----------------------------------------------------------------------------
void CBaseModPanel::RunEngineCommand(const char *command)
{
	engine->ClientCmd_Unrestricted(command);
}

//-----------------------------------------------------------------------------
// Purpose: runs an animation to close a dialog and cleans up after close
//-----------------------------------------------------------------------------
void CBaseModPanel::RunCloseAnimation( const char *animName )
{
	RunAnimationWithCallback( this, animName, new KeyValues( "FinishDialogClose" ) );
}

//-----------------------------------------------------------------------------
// Purpose: cleans up after a menu closes
//-----------------------------------------------------------------------------
void CBaseModPanel::FinishDialogClose( void )
{
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
IVTFTexture *LoadVTF( CUtlBuffer &temp, const char *szFileName )
{
	if ( !g_pFullFileSystem->ReadFile( szFileName, NULL, temp ) )
		return NULL;

	IVTFTexture *texture = CreateVTFTexture();
	if ( !texture->Unserialize( temp ) )
	{
		Error( "Invalid or corrupt background texture %s\n", szFileName );
		return NULL;
	}
	texture->ConvertImageFormat( IMAGE_FORMAT_RGBA8888, false );
	return texture;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseModPanel::PrepareStartupGraphic()
{
	CUtlBuffer buf;

	// load in the background vtf
	buf.Clear();

	m_pBackgroundTexture = LoadVTF( buf, m_szFadeFilename );

	if ( !m_pBackgroundTexture )
	{
		Error( "Can't find background image '%s'\n", m_szFadeFilename );
		return;
	}

	// Allocate a white material
	m_pVMTKeyValues = new KeyValues( "UnlitGeneric" );
	m_pVMTKeyValues->SetString( "$basetexture", m_szFadeFilename + 10 );
	m_pVMTKeyValues->SetInt( "$ignorez", 1 );
	m_pVMTKeyValues->SetInt( "$nofog", 1 );
	m_pVMTKeyValues->SetInt( "$no_fullbright", 1 );
	m_pVMTKeyValues->SetInt( "$nocull", 1 );
	m_pVMTKeyValues->SetInt( "$vertexalpha", 1 );
	m_pVMTKeyValues->SetInt( "$vertexcolor", 1 );
	m_pBackgroundMaterial = g_pMaterialSystem->CreateMaterial( "__background", m_pVMTKeyValues );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseModPanel::ReleaseStartupGraphic()
{
	if ( m_pBackgroundMaterial )
		m_pBackgroundMaterial->Release();

	if ( m_pBackgroundTexture )
	{
		DestroyVTFTexture( m_pBackgroundTexture );
		m_pBackgroundTexture = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: we have to draw the startup fade graphic using this function so it perfectly matches the one drawn by the engine during load
//-----------------------------------------------------------------------------
void DrawScreenSpaceRectangleAlpha( IMaterial *pMaterial, 
							  int nDestX, int nDestY, int nWidth, int nHeight,	// Rect to draw into in screen space
							  float flSrcTextureX0, float flSrcTextureY0,		// which texel you want to appear at destx/y
							  float flSrcTextureX1, float flSrcTextureY1,		// which texel you want to appear at destx+width-1, desty+height-1
							  int nSrcTextureWidth, int nSrcTextureHeight,		// needed for fixup
							  void *pClientRenderable,							// Used to pass to the bind proxies
							  int nXDice, int nYDice,							// Amount to tessellate the mesh
							  float fDepth, float flAlpha )									// what Z value to put in the verts (def 0.0)
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );

	if ( ( nWidth <= 0 ) || ( nHeight <= 0 ) )
		return;

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PushMatrix();
	pRenderContext->LoadIdentity();

	pRenderContext->Bind( pMaterial, pClientRenderable );

	int xSegments = MAX( nXDice, 1);
	int ySegments = MAX( nYDice, 1);

	CMeshBuilder meshBuilder;

	IMesh* pMesh = pRenderContext->GetDynamicMesh( true );
	meshBuilder.Begin( pMesh, MATERIAL_QUADS, xSegments * ySegments );

	int nScreenWidth, nScreenHeight;
	pRenderContext->GetRenderTargetDimensions( nScreenWidth, nScreenHeight );
	float flLeftX = nDestX - 0.5f;
	float flRightX = nDestX + nWidth - 0.5f;

	float flTopY = nDestY - 0.5f;
	float flBottomY = nDestY + nHeight - 0.5f;

	float flSubrectWidth = flSrcTextureX1 - flSrcTextureX0;
	float flSubrectHeight = flSrcTextureY1 - flSrcTextureY0;

	float flTexelsPerPixelX = ( nWidth > 1 ) ? flSubrectWidth / ( nWidth - 1 ) : 0.0f;
	float flTexelsPerPixelY = ( nHeight > 1 ) ? flSubrectHeight / ( nHeight - 1 ) : 0.0f;

	float flLeftU = flSrcTextureX0 + 0.5f - ( 0.5f * flTexelsPerPixelX );
	float flRightU = flSrcTextureX1 + 0.5f + ( 0.5f * flTexelsPerPixelX );
	float flTopV = flSrcTextureY0 + 0.5f - ( 0.5f * flTexelsPerPixelY );
	float flBottomV = flSrcTextureY1 + 0.5f + ( 0.5f * flTexelsPerPixelY );

	float flOOTexWidth = 1.0f / nSrcTextureWidth;
	float flOOTexHeight = 1.0f / nSrcTextureHeight;
	flLeftU *= flOOTexWidth;
	flRightU *= flOOTexWidth;
	flTopV *= flOOTexHeight;
	flBottomV *= flOOTexHeight;

	// Get the current viewport size
	int vx, vy, vw, vh;
	pRenderContext->GetViewport( vx, vy, vw, vh );

	// map from screen pixel coords to -1..1
	flRightX = FLerp( -1, 1, 0, vw, flRightX );
	flLeftX = FLerp( -1, 1, 0, vw, flLeftX );
	flTopY = FLerp( 1, -1, 0, vh ,flTopY );
	flBottomY = FLerp( 1, -1, 0, vh, flBottomY );

	// Dice the quad up...
	if ( xSegments > 1 || ySegments > 1 )
	{
		// Screen height and width of a subrect
		float flWidth  = (flRightX - flLeftX) / (float) xSegments;
		float flHeight = (flTopY - flBottomY) / (float) ySegments;

		// UV height and width of a subrect
		float flUWidth  = (flRightU - flLeftU) / (float) xSegments;
		float flVHeight = (flBottomV - flTopV) / (float) ySegments;

		for ( int x=0; x < xSegments; x++ )
		{
			for ( int y=0; y < ySegments; y++ )
			{
				// Top left
				meshBuilder.Position3f( flLeftX   + (float) x * flWidth, flTopY - (float) y * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) x * flUWidth, flTopV + (float) y * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Top right (x+1)
				meshBuilder.Position3f( flLeftX   + (float) (x+1) * flWidth, flTopY - (float) y * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) (x+1) * flUWidth, flTopV + (float) y * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Bottom right (x+1), (y+1)
				meshBuilder.Position3f( flLeftX   + (float) (x+1) * flWidth, flTopY - (float) (y+1) * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) (x+1) * flUWidth, flTopV + (float)(y+1) * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();

				// Bottom left (y+1)
				meshBuilder.Position3f( flLeftX   + (float) x * flWidth, flTopY - (float) (y+1) * flHeight, fDepth );
				meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
				meshBuilder.TexCoord2f( 0, flLeftU   + (float) x * flUWidth, flTopV + (float)(y+1) * flVHeight);
				meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
				meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
				meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
				meshBuilder.AdvanceVertex();
			}
		}
	}
	else // just one quad
	{
		for ( int corner=0; corner<4; corner++ )
		{
			bool bLeft = (corner==0) || (corner==3);
			meshBuilder.Position3f( (bLeft) ? flLeftX : flRightX, (corner & 2) ? flBottomY : flTopY, fDepth );
			meshBuilder.Normal3f( 0.0f, 0.0f, 1.0f );
			meshBuilder.TexCoord2f( 0, (bLeft) ? flLeftU : flRightU, (corner & 2) ? flBottomV : flTopV );
			meshBuilder.TangentS3f( 0.0f, 1.0f, 0.0f );
			meshBuilder.TangentT3f( 1.0f, 0.0f, 0.0f );
			meshBuilder.Color4ub( 255, 255, 255, 255.0f * flAlpha );
			meshBuilder.AdvanceVertex();
		}
	}

	meshBuilder.End();
	pMesh->Draw();

	pRenderContext->MatrixMode( MATERIAL_VIEW );
	pRenderContext->PopMatrix();

	pRenderContext->MatrixMode( MATERIAL_PROJECTION );
	pRenderContext->PopMatrix();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CBaseModPanel::DrawStartupGraphic( float flNormalizedAlpha )
{
	CMatRenderContextPtr pRenderContext( g_pMaterialSystem );
	int w = GetWide();
	int h = GetTall();
	int tw = m_pBackgroundTexture->Width();
	int th = m_pBackgroundTexture->Height();

	float depth = 0.5f;
	int width_at_ratio = h * (16.0f / 9.0f);
	int x = ( w * 0.5f ) - ( width_at_ratio * 0.5f );
	DrawScreenSpaceRectangleAlpha( m_pBackgroundMaterial, x, 0, width_at_ratio, h, 8, 8, tw-8, th-8, tw, th, NULL,1,1,depth,flNormalizedAlpha );
}


#ifdef _X360
//-----------------------------------------------------------------------------
// Purpose: Reload the resource files on the Xbox 360
//-----------------------------------------------------------------------------
void CBaseModPanel::Reload_Resources( const CCommand &args )
{
	m_pConsoleControlSettings->Clear();
	if ( m_pConsoleControlSettings->LoadFromFile( g_pFullFileSystem, "resource/UI/XboxDialogs.res" ) )
	{
		m_pConsoleControlSettings->ProcessResolutionKeys( surface()->GetResolutionKey() );
	}
}
#endif

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CBaseModPanel::CloseBaseDialogs( void )
{
	if ( m_hNewGameDialog.Get() )
		m_hNewGameDialog->Close();

	if ( m_hAchievementsDialog.Get() )
		m_hAchievementsDialog->Close();
	
	if ( m_hBonusMapsDialog.Get() )
		m_hBonusMapsDialog->Close();
	
	if ( m_hControllerDialog.Get() )
		m_hControllerDialog->Close();

	if ( m_hLoadGameDialog_Xbox.Get() )
		m_hLoadGameDialog_Xbox->Close();

	if ( m_hOptionsDialog_Xbox.Get() )
		m_hOptionsDialog_Xbox->Close();

	if ( m_hSaveGameDialog_Xbox.Get() )
		m_hSaveGameDialog_Xbox->Close();

	if ( m_hLoadCommentaryDialog.Get() )
		m_hLoadCommentaryDialog->Close();

	if ( m_hCreateMultiplayerGameDialog.Get() )
		m_hCreateMultiplayerGameDialog->Close();
}

//-----------------------------------------------------------------------------
// Purpose: xbox UI panel that displays button icons and help text for all menus
//-----------------------------------------------------------------------------
CFooterPanel::CFooterPanel( Panel *parent, const char *panelName ) : BaseClass( parent, panelName ) 
{
	SetVisible( true );
	SetAlpha( 0 );
	m_pHelpName = NULL;

	m_pSizingLabel = new vgui::Label( this, "SizingLabel", "" );
	m_pSizingLabel->SetVisible( false );

	m_nButtonGap = 32;
	m_nButtonGapDefault = 32;
	m_ButtonPinRight = 100;
	m_FooterTall = 80;

	int wide, tall;
	surface()->GetScreenSize(wide, tall);

	if ( tall <= 480 )
	{
		m_FooterTall = 60;
	}

	m_ButtonOffsetFromTop = 0;
	m_ButtonSeparator = 4;
	m_TextAdjust = 0;

	m_bPaintBackground = false;
	m_bCenterHorizontal = false;

	m_szButtonFont[0] = '\0';
	m_szTextFont[0] = '\0';
	m_szFGColor[0] = '\0';
	m_szBGColor[0] = '\0';
}

CFooterPanel::~CFooterPanel()
{
	SetHelpNameAndReset( NULL );

	delete m_pSizingLabel;
}

//-----------------------------------------------------------------------------
// Purpose: apply scheme settings
//-----------------------------------------------------------------------------
void CFooterPanel::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	m_hButtonFont = pScheme->GetFont( ( m_szButtonFont[0] != '\0' ) ? m_szButtonFont : "GameUIButtons" );
	m_hTextFont = pScheme->GetFont( ( m_szTextFont[0] != '\0' ) ? m_szTextFont : "MenuLarge" );

	SetFgColor( pScheme->GetColor( m_szFGColor, Color( 255, 255, 255, 255 ) ) );
	SetBgColor( pScheme->GetColor( m_szBGColor, Color( 0, 0, 0, 255 ) ) );

	int x, y, w, h;
	GetParent()->GetBounds( x, y, w, h );
	SetBounds( x, h - m_FooterTall, w, m_FooterTall );
}

//-----------------------------------------------------------------------------
// Purpose: apply settings
//-----------------------------------------------------------------------------
void CFooterPanel::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	// gap between hints
	m_nButtonGap = inResourceData->GetInt( "buttongap", 32 );
	m_nButtonGapDefault = m_nButtonGap;
	m_ButtonPinRight = inResourceData->GetInt( "button_pin_right", 100 );
	m_FooterTall = inResourceData->GetInt( "tall", 80 );
	m_ButtonOffsetFromTop = inResourceData->GetInt( "buttonoffsety", 0 );
	m_ButtonSeparator = inResourceData->GetInt( "button_separator", 4 );
	m_TextAdjust = inResourceData->GetInt( "textadjust", 0 );

	m_bCenterHorizontal = ( inResourceData->GetInt( "center", 0 ) == 1 );
	m_bPaintBackground = ( inResourceData->GetInt( "paintbackground", 0 ) == 1 );

	// fonts for text and button
	Q_strncpy( m_szTextFont, inResourceData->GetString( "fonttext", "MenuLarge" ), sizeof( m_szTextFont ) );
	Q_strncpy( m_szButtonFont, inResourceData->GetString( "fontbutton", "GameUIButtons" ), sizeof( m_szButtonFont ) );

	// fg and bg colors
	Q_strncpy( m_szFGColor, inResourceData->GetString( "fgcolor", "White" ), sizeof( m_szFGColor ) );
	Q_strncpy( m_szBGColor, inResourceData->GetString( "bgcolor", "Black" ), sizeof( m_szBGColor ) );

	for ( KeyValues *pButton = inResourceData->GetFirstSubKey(); pButton != NULL; pButton = pButton->GetNextKey() )
	{
		const char *pName = pButton->GetName();

		if ( !Q_stricmp( pName, "button" ) )
		{
			// Add a button to the footer
			const char *pText = pButton->GetString( "text", "NULL" );
			const char *pIcon = pButton->GetString( "icon", "NULL" );
			AddNewButtonLabel( pText, pIcon );
		}
	}

	InvalidateLayout( false, true ); // force ApplySchemeSettings to run
}

//-----------------------------------------------------------------------------
// Purpose: adds button icons and help text to the footer panel when activating a menu
//-----------------------------------------------------------------------------
void CFooterPanel::AddButtonsFromMap( vgui::Frame *pMenu )
{
	SetHelpNameAndReset( pMenu->GetName() );

	CControllerMap *pMap = dynamic_cast<CControllerMap*>( pMenu->FindChildByName( "ControllerMap" ) );
	if ( pMap )
	{
		int buttonCt = pMap->NumButtons();
		for ( int i = 0; i < buttonCt; ++i )
		{
			const char *pText = pMap->GetBindingText( i );
			if ( pText )
			{
				AddNewButtonLabel( pText, pMap->GetBindingIcon( i ) );
			}
		}
	}
}

void CFooterPanel::SetStandardDialogButtons()
{
	SetHelpNameAndReset( "Dialog" );
	AddNewButtonLabel( "#GameUI_Action", "#GameUI_Icons_A_BUTTON" );
	AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
}

//-----------------------------------------------------------------------------
// Purpose: Caller must tag the button layout. May support reserved names
// to provide stock help layouts trivially.
//-----------------------------------------------------------------------------
void CFooterPanel::SetHelpNameAndReset( const char *pName )
{
	if ( m_pHelpName )
	{
		free( m_pHelpName );
		m_pHelpName = NULL;
	}

	if ( pName )
	{
		m_pHelpName = strdup( pName );
	}

	ClearButtons();
}

//-----------------------------------------------------------------------------
// Purpose: Caller must tag the button layout
//-----------------------------------------------------------------------------
const char *CFooterPanel::GetHelpName()
{
	return m_pHelpName;
}

void CFooterPanel::ClearButtons( void )
{
	m_ButtonLabels.PurgeAndDeleteElements();
}

//-----------------------------------------------------------------------------
// Purpose: creates a new button label with icon and text
//-----------------------------------------------------------------------------
void CFooterPanel::AddNewButtonLabel( const char *text, const char *icon )
{
	ButtonLabel_t *button = new ButtonLabel_t;

	Q_strncpy( button->name, text, MAX_PATH );
	button->bVisible = true;

	// Button icons are a single character
	wchar_t *pIcon = g_pVGuiLocalize->Find( icon );
	if ( pIcon )
	{
		button->icon[0] = pIcon[0];
		button->icon[1] = '\0';
	}
	else
	{
		button->icon[0] = '\0';
	}

	// Set the help text
	wchar_t *pText = g_pVGuiLocalize->Find( text );
	if ( pText )
	{
		wcsncpy( button->text, pText, wcslen( pText ) + 1 );
	}
	else
	{
		button->text[0] = '\0';
	}

	m_ButtonLabels.AddToTail( button );
}

//-----------------------------------------------------------------------------
// Purpose: Shows/Hides a button label
//-----------------------------------------------------------------------------
void CFooterPanel::ShowButtonLabel( const char *name, bool show )
{
	for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
	{
		if ( !Q_stricmp( m_ButtonLabels[ i ]->name, name ) )
		{
			m_ButtonLabels[ i ]->bVisible = show;
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Changes a button's text
//-----------------------------------------------------------------------------
void CFooterPanel::SetButtonText( const char *buttonName, const char *text )
{
	for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
	{
		if ( !Q_stricmp( m_ButtonLabels[ i ]->name, buttonName ) )
		{
			wchar_t *wtext = g_pVGuiLocalize->Find( text );
			if ( text )
			{
				wcsncpy( m_ButtonLabels[ i ]->text, wtext, wcslen( wtext ) + 1 );
			}
			else
			{
				m_ButtonLabels[ i ]->text[ 0 ] = '\0';
			}
			break;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Footer panel background rendering
//-----------------------------------------------------------------------------
void CFooterPanel::PaintBackground( void )
{
	if ( !m_bPaintBackground )
		return;

	BaseClass::PaintBackground();
}

//-----------------------------------------------------------------------------
// Purpose: Footer panel rendering
//-----------------------------------------------------------------------------
void CFooterPanel::Paint( void )
{
	// inset from right edge
	int wide = GetWide();
	int right = wide - m_ButtonPinRight;

	// center the text within the button
	int buttonHeight = vgui::surface()->GetFontTall( m_hButtonFont );
	int fontHeight = vgui::surface()->GetFontTall( m_hTextFont );
	int textY = ( buttonHeight - fontHeight )/2 + m_TextAdjust;

	if ( textY < 0 )
	{
		textY = 0;
	}

	int y = m_ButtonOffsetFromTop;

	if ( !m_bCenterHorizontal )
	{
		// draw the buttons, right to left
		int x = right;

		for ( int i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			int iTextWidth = m_pSizingLabel->GetWide();

			if ( iTextWidth == 0 )
				x += m_nButtonGap;	// There's no text, so remove the gap between buttons
			else
				x -= iTextWidth;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );

			// Draw the button
			// back up button width and a little extra to leave a gap between button and text
			x -= ( vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator );
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );

			// back up to next string
			x -= m_nButtonGap;
		}
	}
	else
	{
		// center the buttons (as a group)
		int x = wide / 2;
		int totalWidth = 0;
		int i = 0;
		int nButtonCount = 0;

		// need to loop through and figure out how wide our buttons and text are (with gaps between) so we can offset from the center
		for ( i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			totalWidth += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] );
			totalWidth += m_ButtonSeparator;
			totalWidth += m_pSizingLabel->GetWide();

			nButtonCount++; // keep track of how many active buttons we'll be drawing
		}

		totalWidth += ( nButtonCount - 1 ) * m_nButtonGap; // add in the gaps between the buttons
		x -= ( totalWidth / 2 );

		for ( i = 0; i < m_ButtonLabels.Count(); ++i )
		{
			ButtonLabel_t *pButton = m_ButtonLabels[i];
			if ( !pButton->bVisible )
				continue;

			// Get the string length
			m_pSizingLabel->SetFont( m_hTextFont );
			m_pSizingLabel->SetText( pButton->text );
			m_pSizingLabel->SizeToContents();

			int iTextWidth = m_pSizingLabel->GetWide();

			// Draw the icon
			vgui::surface()->DrawSetTextFont( m_hButtonFont );
			vgui::surface()->DrawSetTextColor( 255, 255, 255, 255 );
			vgui::surface()->DrawSetTextPos( x, y );
			vgui::surface()->DrawPrintText( pButton->icon, 1 );
			x += vgui::surface()->GetCharacterWidth( m_hButtonFont, pButton->icon[0] ) + m_ButtonSeparator;

			// Draw the string
			vgui::surface()->DrawSetTextFont( m_hTextFont );
			vgui::surface()->DrawSetTextColor( GetFgColor() );
			vgui::surface()->DrawSetTextPos( x, y + textY );
			vgui::surface()->DrawPrintText( pButton->text, wcslen( pButton->text ) );
			
			x += iTextWidth + m_nButtonGap;
		}
	}
}	

DECLARE_BUILD_FACTORY( CFooterPanel );

// X360TBD: Move into a separate module when completed
CMessageDialogHandler::CMessageDialogHandler()
{
	m_iDialogStackTop = -1;
}

void CMessageDialogHandler::ShowMessageDialog( int nType, vgui::Panel *pOwner )
{
	int iSimpleFrame = 0;
	if ( ModInfo().IsSinglePlayerOnly() )
	{
		iSimpleFrame = MD_SIMPLEFRAME;
	}

	switch( nType )
	{
	case MD_SEARCHING_FOR_GAMES:
		CreateMessageDialog( MD_CANCEL|MD_RESTRICTPAINT,
							NULL, 
							"#TF_Dlg_SearchingForGames", 
							NULL,
							"CancelOperation",
							pOwner,
							true ); 
		break;

	case MD_CREATING_GAME:
		CreateMessageDialog( MD_RESTRICTPAINT,
							NULL, 
							"#TF_Dlg_CreatingGame", 
							NULL,
							NULL,
							pOwner,
							true ); 
		break;

	case MD_SESSION_SEARCH_FAILED:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_NoGamesFound", 
							"ShowSessionOptionsDialog",
							"ReturnToMainMenu",
							pOwner ); 
		break;

	case MD_SESSION_CREATE_FAILED:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_CreateFailed", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECTING:
		CreateMessageDialog( 0, 
							NULL, 
							"#TF_Dlg_Connecting", 
							NULL, 
							NULL,
							pOwner,
							true );
		break;

	case MD_SESSION_CONNECT_NOTAVAILABLE:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_JoinRefused", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECT_SESSIONFULL:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_GameFull", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_SESSION_CONNECT_FAILED:
		CreateMessageDialog( MD_OK, 
							NULL, 
							"#TF_Dlg_JoinFailed", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_LOST_HOST:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_LostHost", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_LOST_SERVER:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_LostServer", 
							"ReturnToMainMenu", 
							NULL,
							pOwner );
		break;

	case MD_MODIFYING_SESSION:
		CreateMessageDialog( MD_RESTRICTPAINT, 
							NULL, 
							"#TF_Dlg_ModifyingSession", 
							NULL, 
							NULL,
							pOwner,
							true );
		break;

	case MD_SAVE_BEFORE_QUIT:
		CreateMessageDialog( MD_YESNO|iSimpleFrame|MD_RESTRICTPAINT, 
							"#GameUI_QuitConfirmationTitle", 
							"#GameUI_Console_QuitWarning", 
							"QuitNoConfirm", 
							"CloseQuitDialog_OpenMainMenu",
							pOwner );
		break;

	case MD_QUIT_CONFIRMATION:
		CreateMessageDialog( MD_YESNO|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_QuitConfirmationTitle", 
							 "#GameUI_QuitConfirmationText", 
							 "QuitNoConfirm", 
							 "CloseQuitDialog_OpenMainMenu",
							 pOwner );
		break;

	case MD_QUIT_CONFIRMATION_TF:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							 "#GameUI_QuitConfirmationTitle", 
							 "#GameUI_QuitConfirmationText", 
							 "QuitNoConfirm", 
							 "CloseQuitDialog_OpenMatchmakingMenu",
							 pOwner );
		break;

	case MD_DISCONNECT_CONFIRMATION:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							"", 
							"#GameUI_DisconnectConfirmationText", 
							"DisconnectNoConfirm", 
							"close_dialog",
							pOwner );
		break;

	case MD_DISCONNECT_CONFIRMATION_HOST:
		CreateMessageDialog( MD_YESNO|MD_RESTRICTPAINT, 
							"", 
							"#GameUI_DisconnectHostConfirmationText", 
							"DisconnectNoConfirm", 
							"close_dialog",
							pOwner );
		break;

	case MD_KICK_CONFIRMATION:
		CreateMessageDialog( MD_YESNO, 
							"", 
							"#TF_Dlg_ConfirmKick", 
							"KickPlayer", 
							"close_dialog",
							pOwner );
		break;

	case MD_CLIENT_KICKED:
		CreateMessageDialog( MD_OK|MD_RESTRICTPAINT, 
							"", 
							"#TF_Dlg_ClientKicked", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_EXIT_SESSION_CONFIRMATION:
		CreateMessageDialog( MD_YESNO, 
							"", 
							"#TF_Dlg_ExitSessionText", 
							"ReturnToMainMenu", 
							"close_dialog",
							pOwner );
		break;

	case MD_STORAGE_DEVICES_NEEDED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_StorageRemovedTitle", 
							 "#GameUI_Console_StorageNeededBody", 
							 "ShowDeviceSelector", 
							 "QuitNoConfirm",
							 pOwner );
		break;

	case MD_STORAGE_DEVICES_CHANGED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							"#GameUI_Console_StorageRemovedTitle", 
							"#GameUI_Console_StorageRemovedBody", 
							"ShowDeviceSelector", 
							"clear_storage_deviceID",
							pOwner );
		break;

	case MD_STORAGE_DEVICES_TOO_FULL:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_StorageTooFullTitle", 
							 "#GameUI_Console_StorageTooFullBody", 
							 "ShowDeviceSelector", 
							 "StorageDeviceDenied",
							 pOwner );
		break;

	case MD_PROMPT_STORAGE_DEVICE:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_Console_NoStorageDeviceSelectedTitle", 
							 "#GameUI_Console_NoStorageDeviceSelectedBody", 
							 "ShowDeviceSelector", 
							 "StorageDeviceDenied",
							 pOwner );
		break;

	case MD_PROMPT_STORAGE_DEVICE_REQUIRED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|MD_SIMPLEFRAME, 
							"#GameUI_Console_NoStorageDeviceSelectedTitle", 
							"#GameUI_Console_StorageDeviceRequiredBody", 
							"ShowDeviceSelector", 
							"RequiredStorageDenied",
							pOwner );
		break;

	case MD_PROMPT_SIGNIN:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame, 
							 "#GameUI_Console_NoUserProfileSelectedTitle", 
							 "#GameUI_Console_NoUserProfileSelectedBody", 
							 "ShowSignInUI", 
							 "SignInDenied",
							 pOwner );
		break;

	case MD_PROMPT_SIGNIN_REQUIRED:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_COMMANDAFTERCLOSE|iSimpleFrame, 
							"#GameUI_Console_NoUserProfileSelectedTitle", 
							"#GameUI_Console_UserProfileRequiredBody", 
							"ShowSignInUI", 
							"RequiredSignInDenied",
							pOwner );
		break;

	case MD_NOT_ONLINE_ENABLED:
		CreateMessageDialog( MD_YESNO|MD_WARNING, 
							"", 
							"#TF_Dlg_NotOnlineEnabled", 
							"ShowSigninUI", 
							"close_dialog",
							pOwner );
		break;

	case MD_NOT_ONLINE_SIGNEDIN:
		CreateMessageDialog( MD_YESNO|MD_WARNING, 
							"", 
							"#TF_Dlg_NotOnlineSignedIn", 
							"ShowSigninUI", 
							"close_dialog",
							pOwner );
		break;

	case MD_DEFAULT_CONTROLS_CONFIRM:
		CreateMessageDialog( MD_YESNO|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_RestoreDefaults", 
							 "#GameUI_ControllerSettingsText", 
							 "DefaultControls", 
							 "close_dialog",
							 pOwner );
		break;

	case MD_AUTOSAVE_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmNewGame_Title", 
							 "#GameUI_AutoSave_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GAMEUI_Commentary_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_EXPLANATION_MULTI:
		CreateMessageDialog( MD_OK|MD_WARNING, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GAMEUI_Commentary_Console_Explanation", 
							 "StartNewGameNoCommentaryExplanation", 
							 NULL,
							 pOwner );
		break;

	case MD_COMMENTARY_CHAPTER_UNLOCK_EXPLANATION:
		CreateMessageDialog( MD_OK|MD_WARNING|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_CommentaryDialogTitle", 
							 "#GameUI_CommentaryUnlock", 
							 "close_dialog", 
							 NULL,
							 pOwner );
		break;
		
	case MD_SAVE_BEFORE_LANGUAGE_CHANGE:
		CreateMessageDialog( MD_YESNO|MD_WARNING|MD_SIMPLEFRAME|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ChangeLanguageRestart_Title", 
							 "#GameUI_ChangeLanguageRestart_Info", 
							 "AcceptVocalsLanguageChange", 
							 "CancelVocalsLanguageChange",
							 pOwner );

	case MD_SAVE_BEFORE_NEW_GAME:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmNewGame_Title", 
							 "#GameUI_NewGameWarning", 
							 "StartNewGame", 
							 "close_dialog",
							 pOwner );
		break;

	case MD_SAVE_BEFORE_LOAD:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE|MD_RESTRICTPAINT, 
							 "#GameUI_ConfirmLoadGame_Title", 
							 "#GameUI_LoadWarning", 
							 "LoadGame", 
							 "LoadGameCancelled",
							 pOwner );
		break;

	case MD_DELETE_SAVE_CONFIRM:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmDeleteSaveGame_Title", 
							 "#GameUI_ConfirmDeleteSaveGame_Info", 
							 "DeleteGame", 
							 "DeleteGameCancelled",
							 pOwner );
		break;

	case MD_SAVE_OVERWRITE:
		CreateMessageDialog( MD_OKCANCEL|MD_WARNING|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmOverwriteSaveGame_Title", 
							 "#GameUI_ConfirmOverwriteSaveGame_Info", 
							 "SaveGame", 
							 "OverwriteGameCancelled",
							 pOwner );
		break;

	case MD_SAVING_WARNING:
		CreateMessageDialog( MD_WARNING|iSimpleFrame|MD_COMMANDONFORCECLOSE, 
							 "",
							 "#GameUI_SavingWarning", 
							 "SaveSuccess", 
							 NULL,
							 pOwner,
							 true);
		break;

	case MD_SAVE_COMPLETE:
		CreateMessageDialog( MD_OK|iSimpleFrame|MD_COMMANDAFTERCLOSE, 
							 "#GameUI_ConfirmOverwriteSaveGame_Title", 
							 "#GameUI_GameSaved", 
							 "CloseAndSelectResume", 
							 NULL,
							 pOwner );
		break;

	case MD_LOAD_FAILED_WARNING:
		CreateMessageDialog( MD_OK |MD_WARNING|iSimpleFrame, 
			"#GameUI_LoadFailed", 
			"#GameUI_LoadFailed_Description", 
			"close_dialog", 
			NULL,
			pOwner );
		break;

	case MD_OPTION_CHANGE_FROM_X360_DASHBOARD:
		CreateMessageDialog( MD_OK|iSimpleFrame|MD_RESTRICTPAINT, 
							 "#GameUI_SettingChangeFromX360Dashboard_Title", 
							 "#GameUI_SettingChangeFromX360Dashboard_Info", 
							 "close_dialog", 
							 NULL,
							 pOwner );
		break;

	case MD_STANDARD_SAMPLE:
		CreateMessageDialog( MD_OK, 
							"Standard Dialog", 
							"This is a standard dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_WARNING_SAMPLE:
		CreateMessageDialog( MD_OK | MD_WARNING,
							"#GameUI_Dialog_Warning", 
							"This is a warning dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_ERROR_SAMPLE:
		CreateMessageDialog( MD_OK | MD_ERROR, 
							"Error Dialog", 
							"This is an error dialog", 
							"close_dialog", 
							NULL,
							pOwner );
		break;

	case MD_STORAGE_DEVICES_CORRUPT:
		CreateMessageDialog( MD_OK | MD_WARNING | iSimpleFrame | MD_RESTRICTPAINT,
			"", 
			"#GameUI_Console_FileCorrupt", 
			"close_dialog", 
			NULL,
			pOwner );
		break;

	case MD_CHECKING_STORAGE_DEVICE:
		CreateMessageDialog( iSimpleFrame | MD_RESTRICTPAINT,
			NULL, 
			"#GameUI_Dlg_CheckingStorageDevice",
			NULL,
			NULL,
			pOwner,
			true ); 
		break;

	default:
		break;
	}
}

void CMessageDialogHandler::CloseAllMessageDialogs()
{
	for ( int i = 0; i < MAX_MESSAGE_DIALOGS; ++i )
	{
		CMessageDialog *pDlg = m_hMessageDialogs[i];
		if ( pDlg )
		{
			vgui::surface()->RestrictPaintToSinglePanel(NULL);
			if ( vgui_message_dialog_modal.GetBool() )
			{
				vgui::input()->ReleaseAppModalSurface();
			}

			pDlg->Close();
			m_hMessageDialogs[i] = NULL;
		}
	}
}

void CMessageDialogHandler::CloseMessageDialog( const uint nType )
{
	int nStackIdx = 0;
	if ( nType & MD_WARNING )
	{
		nStackIdx = DIALOG_STACK_IDX_WARNING;
	}
	else if ( nType & MD_ERROR )
	{
		nStackIdx = DIALOG_STACK_IDX_ERROR;
	}

	CMessageDialog *pDlg = m_hMessageDialogs[nStackIdx];
	if ( pDlg )
	{
		vgui::surface()->RestrictPaintToSinglePanel(NULL);
		if ( vgui_message_dialog_modal.GetBool() )
		{
			vgui::input()->ReleaseAppModalSurface();
		}

		pDlg->Close();
		m_hMessageDialogs[nStackIdx] = NULL;
	}
}

void CMessageDialogHandler::CreateMessageDialog( const uint nType, const char *pTitle, const char *pMsg, const char *pCmdA, const char *pCmdB, vgui::Panel *pCreator, bool bShowActivity /*= false*/ )
{
	int nStackIdx = 0;
	if ( nType & MD_WARNING )
	{
		nStackIdx = DIALOG_STACK_IDX_WARNING;
	}
	else if ( nType & MD_ERROR )
	{
		nStackIdx = DIALOG_STACK_IDX_ERROR;
	}

	// Can only show one dialog of each type at a time
	if ( m_hMessageDialogs[nStackIdx].Get() )
	{
		Warning( "Tried to create two dialogs of type %d\n", nStackIdx );
		return;
	}

	// Show the new dialog
	m_hMessageDialogs[nStackIdx] = new CMessageDialog( BasePanel(), nType, pTitle, pMsg, pCmdA, pCmdB, pCreator, bShowActivity );

	m_hMessageDialogs[nStackIdx]->SetControlSettingsKeys( BasePanel()->GetConsoleControlSettings()->FindKey( "MessageDialog.res" ) );

	if ( nType & MD_RESTRICTPAINT )
	{
		vgui::surface()->RestrictPaintToSinglePanel( m_hMessageDialogs[nStackIdx]->GetVPanel() );
	}

	ActivateMessageDialog( nStackIdx );	
}

//-----------------------------------------------------------------------------
// Purpose: Activate a new message dialog
//-----------------------------------------------------------------------------
void CMessageDialogHandler::ActivateMessageDialog( int nStackIdx )
{
	int x, y, wide, tall;
	vgui::surface()->GetWorkspaceBounds( x, y, wide, tall );
	PositionDialog( m_hMessageDialogs[nStackIdx], wide, tall );

	uint nType = m_hMessageDialogs[nStackIdx]->GetType();
	if ( nType & MD_WARNING )
	{
		m_hMessageDialogs[nStackIdx]->SetZPos( 75 );
	}
	else if ( nType & MD_ERROR )
	{
		m_hMessageDialogs[nStackIdx]->SetZPos( 100 );
	}

	// Make sure the topmost item on the stack still has focus
	int idx = MAX_MESSAGE_DIALOGS - 1;
	for ( idx; idx >= nStackIdx; --idx )
	{
		CMessageDialog *pDialog = m_hMessageDialogs[idx];
		if ( pDialog )
		{
			pDialog->Activate();
			if ( vgui_message_dialog_modal.GetBool() )
			{
				vgui::input()->SetAppModalSurface( pDialog->GetVPanel() );
			}
			m_iDialogStackTop = idx;
			break;
		}
	}
}

void CMessageDialogHandler::PositionDialogs( int wide, int tall )
{
	for ( int i = 0; i < MAX_MESSAGE_DIALOGS; ++i )
	{
		if ( m_hMessageDialogs[i].Get() )
		{
			PositionDialog( m_hMessageDialogs[i], wide, tall );
		}
	}
}

void CMessageDialogHandler::PositionDialog( vgui::PHandle dlg, int wide, int tall )
{
	int w, t;
	dlg->GetSize(w, t);
	dlg->SetPos( (wide - w) / 2, (tall - t) / 2 );
}	

//-----------------------------------------------------------------------------
// Purpose: Main Menu Buttom
//-----------------------------------------------------------------------------
CGameMenuItem::CGameMenuItem(vgui::Menu *parent, const char *name)  : BaseClass(parent, name, "GameMenuItem") 
{
	m_bRightAligned = false;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenuItem::ApplySchemeSettings( IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings(pScheme);

	// make fully transparent
	SetFgColor(GetSchemeColor("MainMenu.TextColor", pScheme));
	SetBgColor(Color(0, 0, 0, 0));
	SetDefaultColor(GetSchemeColor("MainMenu.TextColor", pScheme), Color(0, 0, 0, 0));
	SetArmedColor(GetSchemeColor("MainMenu.ArmedTextColor", pScheme), Color(0, 0, 0, 0));
	SetDepressedColor(GetSchemeColor("MainMenu.DepressedTextColor", pScheme), Color(0, 0, 0, 0));
	SetContentAlignment(Label::a_west);
	SetBorder(NULL);
	SetDefaultBorder(NULL);
	SetDepressedBorder(NULL);
	SetKeyFocusBorder(NULL);

	hMainMenuFont = pScheme->GetFont( "MainMenuFont", IsProportional() );
	if ( hMainMenuFont )
		SetFont( hMainMenuFont );
	else
		SetFont( pScheme->GetFont( "MenuLarge", IsProportional() ) );

	SetTextInset(0, 0);
	SetArmedSound("UI/buttonrollover.wav");
	SetDepressedSound("UI/buttonclick.wav");
	SetReleasedSound("UI/buttonclickrelease.wav");
	SetButtonActivationType(Button::ACTIVATE_ONPRESSED);

	if ( GameUI().IsConsoleUI() )
	{
		SetArmedColor(GetSchemeColor("MainMenu.ArmedTextColor", pScheme), GetSchemeColor("Button.ArmedBgColor", pScheme));
		SetTextInset( MAIN_MENU_INDENT_X360, 0 );
	}

	if (m_bRightAligned)
	{
		SetContentAlignment(Label::a_east);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenuItem::PaintBackground()
{
	if ( !IsArmed() || !IsVisible() || GetParent()->GetAlpha() < 32 )
		return;

	int wide, tall;
	GetSize( wide, tall );

	DrawBoxFade( 0, 0, wide, tall, GetButtonBgColor(), 1.0f, 255, 0, true );
	DrawBoxFade( 2, 2, wide - 4, tall - 4, Color( 0, 0, 0, 96 ), 1.0f, 255, 0, true );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenuItem::SetRightAlignedText( bool state )
{
	m_bRightAligned = state;
}

//-----------------------------------------------------------------------------
// Purpose: General purpose 1 of N menu
//-----------------------------------------------------------------------------
CGameMenu::CGameMenu( vgui::Panel *parent, const char *name ) : BaseClass( parent, name ) 
{
	SetZPos( 10 );

	if ( GameUI().IsConsoleUI() )
	{
		// shows graphic button hints
		m_pConsoleFooter = new CFooterPanel( parent, "MainMenuFooter" );

		int iFixedWidth = 245;
		SetFixedWidth( iFixedWidth );
	}
	else
	{
		m_pConsoleFooter = NULL;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	// make fully transparent
	SetMenuItemHeight( atoi( pScheme->GetResourceString( "MainMenu.MenuItemHeight" ) ) );
	SetBgColor( Color( 0, 0, 0, 0 ) );
	SetBorder( NULL );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::LayoutMenuBorder()
{
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::SetVisible( bool state )
{
	// force to be always visible
	BaseClass::SetVisible( true );

	// move us to the back instead of going invisible
	if ( !state )
		ipanel()->MoveToBack( GetVPanel() );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CGameMenu::AddMenuItem( const char *itemName, const char *itemText, const char *command, Panel *target, KeyValues *userData )
{
	MenuItem *item = new CGameMenuItem( this, itemName );
	item->AddActionSignalTarget( target );
	item->SetCommand( command );
	item->SetText( itemText );
	item->SetUserData( userData );

	return BaseClass::AddMenuItem( item );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CGameMenu::AddMenuItem( const char *itemName, const char *itemText, KeyValues *command, Panel *target, KeyValues *userData )
{
	CGameMenuItem *item = new CGameMenuItem( this, itemName );
	item->AddActionSignalTarget( target );
	item->SetCommand( command );
	item->SetText( itemText );
	item->SetRightAlignedText( true );
	item->SetUserData( userData );

	return BaseClass::AddMenuItem( item );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::SetMenuItemBlinkingState( const char *itemName, bool state )
{
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *child = GetChild(i);
		MenuItem *menuItem = dynamic_cast<MenuItem *>(child);
		if ( menuItem )
		{
			if ( Q_strcmp( menuItem->GetCommand()->GetString( "command", "" ), itemName ) == 0 )
				menuItem->SetBlink( state );
		}
	}

	InvalidateLayout();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnCommand( const char *command )
{
	if ( !stricmp( command, "Open" ) )
	{
		MoveToFront();
		RequestFocus();
	}
	else
	{
		BaseClass::OnCommand( command );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnKeyCodePressed( vgui::KeyCode code )
{
	if ( IsX360() )
	{
		if ( GetAlpha() != 255 )
		{
			SetEnabled( false );
			// inhibit key activity during transitions
			return;
		}

		SetEnabled( true );

		if ( code == KEY_XBUTTON_B || code == KEY_XBUTTON_START )
		{
			if ( GameUI().IsInLevel() )
			{
				GetParent()->OnCommand( "ResumeGame" );
			}
			return;
		}
	}

	BaseClass::OnKeyCodePressed( code );

	// HACK: Allow F key bindings to operate even here
	if ( IsPC() && code >= KEY_F1 && code <= KEY_F12 )
	{
		// See if there is a binding for the FKey
		const char *binding = gameuifuncs->GetBindingForButtonCode( code );
		if ( binding && binding[0] )
		{
			// submit the entry as a console commmand
			char szCommand[256];
			Q_strncpy( szCommand, binding, sizeof( szCommand ) );
			engine->ClientCmd_Unrestricted( szCommand );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnKeyCodeReleased( vgui::KeyCode code )
{
	BaseClass::OnKeyCodeReleased( code );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnThink()
{
	BaseClass::OnThink();
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::OnKillFocus()
{
	BaseClass::OnKillFocus();

	// force us to the rear when we lose focus (so it looks like the menu is always on the background)
	surface()->MovePopupToBack(GetVPanel());
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::ShowFooter( bool bShow )
{
	if ( m_pConsoleFooter )
		m_pConsoleFooter->SetVisible( bShow );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CGameMenu::UpdateMenuItemState( bool isInGame, bool isMultiplayer )
{
	bool isSteam = IsPC() && ( CommandLine()->FindParm( "-steam" ) != 0 );
	bool bIsConsoleUI = GameUI().IsConsoleUI();

	// disabled save button if we're not in a game
	for ( int i = 0; i < GetChildCount(); i++ )
	{
		Panel *child = GetChild(i);
		MenuItem *menuItem = dynamic_cast<MenuItem *>( child );
		if ( menuItem )
		{
			bool shouldBeVisible = true;
			// filter the visibility
			KeyValues *kv = menuItem->GetUserData();
			if (!kv)
				continue;

			if ( !isInGame && kv->GetInt( "OnlyInGame" ) )
				shouldBeVisible = false;
			else if ( isMultiplayer && kv->GetInt( "notmulti" ) )
				shouldBeVisible = false;
			else if ( isInGame && !isMultiplayer && kv->GetInt( "notsingle" ) )
				shouldBeVisible = false;
			else if ( isSteam && kv->GetInt( "notsteam" ) )
				shouldBeVisible = false;
			else if ( !bIsConsoleUI && kv->GetInt( "ConsoleOnly" ) )
				shouldBeVisible = false;

			menuItem->SetVisible( shouldBeVisible );
		}
	}

	if ( !isInGame )
	{
		// Sort them into their original order
		for ( int j = 0; j < GetChildCount() - 2; j++ )
			MoveMenuItem( j, j + 1 );
	}
	else
	{
		// Sort them into their in game order
		for ( int i = 0; i < GetChildCount(); i++ )
		{
			for ( int j = i; j < GetChildCount() - 2; j++ )
			{
				int iID1 = GetMenuID( j );
				int iID2 = GetMenuID( j + 1 );

				MenuItem *menuItem1 = GetMenuItem( iID1 );
				MenuItem *menuItem2 = GetMenuItem( iID2 );

				KeyValues *kv1 = menuItem1->GetUserData();
				KeyValues *kv2 = menuItem2->GetUserData();

				if ( kv1->GetInt( "InGameOrder" ) > kv2->GetInt( "InGameOrder" ) )
					MoveMenuItem( iID2, iID1 );
			}
		}
	}

	InvalidateLayout();

	if ( m_pConsoleFooter )
	{
		// update the console footer
		const char *pHelpName;
		if ( !isInGame )
			pHelpName = "MainMenu";
		else
			pHelpName = "GameMenu";

		if ( !m_pConsoleFooter->GetHelpName() || V_stricmp( pHelpName, m_pConsoleFooter->GetHelpName() ) )
		{
			// game menu must re-establish its own help once it becomes re-active
			m_pConsoleFooter->SetHelpNameAndReset( pHelpName );
			m_pConsoleFooter->AddNewButtonLabel( "#GameUI_Action", "#GameUI_Icons_A_BUTTON" );
			if ( isInGame )
				m_pConsoleFooter->AddNewButtonLabel( "#GameUI_Close", "#GameUI_Icons_B_BUTTON" );
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: Editable panel that can replace the GameMenuButtons in CBaseModPanel
//-----------------------------------------------------------------------------
CMainMenuGameLogo::CMainMenuGameLogo( vgui::Panel *parent, const char *name ) : vgui::EditablePanel( parent, name )
{
	m_nOffsetX = 0;
	m_nOffsetY = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySettings( KeyValues *inResourceData )
{
	BaseClass::ApplySettings( inResourceData );

	m_nOffsetX = inResourceData->GetInt( "offsetX", 0 );
	m_nOffsetY = inResourceData->GetInt( "offsetY", 0 );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CMainMenuGameLogo::ApplySchemeSettings( vgui::IScheme *pScheme )
{
	BaseClass::ApplySchemeSettings( pScheme );

	LoadControlSettings( "Resource/GameLogo.res" );
}

static void CC_GameUIShowDialog( const CCommand &args )
{
	int c = args.ArgC();

	if ( c < 2 )
	{
		Msg( "Usage: gameui_show_dialog <commandname>\n" );
		return;
	}

	GameUI().ShowMessageDialog( atoi(args[1]) );
}
static ConCommand gameui_show_dialog( "gameui_show_dialog", CC_GameUIShowDialog, "Show an arbitrary Dialog.", 0 );

static void CC_GameUIHideDialog( const CCommand &args )
{
	int c = args.ArgC();
	if ( c < 1 )
	{
		Msg( "Usage: gameui_hide_dialog <commandname>\n" );
		return;
	}

	GameUI().CloseMessageDialog( 0 );
}
static ConCommand gameui_hide_dialog( "gameui_hide_dialog", CC_GameUIHideDialog, "Hides an arbitrary Dialog.", 0 );
