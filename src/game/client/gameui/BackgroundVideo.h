//===== Copyright © 1996-2005, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//===========================================================================//
#ifndef BACKGROUNDVIDEO_H
#define BACKGROUNDVIDEO_H
#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <vgui_controls/EditablePanel.h>
#include "avi/ibik.h"

class CBackground_Video
{
public:
	CBackground_Video();
	~CBackground_Video() {}

	void Update();
	void SetCurrentMovie( const char *szFilename );
	int SetTextureMaterial();
	void ClearCurrentMovie();
 
private:
	BIKMaterial_t m_nBIKMaterial;
	int m_nTextureID;
	char m_szCurrentMovie[ MAX_PATH ];
	int m_nLastGameState;
};

CBackground_Video* BackgroundVideo();

#endif // BACKGROUNDVIDEO_H