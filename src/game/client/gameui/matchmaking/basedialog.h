//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: All matchmaking dialogs inherit from this
//
//=============================================================================//

#ifndef BASEDIALOG_H
#define BASEDIALOG_H
#ifdef _WIN32
#pragma once
#endif

#include "dialogmenu.h"
#include "vgui_controls/Label.h"
#include "KeyValues.h"
#include "BaseModPanel.h"

#if !defined( _X360 )
#include "xbox/xboxstubs.h"
#endif

class CFooterPanel;

// this should not be here
enum SESSION_NOTIFY
{
	SESSION_NOTIFY_FAIL_SEARCH,
	SESSION_NOTIFY_SEARCH_COMPLETED,
	SESSION_NOFIFY_MODIFYING_SESSION,
	SESSION_NOTIFY_MODIFYING_COMPLETED_HOST,
	SESSION_NOTIFY_MODIFYING_COMPLETED_CLIENT,
	SESSION_NOTIFY_MIGRATION_COMPLETED,
	SESSION_NOTIFY_CONNECT_SESSIONFULL,
	SESSION_NOTIFY_CONNECT_NOTAVAILABLE,
	SESSION_NOTIFY_CONNECTED_TOSESSION,
	SESSION_NOTIFY_CONNECTED_TOSERVER,
	SESSION_NOTIFY_CONNECT_FAILED,
	SESSION_NOTIFY_FAIL_CREATE,
	SESSION_NOTIFY_FAIL_MIGRATE,
	SESSION_NOTIFY_REGISTER_COMPLETED,
	SESSION_NOTIFY_FAIL_REGISTER,
	SESSION_NOTIFY_CLIENT_KICKED,
	SESSION_NOTIFY_CREATED_HOST,
	SESSION_NOTIFY_CREATED_CLIENT,
	SESSION_NOTIFY_LOST_HOST,
	SESSION_NOTIFY_LOST_SERVER,
	SESSION_NOTIFY_COUNTDOWN,
	SESSION_NOTIFY_ENDGAME_RANKED,	// Ranked
	SESSION_NOTIFY_ENDGAME_HOST,	// Unranked
	SESSION_NOTIFY_ENDGAME_CLIENT,	// Unranked
	SESSION_NOTIFY_DUMPSTATS,		// debugging
	SESSION_NOTIFY_WELCOME,			// Close all dialogs and show the welcome main menu
};

enum SESSION_PROPS
{
	SESSION_CONTEXT,
	SESSION_PROPERTY,
	SESSION_FLAG,
};

#define NO_TIME_LIMIT	65000

//-----------------------------------------------------------------------------
// Purpose: A Label with an extra string to hold a session property lookup key
//-----------------------------------------------------------------------------
class CPropertyLabel : public vgui::Label
{
	DECLARE_CLASS_SIMPLE( CPropertyLabel, vgui::Label );

public:
	CPropertyLabel( Panel *parent, const char *panelName, const char *text ) : BaseClass( parent, panelName, text ) 
	{
	}

	virtual void ApplySettings( KeyValues *pResourceData )
	{
		BaseClass::ApplySettings( pResourceData );

		m_szPropertyString[0] = 0;
		const char *pString = pResourceData->GetString( "PropertyString", NULL );
		if ( pString )
		{
			Q_strncpy( m_szPropertyString, pString, sizeof( m_szPropertyString ) );
		}
	}

	char m_szPropertyString[ MAX_PATH ];
};

//--------------------------------
// CBaseDialog
//--------------------------------
class CBaseDialog : public vgui::Frame
{
	DECLARE_CLASS_SIMPLE( CBaseDialog, vgui::Frame ); 

public:
	CBaseDialog( vgui::Panel *parent, const char *pName );
	~CBaseDialog();

	// IPanel interface
	virtual void		ApplySchemeSettings( vgui::IScheme *pScheme );
	virtual void		ApplySettings( KeyValues *pResourceData );
	virtual void		PerformLayout();
	virtual void		Activate();

	virtual void		OnKeyCodePressed( vgui::KeyCode code );
	virtual void		OnKeyCodeReleased( vgui::KeyCode code);
	virtual void		OnCommand( const char *pCommand );
	virtual void		OnClose();
	virtual void		OnThink();

	virtual void		OverrideMenuItem( KeyValues *pKeys );
	virtual void		SwapMenuItems( int iOne, int iTwo );

	virtual void		HandleKeyRepeated( vgui::KeyCode code );

protected:
	int				m_nBorderWidth;
	int				m_nMinWide;

	CDialogMenu		m_Menu;
	vgui::Label		*m_pTitle;
	vgui::Panel		*m_pParent;

	KeyValues		*m_pFooterInfo;
	int				m_nButtonGap;
};


//---------------------------------------------------------------------
// Helper object to display the map picture and descriptive text
//---------------------------------------------------------------------
class CScenarioInfoPanel : public vgui::EditablePanel
{
	DECLARE_CLASS_SIMPLE( CScenarioInfoPanel, vgui::EditablePanel );

public:
	CScenarioInfoPanel( vgui::Panel *parent, const char *pName );
	~CScenarioInfoPanel();

	virtual void	PerformLayout();
	virtual void	ApplySettings( KeyValues *pResourceData );
	virtual void	ApplySchemeSettings( vgui::IScheme *pScheme );

	vgui::ImagePanel	*m_pMapImage;
	CPropertyLabel		*m_pTitle;
	CPropertyLabel		*m_pSubtitle;	
	CPropertyLabel		*m_pDescOne;	
	CPropertyLabel		*m_pDescTwo;	
	CPropertyLabel		*m_pDescThree;	
	CPropertyLabel		*m_pValueTwo;	
	CPropertyLabel		*m_pValueThree;	
};

#endif	// BASEDIALOG_H