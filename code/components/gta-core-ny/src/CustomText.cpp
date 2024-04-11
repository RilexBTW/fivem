/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "MinHook.h"
#include "Hooking.h"

#include <stack>
#include <shared_mutex>

#include <CustomText.h>

CText* TheText;

static const wchar_t* (__fastcall* g_origGetText)(void*, void*, const char*);

static std::shared_mutex g_textMutex;
static std::unordered_map<uint32_t, std::wstring> g_customTexts;

namespace game
{
	void AddCustomText(uint32_t hash, const std::string& value)
	{
		std::unique_lock lock(g_textMutex);
		g_customTexts[hash] = ToWide(value);
	}

	void AddCustomText(const std::string& key, const std::string& value)
	{
		game::AddCustomText(HashString(key), value);
	}

	void RemoveCustomText(uint32_t hash)
	{
		std::unique_lock lock(g_textMutex);
		g_customTexts.erase(hash);
	}
}

/*static hook::thiscall_stub<const wchar_t* (CText*, const char* textKey)> _CText__Get([]()
{
	return hook::get_pattern("83 EC 44 A1 ? ? ? ? 33 C4 89 44 24 40 8B 44 24 48 56 8B F1");
});
*/

static const wchar_t* GetText(CText* TheText, const char* hash)
{
	{
		std::shared_lock lock(g_textMutex);

		auto it = g_customTexts.find(HashString(hash));

		if (it != g_customTexts.end())
		{
			return it->second.c_str();
		}
	}

	return g_origGetText(TheText, nullptr, hash);
}

const wchar_t* CText::Get(const char* textKey)
{
	return GetText(this, textKey);
}

void CText::SetCustom(uint32_t key, const wchar_t* value)
{
	std::shared_lock lock(g_textMutex);
	g_customTexts[key] = value;
}

void CText::SetCustom(const char* key, const wchar_t* value)
{
	std::shared_lock lock(g_textMutex);
	g_customTexts[HashString(key)] = value;
}

const wchar_t* __fastcall CText_GetHook(void* self, void* dummy, const char* text)
{
	{
		std::shared_lock lock(g_textMutex);

		auto it = g_customTexts.find(HashString(text));

		if (it != g_customTexts.end())
		{
			return it->second.c_str();
		}
	}

	return g_origGetText(self, dummy, text);
}

static HookFunction hookFunc([]()
{
	TheText = *hook::get_pattern<CText*>("EB ? A1 ? ? ? ? 48 83 F8 03", -9);
	MH_Initialize();
	MH_CreateHook(hook::get_pattern("56 8B F1 85 C0 75 3F E8", -0x12), CText_GetHook, (void**)&g_origGetText);
	MH_EnableHook(MH_ALL_HOOKS);
});
