//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: http://pastebin.com/pHfTRTjp
//
// $NoKeywords: $
//=============================================================================//
 
#include "cbase.h"
#include "in_buttons.h"
#include "takedamageinfo.h"
#include "weapon_sdkbase.h"
#include "ammodef.h"
#include "sdk_fx_shared.h"
 
#if defined( CLIENT_DLL )
	#include "c_sdk_player.h"
#else
	#include "sdk_player.h"
#endif
 
// ----------------------------------------------------------------------------- //
// CWeaponSDKBase tables.
// ----------------------------------------------------------------------------- //
 
IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSDKBase, DT_WeaponSDKBase )
 
BEGIN_NETWORK_TABLE( CWeaponSDKBase, DT_WeaponSDKBase )
#ifdef CLIENT_DLL
  	RecvPropFloat( RECVINFO( m_flDecreaseShotsFired ) ),
#else
	SendPropExclude( "DT_BaseAnimating", "m_nNewSequenceParity" ),
	SendPropExclude( "DT_BaseAnimating", "m_nResetEventsParity" ),
	SendPropFloat( SENDINFO( m_flDecreaseShotsFired ) ),
#endif
END_NETWORK_TABLE()
 
#ifdef CLIENT_DLL
BEGIN_PREDICTION_DATA( CWeaponSDKBase )
	DEFINE_PRED_FIELD( m_flTimeWeaponIdle, FIELD_FLOAT, FTYPEDESC_OVERRIDE | FTYPEDESC_NOERRORCHECK ),
END_PREDICTION_DATA()
#endif

#ifdef GAME_DLL
BEGIN_DATADESC( CWeaponSDKBase )
 	// New weapon Think and Touch Functions go here..
 END_DATADESC() 
#endif
 
// ----------------------------------------------------------------------------- //
// CWeaponCSBase implementation. 
// ----------------------------------------------------------------------------- //
CWeaponSDKBase::CWeaponSDKBase()
#ifdef CLIENT_DLL
	:m_GlowObject( this )
#endif
{
	SetPredictionEligible( true );
 
	AddSolidFlags( FSOLID_TRIGGER ); // Nothing collides with these but it gets touches.

#ifdef CLIENT_DLL
	m_GlowObject.SetColor( Vector( 0.3f, 0.6f, 0.1f ) );
	m_GlowObject.SetRenderFlags( false, true );
#endif
}
 
#ifdef CLIENT_DLL
bool CWeaponSDKBase::ShouldPredict()
{
       if ( GetOwner() && GetOwner() == C_BasePlayer::GetLocalPlayer())
               return true;
 
       return BaseClass::ShouldPredict();
}

void CWeaponSDKBase::OnDataChanged( DataUpdateType_t updateType )
{
	BaseClass::OnDataChanged( updateType );

	if ( updateType == DATA_UPDATE_CREATED )
	{
		SetNextClientThink( CLIENT_THINK_ALWAYS );
	}
}

void CWeaponSDKBase::ClientThink()
{
	BaseClass::ClientThink();

	if ( !GetOwner() )
		m_GlowObject.SetRenderFlags( true, true );
	else
		m_GlowObject.SetRenderFlags( false, false );
}
#endif

const CSDKWeaponInfo &CWeaponSDKBase::GetSDKWpnData() const
{
	const FileWeaponInfo_t *pWeaponInfo = &GetWpnData();
	const CSDKWeaponInfo *pSDKInfo;
 
#ifdef _DEBUG
	pSDKInfo = dynamic_cast< const CSDKWeaponInfo* >( pWeaponInfo );
	Assert( pSDKInfo );
#else
	pSDKInfo = static_cast< const CSDKWeaponInfo* >( pWeaponInfo );
#endif
 
	return *pSDKInfo;
}
 
bool CWeaponSDKBase::PlayEmptySound()
{
	CPASAttenuationFilter filter( this );
	filter.UsePredictionRules();
 
	EmitSound( filter, entindex(), "Default.ClipEmpty_Rifle" );
 
	return 0;
}
 
CSDKPlayer* CWeaponSDKBase::GetPlayerOwner() const
{
	return dynamic_cast< CSDKPlayer* >( GetOwner() );
}
 
#ifdef CLIENT_DLL
void UTIL_ClipPunchAngleOffset( QAngle &in, const QAngle &punch, const QAngle &clip )
{
	QAngle	final = in + punch;
 
	//Clip each component
	for ( int i = 0; i < 3; i++ )
	{
		if ( final[i] > clip[i] )
		{
			final[i] = clip[i];
		}
		else if ( final[i] < -clip[i] )
		{
			final[i] = -clip[i];
		}
 
		//Return the result
		in[i] = final[i] - punch[i];
	}
}
#endif
 
