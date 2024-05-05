/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "Hooking.h"
#include "scrThread.h"
#include "scrEngine.h"
#include <MinHook.h>

static uint32_t gtaThreadVTable;

void(*g_origScriptInit)(bool);

void ScriptInitHook(bool dontStartScripts)
{
	g_origScriptInit(dontStartScripts);
	rage::scrEngine::OnScriptInit();
}

bool IsSafeScriptVTable(uint32_t vTable)
{
	return (vTable != gtaThreadVTable);
}

static rage::eThreadState ScriptTickDo(rage::scrThread* thread, int time)
{
	bool allowed = true;

	//rage::scrEngine::CheckNativeScriptAllowed(allowed);

	if (allowed || IsSafeScriptVTable(*(uint32_t*)thread))
	{
		thread->Tick(time);
	}

	return thread->GetContext()->State;
}

static void __declspec(naked) ScriptTick()
{
	__asm
	{
		push edi
		push ecx
		call ScriptTickDo
		add esp, 8h

		retn
	}
}

static HookFunction hookFunction([] ()
{
	MH_Initialize();
	MH_CreateHook(hook::get_pattern("55 8B EC 83 E4 ? 83 EC ? 56 57 6A ? 68 ? ? ? ? E8 ? ? ? ? 8B 0D"), ScriptInitHook, (void**)&g_origScriptInit);
	MH_EnableHook(MH_ALL_HOOKS);
	// ignore startup/network_startup
	hook::put<uint8_t>(hook::get_pattern("46 83 FE 20 72 ? 80 7D 08 00", 17), 0xEB);

	auto loc = hook::get_pattern<char>("83 79 ? ? 74 ? 8B 01 57");
	hook::nop(loc + 6, 6);
	hook::call(loc + 6, ScriptTick);

	gtaThreadVTable = *hook::get_pattern<uint32_t>("C7 86 A8 00 00 00 00 00 00 00 8B C6", -9);
});
