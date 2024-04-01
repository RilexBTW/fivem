/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "grcTexture.h"
#include "CrossLibraryInterfaces.h"
#include <Hooking.h>

namespace rage
{
grcTextureFactory* grcTextureFactory::getInstance()
{
	static rage::grcTextureFactory** instance = *hook::get_pattern<rage::grcTextureFactory**>("83 C4 0C 8B 06 6A 00 68", -4);
	return *instance;
}

grcTexture* grcTextureFactory::GetNoneTexture()
{
	static rage::grcTexture** noneTexture = *hook::get_pattern<rage::grcTexture**>("FF 50 0C A3 ? ? ? ? 8B 06", 4);
	return *noneTexture;
}
}

__declspec(dllexport) fwEvent<> OnD3DPostReset;
__declspec(dllexport) fwEvent<> OnD3DPreReset;

static hook::cdecl_stub<void(bool, int, bool, float, bool, int)> _clearRenderTarget([]()
{
	return hook::get_call(hook::get_pattern("6A 01 6A 00 6A 00 E8 ? ? ? ? 83 C4 18 5e", 6));
});

void ClearRenderTarget(bool a1, int value1, bool a2, float value2, bool a3, int value3) 
{
	_clearRenderTarget(a1, value1, a2, value2, a3, value3);
}