//Tony; added as a default primary attack if it doesn't get overridden, ie: by CSDKWeaponMelee
void CWeaponSDKBase::PrimaryAttack( void )
{
	// If my clip is empty (and I use clips) start reload
	if ( UsesClipsForAmmo1() && !m_iClip1 ) 
	{
		Reload();
		return;
	}
 
	CSDKPlayer *pPlayer = GetPlayerOwner();
 
	if (!pPlayer)
		return;
 
	//Tony; check firemodes -- 
	switch(GetFireMode())
	{
	case FM_SEMIAUTOMATIC:
		if (pPlayer->GetShotsFired() > 0)
			return;
		break;
		//Tony; added an accessor to determine the MAX burst on a per-weapon basis.
	case FM_BURST:
		if (pPlayer->GetShotsFired() > MaxBurstShots())
			return;
		break;
	}
#ifdef GAME_DLL
	pPlayer->NoteWeaponFired();
#endif
 
	pPlayer->DoMuzzleFlash();
 
	SendWeaponAnim( GetPrimaryAttackActivity() );
 
	// Make sure we don't fire more than the amount in the clip
	if ( UsesClipsForAmmo1() )
		m_iClip1 --;
	else
		pPlayer->RemoveAmmo(1, m_iPrimaryAmmoType );
 
	pPlayer->IncreaseShotsFired();
 
	float flSpread = GetWeaponSpread();
 
	FX_FireBullets(
		pPlayer->entindex(),
		pPlayer->Weapon_ShootPosition(),
		pPlayer->EyeAngles() + pPlayer->GetPunchAngle(),
		GetWeaponID(),
		0, //Tony; fire mode - this is unused at the moment, left over from CSS when SDK* was created in the first place.
		CBaseEntity::GetPredictionRandomSeed() & 255,
		flSpread
		);
 
 
	//Add our view kick in
	AddViewKick();
 
	//Tony; update our weapon idle time
	SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
 
	m_flNextPrimaryAttack = gpGlobals->curtime + GetFireRate();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}
 
void CWeaponSDKBase::SecondaryAttack()
{
}
 
//Tony; added so we can have base functionality without implementing it into every weapon.
void CWeaponSDKBase::ItemPostFrame( void )
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
 
	//
	//Tony; totally override the baseclass
	//
 
	if ( UsesClipsForAmmo1() )
		CheckReload();
 
	bool bFired = false;
 
	// Secondary attack has priority
	if ((pPlayer->m_nButtons & IN_ATTACK2) && (m_flNextSecondaryAttack <= gpGlobals->curtime) && pPlayer->CanAttack())
	{
		if (UsesSecondaryAmmo() && pPlayer->GetAmmoCount(m_iSecondaryAmmoType)<=0 )
		{
			if (m_flNextEmptySoundTime < gpGlobals->curtime)
			{
				WeaponSound(EMPTY);
				m_flNextSecondaryAttack = m_flNextEmptySoundTime = gpGlobals->curtime + 0.5;
			}
		}
		else if (pPlayer->GetWaterLevel() == 3 && m_bAltFiresUnderwater == false)
		{
			// This weapon doesn't fire underwater
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}
		else
		{
			bFired = true;
			SecondaryAttack();
 
			// Secondary ammo doesn't have a reload animation
			if ( UsesClipsForAmmo2() )
			{
				// reload clip2 if empty
				if (m_iClip2 < 1)
				{
					pPlayer->RemoveAmmo( 1, m_iSecondaryAmmoType );
					m_iClip2 = m_iClip2 + 1;
				}
			}
		}
	}
 
	if ( !bFired && (pPlayer->m_nButtons & IN_ATTACK) && (m_flNextPrimaryAttack <= gpGlobals->curtime) && pPlayer->CanAttack())
	{
		// Clip empty? Or out of ammo on a no-clip weapon?
		if ( !IsMeleeWeapon() && (( UsesClipsForAmmo1() && m_iClip1 <= 0) || ( !UsesClipsForAmmo1() && pPlayer->GetAmmoCount(m_iPrimaryAmmoType)<=0 )) )
		{
			HandleFireOnEmpty();
		}
		else if (pPlayer->GetWaterLevel() == 3 && m_bFiresUnderwater == false)
		{
			// This weapon doesn't fire underwater
			WeaponSound(EMPTY);
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2;
		}
		else
		{
			PrimaryAttack();
		}
	}
 
	// -----------------------
	//  Reload pressed / Clip Empty
	// -----------------------
	if ( pPlayer->m_nButtons & IN_RELOAD && UsesClipsForAmmo1() && !m_bInReload) 
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
 
	// -----------------------
	//  No buttons down
	// -----------------------
	if (!((pPlayer->m_nButtons & IN_ATTACK) || (pPlayer->m_nButtons & IN_ATTACK2) || (pPlayer->m_nButtons & IN_RELOAD)))
	{
		// no fire buttons down or reloading
		if ( !ReloadOrSwitchWeapons() && ( m_bInReload == false ) )
		{
			WeaponIdle();
		}
	}
 
	// Tony; decrease shots fired count - tweak the time as necessary.
	if ( !( pPlayer->m_nButtons & IN_ATTACK ) )
	{
		//Tony; check firemodes -- If we're semi or burst, we will clear shots fired now that the player has let go of the button.
		switch(GetFireMode())
		{
		case FM_SEMIAUTOMATIC:
			if (pPlayer->GetShotsFired() > 0)
				pPlayer->ClearShotsFired();
			break;
			//Tony; TODO; add an accessor to determine the MAX burst on a per-weapon basis!!
			//DONE!
		case FM_BURST:
			if (pPlayer->GetShotsFired() > MaxBurstShots())
				pPlayer->ClearShotsFired();
			break;
		}
 
		m_bFireOnEmpty = false;
		if ( (pPlayer->GetShotsFired() > 0) && (m_flDecreaseShotsFired < gpGlobals->curtime)	)
		{
			m_flDecreaseShotsFired = gpGlobals->curtime + 0.05495;
			pPlayer->DecreaseShotsFired();
		}
	}
}
 
