#include "chooker.h"
#include "amxxmodule.h"
#include "engine_structs.h"

#define SVC_UPDATEUSERINFO	13
// cs team offset
#if defined __linux__
const int CSTeamOffset = 119;
#elif defined WIN32
const int CSTeamOffset = 114;
#endif
const int CS_TEAM_CT = 2;
const int CS_TEAM_T = 1;
const int CS_TEAM_UNNASIGNED = 3;
const int CS_TEAM_SPECTATOR = 0;

CHooker HookerClass;
CHooker *Hooker = &HookerClass;

// Svs struct
client_t *gclientPlayers[33];
server_static_t *g_pEGV_svs;

// SV_FullClientUpdate prototype
typedef void(*SV_FullClientUpdate)(client_t * client, sizebuf_t *size);
SV_FullClientUpdate SV_FullClientUpdate_f;
CFunc *SV_FullClientUpdate_f_hook = NULL;

// TeamInfo game event.
bool g_bPatched, g_bInvisiblePlayers[33], g_bTeamInfoActive;
int g_iTeamInfo, g_iTeamInfoArg1;


// win old: 0x55 0x8B 0xEC 0x81 0xEC ? ? ? ? 0x53 0x8B 0x1D ? ? ? ? 0x56 0x57 0x8B 0x7D ? 0xB8
// linux: SV_FullClientUpdate



bool getHost(CHooker *hk) {
#if defined LINUX
	g_pEGV_svs = hk->MemorySearch<server_static_t *>("svs", (void*)gpGlobals, TRUE);
#else
	uintptr_t vBaseAddress = *reinterpret_cast<uintptr_t *>(reinterpret_cast<byte *>(g_engfuncs.pfnGetCurrentPlayer) + 8);
	g_pEGV_svs = reinterpret_cast<server_static_t *>(vBaseAddress - 4);
#endif
	if (g_pEGV_svs)
		return true;
	return false;
}

int GET_TARGETID_BY_ENGCLIENT_ENTINDEX(const client_t *pClientHandle) {
	if (pClientHandle == NULL)
		return -1;

	return ENTINDEX(pClientHandle->edict);
}
int GET_TARGETID_BY_ENGCLIENT_SVS(const client_t *pClientHandle) {
	if (pClientHandle == NULL)
		return -1;
	for (int index = 0;index <= gpGlobals->maxClients;index++) {
		if (gclientPlayers[index] == pClientHandle)
			return index;
	}
	return -1;
}

client_t *GET_ENGCLIENT_BY_TARGETID(const int iTargetID) {
	if (!g_pEGV_svs)
		return NULL;
	if (iTargetID == NULL)
		return &g_pEGV_svs->clients[gpGlobals->maxClients];
	if (MF_IsPlayerValid(iTargetID))
		return &g_pEGV_svs->clients[iTargetID - 1];
	return NULL;
}

void setClientPlayers() {
	if (g_pEGV_svs) {
		for (int index = 0;index <= gpGlobals->maxClients;index++)
				gclientPlayers[index] = GET_ENGCLIENT_BY_TARGETID(index);
	}
}


void ResetUserModel(const int iPlayer);
void SV_FullClientUpdate_f_Call(client_t * client, sizebuf_t *size) {
	//SERVER_PRINT("SV_FullClientUpdate called \n");
	
	int iPlayer = GET_TARGETID_BY_ENGCLIENT_ENTINDEX(client);
	if (!g_bInvisiblePlayers[iPlayer]) {
		if (SV_FullClientUpdate_f_hook->Disable()) {
			SV_FullClientUpdate_f(client, size);
			SV_FullClientUpdate_f_hook->Enable();
		}
		return;
	}
	MESSAGE_BEGIN(MSG_ALL, SVC_UPDATEUSERINFO);
	WRITE_BYTE(iPlayer - 1);
	WRITE_LONG(GETPLAYERUSERID(INDEXENT(iPlayer)));
	WRITE_STRING("");
	WRITE_LONG(0);
	WRITE_LONG(0);
	WRITE_LONG(0);
	WRITE_LONG(0);
	MESSAGE_END();
}

bool CreateHook_SV_FullClientUpdate() {
#if defined LINUX
	SV_FullClientUpdate_f = Hooker->MemorySearch<SV_FullClientUpdate>("SV_FullClientUpdate", (void *)gpGlobals, TRUE);
#else
	SV_FullClientUpdate_f = Hooker->MemorySearch<SV_FullClientUpdate>("0x55,0x8B,*,0x81,*,*,*,*,*,0x53,0x8B,*,*,*,*,*,0x56", (void *)gpGlobals, FALSE);
#endif
	if (SV_FullClientUpdate_f) {
		SV_FullClientUpdate_f_hook = Hooker->CreateHook((void *)SV_FullClientUpdate_f, (void *)SV_FullClientUpdate_f_Call, TRUE);
	}

	if (!(SV_FullClientUpdate_f && SV_FullClientUpdate_f_hook)) {
		SERVER_PRINT("Unable to hook the \"SV_FullClientUpdate\" engine function.\n");
		return false;
	}
	return true;
}

