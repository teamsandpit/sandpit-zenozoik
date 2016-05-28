//===== Copyright © 1996-2008, Valve Corporation, All rights reserved. ======//
//
// Purpose: 
//
// $NoKeywords: $
//
//===========================================================================//

#include <stdio.h>


#include <stdio.h>
#include "interface.h"
#include "filesystem.h"
#include "engine/iserverplugin.h"
#include "eiface.h"
#include "igameevents.h"
#include "convar.h"
#include "Color.h"
#include "vstdlib/random.h"
#include "engine/IEngineTrace.h"
#include "tier2/tier2.h"
#include "game/server/iplayerinfo.h"

#include "czeno_player.h"

// Uncomment this to compile the sample TF2 plugin code, note: most of this is duplicated in serverplugin_tony, but kept here for reference!
//#define SAMPLE_TF2_PLUGIN
// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// Interfaces from the engine
IVEngineServer	*engine = NULL; // helper functions (messaging clients, loading content, making entities, running commands, etc)
IGameEventManager *gameeventmanager = NULL; // game events interface
IPlayerInfoManager *playerinfomanager = NULL; // game dll interface to interact with players
IBotManager *botmanager = NULL; // game dll interface to interact with bots
IServerPluginHelpers *helpers = NULL; // special 3rd party plugin helpers from the engine
IUniformRandomStream *randomStr = NULL;
IEngineTrace *enginetrace = NULL;


CGlobalVars *gpGlobals = NULL;

// function to initialize any cvars/command in this plugin
void Bot_RunAll( void ); 

// useful helper func
inline bool FStrEq(const char *sz1, const char *sz2)
{
	return(Q_stricmp(sz1, sz2) == 0);
}
//---------------------------------------------------------------------------------
// Purpose: a sample 3rd party plugin class
//---------------------------------------------------------------------------------
class CEmptyServerPlugin: public IServerPluginCallbacks, public IGameEventListener
{
public:
	CEmptyServerPlugin();
	~CEmptyServerPlugin();

	// IServerPluginCallbacks methods
	virtual bool			Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory );
	virtual void			Unload( void );
	virtual void			Pause( void );
	virtual void			UnPause( void );
	virtual const char     *GetPluginDescription( void );      
	virtual void			LevelInit( char const *pMapName );
	virtual void			ServerActivate( edict_t *pEdictList, int edictCount, int clientMax );
	virtual void			GameFrame( bool simulating );
	virtual void			LevelShutdown( void );
	virtual void			ClientActive( edict_t *pEntity );
	virtual void			ClientDisconnect( edict_t *pEntity );
	virtual void			ClientPutInServer( edict_t *pEntity, char const *playername );
	virtual void			SetCommandClient( int index );
	virtual void			ClientSettingsChanged( edict_t *pEdict );
	virtual PLUGIN_RESULT	ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen );
	virtual PLUGIN_RESULT	ClientCommand( edict_t *pEntity, const CCommand &args );
	virtual PLUGIN_RESULT	NetworkIDValidated( const char *pszUserName, const char *pszNetworkID );
	virtual void			OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue );

	// added with version 3 of the interface.
	virtual void			OnEdictAllocated( edict_t *edict );
	virtual void			OnEdictFreed( const edict_t *edict  );	

	// IGameEventListener Interface
	virtual void FireGameEvent( KeyValues * event );

	virtual int GetCommandIndex() { return m_iClientCommandIndex; }
private:
	int m_iClientCommandIndex;
};


// 
// The plugin is a static singleton that is exported as an interface
//
CEmptyServerPlugin g_EmtpyServerPlugin;
EXPOSE_SINGLE_INTERFACE_GLOBALVAR(CEmptyServerPlugin, IServerPluginCallbacks, INTERFACEVERSION_ISERVERPLUGINCALLBACKS, g_EmtpyServerPlugin );

//---------------------------------------------------------------------------------
// Purpose: constructor/destructor
//---------------------------------------------------------------------------------
CEmptyServerPlugin::CEmptyServerPlugin()
{
	m_iClientCommandIndex = 0;
}

