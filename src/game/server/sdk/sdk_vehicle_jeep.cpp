//========= Copyright © 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "vehicle_base.h"
#include "engine/IEngineSound.h"
#include "in_buttons.h"
#include "ammodef.h"
#include "IEffects.h"
#include "soundenvelope.h"
#include "decals.h"
#include "soundent.h"
#include "te_effect_dispatch.h"
#include "ndebugoverlay.h"
#include "movevars_shared.h"
#include "bone_setup.h"
#include "eventqueue.h"
#include "rumble_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define	VEHICLE_HITBOX_DRIVER		1
#define LOCK_SPEED					10

#define OVERTURNED_EXIT_WAITTIME	2.0f

#define JEEP_AMMOCRATE_HITGROUP		5
#define JEEP_WHEEL_COUNT			4

#define JEEP_STEERING_SLOW_ANGLE	50.0f
#define JEEP_STEERING_FAST_ANGLE	15.0f

#define	JEEP_AMMO_CRATE_CLOSE_DELAY	2.0f

#define JEEP_DELTA_LENGTH_MAX	12.0f			// 1 foot
#define JEEP_FRAMETIME_MIN		1e-6

ConVar	hud_jeephint_numentries( "hud_jeephint_numentries", "10", FCVAR_NONE );
ConVar	g_jeepexitspeed( "g_jeepexitspeed", "100", FCVAR_CHEAT );

//=============================================================================
//
// Jeep water data.
//
struct JeepWaterData_t
{
	bool		m_bWheelInWater[JEEP_WHEEL_COUNT];
	bool		m_bWheelWasInWater[JEEP_WHEEL_COUNT];
	Vector		m_vecWheelContactPoints[JEEP_WHEEL_COUNT];
	float		m_flNextRippleTime[JEEP_WHEEL_COUNT];
	bool		m_bBodyInWater;
	bool		m_bBodyWasInWater;

	DECLARE_SIMPLE_DATADESC();
};

BEGIN_SIMPLE_DATADESC( JeepWaterData_t )
	DEFINE_ARRAY( m_bWheelInWater,			FIELD_BOOLEAN,	JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_bWheelWasInWater,			FIELD_BOOLEAN,	JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_vecWheelContactPoints,	FIELD_VECTOR,	JEEP_WHEEL_COUNT ),
	DEFINE_ARRAY( m_flNextRippleTime,			FIELD_TIME,		JEEP_WHEEL_COUNT ),
	DEFINE_FIELD( m_bBodyInWater,				FIELD_BOOLEAN ),
	DEFINE_FIELD( m_bBodyWasInWater,			FIELD_BOOLEAN ),
END_DATADESC()	

//-----------------------------------------------------------------------------
// Purpose: Four wheel physics vehicle server vehicle with weaponry
//-----------------------------------------------------------------------------
class CJeepFourWheelServerVehicle : public CFourWheelServerVehicle
{
	typedef CFourWheelServerVehicle BaseClass;

// IServerVehicle
public:

	virtual bool IsPassengerVisible( int nRole = VEHICLE_ROLE_DRIVER ) { return true; } //Tony; in SDK vehicle, make players visible in vehicles.
	virtual bool NPC_HasPrimaryWeapon( void ) { return false; }
	virtual int	GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked );
};

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
class CSDK_PropJeep : public CPropVehicleDriveable
{
	DECLARE_CLASS( CSDK_PropJeep, CPropVehicleDriveable );

public:

	DECLARE_SERVERCLASS();
	DECLARE_DATADESC();

	CSDK_PropJeep( void );

