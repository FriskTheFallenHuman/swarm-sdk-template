//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//
#ifndef C_SDK_VEHICLE_JEEP_H
#define C_SDK_VEHICLE_JEEP_H
#pragma once

#include "cbase.h"
#include "c_prop_vehicle.h"
#include "flashlighteffect.h"

//=============================================================================
//
// Client-side Jeep Class
//
class C_SDK_PropJeep : public C_PropVehicleDriveable
{

	DECLARE_CLASS( C_SDK_PropJeep, C_PropVehicleDriveable );

public:

	DECLARE_CLIENTCLASS();
	DECLARE_INTERPOLATION();

	C_SDK_PropJeep();
	~C_SDK_PropJeep();

public:

	void UpdateViewAngles( C_BasePlayer *pLocalPlayer, CUserCmd *pCmd );
	void DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );

	void OnEnteredVehicle( C_BasePlayer *pPlayer );
	bool Simulate( void );

private:

	void DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void ComputePDControllerCoefficients( float *pCoefficientsOut, float flFrequency, float flDampening, float flDeltaTime );

private:

	Vector		m_vecLastEyePos;
	Vector		m_vecLastEyeTarget;
	Vector		m_vecEyeSpeed;
	Vector		m_vecTargetSpeed;

	float		m_flViewAngleDeltaTime;

	float		m_flJeepFOV;
	CHeadlightEffect *m_pHeadlight;
	bool		m_bHeadlightIsOn;
};

#endif C_SDK_VEHICLE_JEEP_H