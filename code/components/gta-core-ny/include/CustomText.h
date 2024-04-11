/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#pragma once

#ifndef GTA_CORE_EXPORT
#ifdef COMPILING_GTA_CORE_NY
#define GTA_CORE_EXPORT __declspec(dllexport)
#else
#define GTA_CORE_EXPORT __declspec(dllimport)
#endif
#endif

namespace game
{
	void GTA_CORE_EXPORT AddCustomText(const std::string& key, const std::string& value);

	void GTA_CORE_EXPORT AddCustomText(uint32_t hash, const std::string& value);

	void GTA_CORE_EXPORT RemoveCustomText(uint32_t hash);
}

class GTA_CORE_EXPORT CText
{
public:
	const wchar_t* Get(const char* key);

	void SetCustom(const char* key, const wchar_t* value);

	void SetCustom(uint32_t key, const wchar_t* value);
};

extern GTA_CORE_EXPORT CText* TheText;