CEmptyServerPlugin::~CEmptyServerPlugin()
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is loaded, load the interface we need from the engine
//---------------------------------------------------------------------------------
bool CEmptyServerPlugin::Load(	CreateInterfaceFn interfaceFactory, CreateInterfaceFn gameServerFactory )
{
	ConnectTier1Libraries( &interfaceFactory, 1 );
	ConnectTier2Libraries( &interfaceFactory, 1 );

	playerinfomanager = (IPlayerInfoManager *)gameServerFactory(INTERFACEVERSION_PLAYERINFOMANAGER,NULL);
	if ( !playerinfomanager )
	{
		Warning( "Unable to load playerinfomanager, ignoring\n" ); // this isn't fatal, we just won't be able to access specific player data
	}

	botmanager = (IBotManager *)gameServerFactory(INTERFACEVERSION_PLAYERBOTMANAGER, NULL);
	if ( !botmanager )
	{
		Warning( "Unable to load botcontroller, ignoring\n" ); // this isn't fatal, we just won't be able to access specific bot functions
	}
	engine = (IVEngineServer*)interfaceFactory(INTERFACEVERSION_VENGINESERVER, NULL);
	gameeventmanager = (IGameEventManager *)interfaceFactory(INTERFACEVERSION_GAMEEVENTSMANAGER,NULL);
	helpers = (IServerPluginHelpers*)interfaceFactory(INTERFACEVERSION_ISERVERPLUGINHELPERS, NULL);
	enginetrace = (IEngineTrace *)interfaceFactory(INTERFACEVERSION_ENGINETRACE_SERVER,NULL);
	randomStr = (IUniformRandomStream *)interfaceFactory(VENGINE_SERVER_RANDOM_INTERFACE_VERSION, NULL);

	// get the interfaces we want to use
	if(	! ( engine && gameeventmanager && g_pFullFileSystem && helpers && enginetrace && randomStr ) )
	{
		return false; // we require all these interface to function
	}

	if ( playerinfomanager )
	{
		gpGlobals = playerinfomanager->GetGlobalVars();
	}

	MathLib_Init( 2.2f, 2.2f, 0.0f, 2.0f );
	ConVar_Register( 0 );
	return true;
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unloaded (turned off)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::Unload( void )
{
	gameeventmanager->RemoveListener( this ); // make sure we are unloaded from the event system

	ConVar_Unregister( );
	DisconnectTier2Libraries( );
	DisconnectTier1Libraries( );
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is paused (i.e should stop running but isn't unloaded)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::Pause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when the plugin is unpaused (i.e should start executing again)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::UnPause( void )
{
}

//---------------------------------------------------------------------------------
// Purpose: the name of this plugin, returned in "plugin_print" command
//---------------------------------------------------------------------------------
const char *CEmptyServerPlugin::GetPluginDescription( void )
{
	return "Sandpit: Zenozoik";
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::LevelInit( char const *pMapName )
{
	Msg( "Level \"%s\" has been loaded\n", pMapName );
	gameeventmanager->AddListener( this, true );
}

//---------------------------------------------------------------------------------
// Purpose: called on level start, when the server is ready to accept client connections
//		edictCount is the number of entities in the level, clientMax is the max client count
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ServerActivate( edict_t *pEdictList, int edictCount, int clientMax )
{
}

//---------------------------------------------------------------------------------
// Purpose: called once per server frame, do recurring work here (like checking for timeouts)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::GameFrame( bool simulating )
{
	if ( simulating )
	{
		Bot_RunAll();
	}
}

//---------------------------------------------------------------------------------
// Purpose: called on level end (as the server is shutting down or going to a new map)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::LevelShutdown( void ) // !!!!this can get called multiple times per map change
{
	gameeventmanager->RemoveListener( this );
}

VFuncs f;

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientActive( edict_t *pEntity )
{
	// TODO: set the origin to the location of a random info_player_deathmatch
	helpers->ClientCommand( pEntity, "combaton" );
	Msg("%s\n", STRING(f.GetModelName(pEntity->GetUnknown()->GetBaseEntity())));
	f.SetModel( pEntity->GetUnknown()->GetBaseEntity(), "models/weapons/skullbombs/skullbomb.mdl" );
	Msg("%s\n", STRING(f.GetModelName(pEntity->GetUnknown()->GetBaseEntity())));
}

//---------------------------------------------------------------------------------
// Purpose: called when a client leaves a server (or is timed out)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientDisconnect( edict_t *pEntity )
{
}

//---------------------------------------------------------------------------------
// Purpose: called on 
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientPutInServer( edict_t *pEntity, char const *playername )
{
#if 0
	KeyValues *kv = new KeyValues( "msg" );
	kv->SetString( "title", "Hello" );
	kv->SetString( "msg", "Hello there" );
	kv->SetColor( "color", Color( 255, 0, 0, 255 ));
	kv->SetInt( "level", 5);
	kv->SetInt( "time", 10);
	helpers->CreateMessage( pEntity, DIALOG_MSG, kv, this );
	kv->deleteThis();
#endif
}

//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::SetCommandClient( int index )
{
	m_iClientCommandIndex = index;
}

void ClientPrint( edict_t *pEdict, char *format, ... )
{
	va_list		argptr;
	static char		string[1024];
	
	va_start (argptr, format);
	Q_vsnprintf(string, sizeof(string), format,argptr);
	va_end (argptr);

	engine->ClientPrintf( pEdict, string );
}
//---------------------------------------------------------------------------------
// Purpose: called on level start
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientSettingsChanged( edict_t *pEdict )
{
	if ( playerinfomanager )
	{
		IPlayerInfo *playerinfo = playerinfomanager->GetPlayerInfo( pEdict );

		const char * name = engine->GetClientConVarValue( engine->IndexOfEdict(pEdict), "name" );

		if ( playerinfo && name && playerinfo->GetName() && 
			 Q_stricmp( name, playerinfo->GetName()) ) // playerinfo may be NULL if the MOD doesn't support access to player data 
													   // OR if you are accessing the player before they are fully connected
		{
			ClientPrint( pEdict, "Your name changed to \"%s\" (from \"%s\"\n", name, playerinfo->GetName() );
						// this is the bad way to check this, the better option it to listen for the "player_changename" event in FireGameEvent()
						// this is here to give a real example of how to use the playerinfo interface
		}
	}
}

// https://wiki.alliedmods.net/CINSPlayer_Offset_List_(Insurgency)

class VfuncEmptyClass {};

typedef void* lpvoid;

const string_t VFuncs::GetModelName(CBaseEntity *pThisPtr)
{
	// get this
	void **this_ptr = *(void ***)&pThisPtr;
	// get the vtable as an array of void *
	void **vtable = *(void ***)pThisPtr;
	void *func = vtable[7]; 

	// use a union to get the address as a function pointer
	union
	{
		const string_t (VfuncEmptyClass::*mfpnew)(void);
	#ifndef __linux__
		void *addr;
	} u; 
	
	u.addr = func;
	#else // GCC's member function pointers all contain a this pointer adjustor. You'd probably set it to 0 
		struct
		{
			void *addr;
			intptr_t adjustor;
		} s;
	} u;
	
	u.s.addr = func;
	u.s.adjustor = 0;
	#endif
 
	return (const string_t) (reinterpret_cast<VfuncEmptyClass*>(this_ptr)->*u.mfpnew)();
}

void VFuncs::Spawn(CBaseEntity *pThisPtr)
{
	// get this
	void **this_ptr = *(void ***)&pThisPtr;
	// get the vtable as an array of void *
	void **vtable = *(void ***)pThisPtr;
	void *func = vtable[21]; 

	// use a union to get the address as a function pointer
	union
	{
		void (VfuncEmptyClass::*mfpnew)(void);
	#ifndef __linux__
		void *addr;
	} u; 
	
	u.addr = func;
	#else // GCC's member function pointers all contain a this pointer adjustor. You'd probably set it to 0 
		struct
		{
			void *addr;
			intptr_t adjustor;
		} s;
	} u;
	
	u.s.addr = func;
	u.s.adjustor = 0;
	#endif
 
	(void) (reinterpret_cast<VfuncEmptyClass*>(this_ptr)->*u.mfpnew)();
}

void VFuncs::SetModel(CBaseEntity *pThisPtr, const char *name)
{
	// http://kaisar-haque.blogspot.com.au/2008/07/c-accessing-virtual-table.html

	// get this
	void **this_ptr = *(void ***)&pThisPtr;
	// get the vtable as an array of void *
	void **vtable = *(void ***)pThisPtr;
	void *func = vtable[24]; 

	// use a union to get the address as a function pointer
	union
	{
		void (VfuncEmptyClass::*mfpnew)(const char *);
	#ifndef __linux__
		void *addr;
	} u; 
	
	u.addr = func;
	#else // GCC's member function pointers all contain a this pointer adjustor. You'd probably set it to 0 
		struct
		{
			void *addr;
			intptr_t adjustor;
		} s;
	} u;
	
	u.s.addr = func;
	u.s.adjustor = 0;
	#endif
 
	(void) (reinterpret_cast<VfuncEmptyClass*>(this_ptr)->*u.mfpnew)(name);
}

// https://wiki.alliedmods.net/Virtual_Offsets_(Source_Mods)
void VFuncs::ZenoCombat(CBaseEntity *pThisPtr, int on_or_off)
{
	// http://kaisar-haque.blogspot.com.au/2008/07/c-accessing-virtual-table.html

	// get this
	void **this_ptr = *(void ***)&pThisPtr;
	// get the vtable as an array of void *
	void **vtable = *(void ***)pThisPtr;
	void *func = vtable[416]; 

	// use a union to get the address as a function pointer
	union
	{
		void (VfuncEmptyClass::*mfpnew)(int);
	#ifndef __linux__
		void *addr;
	} u; 
	
	u.addr = func;
	#else // GCC's member function pointers all contain a this pointer adjustor. You'd probably set it to 0 
		struct
		{
			void *addr;
			intptr_t adjustor;
		} s;
	} u;
	
	u.s.addr = func;
	u.s.adjustor = 0;
	#endif
 
	(void) (reinterpret_cast<VfuncEmptyClass*>(this_ptr)->*u.mfpnew)(on_or_off);
}

