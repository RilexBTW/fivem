#pragma once

#include <WS2tcpip.h>

namespace rage
{
	struct rlGamerInfo;
}

class CPed;

#ifdef COMPILING_GTA_GAME_NY
#define GAME_EXPORT DLL_EXPORT
#else
#define GAME_EXPORT DLL_IMPORT
#endif

class GAME_EXPORT CPlayerInfo
{
private:
	char pad[0x4C]; // 76
	char m_Name[20]; // + 20
	char pad2[0x390]; // + 912
	void* pad_3; // + 4
	char pad4[0x20]; // + 32
	float m_stamina; // + 8
	char pad5[0X7C]; // 124
	uint32_t m_lastHitPedTime; // + 4
	uint32_t m_lastHitBuildingTime; // + 4
	uint32_t m_lastHitObjectTime; // + 4
	uint32_t m_lastRanLightTime; // + 4
	uint32_t m_lastDroveAgainstTrafficTime; // + 4
	uint8_t pad6[0x10]; // + 16
	uint32_t m_controlFlags; // + 4
	uint8_t pad7[0X1A]; // + 26
	uint8_t m_playerId; // + 1
	uint8_t pad8; // +1
	uint8_t m_sessionState; // + 1;
	char pad9[190];
	CPed* ped;
public:
	static CPlayerInfo* GetLocalPlayer();

	static CPlayerInfo* GetPlayer(int index);

	inline void* GetPed() 
	{
		return ped;
	}

	inline rage::rlGamerInfo* GetGamerInfo()
	{
		return (rage::rlGamerInfo*)this;
	}
};