void CWeaponSDKBase::WeaponIdle( void )
{
	//Idle again if we've finished
	if ( HasWeaponIdleTimeElapsed() )
	{
		SendWeaponAnim( GetIdleActivity() );
		SetWeaponIdleTime( gpGlobals->curtime + SequenceDuration() );
	}
}
bool CWeaponSDKBase::Reload( void )
{
	bool fRet;
	float fCacheTime = m_flNextSecondaryAttack;
 
	fRet = DefaultReload( GetMaxClip1(), GetMaxClip2(), GetReloadActivity() );
	if ( fRet )
	{
 
		SendReloadEvents();
 
		// Undo whatever the reload process has done to our secondary
		// attack timer. We allow you to interrupt reloading to fire
		// a grenade.
		m_flNextSecondaryAttack = GetOwner()->m_flNextAttack = fCacheTime;
 
		WeaponSound( RELOAD );
		if (GetPlayerOwner()) 
			GetPlayerOwner()->ClearShotsFired();
	}
 
	return fRet;
}
void CWeaponSDKBase::SendReloadEvents()
{
	CSDKPlayer *pPlayer = GetPlayerOwner();
	if ( !pPlayer )
		return;
 
#ifdef GAME_DLL
	// Send a message to any clients that have this entity to play the reload.
	CPASFilter filter( pPlayer->GetAbsOrigin() );
	filter.RemoveRecipient( pPlayer );
 
	UserMessageBegin( filter, "ReloadEffect" );
	WRITE_SHORT( pPlayer->entindex() );
	MessageEnd();
#endif
	// Make the player play his reload animation.
	pPlayer->DoAnimationEvent( PLAYERANIMEVENT_RELOAD );
}
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
bool CWeaponSDKBase::Deploy( )
{
	//MDLCACHE_CRITICAL_SECTION();
 
	//Tony; on deploy clear shots fired.
	if (GetPlayerOwner())
		GetPlayerOwner()->ClearShotsFired();
 
	return DefaultDeploy( (char*)GetViewModel(), (char*)GetWorldModel(), GetDeployActivity(), (char*)GetAnimPrefix() );
}
 
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
//Tony; use the same name as the base context one.
#define SDK_HIDEWEAPON_THINK_CONTEXT			"BaseCombatWeapon_HideThink"
bool CWeaponSDKBase::Holster( CBaseCombatWeapon *pSwitchingTo )
{ 
	//MDLCACHE_CRITICAL_SECTION();
 
	// cancel any reload in progress.
	m_bInReload = false; 
 
	// kill any think functions
	SetThink(NULL);
 
	// Some weapon's don't have holster anims yet, so detect that
	float flSequenceDuration = 0;
	SendWeaponAnim( GetHolsterActivity() );
	{
		flSequenceDuration = SequenceDuration();
	}
 
	CBaseCombatCharacter *pOwner = GetOwner();
	if (pOwner)
	{
		pOwner->SetNextAttack( gpGlobals->curtime + flSequenceDuration );
	}
 
	// If we don't have a holster anim, hide immediately to avoid timing issues
	if ( !flSequenceDuration )
	{
		SetWeaponVisible( false );
	}
	else
	{
		// Hide the weapon when the holster animation's finished
		SetContextThink( &CBaseCombatWeapon::HideThink, gpGlobals->curtime + flSequenceDuration, SDK_HIDEWEAPON_THINK_CONTEXT );
	}
 
	return true;
}
 
