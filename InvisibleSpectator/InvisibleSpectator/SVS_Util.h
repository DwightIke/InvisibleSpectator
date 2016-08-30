#pragma once
#include "amxxmodule.h"
#include "quake_server.h"
#include "chooker.h"
struct server_t;

// Store client struct
extern client_t *gclientPlayers[33];
extern server_static_t *g_pEGV_svs;

bool getHost(server_static_t *svs) {
#if defined LINUX
	svs = Hooker->MemorySearch<server_static_t *>("svs", (void*)gpGlobals, TRUE);
#else
	uintptr_t vBaseAddress = *reinterpret_cast<uintptr_t *>(reinterpret_cast<byte *>(g_engfuncs.pfnGetCurrentPlayer) + 8);
	svs = reinterpret_cast<server_static_t *>(vBaseAddress - 4);
#endif
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