	// CPropVehicle
	virtual void	DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased );
	virtual void	SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move );
	virtual void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void	DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles );
	virtual bool	AllowBlockedExit( CBasePlayer *pPlayer, int nRole ) { return false; }
	virtual bool	CanExitVehicle( CBaseEntity *pEntity );
	virtual bool	IsVehicleBodyInWater() { return m_WaterData.m_bBodyInWater; }
	
	// Passengers do not directly receive damage from blasts or radiation damage
	virtual bool PassengerShouldReceiveDamage( CTakeDamageInfo &info ) 
	{ 
		if ( GetServerVehicle() && GetServerVehicle()->IsPassengerExiting() )
			return false;

		if ( info.GetDamageType() & DMG_VEHICLE )
			return true;

		return (info.GetDamageType() & (DMG_RADIATION|DMG_BLAST) ) == 0; 
	}

	// CBaseEntity
	void			Think(void);
	void			Precache( void );
	void			Spawn( void ); 
	void			Activate( void );

	virtual void	CreateServerVehicle( void );
	virtual Vector	BodyTarget( const Vector &posSrc, bool bNoisy = true );
	virtual void	TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr );
	virtual int		OnTakeDamage( const CTakeDamageInfo &info );
	virtual float	PassengerDamageModifier( const CTakeDamageInfo &info );
	virtual void	EnterVehicle( CBasePlayer *pPlayer );
	virtual void	ExitVehicle( int nRole );

	bool HeadlightIsOn( void ) { return m_bHeadlightIsOn; }
	void HeadlightTurnOn( void ) { m_bHeadlightIsOn = true; }
	void HeadlightTurnOff( void ) { m_bHeadlightIsOn = false; }

private:

	void		InitWaterData( void );
	void		HandleWater( void );
	bool		CheckWater( void );
	void		CheckWaterLevel( void );
	void		CreateSplash( const Vector &vecPosition );
	void		CreateRipple( const Vector &vecPosition );

	void		ComputePDControllerCoefficients( float *pCoefficientsOut, float flFrequency, float flDampening, float flDeltaTime );
	void		DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );
	void		DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime );

	void		InputShowHudHint( inputdata_t &inputdata );

private:

	float			m_throttleDisableTime;
	float			m_flAmmoCrateCloseTime;

	// handbrake after the fact to keep vehicles from rolling
	float			m_flHandbrakeTime;
	bool			m_bInitialHandbrake;
	
	float			m_flOverturnedTime;

	Vector			m_vecLastEyePos;
	Vector			m_vecLastEyeTarget;
	Vector			m_vecEyeSpeed;
	Vector			m_vecTargetSpeed;

	JeepWaterData_t	m_WaterData;

	int				m_iNumberOfEntries;

	CNetworkVar( bool, m_bHeadlightIsOn );
};

BEGIN_DATADESC( CSDK_PropJeep )
	DEFINE_FIELD( m_throttleDisableTime, FIELD_TIME ),
	DEFINE_FIELD( m_flHandbrakeTime, FIELD_TIME ),
	DEFINE_FIELD( m_bInitialHandbrake, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_flOverturnedTime, FIELD_TIME ),
	DEFINE_FIELD( m_flAmmoCrateCloseTime, FIELD_FLOAT ),
	DEFINE_FIELD( m_vecLastEyePos, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecLastEyeTarget, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecEyeSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_vecTargetSpeed, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_bHeadlightIsOn, FIELD_BOOLEAN ),
	DEFINE_EMBEDDED( m_WaterData ),

	DEFINE_FIELD( m_iNumberOfEntries, FIELD_INTEGER ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CSDK_PropJeep, DT_SDK_PropJeep )
	SendPropBool( SENDINFO( m_bHeadlightIsOn ) ),
END_SEND_TABLE();

