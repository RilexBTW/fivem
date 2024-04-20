/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "CrossLibraryInterfaces.h"
#include "InputHook.h"
#include "Hooking.h"

#include <nutsnbolts.h>

#include <MinHook.h>
#include <CrossBuildRuntime.h>

WNDPROC origWndProc;

bool g_isFocused = true;
static bool g_enableSetCursorPos = false;
static bool g_isFocusStolen = false;
static void(*disableFocus)();

static void DisableFocus()
{
	if (!g_isFocusStolen)
	{
		if (disableFocus)
		{
			disableFocus();
		}
	}
}

static void(*enableFocus)();

static void EnableFocus()
{
	if (!g_isFocusStolen)
	{
		if (enableFocus)
		{
			enableFocus();
		}
	}
}

void InputHook::SetGameMouseFocus(bool focus)
{
	g_isFocusStolen = !focus;

	if (enableFocus && disableFocus)
	{
		return (focus) ? enableFocus() : disableFocus();
	}
}

void InputHook::EnableSetCursorPos(bool enabled) 
{
	g_enableSetCursorPos = enabled;
}

static bool g_useHostCursor;

void EnableHostCursor()
{
	while (ShowCursor(TRUE) < 0)
		;
}

void DisableHostCursor()
{
	while (ShowCursor(FALSE) >= 0)
		;
}

static INT HookShowCursor(BOOL show)
{
	if (g_useHostCursor)
	{

		return (show) ? 0 : -1;
	}

	return ShowCursor(show);
}

void InputHook::SetHostCursorEnabled(bool enabled)
{
	static bool lastEnabled = false;

	if (!lastEnabled && enabled)
	{
		EnableHostCursor();
	}
	else if (lastEnabled && !enabled)
	{
		DisableHostCursor();
	}

	g_useHostCursor = enabled;
}

BOOL WINAPI SetCursorPosWrap(int X, int Y)
{
	if (!g_isFocused || g_enableSetCursorPos)
	{
		return SetCursorPos(X, Y);
	}

	return TRUE;
}

LRESULT APIENTRY grcWindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_ACTIVATEAPP)
	{
		g_isFocused = (wParam) ? true : false;
	}

	if (uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
	{
		if (!g_isFocused)
		{
			return 0;
		}
	}

	bool pass = true;
	LRESULT lresult;

	InputHook::DeprecatedOnWndProc(hwnd, uMsg, wParam, lParam, pass, lresult);

	if (uMsg == WM_PARENTNOTIFY)
	{
		pass = false;
		lresult = DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	if (!pass)
	{
		return lresult;
	}

	return CallWindowProc(origWndProc, hwnd, uMsg, wParam, lParam);
}

static void RepairInput()
{
}

int LockEnabled()
{
	int retval = 1;
	InputHook::QueryMayLockCursor(retval);

	return retval;
} 

void(__cdecl* g_origLockMouseDevice)(BYTE a1);

static void __cdecl LockMouseDeviceHook(BYTE a1)
{
	if (!LockEnabled())
	{
		a1 = 0;
	}

	g_origLockMouseDevice(a1);
}

static int32_t* mouseAxisX;
static int32_t* mouseAxisY;
static bool* isMenuActive;

//Smoother mouse controls, as the function often returns random numbers, leading to inconsistent mouse movement behaviour
static double SetMouseAxisDelta(int input, int32_t axis)
{
	if (!input || (isMenuActive && *isMenuActive))
	{
		return 0.0;
	}

	if (axis == 0)
	{
		return (float)*mouseAxisX * 0.0078125;
	}
	else if (axis == 1)
	{
		return (float)*mouseAxisY * 0.0078125;
	}

	return 0.0;
}

struct IRgscUi
{
	virtual HRESULT __stdcall QueryInterface(REFIID iid, void** ppvOut) = 0;
};

static BOOL __stdcall RgscIsUiShown(void* self)
{
	return g_isFocusStolen;
}

static void __stdcall QueryRgscUI(IRgscUi* rgsc, REFIID iid, void** ppvOut)
{
	rgsc->QueryInterface(iid, ppvOut);

	intptr_t* vtbl = *(intptr_t**)(*ppvOut);
	hook::putVP(&vtbl[2], RgscIsUiShown);
}

static HookFunction hookFunction([]()
{
	// d3d fpu preserve, STILL FIXME move it elsewhere
	/*hook::put<uint8_t>(0x6213C7, 0x46);
	hook::put<uint8_t>(0x62140F, 0x16);
	hook::put<uint8_t>(0x621441, 0x46);
	hook::put<uint8_t>(0x621481, 0x86);
	hook::put<uint8_t>(0x6214C1, 0x26);*/

	// window procedure
	{
		auto location = hook::get_pattern("C7 44 24 2C 08 00 00 00", 12);
		origWndProc = *(WNDPROC*)(location);
		*(WNDPROC*)(location) = grcWindowProcedure;
	}

	// ignore WM_ACTIVATEAPP deactivates (to fix the weird crashes)
	hook::nop(hook::get_pattern("8B DF 8B 7D 08 85 DB", 7), 2);

	// disable dinput
	hook::return_function(hook::get_pattern("8B 0D ? ? ? ? 83 EC 14 85 C9"));

	// some mouse stealer -- removes dinput and doesn't get toggled back (RepairInput instead fixes the mouse)
	MH_Initialize();
	MH_CreateHook(hook::get_pattern("83 3D ? ? ? ? 00 74 7D 80 3D ? ? ? ? 00"), LockMouseDeviceHook, (void**)&g_origLockMouseDevice);
	MH_EnableHook(MH_ALL_HOOKS);

	// don't allow SetCursorPos during focus
	hook::iat("user32.dll", SetCursorPosWrap, "SetCursorPos");
	// NUI OS cursors
	hook::iat("user32.dll", HookShowCursor, "ShowCursor");

	{
		auto location = hook::get_pattern<char>("50 FF 15 ? ? ? ? 5F 5E 5B C3");
		hook::jump(location + 10, RepairInput); // tail of above function
		hook::nop(location, 7); // ignore ShowCursor calls
	}

	{
		auto pattern = hook::pattern("80 3D ? ? ? ? ? 74 4B E8 ? ? ? ? 84 C0");
		isMenuActive = *pattern.get_first<bool*>(2);
		auto location = hook::get_pattern<char>("51 8B 54 24 0C C7 04 24 00 00 00 00");
		hook::jump(location, SetMouseAxisDelta);
		mouseAxisX = *(int32_t**)(location + 0x12);
		mouseAxisY = *(int32_t**)(location + 0x28);
	}

	// RGSC UI hook for overlay checking (on QueryInterface)	
	{
		auto location = hook::get_pattern(xbr::IsGameBuildOrGreater<59>() ? "51 FF 10 85 C0 0F 85 ? ? ? ? 8B 0D" : "51 FF 10 85 C0 0F 85 41 02 00 00", 1);
		hook::nop(location, 10);
		hook::call(location, QueryRgscUI);
	}
});

fwEvent<HWND, UINT, WPARAM, LPARAM, bool&, LRESULT&> InputHook::DeprecatedOnWndProc;

fwEvent<int&> InputHook::QueryMayLockCursor;
fwEvent<std::vector<InputTarget*>&> InputHook::QueryInputTarget;
