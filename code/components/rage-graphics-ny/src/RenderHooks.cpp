/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */

#include "StdInc.h"
#include "DrawCommands.h"
#include "Hooking.h"

#include <HostSharedData.h>
#include <CfxState.h>
#include <ReverseGameData.h>
#include <CrossBuildRuntime.h>

fwEvent<> OnGrcCreateDevice;
fwEvent<> OnGrcBeginScene;
fwEvent<> OnGrcEndScene;

static void InvokeEndSceneCB()
{
	OnGrcEndScene();
	OnPostFrontendRender();
}

static void* origEndScene;

static void __declspec(naked) InvokeEndSceneCBStub()
{
	__asm
	{
		call InvokeEndSceneCB

		push origEndScene
		retn
	}
}

static void __stdcall InvokeCreateCB(void* a1, HEAP_INFORMATION_CLASS a2, void* a3, SIZE_T a4)
{
	HeapSetInformation(a1, a2, a3, a4);

	OnGrcCreateDevice();
}

static HWND g_gtaWindow;
static decltype(&CreateWindowExW) g_origCreateWindowExW;

static HWND WINAPI HookCreateWindowExW(_In_ DWORD dwExStyle, _In_opt_ LPCWSTR lpClassName, _In_opt_ LPCWSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam)
{
	static HostSharedData<CfxState> initState("CfxInitState");
	HWND w;

	auto wndName = L"LibertyM™ by Cfx.re";

	if (initState->isReverseGame)
	{
		static HostSharedData<ReverseGameData> rgd("CfxReverseGameData");

		w = g_origCreateWindowExW(dwExStyle, lpClassName, wndName, WS_POPUP | WS_CLIPSIBLINGS, 0, 0, rgd->width, rgd->height, NULL, hMenu, hInstance, lpParam);
	}
	else
	{
		w = g_origCreateWindowExW(dwExStyle, lpClassName, wndName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	}

	if (lpClassName && wcscmp(lpClassName, L"grcWindow") == 0)
	{
		CoreSetGameWindow(w);
	}

	g_gtaWindow = w;

	return w;
}

static HookFunction hookFunction([] ()
{
	static hook::inject_call<void, int> beginSceneCB((ptrdiff_t)hook::get_pattern("56 8B F1 E8 ? ? ? ? 80 7C 24 08 00", 3));
	beginSceneCB.inject([] (int)
	{
		beginSceneCB.call();

		OnGrcBeginScene();
	});

	// end scene callback dc
	//hook::put(0x796B9E, InvokeEndSceneCBStub);

	// same, for during loading text
	{
		auto location = hook::get_pattern("8B 8C 24 C4 0E 00 00", -5);
		origEndScene = hook::get_call(location);
		hook::call(location, InvokeEndSceneCBStub);
	}

	// device creation
	{
		auto location = hook::get_pattern("6A 00 6A 00 6A 01 6A 00 FF 15", 8);
		hook::nop(location, 6);
		hook::call(location, InvokeCreateCB);
	}

	g_origCreateWindowExW = hook::iat("user32.dll", HookCreateWindowExW, "CreateWindowExW");
});