//---------------------------------------------------------------------------------
// Purpose: called when a client joins a server
//---------------------------------------------------------------------------------
PLUGIN_RESULT CEmptyServerPlugin::ClientConnect( bool *bAllowConnect, edict_t *pEntity, const char *pszName, const char *pszAddress, char *reject, int maxrejectlen )
{
	return PLUGIN_CONTINUE;
}

#if 0
CON_COMMAND( DoAskConnect, "Server plugin example of using the ask connect dialog" )
{
	if ( args.ArgC() < 2 )
	{
		Warning ( "DoAskConnect <server IP>\n" );
	}
	else
	{
		const char *pServerIP = args.Arg( 1 );

		KeyValues *kv = new KeyValues( "menu" );
		kv->SetString( "title", pServerIP );	// The IP address of the server to connect to goes in the "title" field.
		kv->SetInt( "time", 3 );

		for ( int i=1; i < gpGlobals->maxClients; i++ )
		{
			edict_t *pEdict = engine->PEntityOfEntIndex( i );
			if ( pEdict )
			{
				helpers->CreateMessage( pEdict, DIALOG_ASKCONNECT, kv, &g_EmtpyServerPlugin );
			}
		}

		kv->deleteThis();
	}
}
#endif

//---------------------------------------------------------------------------------
// Purpose: called when a client types in a command (only a subset of commands however, not CON_COMMAND's)
//---------------------------------------------------------------------------------
PLUGIN_RESULT CEmptyServerPlugin::ClientCommand( edict_t *pEntity, const CCommand &args )
{
	const char *pcmd = args[0];

	if ( !pEntity || pEntity->IsFree() ) 
	{
		return PLUGIN_CONTINUE;
	}

	// TODO: look at UserMessageBegin (use GetUserMessageInfo to get information about messages) for ShowMenu or VGUIMenu

	if ( FStrEq( pcmd, "controlmenu" ) )
	{
		KeyValues *kv = new KeyValues( "controlmenu" );
		kv->SetString( "title", "Control Menu: hit ESC to open or close" );
		kv->SetInt( "level", 0 );
		kv->SetInt( "time", 200 );
		kv->SetColor( "color", Color( 255, 0, 0, 255 ));
		kv->SetString( "msg", "Click an option then hit ESC to return to the game." );

		for( int i = 1; i < 6; i++ )
		{
			char num[4], msg[128], cmd[128];
			Q_snprintf( num, sizeof(num), "%i", i );

			switch (i)
			{
			case 1:
				Q_snprintf( msg, sizeof(msg), "ai_disable 1", i );
				Q_snprintf( cmd, sizeof(cmd), "ai_disable 1", i );
				
				break;
			case 2:
				Q_snprintf( msg, sizeof(msg), "ai_disable 0", i );
				Q_snprintf( cmd, sizeof(cmd), "ai_disable 0", i );

				break;
			case 3:
				Q_snprintf( msg, sizeof(msg), "notarget", i );
				Q_snprintf( cmd, sizeof(cmd), "notarget", i );

				break;
			case 4:
				Q_snprintf( msg, sizeof(msg), "zenocombaton", i );
				Q_snprintf( cmd, sizeof(cmd), "zenocombaton", i );

				break;
			case 5:
				Q_snprintf( msg, sizeof(msg), "zenocombatoff", i );
				Q_snprintf( cmd, sizeof(cmd), "zenocombatoff", i );

				break;
			}

			KeyValues *item1 = kv->FindKey( num, true );
			item1->SetString( "msg", msg );
			item1->SetString( "command", cmd );
		}

		helpers->CreateMessage( pEntity, DIALOG_MENU, kv, this );

		kv->deleteThis();
		return PLUGIN_STOP; // we handled this function
	}
	else if ( FStrEq( pcmd, "spawnmenu" ) )
	{
		KeyValues *kv = new KeyValues( "spawnmenu" );
		kv->SetString( "title", "Spawn Menu: hit ESC to open or close" );
		kv->SetInt( "level", 0 );
		kv->SetInt( "time", 200 );
		kv->SetColor( "color", Color( 255, 0, 0, 255 ));
		kv->SetString( "msg", "Click an option then hit ESC to return to the game." );

		for( int i = 1; i < 6; i++ )
		{
			char num[4], msg[128], cmd[128];
			Q_snprintf( num, sizeof(num), "%i", i );

			switch (i)
			{
			case 1:
				Q_snprintf( msg, sizeof(msg), "Deadra", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_deadra", i );

				break;
			case 2:
				Q_snprintf( msg, sizeof(msg), "Deinother", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_elephant1", i );

				break;
			case 3:
				Q_snprintf( msg, sizeof(msg), "FatherMother", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_fathermother", i );

				break;
			case 4:
				Q_snprintf( msg, sizeof(msg), "Gastornis", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_gastornis", i );

				break;
			case 5:
				Q_snprintf( msg, sizeof(msg), "Mechanic", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_mechanic", i );

				break;
			case 6:
				Q_snprintf( msg, sizeof(msg), "Jsaj", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_punk", i );

				break;
			case 7:
				Q_snprintf( msg, sizeof(msg), "Tsekung", i );
				Q_snprintf( cmd, sizeof(cmd), "npc_create npc_zeno_tsekung", i );

				break;
			}

			strcat(cmd, "; ent_fire !picker SetHealth 70;");

			KeyValues *item1 = kv->FindKey( num, true );
			item1->SetString( "msg", msg );
			item1->SetString( "command", cmd );
		}

		helpers->CreateMessage( pEntity, DIALOG_MENU, kv, this );

		kv->deleteThis();
		return PLUGIN_STOP; // we handled this function
	}
	else if (FStrEq( pcmd, "combaton" ))
	{
		Msg("zenocombaton for entity %d\n", engine->IndexOfEdict(pEntity));

		// Vector origin = playerinfomanager->GetPlayerInfo(pEntity)->GetAbsOrigin();

		// Msg("origin %f %f %f\n", origin.x, origin.y, origin.z);

		CBaseEntity *pent = pEntity->GetUnknown()->GetBaseEntity();

		if (pent)
		{
			f.ZenoCombat(pent, 1);
		}
		else
		{
			Msg("NULL pent\n");
		}

		return PLUGIN_STOP; // we handled this function
	}
	else if (FStrEq( pcmd, "combatoff" ))
	{
		Msg("zenocombatoff for entity %d\n", engine->IndexOfEdict(pEntity));

		CBaseEntity *pent = pEntity->GetUnknown()->GetBaseEntity();

		if (pent)
		{
			f.ZenoCombat(pent, 0);
		}
		else
		{
			Msg("NULL pent\n");
		}

		return PLUGIN_STOP; // we handled this function
	}

	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a client is authenticated
//---------------------------------------------------------------------------------
PLUGIN_RESULT CEmptyServerPlugin::NetworkIDValidated( const char *pszUserName, const char *pszNetworkID )
{
	return PLUGIN_CONTINUE;
}

//---------------------------------------------------------------------------------
// Purpose: called when a cvar value query is finished
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::OnQueryCvarValueFinished( QueryCvarCookie_t iCookie, edict_t *pPlayerEntity, EQueryCvarValueStatus eStatus, const char *pCvarName, const char *pCvarValue )
{
	Msg( "Cvar query (cookie: %d, status: %d) - name: %s, value: %s\n", iCookie, eStatus, pCvarName, pCvarValue );
}
void CEmptyServerPlugin::OnEdictAllocated( edict_t *edict )
{
}
void CEmptyServerPlugin::OnEdictFreed( const edict_t *edict  )
{
}

//---------------------------------------------------------------------------------
// Purpose: called when an event is fired
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::FireGameEvent( KeyValues * event )
{
	const char * name = event->GetName();
	Msg( "CEmptyServerPlugin::FireGameEvent: Got event \"%s\"\n", name );

	// see CBaseEntity::SendOnKilledGameEvent
	if (FStrEq(name, "entity_killed"))
	{
		KeyValues *pEntKilled = event->FindKey("entindex_killed", false);

		CBaseEntity *pent = engine->PEntityOfEntIndex( pEntKilled->GetInt() )->GetUnknown()->GetBaseEntity();

		if (pent)
		{
			// see CHL2MP_Player::Spawn - spawn, then reset the flags etc
			Msg("respawning\n");
			f.Spawn(pent);

			// pl.deadflag = false;
			// RemoveSolidFlags( FSOLID_NOT_SOLID );

			// RemoveEffects( EF_NODRAW );
		}
		else
		{
			Msg("NULL pent\n");
		}
	}
	else if (FStrEq(name, "player_damage"))
	{
		KeyValues *pKey = event->FindKey("damage", false);

		Msg("damage %d\n", pKey->GetInt());
	}

	/*
	CEmptyServerPlugin::FireGameEvent: Got event "entity_killed"
	Key name: entindex_killed type 0
	Key name: entindex_attacker type 0
	Key name: entindex_inflictor type 0
	Key name: damagebits type 0
	*/

	for ( KeyValues *pKey = event->GetFirstSubKey(); pKey; pKey = pKey->GetNextKey() )
	{
		Msg( "Key name: %s type %d\n", pKey->GetName(), pKey->GetDataType(pKey->GetName()) );
	}
}

#if 0
//---------------------------------------------------------------------------------
// Purpose: an example of how to implement a new command
//---------------------------------------------------------------------------------
CON_COMMAND( empty_version, "prints the version of the empty plugin" )
{
	Msg( "Version:2.0.0.0\n" );
}

CON_COMMAND( empty_log, "logs the version of the empty plugin" )
{
	engine->LogPrint( "Version:2.0.0.0\n" );
}

//---------------------------------------------------------------------------------
// Purpose: an example cvar
//---------------------------------------------------------------------------------
static ConVar empty_cvar("plugin_empty", "0", 0, "Example plugin cvar");
#endif