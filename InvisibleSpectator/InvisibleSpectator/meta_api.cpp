#include "chooker.h"
#include "amxxmodule.h"

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

typedef void(*SV_FullClientUpdate)(int * client, int *buf);
SV_FullClientUpdate SV_FullClientUpdate_f;
CFunc *SV_FullClientUpdate_f_hook = NULL;

bool g_bPatched, g_bInvisiblePlayers[33], g_bTeamInfoActive;
int g_iTeamInfo, g_iTeamInfoArg1;
// win old: 0x55 0x8B 0xEC 0x81 0xEC ? ? ? ? 0x53 0x8B 0x1D ? ? ? ? 0x56 0x57 0x8B 0x7D ? 0xB8
// linux: SV_FullClientUpdate
void ResetUserModel(const int iPlayer);
void SV_FullClientUpdate_f_Call() {
	SERVER_PRINT("SV_FullClientUpdate called \n");
	
	
	int iPlayer = ENGINE_CURRENT_PLAYER() - 1;
	if (!g_bInvisiblePlayers[iPlayer])
		return;
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
	SV_FullClientUpdate_f = Hooker->MemorySearch<SV_FullClientUpdate>("0x55,0x8B,*,0x81,*,*,*,*,*,0x53,0x8B,*,*,*,*,*,0x56,0x57", (void *)gpGlobals, FALSE);
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
	g_bPatched = CreateHook_SV_FullClientUpdate();
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
void FN_WriteString_Post(const char *sz) {
	if (!g_bPatched) RETURN_META(MRES_IGNORED);
	if (g_bTeamInfoActive) {
		if (strcmp(sz, "TERRORIST") != 0 && strcmp(sz, "CT") != 0)
			g_bTeamInfoActive = false;
	}
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
	if (!FNullEnt(pEntity))
		g_bInvisiblePlayers[ENTINDEX(pEntity)] = false;
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
	if (!FNullEnt(pEntity))
		g_bInvisiblePlayers[ENTINDEX(pEntity)] = false;
	RETURN_META(MRES_IGNORED);
}
void FN_ClientCommand(edict_t *pEntity) {
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