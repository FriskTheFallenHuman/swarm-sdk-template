//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#include "cbase.h"
#include "LoadingDialog.h"
#include "BaseModPanel.h"
#include "BackgroundVideo.h"
#include <vgui/ISurface.h>
#include "vgui_hudvideo.h"
#include "VGUIMatSurface/IMatSystemSurface.h"
#include "gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern vgui::DHANDLE<CLoadingDialog> g_hLoadingDialog;

CBackground_Video *g_pBackgroundMovie = NULL;

CBackground_Video* BackgroundVideo()
{
	if ( !g_pBackgroundMovie )
		g_pBackgroundMovie = new CBackground_Video();

	return g_pBackgroundMovie;
}

CBackground_Video::CBackground_Video()
{
	m_nBIKMaterial = BIKMATERIAL_INVALID;
	m_nTextureID = -1;
	m_szCurrentMovie[0] = 0;
	m_nLastGameState = -1;
}

void CBackground_Video::SetCurrentMovie( const char *szFilename )
{
	if ( Q_strcmp( m_szCurrentMovie, szFilename ) )
	{
		if ( m_nBIKMaterial != BIKMATERIAL_INVALID )
		{
			// FIXME: Make sure the m_pMaterial is actually destroyed at this point!
			g_pBIK->DestroyMaterial( m_nBIKMaterial );
			m_nBIKMaterial = BIKMATERIAL_INVALID;
			m_nTextureID = -1;
		}

		char szMaterialName[ MAX_PATH ];
		Q_snprintf( szMaterialName, sizeof( szMaterialName ), "BackgroundBIKMaterial%i", g_pBIK->GetGlobalMaterialAllocationNumber() );
		m_nBIKMaterial = bik->CreateMaterial( szMaterialName, szFilename, "GAME", BIK_LOOP );

		Q_snprintf( m_szCurrentMovie, sizeof( m_szCurrentMovie ), "%s", szFilename );
	}
}

void CBackground_Video::ClearCurrentMovie()
{
	if ( m_nBIKMaterial != BIKMATERIAL_INVALID )
	{
		// FIXME: Make sure the m_pMaterial is actually destroyed at this point!
		g_pBIK->DestroyMaterial( m_nBIKMaterial );
		m_nBIKMaterial = BIKMATERIAL_INVALID;
		m_nTextureID = -1;
	}
}

int CBackground_Video::SetTextureMaterial()
{
	if ( m_nBIKMaterial == BIKMATERIAL_INVALID )
		return -1;

	if ( m_nTextureID == -1 )
		m_nTextureID = g_pMatSystemSurface->CreateNewTextureID( true );

	g_pMatSystemSurface->DrawSetTextureMaterial( m_nTextureID, g_pBIK->GetMaterial( m_nBIKMaterial ) );

	return m_nTextureID;
}

void CBackground_Video::Update()
{
	if ( engine->IsConnected() && GameRules() )
	{
		int nGameState = 0;
		if ( nGameState != m_nLastGameState )
		{
			SetCurrentMovie( "media/BG_01.bik" );
			m_nLastGameState = nGameState;
		}
	}
	else
	{
		int nGameState = 0;
		if ( nGameState != m_nLastGameState )
		{
			SetCurrentMovie( "media/BG_02.bik" );
			m_nLastGameState = nGameState;
		}
	}

	if ( m_nBIKMaterial == BIKMATERIAL_INVALID )
		return;

	if ( g_pBIK->ReadyForSwap( m_nBIKMaterial ) )
	{
		if ( g_pBIK->Update( m_nBIKMaterial ) == false )
		{
			// FIXME: Make sure the m_pMaterial is actually destroyed at this point!
			g_pBIK->DestroyMaterial( m_nBIKMaterial );
			m_nBIKMaterial = BIKMATERIAL_INVALID;
		}
	}
}