#ifdef GAME_DLL
void CWeaponSDKBase::SetDieThink( bool bDie )
{
	if( bDie )
		SetContextThink( &CWeaponSDKBase::Die, gpGlobals->curtime + 45.0f, "DieContext" );
	else
		SetContextThink( NULL, gpGlobals->curtime, "DieContext" );
}
void CWeaponSDKBase::Die( void )
{
	UTIL_Remove( this );
}
#endif

#ifdef CLIENT_DLL
ConVar cl_bobcycle( "cl_bobcycle", "0.45", 0 , "How fast the bob cycles", true, 0.01f, false, 0.0f );
ConVar cl_bobup( "cl_bobup", "0.5", 0 , "Don't change...", true, 0.01f, true, 0.99f );
ConVar cl_bobvertscale( "cl_bobvertscale", "0.6", 0, "Vertical scale" ); // Def. is 0.1
ConVar cl_boblatscale( "cl_boblatscale", "0.8", 0, "Lateral scale" );
ConVar cl_bobenable( "cl_bobenable", "1" );

float g_lateralBob;
float g_verticalBob;

float CWeaponSDKBase::CalcViewmodelBob()
{
    static float	bobtime;
    static float	lastbobtime;
    float	cycle;

    float	bobup = cl_bobup.GetFloat();
    float	bobcycle = cl_bobcycle.GetFloat();

    C_BasePlayer* player = GetPlayerOwner();

    //NOTENOTE: For now, let this cycle continue when in the air, because it snaps badly without it
    if (!player || !gpGlobals->frametime || bobcycle <= 0.0f || bobup <= 0.0f || bobup >= 1.0f )
        return 0.0f;

    float speed = player->GetLocalVelocity().Length2D();

    speed = clamp( speed, -320, 320 );

    float bob_offset = RemapVal( speed, 0, 320, 0.0f, 1.0f );
    
    bobtime += ( gpGlobals->curtime - lastbobtime ) * bob_offset;
    lastbobtime = gpGlobals->curtime;

    //Calculate the vertical bob
    cycle = bobtime - (int)(bobtime/bobcycle)*bobcycle;
    cycle /= bobcycle;

    if ( cycle < bobup )
        cycle = M_PI * cycle / bobup;
    else
        cycle = M_PI + M_PI*(cycle-bobup)/(1.0 - bobup);
   
    g_verticalBob = speed*0.005f;
    g_verticalBob = g_verticalBob*0.3 + g_verticalBob*0.7*sin(cycle);

    g_verticalBob = clamp( g_verticalBob, -7.0f, 4.0f );

    //Calculate the lateral bob
    cycle = bobtime - (int)(bobtime/bobcycle*2)*bobcycle*2;
    cycle /= bobcycle*2;

    if ( cycle < bobup )
        cycle = M_PI * cycle / bobup;
    else
        cycle = M_PI + M_PI*(cycle-bobup)/(1.0 - bobup);

    g_lateralBob = speed*0.005f;
    g_lateralBob = g_lateralBob*0.3 + g_lateralBob*0.7*sin(cycle);
    g_lateralBob = clamp( g_lateralBob, -7.0f, 4.0f );
    
    //NOTENOTE: We don't use this return value in our case (need to restructure the calculation function setup!)
    return 0.0f;
}

void CWeaponSDKBase::AddViewmodelBob( CBaseViewModel *viewmodel, Vector& origin, QAngle& angles )
{
    if ( !cl_bobenable.GetBool() )
        return;

    Vector	forward, right;
    AngleVectors( angles, &forward, &right, NULL );

    CalcViewmodelBob();

    // Apply bob, but scaled down to 40%
    VectorMA( origin, g_verticalBob * cl_bobvertscale.GetFloat(), forward, origin );
    
    // Z bob a bit more
    origin[2] += g_verticalBob * 0.1f;
    
    // bob the angles
    angles[ ROLL ]	+= g_verticalBob * 0.5f;
    angles[ PITCH ]	-= g_verticalBob * 0.4f;

    angles[ YAW ]	-= g_lateralBob  * 0.3f;

    VectorMA( origin, g_lateralBob * cl_boblatscale.GetFloat(), right, origin );
}
#endif