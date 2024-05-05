/*
 * This file is part of the CitizenFX project - http://citizen.re/
 *
 * See LICENSE and MENTIONS in the root of the source tree for information
 * regarding licensing.
 */
#include "StdInc.h"
#include "DrawCommands.h"
#include "Hooking.h"
#include <MinHook.h>

#include <HostSharedData.h>
#include <CfxState.h>
#include <ReverseGameData.h>
#include <CrossBuildRuntime.h>

#if __has_include(<scrEngine.h>)
#include <scrEngine.h>
#define LOG_DEPTH_PATCHES
#endif

#include <Error.h>

fwEvent<> OnGrcCreateDevice;
fwEvent<> OnGrcBeginScene;
fwEvent<> OnGrcEndScene;

static HANDLE g_gameWindowEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

void DLL_EXPORT UiDone()
{
	static HostSharedData<CfxState> initState("CfxInitState");
	WaitForSingleObject(g_gameWindowEvent, INFINITE);

	auto uiExitEvent = CreateEventW(NULL, TRUE, FALSE, va(L"CitizenFX_PreUIExit%s", IsCL2() ? L"CL2" : L""));
	auto uiDoneEvent = CreateEventW(NULL, FALSE, FALSE, va(L"CitizenFX_PreUIDone%s", IsCL2() ? L"CL2" : L""));

	if (uiExitEvent)
	{
		SetEvent(uiExitEvent);
	}

	if (uiDoneEvent)
	{
		WaitForSingleObject(uiDoneEvent, INFINITE);
	}
}

#include <dxgi1_4.h>
#include <dxgi1_6.h>
#include <wrl.h>
#pragma comment(lib, "dxgi.lib")
namespace WRL = Microsoft::WRL;
static void RegisterPrimaryGPU()
{
	{
		std::vector<std::string> gpuList;

		IDXGIAdapter4* pAdapter = nullptr;
		IDXGIFactory6* pFactory = nullptr;
		HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory6), (void**)&pFactory);

		if (SUCCEEDED(hr))
		{
			for (UINT adapterIndex = 0; 
				DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, __uuidof(IDXGIAdapter4), (void**)&pAdapter); 
				adapterIndex++)
			{
				DXGI_ADAPTER_DESC1 desc;
				pAdapter->GetDesc1(&desc);

				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					// Don't select the Basic Render Driver adapter.
					continue;
				}


				static auto _ = ([&desc]
				{
					AddCrashometry("gpu_name", "%s", ToNarrow(desc.Description));
					AddCrashometry("gpu_id", "%04x:%04x", desc.VendorId, desc.DeviceId);
					return true;
				})();
			}
		}
	}
}

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
	RegisterPrimaryGPU();
	SetEvent(g_gameWindowEvent);
	HeapSetInformation(a1, a2, a3, a4);
	OnGrcCreateDevice();
}

void PreD3DReset()
{
	OnD3DPreReset();
}

void PostD3DReset()
{
	OnD3DPostReset();
}

#include "dxerr.h"
#include <Error.h>
static void D3DResetFailed(HRESULT hr)
{
	constexpr auto errorBufferCount = 8192;
	auto errorBuffer = std::unique_ptr<wchar_t[]>(new wchar_t[errorBufferCount]);
	memset(errorBuffer.get(), 0, errorBufferCount * sizeof(wchar_t));
	DXGetErrorDescriptionW(hr, errorBuffer.get(), errorBufferCount);

	auto errorString = DXGetErrorStringW(hr);
	
	if (!errorString)
	{
		errorString = va(L"0x%08x", hr);
	}

	auto errorBody = fmt::sprintf("%s - %s", ToNarrow(errorString), ToNarrow(errorBuffer.get()));
	GlobalError("Direct3D encountered an error while resetting: %s", errorBody);
}

static void __declspec(naked) PreAndPostD3DReset()
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