LINK_ENTITY_TO_CLASS( prop_vehicle_jeep, CSDK_PropJeep );

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CSDK_PropJeep::CSDK_PropJeep( void )
{
	m_bHasGun = false;
	m_flOverturnedTime = 0.0f;
	m_iNumberOfEntries = 0;

	m_vecEyeSpeed.Init();

	InitWaterData();

	m_bUnableToFire = true;
	m_flAmmoCrateCloseTime = 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::CreateServerVehicle( void )
{
	// Create our armed server vehicle
	m_pServerVehicle = new CJeepFourWheelServerVehicle();
	m_pServerVehicle->SetVehicle( this );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::Precache( void )
{
	PrecacheScriptSound( "PropJeep.AmmoClose" );
	PrecacheScriptSound( "PropJeep.AmmoOpen" );

	BaseClass::Precache();
}

//------------------------------------------------
// Spawn
//------------------------------------------------
void CSDK_PropJeep::Spawn( void )
{
	// Setup vehicle as a real-wheels car.
	SetVehicleType( VEHICLE_TYPE_CAR_WHEELS );

	BaseClass::Spawn();
	m_flHandbrakeTime = gpGlobals->curtime + 0.1;
	m_bInitialHandbrake = false;

	m_VehiclePhysics.SetHasBrakePedal( false );

	m_flMinimumSpeedToEnterExit = LOCK_SPEED;

	SetBodygroup( 1, false );

	AddSolidFlags( FSOLID_NOT_STANDABLE );
}
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CSDK_PropJeep::Activate()
{
	BaseClass::Activate();

	CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
	if ( pServerVehicle )
	{
		if( pServerVehicle->GetPassenger() )
		{
			// If a jeep comes back from a save game with a driver, make sure the engine rumble starts up.
			pServerVehicle->StartEngineRumble();
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::TraceAttack( const CTakeDamageInfo &inputInfo, const Vector &vecDir, trace_t *ptr )
{
	CTakeDamageInfo info = inputInfo;
	if ( ptr->hitbox != VEHICLE_HITBOX_DRIVER )
	{
		if ( inputInfo.GetDamageType() & DMG_BULLET )
			info.ScaleDamage( 0.0001 );
	}

	BaseClass::TraceAttack( info, vecDir, ptr );
}

//-----------------------------------------------------------------------------
// Purpose: Modifies the passenger's damage taken through us
//-----------------------------------------------------------------------------
float CSDK_PropJeep::PassengerDamageModifier( const CTakeDamageInfo &info )
{
	return 1.0f;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
int CSDK_PropJeep::OnTakeDamage( const CTakeDamageInfo &inputInfo )
{
	//Do scaled up physics damage to the car
	CTakeDamageInfo info = inputInfo;
	info.ScaleDamage( 25 );
	
	// HACKHACK: Scale up grenades until we get a better explosion/pressure damage system
	if ( inputInfo.GetDamageType() & DMG_BLAST )
		info.SetDamageForce( inputInfo.GetDamageForce() * 10 );

	VPhysicsTakeDamage( info );

	// reset the damage
	info.SetDamage( inputInfo.GetDamage() );

	// small amounts of shock damage disrupt the car, but aren't transferred to the player
	if ( info.GetDamageType() == DMG_SHOCK )
	{
		if ( info.GetDamage() <= 10 )
		{
			// take 10% damage and make the engine stall
			info.ScaleDamage( 0.1 );
			m_throttleDisableTime = gpGlobals->curtime + 2;
		}
	}

	//Check to do damage to driver
	if ( GetDriver() )
	{
		//Take no damage from physics damages
		if ( info.GetDamageType() & DMG_CRUSH )
			return 0;

		// Scale the damage and mark that we're passing it in so the base player accepts the damage
		info.ScaleDamage( PassengerDamageModifier( info ) );
		info.SetDamageType( info.GetDamageType() | DMG_VEHICLE );
		
		// Deal the damage to the passenger
		GetDriver()->TakeDamage( info );
	}

	return 0;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
Vector CSDK_PropJeep::BodyTarget( const Vector &posSrc, bool bNoisy )
{
	Vector	shotPos;
	matrix3x4_t	matrix;

	int eyeAttachmentIndex = LookupAttachment("vehicle_driver_eyes");
	GetAttachment( eyeAttachmentIndex, matrix );
	MatrixGetColumn( matrix, 3, shotPos );

	if ( bNoisy )
	{
		shotPos[0] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[1] += random->RandomFloat( -8.0f, 8.0f );
		shotPos[2] += random->RandomFloat( -8.0f, 8.0f );
	}

	return shotPos;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::InitWaterData( void )
{
	m_WaterData.m_bBodyInWater = false;
	m_WaterData.m_bBodyWasInWater = false;

	for ( int iWheel = 0; iWheel < JEEP_WHEEL_COUNT; ++iWheel )
	{
		m_WaterData.m_bWheelInWater[iWheel] = false;
		m_WaterData.m_bWheelWasInWater[iWheel] = false;
		m_WaterData.m_vecWheelContactPoints[iWheel].Init();
		m_WaterData.m_flNextRippleTime[iWheel] = 0;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::HandleWater( void )
{
	// Only check the wheels and engine in water if we have a driver (player).
	if ( !GetDriver() )
		return;

	// Check to see if we are in water.
	if ( CheckWater() )
	{
		for ( int iWheel = 0; iWheel < JEEP_WHEEL_COUNT; ++iWheel )
		{
			// Create an entry/exit splash!
			if ( m_WaterData.m_bWheelInWater[iWheel] != m_WaterData.m_bWheelWasInWater[iWheel] )
			{
				CreateSplash( m_WaterData.m_vecWheelContactPoints[iWheel] );
				CreateRipple( m_WaterData.m_vecWheelContactPoints[iWheel] );
			}
			
			// Create ripples.
			if ( m_WaterData.m_bWheelInWater[iWheel] && m_WaterData.m_bWheelWasInWater[iWheel] )
			{
				if ( m_WaterData.m_flNextRippleTime[iWheel] < gpGlobals->curtime )
				{
					// Stagger ripple times
					m_WaterData.m_flNextRippleTime[iWheel] = gpGlobals->curtime + RandomFloat( 0.1, 0.3 );
					CreateRipple( m_WaterData.m_vecWheelContactPoints[iWheel] );
				}
			}
		}
	}

	// Save of data from last think.
	for ( int iWheel = 0; iWheel < JEEP_WHEEL_COUNT; ++iWheel )
	{
		m_WaterData.m_bWheelWasInWater[iWheel] = m_WaterData.m_bWheelInWater[iWheel];
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSDK_PropJeep::CheckWater( void )
{
	bool bInWater = false;

	// Check all four wheels.
	for ( int iWheel = 0; iWheel < JEEP_WHEEL_COUNT; ++iWheel )
	{
		// Get the current wheel and get its contact point.
		IPhysicsObject *pWheel = m_VehiclePhysics.GetWheel( iWheel );
		if ( !pWheel )
			continue;

		// Check to see if we hit water.
		if ( pWheel->GetContactPoint( &m_WaterData.m_vecWheelContactPoints[iWheel], NULL ) )
		{
			m_WaterData.m_bWheelInWater[iWheel] = ( UTIL_PointContents( m_WaterData.m_vecWheelContactPoints[iWheel] ,MASK_WATER ) ) ? true : false;
			if ( m_WaterData.m_bWheelInWater[iWheel] )
				bInWater = true;
		}
	}

	// Check the body and the BONNET.
	int iEngine = LookupAttachment( "vehicle_engine" );
	Vector vecEnginePoint;
	QAngle vecEngineAngles;
	GetAttachment( iEngine, vecEnginePoint, vecEngineAngles );

	m_WaterData.m_bBodyInWater = ( UTIL_PointContents( vecEnginePoint, MASK_WATER ) ) ? true : false;
	if ( m_WaterData.m_bBodyInWater )
	{
		if ( !m_VehiclePhysics.IsEngineDisabled() )
			m_VehiclePhysics.SetDisableEngine( true );
	}
	else
	{
		if ( m_VehiclePhysics.IsEngineDisabled() )
			m_VehiclePhysics.SetDisableEngine( false );
	}

	if ( bInWater )
	{
		// Check the player's water level.
		CheckWaterLevel();
	}

	return bInWater;
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::CheckWaterLevel( void )
{
	CBaseEntity *pEntity = GetDriver();
	if ( pEntity && pEntity->IsPlayer() )
	{
		CBasePlayer *pPlayer = static_cast<CBasePlayer*>( pEntity );
		
		Vector vecAttachPoint;
		QAngle vecAttachAngles;
		
		// Check eyes. (vehicle_driver_eyes point)
		int iAttachment = LookupAttachment( "vehicle_driver_eyes" );
		GetAttachment( iAttachment, vecAttachPoint, vecAttachAngles );

		// Add the jeep's Z view offset
		Vector vecUp;
		AngleVectors( vecAttachAngles, NULL, NULL, &vecUp );
		vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
		vecAttachPoint.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

		bool bEyes = ( UTIL_PointContents( vecAttachPoint, MASK_WATER ) ) ? true : false;
		if ( bEyes )
		{
			pPlayer->SetWaterLevel( WL_Eyes );
			return;
		}

		// Check waist.  (vehicle_engine point -- see parent function).
		if ( m_WaterData.m_bBodyInWater )
		{
			pPlayer->SetWaterLevel( WL_Waist );
			return;
		}

		// Check feet. (vehicle_feet_passenger0 point)
		iAttachment = LookupAttachment( "vehicle_feet_passenger0" );
		GetAttachment( iAttachment, vecAttachPoint, vecAttachAngles );
		bool bFeet = ( UTIL_PointContents( vecAttachPoint , MASK_WATER) ) ? true : false;
		if ( bFeet )
		{
			pPlayer->SetWaterLevel( WL_Feet );
			return;
		}

		// Not in water.
		pPlayer->SetWaterLevel( WL_NotInWater );
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::CreateSplash( const Vector &vecPosition )
{
	// Splash data.
	CEffectData	data;
	data.m_fFlags = 0;
	data.m_vOrigin = vecPosition;
	data.m_vNormal.Init( 0.0f, 0.0f, 1.0f );
	VectorAngles( data.m_vNormal, data.m_vAngles );
	data.m_flScale = 10.0f + random->RandomFloat( 0, 2 );

	// Create the splash..
	DispatchEffect( "watersplash", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::CreateRipple( const Vector &vecPosition )
{
	// Ripple data.
	CEffectData	data;
	data.m_fFlags = 0;
	data.m_vOrigin = vecPosition;
	data.m_vNormal.Init( 0.0f, 0.0f, 1.0f );
	VectorAngles( data.m_vNormal, data.m_vAngles );
	data.m_flScale = 10.0f + random->RandomFloat( 0, 2 );
	if ( GetWaterType() & CONTENTS_SLIME )
	{
		data.m_fFlags |= FX_WATER_IN_SLIME;
	}

	// Create the ripple.
	DispatchEffect( "waterripple", data );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::Think(void)
{
	BaseClass::Think();

	//Tony; get the driver instead of localplayer.
	CBasePlayer *pPlayer = ToBasePlayer( GetDriver() );
	if ( m_bEngineLocked )
	{
		m_bUnableToFire = true;
		
		if ( pPlayer != NULL )
			pPlayer->m_Local.m_iHideHUD |= HIDEHUD_VEHICLE_CROSSHAIR;
	}
	else
	{
		// Start this as false and update it again each frame
		m_bUnableToFire = false;

		if ( pPlayer != NULL )
			pPlayer->m_Local.m_iHideHUD &= ~HIDEHUD_VEHICLE_CROSSHAIR;
	}

	// Water!?
	HandleWater();

	SetSimulationTime( gpGlobals->curtime );
	
	SetNextThink( gpGlobals->curtime );
	SetAnimatedEveryTick( true );

    if ( !m_bInitialHandbrake )	// after initial timer expires, set the handbrake
	{
		m_bInitialHandbrake = true;
		m_VehiclePhysics.SetHandbrake( true );
		m_VehiclePhysics.Think();
	}

	// Check overturned status.
	if ( !IsOverturned() )
		m_flOverturnedTime = 0.0f;
	else
		m_flOverturnedTime += gpGlobals->frametime;

	StudioFrameAdvance();

	// If the enter or exit animation has finished, tell the server vehicle
	if ( IsSequenceFinished() && (m_bExitAnimOn || m_bEnterAnimOn) )
	{
		if ( m_bEnterAnimOn )
		{
			m_VehiclePhysics.ReleaseHandbrake();
			StartEngine();

			// HACKHACK: This forces the jeep to play a sound when it gets entered underwater
			if ( m_VehiclePhysics.IsEngineDisabled() )
			{
				CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
				if ( pServerVehicle )
					pServerVehicle->SoundStartDisabled();
			}

			// The first few time we get into the jeep, print the jeep help
			if ( m_iNumberOfEntries < hud_jeephint_numentries.GetInt() )
			{
				g_EventQueue.AddEvent( this, "ShowHudHint", 1.5f, this, this );
				m_iNumberOfEntries++;
			}
		}
		
		// If we're exiting and have had the tau cannon removed, we don't want to reset the animation
		GetServerVehicle()->HandleEntryExitFinish( m_bExitAnimOn, true );
	}

	// See if the ammo crate needs to close
	if ( ( m_flAmmoCrateCloseTime < gpGlobals->curtime ) && ( GetSequence() == LookupSequence( "ammo_open" ) ) )
	{
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		ResetSequence( LookupSequence( "ammo_close" ) );
	}
	else if ( ( GetSequence() == LookupSequence( "ammo_close" ) ) && IsSequenceFinished() )
	{
		m_flAnimTime = gpGlobals->curtime;
		m_flPlaybackRate = 0.0;
		SetCycle( 0 );
		
		int nSequence = SelectWeightedSequence( ACT_IDLE );
		ResetSequence( nSequence );

		CPASAttenuationFilter sndFilter( this, "PropJeep.AmmoClose" );
		EmitSound( sndFilter, entindex(), "PropJeep.AmmoClose" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: If the player uses the jeep while at the back, he gets ammo from the crate instead
//-----------------------------------------------------------------------------
void CSDK_PropJeep::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	CBasePlayer *pPlayer = ToBasePlayer( pActivator );	
	if ( pPlayer == NULL)
		return;

	// Find out if the player's looking at our ammocrate hitbox 
	Vector vecForward;
	pPlayer->EyeVectors( &vecForward, NULL, NULL );

	trace_t tr;
	Vector vecStart = pPlayer->EyePosition();
	UTIL_TraceLine( vecStart, vecStart + vecForward * 1024, MASK_SOLID | CONTENTS_DEBRIS | CONTENTS_HITBOX, pPlayer, COLLISION_GROUP_NONE, &tr );
	
	if ( tr.m_pEnt == this && tr.hitgroup == JEEP_AMMOCRATE_HITGROUP )
	{
		// Player's using the crate.
		pPlayer->GiveAmmo( 300, pPlayer->GetActiveWeapon()->GetPrimaryAmmoType() );
		
		if ( ( GetSequence() != LookupSequence( "ammo_open" ) ) && ( GetSequence() != LookupSequence( "ammo_close" ) ) )
		{
			// Open the crate
			m_flAnimTime = gpGlobals->curtime;
			m_flPlaybackRate = 0.0;
			SetCycle( 0 );
			ResetSequence( LookupSequence( "ammo_open" ) );
			
			CPASAttenuationFilter sndFilter( this, "PropJeep.AmmoOpen" );
			EmitSound( sndFilter, entindex(), "PropJeep.AmmoOpen" );
		}

		m_flAmmoCrateCloseTime = gpGlobals->curtime + JEEP_AMMO_CRATE_CLOSE_DELAY;
		return;
	}

	// Fall back and get in the vehicle instead
	BaseClass::Use( pActivator, pCaller, useType, value );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CSDK_PropJeep::CanExitVehicle( CBaseEntity *pEntity )
{
	return ( !m_bEnterAnimOn && !m_bExitAnimOn && !m_bLocked && (m_nSpeed <= g_jeepexitspeed.GetFloat() ) );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::DampenEyePosition( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles )
{
	// Get the frametime. (Check to see if enough time has passed to warrent dampening).
	float flFrameTime = gpGlobals->frametime;
	if ( flFrameTime < JEEP_FRAMETIME_MIN )
	{
		vecVehicleEyePos = m_vecLastEyePos;
		DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, 0.0f );
		return;
	}

	// Keep static the sideways motion.

	// Dampen forward/backward motion.
	DampenForwardMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );

	// Blend up/down motion.
	DampenUpMotion( vecVehicleEyePos, vecVehicleEyeAngles, flFrameTime );
}

//-----------------------------------------------------------------------------
// Use the controller as follows:
// speed += ( pCoefficientsOut[0] * ( targetPos - currentPos ) + pCoefficientsOut[1] * ( targetSpeed - currentSpeed ) ) * flDeltaTime;
//-----------------------------------------------------------------------------
void CSDK_PropJeep::ComputePDControllerCoefficients( float *pCoefficientsOut,
												  float flFrequency, float flDampening,
												  float flDeltaTime )
{
	float flKs = 9.0f * flFrequency * flFrequency;
	float flKd = 4.5f * flFrequency * flDampening;

	float flScale = 1.0f / ( 1.0f + flKd * flDeltaTime + flKs * flDeltaTime * flDeltaTime );

	pCoefficientsOut[0] = flKs * flScale;
	pCoefficientsOut[1] = ( flKd + flKs * flDeltaTime ) * flScale;
}
 
//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSDK_PropJeep::DampenForwardMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get forward vector.
	Vector vecForward;
	AngleVectors( vecVehicleEyeAngles, &vecForward);

	// Simulate the eye position forward based on the data from last frame
	// (assumes no acceleration - it will get that from the "spring").
	Vector vecCurrentEyePos = m_vecLastEyePos + m_vecEyeSpeed * flFrameTime;

	// Calculate target speed based on the current vehicle eye position and the last vehicle eye position and frametime.
	Vector vecVehicleEyeSpeed = ( vecVehicleEyePos - m_vecLastEyeTarget ) / flFrameTime;
	m_vecLastEyeTarget = vecVehicleEyePos;	

	// Calculate the speed and position deltas.
	Vector vecDeltaSpeed = vecVehicleEyeSpeed - m_vecEyeSpeed;
	Vector vecDeltaPos = vecVehicleEyePos - vecCurrentEyePos;

	// Clamp.
	if ( vecDeltaPos.Length() > JEEP_DELTA_LENGTH_MAX )
	{
		float flSign = vecForward.Dot( vecVehicleEyeSpeed ) >= 0.0f ? -1.0f : 1.0f;
		vecVehicleEyePos += flSign * ( vecForward * JEEP_DELTA_LENGTH_MAX );
		m_vecLastEyePos = vecVehicleEyePos;
		m_vecEyeSpeed = vecVehicleEyeSpeed;
		return;
	}

	// Generate an updated (dampening) speed for use in next frames position extrapolation.
	float flCoefficients[2];
	ComputePDControllerCoefficients( flCoefficients, r_JeepViewDampenFreq.GetFloat(), r_JeepViewDampenDamp.GetFloat(), flFrameTime );
	m_vecEyeSpeed += ( ( flCoefficients[0] * vecDeltaPos + flCoefficients[1] * vecDeltaSpeed ) * flFrameTime );

	// Save off data for next frame.
	m_vecLastEyePos = vecCurrentEyePos;

	// Move eye forward/backward.
	Vector vecForwardOffset = vecForward * ( vecForward.Dot( vecDeltaPos ) );
	vecVehicleEyePos -= vecForwardOffset;
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void CSDK_PropJeep::DampenUpMotion( Vector &vecVehicleEyePos, QAngle &vecVehicleEyeAngles, float flFrameTime )
{
	// Get up vector.
	Vector vecUp;
	AngleVectors( vecVehicleEyeAngles, NULL, NULL, &vecUp );
	vecUp.z = clamp( vecUp.z, 0.0f, vecUp.z );
	vecVehicleEyePos.z += r_JeepViewZHeight.GetFloat() * vecUp.z;

	// NOTE: Should probably use some damped equation here.
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::SetupMove( CBasePlayer *player, CUserCmd *ucmd, IMoveHelper *pHelper, CMoveData *move )
{
	// If we are overturned and hit any key - leave the vehicle (IN_USE is already handled!).
	if ( m_flOverturnedTime > OVERTURNED_EXIT_WAITTIME )
	{
		if ( (ucmd->buttons & (IN_FORWARD|IN_BACK|IN_MOVELEFT|IN_MOVERIGHT|IN_SPEED|IN_JUMP|IN_ATTACK|IN_ATTACK2) ) && !m_bExitAnimOn )
		{
			// Can't exit yet? We're probably still moving. Swallow the keys.
			if ( !CanExitVehicle(player) )
				return;

			if ( !GetServerVehicle()->HandlePassengerExit( m_hPlayer ) && ( m_hPlayer != NULL ) )
			{
				m_hPlayer->PlayUseDenySound();
			}
			return;
		}
	}

	// If the throttle is disabled or we're upside-down, don't allow throttling (including turbo)
	CUserCmd tmp;
	if ( ( m_throttleDisableTime > gpGlobals->curtime ) || ( IsOverturned() ) )
	{
		m_bUnableToFire = true;
		
		tmp = (*ucmd);
		tmp.buttons &= ~(IN_FORWARD|IN_BACK|IN_SPEED);
		ucmd = &tmp;
	}
	
	BaseClass::SetupMove( player, ucmd, pHelper, move );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::DriveVehicle( float flFrameTime, CUserCmd *ucmd, int iButtonsDown, int iButtonsReleased )
{
	//Adrian: No headlights on Superfly.
	if ( ucmd->impulse == 100 )
	{
		if ( HeadlightIsOn() )
			HeadlightTurnOff();
        else 
			HeadlightTurnOn();
	}

	BaseClass::DriveVehicle( flFrameTime, ucmd, iButtonsDown, iButtonsReleased );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::EnterVehicle( CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return;

	CheckWater();

	BaseClass::EnterVehicle( pPlayer );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CSDK_PropJeep::ExitVehicle( int nRole )
{
	HeadlightTurnOff();

	BaseClass::ExitVehicle( nRole );
}

//-----------------------------------------------------------------------------
// Purpose: Show people how to drive!
//-----------------------------------------------------------------------------
void CSDK_PropJeep::InputShowHudHint( inputdata_t &inputdata )
{
	CBaseServerVehicle *pServerVehicle = dynamic_cast<CBaseServerVehicle *>(GetServerVehicle());
	if ( pServerVehicle )
	{
		if( pServerVehicle->GetPassenger( VEHICLE_ROLE_DRIVER ) )
		{
			UTIL_HudHintText( m_hPlayer, "#Valve_Hint_JeepKeys" );
			m_iNumberOfEntries++;
		}
	}
}

//-----------------------------------------------------------------------------
// Purpose: 
// Input  : &vecEyeExitEndpoint - 
// Output : int
//-----------------------------------------------------------------------------
int CJeepFourWheelServerVehicle::GetExitAnimToUse( Vector &vecEyeExitEndpoint, bool &bAllPointsBlocked )
{
	bAllPointsBlocked = false;

	if ( !m_bParsedAnimations )
	{
		// Load the entry/exit animations from the vehicle
		ParseEntryExitAnims();
		m_bParsedAnimations = true;
	}

	return BaseClass::GetExitAnimToUse( vecEyeExitEndpoint, bAllPointsBlocked );
}
