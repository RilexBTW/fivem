#include "StdInc.h"
#include "CommandLineOption.h"

#include <CfxState.h>
#include <ReverseGameData.h>
#include <CrossBuildRuntime.h>

static RECT g_curRect;
static HWND g_window;

static CommandLineOption* g_noborder;

static HMONITOR GetPrimaryMonitorHandle()
{
	const POINT ptZero = { 0, 0 };
	return MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
}


static bool WINAPI MoveWindowCustom(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint)
{
	RECT rect = { X, Y, nWidth, nHeight };
	MONITORINFO info;
	HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
	info.cbSize = sizeof(MONITORINFO);
	GetMonitorInfo(monitor, &info);
	float DesktopResW = info.rcMonitor.right - info.rcMonitor.left;
	float DesktopResH = info.rcMonitor.bottom - info.rcMonitor.top;
	if ((rect.right - rect.left >= DesktopResW) || (rect.bottom - rect.top >= DesktopResH))
	{
		rect = g_curRect;
	}
	rect.left = (LONG)((DesktopResW / 2.0f) - (rect.right / 2.0f));
	rect.top = (LONG)((DesktopResH / 2.0f) - (rect.bottom / 2.0f));
	return MoveWindow(hWnd, rect.left, rect.top, rect.right, rect.bottom, bRepaint);
}

static void HandleWindowStyle()
{
	if (g_window)
	{
		RECT rect = g_curRect;
		LONG lStyle = GetWindowLong(g_window, GWL_STYLE);

		if (g_noborder->value)
		{
			lStyle &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
		}
		else
		{
			GetWindowRect(g_window, &rect);
			lStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
		}
		AdjustWindowRect(&rect, lStyle, FALSE);
		SetWindowLong(g_window, GWL_STYLE, lStyle);
		MoveWindowCustom(g_window, 0, 0, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}
}

static HWND g_gtaWindow;
static decltype(&CreateWindowExA) g_origCreateWindowExA;

static HWND WINAPI HookCreateWindowExA(_In_ DWORD dwExStyle, _In_opt_ LPCSTR lpClassName, _In_opt_ LPCSTR lpWindowName, _In_ DWORD dwStyle, _In_ int X, _In_ int Y, _In_ int nWidth, _In_ int nHeight, _In_opt_ HWND hWndParent, _In_opt_ HMENU hMenu, _In_opt_ HINSTANCE hInstance, _In_opt_ LPVOID lpParam)
{
	static HostSharedData<CfxState> initState("CfxInitState");
	HWND w;

	auto wndName = L"LibertyM™ by Cfx.re";

	if (!strcmp(lpClassName, "grcWindow"))
	{
		if (g_noborder->value)
		{
			MONITORINFO info;
			info.cbSize = sizeof(MONITORINFO);
			const POINT ptZero = { 0, 0 };
			GetMonitorInfo(MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY), &info);

			dwStyle = WS_VISIBLE | WS_POPUP;

			X = 0;
			Y = 0;
			nWidth = info.rcMonitor.right;
			nHeight = info.rcMonitor.bottom;

			dwExStyle = 0;
		}
	}

	if (initState->isReverseGame)
	{
		static HostSharedData<ReverseGameData> rgd("CfxReverseGameData");

		w = CreateWindowExW(dwExStyle, L"grcWindow", wndName, WS_POPUP | WS_CLIPSIBLINGS, 0, 0, rgd->width, rgd->height, NULL, hMenu, hInstance, lpParam);
	}
	else
	{
		w = CreateWindowExW(dwExStyle, L"grcWindow", wndName, dwStyle, X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
	}

	if (lpClassName && strcmp(lpClassName, "grcWindow") == 0)
	{
		CoreSetGameWindow(w);
	}

	g_gtaWindow = w;

	return w;
}

static bool WINAPI SetRectCustom(LPRECT lprc, int xLeft, int yTop, int xRight, int yBottom)
{
	g_curRect = { xLeft, yTop, xRight, yBottom };
	return SetRect(lprc, xLeft, yTop, xRight, yBottom);
}

static bool WINAPI AdjustWindowRectCustom(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
	if (g_noborder->value)
	{
		dwStyle = WS_VISIBLE | WS_POPUP;
	}
	else
	{
		dwStyle |= (WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU);
	}

	return AdjustWindowRect(lpRect, dwStyle, bMenu);
}

static HookFunction windowInit([]()
{
	g_origCreateWindowExA = hook::iat("user32.dll", HookCreateWindowExA, "CreateWindowExA");
	hook::iat("user32.dll", MoveWindowCustom, "MoveWindow");
	hook::iat("user32.dll", AdjustWindowRectCustom, "AdjustWindowRect");
	hook::iat("user32.dll", SetRectCustom, "SetRect");

	// 1.2.0.30: "8A 44 24 0C 88 41 10", 8
	g_curOption = *hook::get_pattern<CommandLineOption**>("8B 44 24 10 89 41 04 A1", 8);
	g_noborder = new CommandLineOption("border", "[GRAPHICS] Force disable borderless mode (needs -windowed)");

	// default to non-fullscreen mode
	//hook::put<uint8_t>(0x796BEB, 1);

	// don't unset width, height, fullscreen, refreshrate values during initialization
	hook::nop(hook::get_pattern("0F 45 C8 89 4C 24 40 68", 17), 10 * 5);
});