static int __stdcall adjustTimeCycleModf(const char* i, const char* fmt, char* Name, float* MinFarClip, float* MaxFarClip, float* MinFogStart,
	float* MaxFogStart, int* AmbientLight0R, int* AmbientLight0G, int* AmbientLight0B, int* AmbientLight0A, float* AmbientLight0M,
	int* AmbientLight1R, int* AmbientLight1G, int* AmbientLight1B, int* AmbientLight1A, float* AmbientLight1M, int* DirectionalLightR,
	int* DirectionalLightG, int* DirectionalLightB, int* DirectionalLightA, float* DirectionalLightM, float* AmbientDownMult, int* FogR,
	int* FogG, int* FogB, int* FogA, int* NearFogR, int* NearFogG, int* NearFogB, int* NearFogA, float* FogMult, int* NearFog, float* Axis,
	float* Temperature, float* PostFxStrength, float* Exposure, float* ExposureMultiplier, float* BrightPassThreshold, float* MidGreyValue,
	int* IntensityBloomR, int* IntensityBloomG, int* IntensityBloomB, int* ColourCorrectR, int* ColourCorrectG, int* ColourCorrectB, float* ColourAdd,
	float* Desaturation, float* Contrast, float* Gamma, float* DesaturationFar, float* ContrastFar, float* GammaFar, float* DepthFxNear, float* DepthFxFar,
	float* LumMin, float* LumMax, float* LumAdaptDelay, float* GlobEnvRefl, float* MinFarDOF, float* MaxFarDOF, float* MinNearDOF, float* MaxNearDOF,
	float* NearBlurDOF, float* MidBlurDOF, float* FarBlurDOF, float* WaterReflection, float* ParticleHDR, float* AmbientOcclusionMult, float* SkyMultiplier,
	float* PedAmbOccHack, int* NearFogAmount)
{
	int res = sscanf(i, fmt, Name, MinFarClip, MaxFarClip, MinFogStart, MaxFogStart, AmbientLight0R, AmbientLight0G, AmbientLight0B, AmbientLight0A,
		AmbientLight0M, AmbientLight1R, AmbientLight1G, AmbientLight1B, AmbientLight1A, AmbientLight1M, DirectionalLightR, DirectionalLightG, DirectionalLightB,
		DirectionalLightA, DirectionalLightM, AmbientDownMult, FogR, FogG, FogB, FogA, NearFogR, NearFogG, NearFogB, NearFogA, FogMult, NearFog, Axis, Temperature,
		PostFxStrength, Exposure, ExposureMultiplier, BrightPassThreshold, MidGreyValue, IntensityBloomR, IntensityBloomG, IntensityBloomB, ColourCorrectR,
		ColourCorrectG, ColourCorrectB, ColourAdd, Desaturation, Contrast, Gamma, DesaturationFar, ContrastFar, GammaFar, DepthFxNear, DepthFxFar, LumMin, LumMax,
		LumAdaptDelay, GlobEnvRefl, MinFarDOF, MaxFarDOF, MinNearDOF, MaxNearDOF, NearBlurDOF, MidBlurDOF, FarBlurDOF, WaterReflection, ParticleHDR, AmbientOcclusionMult,
		SkyMultiplier, PedAmbOccHack, NearFogAmount);
	*MinFarClip = -1.0f;
	*MaxFarClip = -1.0f;
	return res;
}

static HookFunction hookFunction([]()
{
	// D3D Device Begin Scene
	static hook::inject_call<void, int> beginSceneCB((ptrdiff_t)hook::get_pattern("E8 ? ? ? ? 80 7C 24 ? ? 74 ? 80 BE ? ? ? ? ? 74"));
	beginSceneCB.inject([] (int)
	{
		beginSceneCB.call();
		OnGrcBeginScene();
	});

	// D3D Device Creation
	{
		auto location = hook::get_pattern("6A 00 6A 00 6A 01 6A 00 FF 15", 8);
		hook::nop(location, 6);
		hook::call(location, InvokeCreateCB);
	}

	// D3D Device End Scene
	{
		auto location = hook::get_pattern("8B 8C 24 C4 0E 00 00", -5);
		origEndScene = hook::get_call(location);
		hook::call(location, InvokeEndSceneCBStub);
	}

	// Handle pre, post and D3D Reset failures
	// TODO: update pattern
	//hook::jump(hook::get_pattern("83 C0 10 3D 00 10 00 00", 22), PreAndPostD3DReset);

	//Implements the needed changes and custom registers for the LogDepth patches to eliminate z-fighting/clipping
#ifdef LOG_DEPTH_PATCHES
	auto loadingScreenActive = hook::get_pattern<uint8_t*>("80 3D ? ? ? ? ? 53 56 8A FA", 2);

	//Custom calls to default Min/MaxFarClip to -1.f
	auto pattern = hook::get_pattern("E8 ? ? ? ? 6A ? E8 ? ? ? ? 8B 0D ? ? ? ? 83 C4 ? 89 04 8D ? ? ? ? 8B 4C 24 ? 89 08 A1 ? ? ? ? F3 0F 10 44 24");
	hook::call(pattern, adjustTimeCycleModf);

	OnGrcBeginScene.Connect([loadingScreenActive]()
	{
		LPDIRECT3DDEVICE9 device = GetD3D9Device();
		auto shouldRenderFarClip = *hook::get_pattern<void**>("A3 ? ? ? ? C7 80", 1);
		//auto viewport = (char*)0x017F583C;
		auto bLoadscreenActive = false;//(loadingScreenActive && *loadingScreenActive);

		if (*shouldRenderFarClip && !bLoadscreenActive)
		{
			static auto Cam = 0;
			NativeInvoke::Invoke<0x75E005F1, void>(&Cam);
			if (Cam)
			{
				static float nearClip;
				static float farClip;
				NativeInvoke::Invoke<0x2EF477FD, void>(Cam, &nearClip);
				NativeInvoke::Invoke<0x752643C9, void>(Cam, &nearClip);

				static float clips[4];
				clips[0] = nearClip;
				clips[1] = farClip;
				clips[2] = nearClip;
				clips[3] = farClip;

				device->SetVertexShaderConstantF(227, &clips[0], 1);
			}
		}
	});
#endif
});