void OnPluginsLoaded(void) {
	g_bPatched = CreateHook_SV_FullClientUpdate() && getHost(Hooker);
	setClientPlayers();
}
void OnPluginsUnloaded(void) {
	if (g_bPatched) {
		SV_FullClientUpdate_f_hook->Disable();
	}
}

int RegUserMsg_Post(const char *pName, int iSize) {
	if (!strcmp(pName, "TeamInfo"))
	{
		g_iTeamInfo = META_RESULT_ORIG_RET(int);
		RETURN_META_VALUE(MRES_IGNORED, 0);
	}
	RETURN_META_VALUE(MRES_IGNORED, 0);
}
void MessageBegin_Post(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed) {
	if (!g_bPatched) RETURN_META(MRES_IGNORED);
	if (msg_type == g_iTeamInfo && ed == NULL) {
		g_bTeamInfoActive = true;
	}
	RETURN_META(MRES_IGNORED);
}
void WriteByte_Post(int iValue) {
	if (!g_bPatched) RETURN_META(MRES_IGNORED);
	if (g_bTeamInfoActive)
		g_iTeamInfoArg1 = iValue;
	RETURN_META(MRES_IGNORED);
}
void WriteString_Post(const char *sz) {
	if (!g_bPatched) RETURN_META(MRES_IGNORED);
	if (g_bTeamInfoActive) {
		if (!strcmp(sz, "UNASSIGNED") || !strcmp(sz, "SPECTATOR"))
			g_bTeamInfoActive = false;
	}
	RETURN_META(MRES_IGNORED);
}
void MessageEnd_Post(void) {
	if (!g_bPatched) RETURN_META(MRES_IGNORED);
	if (g_bTeamInfoActive) {
		if (g_bInvisiblePlayers[g_iTeamInfoArg1]) {
			g_bInvisiblePlayers[g_iTeamInfoArg1] = false;
			ResetUserModel(g_iTeamInfoArg1);
		}
		g_bTeamInfoActive = false;
	}
	RETURN_META(MRES_IGNORED);
}

void ResetUserModel(const int iPlayer) {
	edict_t *ePlayer = INDEXENT(iPlayer);
	char *model = ENTITY_KEYVALUE(ePlayer, "model");
	ENTITY_SET_KEYVALUE(ePlayer, "model", "");
	ENTITY_SET_KEYVALUE(ePlayer, "model", model);
}

BOOL ClientConnect_Post(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128]) {
	if (!g_bPatched)
		RETURN_META_VALUE(MRES_IGNORED, 0);
	if (!FNullEnt(pEntity)) {
		g_bInvisiblePlayers[ENTINDEX(pEntity)] = false;
	}
	RETURN_META_VALUE(MRES_IGNORED, 0);
}
void ClientDisconnect_Post(edict_t *pEntity) {
	if (!g_bPatched)
		RETURN_META(MRES_IGNORED);
	if (!FNullEnt(pEntity))
		g_bInvisiblePlayers[ENTINDEX(pEntity)] = false;
	RETURN_META(MRES_IGNORED);
}
void ClientPutInServer_Post(edict_t *pEntity) {
	if (!g_bPatched)
		RETURN_META(MRES_IGNORED);
	if (!FNullEnt(pEntity)) {
		g_bInvisiblePlayers[ENTINDEX(pEntity)] = false;
	}
	RETURN_META(MRES_IGNORED);
}
void ClientCommand(edict_t *pEntity) {
	static const char *cmd = NULL;
	cmd = CMD_ARGV(0);
	int iPlayer = ENTINDEX(pEntity);
	if (!strcmp(cmd, "amx_spectate")) {
		if (!(MF_GetPlayerFlags(iPlayer) & (1 << 2))) { // admin_kick 
			CLIENT_PRINTF(pEntity, print_console, "[InvisibleSpec] Nu ai acces la aceasta comanda !\n");
			RETURN_META(MRES_SUPERCEDE);
		}
		if (MF_IsPlayerAlive(iPlayer) || (*((int *)pEntity->pvPrivateData + CSTeamOffset)) != 3) {
			CLIENT_PRINTF(pEntity, print_console, "[InvisibleSpec] Trebuie sa fii mort sau spec !\n");
			RETURN_META(MRES_SUPERCEDE);
		}
		if (g_bInvisiblePlayers[iPlayer]) {
			g_bInvisiblePlayers[iPlayer] = false;
			ResetUserModel(iPlayer);
			CLIENT_PRINTF(pEntity, print_console, "[InvisibleSpec] Acum esti VIZIBIL !\n");
		}
		else {
			g_bInvisiblePlayers[iPlayer] = true;
			MESSAGE_BEGIN(MSG_ALL, SVC_UPDATEUSERINFO);
			WRITE_BYTE(iPlayer - 1);
			WRITE_LONG(GETPLAYERUSERID(pEntity));
			WRITE_STRING("");
			WRITE_LONG(0);
			WRITE_LONG(0);
			WRITE_LONG(0);
			WRITE_LONG(0);
			MESSAGE_END();
			CLIENT_PRINTF(pEntity, print_console, "[InvisibleSpec] Acum esti INVIZIBIL !\n");
		}
		RETURN_META(MRES_SUPERCEDE);
	}
}