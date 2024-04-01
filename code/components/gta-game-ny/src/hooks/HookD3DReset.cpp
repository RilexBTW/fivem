#include "StdInc.h"
#include <grcTexture.h>
#include <CrossBuildRuntime.h>
#include <Error.h>
#include <MinHook.h>

void PreD3DReset()
{
	OnD3DPreReset();
}

void PostD3DReset()
{
	OnD3DPostReset();
}

void D3DResetFailed(HRESULT error)
{
	GlobalError("Direct3D resetting failed with error 0x%08x. This is fatal. You will probably die.", error);
}

void __declspec(naked) PreAndPostD3DReset()
{
	__asm
	{
		mov ecx, [esp + 8h]

		push ecx

		mov ecx, [eax]

		push eax

		push ecx
		call PreD3DReset
		pop ecx

		mov eax, [ecx + 40h]
		call eax

		cmp eax, 0
		jnz debugStuff

		push eax
		call PostD3DReset
		pop eax

		retn 8

debugStuff:
		push eax
		call D3DResetFailed
		add esp, 4h

		retn 8
	}
}

class grcWindow
{
public:
	grcWindow(std::wstring_view className = L"grcWindow", std::wstring windowName = L"grcWindow")
	{
		windowClass.cbSize = sizeof(WNDCLASSEX);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = DefWindowProc;
		windowClass.cbClsExtra = 0;
		windowClass.cbWndExtra = 0;
		windowClass.hInstance = GetModuleHandle(NULL);
		windowClass.hIcon = NULL;
		windowClass.hCursor = NULL;
		windowClass.hbrBackground = NULL;
		windowClass.lpszMenuName = NULL;
		windowClass.lpszClassName = className.data();
		windowClass.hIconSm = NULL;
		RegisterClassExW(&windowClass);
		window = CreateWindowW(windowClass.lpszClassName, windowName.data(), WS_OVERLAPPEDWINDOW, 0, 0, 100, 100, NULL, NULL, windowClass.hInstance, NULL);
	}
	~grcWindow()
	{
		DestroyWindow(window);
		UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
	}
	static HWND GetHookWindow(grcWindow& hw)
	{
		return hw.window;
	}
private:
	WNDCLASSEX windowClass{};
	HWND window{};
};

template<typename TFnLeft, typename TFnRight>
void VHook(intptr_t& ref, TFnLeft fn, TFnRight out)
{
	if (ref == (intptr_t)fn)
	{
		return;
	}

	if (*out)
	{
		return;
	}

	*out = (decltype(*out))ref;
	ref = (intptr_t)fn;
}

IDirect3D9* (*g_Direct3DCreate9)(
	UINT SDKVersion
);

HRESULT(*g_origCreateDevice)(
	LPDIRECT3D9			  device,
	UINT                  Adapter,
	D3DDEVTYPE            DeviceType,
	HWND                  hFocusWindow,
	DWORD                 BehaviorFlags,
	D3DPRESENT_PARAMETERS* pPresentationParameters,
	IDirect3DDevice9** ppReturnedDeviceInterface
);

HRESULT (*g_CreateVertexBuffer)(
	LPDIRECT3DDEVICE9      pDevice,
	UINT                   Length,
	DWORD                  Usage,
	DWORD                  FVF,
	D3DPOOL                Pool,
	IDirect3DVertexBuffer9** ppVertexBuffer,
	HANDLE* pSharedHandle
);


HRESULT CreateVertexBuffer(
	LPDIRECT3DDEVICE9      pDevice,
	UINT                   Length,
	DWORD                  Usage,
	DWORD                  FVF,
	D3DPOOL                Pool,
	IDirect3DVertexBuffer9** ppVertexBuffer,
	HANDLE* pSharedHandle
)
{
	auto result = g_CreateVertexBuffer(pDevice, Length, Usage, FVF, D3DPOOL_MANAGED, ppVertexBuffer, pSharedHandle);
	return result;
}


static HookFunction hookFunction([]()
{
	auto hD3D9 = GetModuleHandleW(L"d3d9.dll");
	if (!hD3D9)
	{
		return;
	}

	/*auto grcWindowCtor = grcWindow(L"grcWindowD3D9", L"grcWindowD3D9");
	auto hWnd = grcWindow::GetHookWindow(grcWindowCtor);

	D3DPRESENT_PARAMETERS params;
	params.BackBufferWidth = 0;
	params.BackBufferHeight = 0;
	params.BackBufferFormat = D3DFMT_UNKNOWN;
	params.BackBufferCount = 0;
	params.MultiSampleType = D3DMULTISAMPLE_NONE;
	params.MultiSampleQuality = NULL;
	params.SwapEffect = D3DSWAPEFFECT_DISCARD;
	params.hDeviceWindow = hWnd;
	params.Windowed = 1;
	params.EnableAutoDepthStencil = 0;
	params.AutoDepthStencilFormat = D3DFMT_UNKNOWN;
	params.Flags = NULL;
	params.FullScreen_RefreshRateInHz = 0;
	params.PresentationInterval = 0;

	auto deviceVtbl = new intptr_t[119];
	auto d3d9Vtbl = new intptr_t[15];

	LPDIRECT3DDEVICE9 Device;
	if (Direct3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF, hWnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT, &params, &Device) >= 0)
	{

		auto D3D9CreateDevice = [](LPDIRECT3D9 pDirect3D9, UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)->HRESULT
		{
		};

		auto vtbl = **(intptr_t***)Device;
		auto vtbl2 = **(intptr_t***)Direct3D9;


		memcpy(deviceVtbl, vtbl, 119 * sizeof(intptr_t));
		memcpy(d3d9Vtbl, vtbl2, 15 * sizeof(intptr_t));

		VHook(d3d9Vtbl[15], &CreateDevice, &g_origCreateDevice);
		VHook(deviceVtbl[26], &CreateVertexBuffer, &g_CreateVertexBuffer);

		trace("release\n");
		Device->Release();
	}
	Direct3D9->Release();
	*/
	//hook::jump(hook::get_pattern("83 C0 10 3D 00 10 00 00", 22), PreAndPostD3DReset);
});
