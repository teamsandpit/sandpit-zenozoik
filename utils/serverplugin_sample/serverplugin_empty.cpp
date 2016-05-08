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

//---------------------------------------------------------------------------------
// Purpose: called when a client spawns into a server (i.e as they begin to play)
//---------------------------------------------------------------------------------
void CEmptyServerPlugin::ClientActive( edict_t *pEntity )
{
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
class CZeno_Player
{
public:
	virtual ~CZeno_Player() {}
	virtual void Unknown2() = 0;
	virtual void Unknown3() = 0;
	virtual void Unknown4() = 0;
	virtual void Unknown5() = 0;
	virtual void Unknown6() = 0;
	virtual void Unknown7() = 0;
	virtual void Unknown8() = 0;
	virtual void Unknown9() = 0;
	virtual void Unknown10() = 0;
	virtual void Unknown11() = 0;
	virtual void Unknown12() = 0;
	virtual void Unknown13() = 0;
	virtual void Unknown14() = 0;
	virtual void Unknown15() = 0;
	virtual void Unknown16() = 0;
	virtual void Unknown17() = 0;
	virtual void Unknown18() = 0;
	virtual void Unknown19() = 0;
	virtual void Unknown20() = 0;
	virtual void Unknown21() = 0;
	virtual void Spawn() = 0;
	virtual void Precache() = 0;
	virtual void Unknown24() = 0;
	virtual void Unknown25() = 0;
	virtual void Unknown26() = 0;
	virtual void Unknown27() = 0;
	virtual void Unknown28() = 0;
	virtual void Unknown29() = 0;
	virtual void Unknown30() = 0;
	virtual void Unknown31() = 0;
	virtual void Unknown32() = 0;
	virtual void Unknown33() = 0;
	virtual void Unknown34() = 0;
	virtual void Unknown35() = 0;
	virtual void Unknown36() = 0;
	virtual void Unknown37() = 0;
	virtual void Unknown38() = 0;
	virtual void Unknown39() = 0;
	virtual void Unknown40() = 0;
	virtual void Unknown41() = 0;
	virtual void Unknown42() = 0;
	virtual void Unknown43() = 0;
	virtual void Unknown44() = 0;
	virtual void Unknown45() = 0;
	virtual void Unknown46() = 0;
	virtual void Unknown47() = 0;
	virtual void Unknown48() = 0;
	virtual void Unknown49() = 0;
	virtual void Unknown50() = 0;
	virtual void Unknown51() = 0;
	virtual void Unknown52() = 0;
	virtual void Unknown53() = 0;
	virtual void Unknown54() = 0;
	virtual void Unknown55() = 0;
	virtual void Unknown56() = 0;
	virtual void Unknown57() = 0;
	virtual void Unknown58() = 0;
	virtual void Unknown59() = 0;
	virtual void Unknown60() = 0;
	virtual void Unknown61() = 0;
	virtual void Unknown62() = 0;
	virtual void Unknown63() = 0;
	virtual void Unknown64() = 0;
	virtual void Unknown65() = 0;
	virtual void Unknown66() = 0;
	virtual void Unknown67() = 0;
	virtual void Unknown68() = 0;
	virtual void Unknown69() = 0;
	virtual void Unknown70() = 0;
	virtual void Unknown71() = 0;
	virtual void Unknown72() = 0;
	virtual void Unknown73() = 0;
	virtual void Unknown74() = 0;
	virtual void Unknown75() = 0;
	virtual void Unknown76() = 0;
	virtual void Unknown77() = 0;
	virtual void Unknown78() = 0;
	virtual void Unknown79() = 0;
	virtual void Unknown80() = 0;
	virtual void Unknown81() = 0;
	virtual void Unknown82() = 0;
	virtual void Unknown83() = 0;
	virtual void Unknown84() = 0;
	virtual void Unknown85() = 0;
	virtual void Unknown86() = 0;
	virtual void Unknown87() = 0;
	virtual void Unknown88() = 0;
	virtual void Unknown89() = 0;
	virtual void Unknown90() = 0;
	virtual void Unknown91() = 0;
	virtual void Unknown92() = 0;
	virtual void Unknown93() = 0;
	virtual void Unknown94() = 0;
	virtual void Unknown95() = 0;
	virtual void Unknown96() = 0;
	virtual void Unknown97() = 0;
	virtual void Unknown98() = 0;
	virtual void Unknown99() = 0;
	virtual void Unknown100() = 0;
	virtual void Unknown101() = 0;
	virtual void Unknown102() = 0;
	virtual void Unknown103() = 0;
	virtual void Unknown104() = 0;
	virtual void Unknown105() = 0;
	virtual void Unknown106() = 0;
	virtual void Unknown107() = 0;
	virtual void Unknown108() = 0;
	virtual void Unknown109() = 0;
	virtual void Unknown110() = 0;
	virtual void Unknown111() = 0;
	virtual void Unknown112() = 0;
	virtual void Unknown113() = 0;
	virtual void Unknown114() = 0;
	virtual void Unknown115() = 0;
	virtual void Unknown116() = 0;
	virtual void Unknown117() = 0;
	virtual void Unknown118() = 0;
	virtual void Unknown119() = 0;
	virtual void Unknown120() = 0;
	virtual void Unknown121() = 0;
	virtual void Unknown122() = 0;
	virtual void Unknown123() = 0;
	virtual void Unknown124() = 0;
	virtual void Unknown125() = 0;
	virtual void Unknown126() = 0;
	virtual void Unknown127() = 0;
	virtual void Unknown128() = 0;
	virtual void Unknown129() = 0;
	virtual void Unknown130() = 0;
	virtual void Unknown131() = 0;
	virtual void Unknown132() = 0;
	virtual void Unknown133() = 0;
	virtual void Unknown134() = 0;
	virtual void Unknown135() = 0;
	virtual void Unknown136() = 0;
	virtual void Unknown137() = 0;
	virtual void Unknown138() = 0;
	virtual void Unknown139() = 0;
	virtual void Unknown140() = 0;
	virtual void Unknown141() = 0;
	virtual void Unknown142() = 0;
	virtual void Unknown143() = 0;
	virtual void Unknown144() = 0;
	virtual void Unknown145() = 0;
	virtual void Unknown146() = 0;
	virtual void Unknown147() = 0;
	virtual void Unknown148() = 0;
	virtual void Unknown149() = 0;
	virtual void Unknown150() = 0;
	virtual void Unknown151() = 0;
	virtual void Unknown152() = 0;
	virtual void Unknown153() = 0;
	virtual void Unknown154() = 0;
	virtual void Unknown155() = 0;
	virtual void Unknown156() = 0;
	virtual void Unknown157() = 0;
	virtual void Unknown158() = 0;
	virtual void Unknown159() = 0;
	virtual void Unknown160() = 0;
	virtual void Unknown161() = 0;
	virtual void Unknown162() = 0;
	virtual void Unknown163() = 0;
	virtual void Unknown164() = 0;
	virtual void Unknown165() = 0;
	virtual void Unknown166() = 0;
	virtual void Unknown167() = 0;
	virtual void Unknown168() = 0;
	virtual void Unknown169() = 0;
	virtual void Unknown170() = 0;
	virtual void Unknown171() = 0;
	virtual void Unknown172() = 0;
	virtual void Unknown173() = 0;
	virtual void Unknown174() = 0;
	virtual void Unknown175() = 0;
	virtual void Unknown176() = 0;
	virtual void Unknown177() = 0;
	virtual void Unknown178() = 0;
	virtual void Unknown179() = 0;
	virtual void Unknown180() = 0;
	virtual void Unknown181() = 0;
	virtual void Unknown182() = 0;
	virtual void Unknown183() = 0;
	virtual void Unknown184() = 0;
	virtual void Unknown185() = 0;
	virtual void Unknown186() = 0;
	virtual void Unknown187() = 0;
	virtual void Unknown188() = 0;
	virtual void Unknown189() = 0;
	virtual void Unknown190() = 0;
	virtual void Unknown191() = 0;
	virtual void Unknown192() = 0;
	virtual void Unknown193() = 0;
	virtual void Unknown194() = 0;
	virtual void Unknown195() = 0;
	virtual void Unknown196() = 0;
	virtual void Unknown197() = 0;
	virtual void Unknown198() = 0;
	virtual void Unknown199() = 0;
};

class VfuncEmptyClass {};

class VFuncs
{
public:
	void SetModel(CBaseEntity *pThisPtr, const char *name);
	void ZenoCombat(CBaseEntity *pThisPtr, int on_or_off);
};

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

	VFuncs f;

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
		Msg("zenocombaton\n");

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
		Msg("zenocombatoff\n");

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