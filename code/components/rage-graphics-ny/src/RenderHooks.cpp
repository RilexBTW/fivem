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

#include <Error.h>
#include <dxgi1_4.h>
#include <dxgi1_5.h>
#include <dxgi1_6.h>
#include <wrl.h>
#pragma comment(lib, "dxgi.lib")
namespace WRL = Microsoft::WRL;

fwEvent<> OnGrcCreateDevice;
fwEvent<> OnGrcBeginScene;
fwEvent<> OnGrcEndScene;

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

class IDirect3D9DeviceProxy : public IDirect3DDevice9
{
private:
	LPDIRECT3DDEVICE9 m_device;
public:
	IDirect3D9DeviceProxy(LPDIRECT3DDEVICE9 device) : m_device(device)
	{
	}
	HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj)
	{
		return m_device->QueryInterface(riid, ppvObj);
	}
	ULONG WINAPI AddRef()
	{
		return m_device->AddRef();
	}
	ULONG WINAPI Release()
	{
		ULONG remainingDevices = m_device->Release();
		if (remainingDevices <= 0)
		{
			delete this;
		}
		return remainingDevices;
	}
	HRESULT WINAPI TestCooperativeLevel()
	{
		return m_device->TestCooperativeLevel();
	}
	UINT WINAPI GetAvailableTextureMem()
	{
		return m_device->GetAvailableTextureMem();
	}
	HRESULT WINAPI EvictManagedResources()
	{
		return m_device->EvictManagedResources();
	}
	HRESULT WINAPI GetDirect3D(IDirect3D9** ppD3D9)
	{
		return m_device->GetDirect3D(ppD3D9);
	}
	HRESULT WINAPI GetDeviceCaps(D3DCAPS9* pCaps)
	{
		return m_device->GetDeviceCaps(pCaps);
	}
	HRESULT WINAPI GetDisplayMode(UINT iSwapChain, D3DDISPLAYMODE* pMode)
	{
		return m_device->GetDisplayMode(iSwapChain, pMode);
	}
	HRESULT WINAPI GetCreationParameters(D3DDEVICE_CREATION_PARAMETERS* pParameters)
	{
		return m_device->GetCreationParameters(pParameters);
	}
	HRESULT WINAPI SetCursorProperties(UINT XHotSpot, UINT YHotSpot, IDirect3DSurface9* pCursorBitmap)
	{
		return m_device->SetCursorProperties(XHotSpot, YHotSpot, pCursorBitmap);
	}
	void WINAPI SetCursorPosition(int X, int Y, DWORD Flags)
	{
		m_device->SetCursorPosition(X, Y, Flags);
	}
	BOOL WINAPI ShowCursor(BOOL bShow)
	{
		return m_device->ShowCursor(bShow);
	}
	HRESULT WINAPI CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DSwapChain9** pSwapChain)
	{
		return m_device->CreateAdditionalSwapChain(pPresentationParameters, pSwapChain);
	}
	HRESULT WINAPI GetSwapChain(UINT iSwapChain, IDirect3DSwapChain9** pSwapChain)
	{
		return m_device->GetSwapChain(iSwapChain, pSwapChain);
	}
	UINT WINAPI GetNumberOfSwapChains()
	{
		return m_device->GetNumberOfSwapChains();
	}
	HRESULT WINAPI Reset(D3DPRESENT_PARAMETERS* pPresentationParameters)
	{
		return m_device->Reset(pPresentationParameters);
	}
	HRESULT WINAPI Present(CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion)
	{
		return m_device->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
	}
	HRESULT WINAPI GetBackBuffer(UINT iSwapChain, UINT iBackBuffer, D3DBACKBUFFER_TYPE Type, IDirect3DSurface9** ppBackBuffer)
	{
		return m_device->GetBackBuffer(iSwapChain, iBackBuffer, Type, ppBackBuffer);
	}
	HRESULT WINAPI GetRasterStatus(UINT iSwapChain, D3DRASTER_STATUS* pRasterStatus)
	{
		return m_device->GetRasterStatus(iSwapChain, pRasterStatus);
	}
	HRESULT WINAPI SetDialogBoxMode(BOOL bEnableDialogs)
	{
		return m_device->SetDialogBoxMode(bEnableDialogs);
	}
	void WINAPI SetGammaRamp(UINT iSwapChain, DWORD Flags, CONST D3DGAMMARAMP* pRamp)
	{
		m_device->SetGammaRamp(iSwapChain, Flags, pRamp);
	}
	void WINAPI GetGammaRamp(UINT iSwapChain, D3DGAMMARAMP* pRamp)
	{
		m_device->GetGammaRamp(iSwapChain, pRamp);
	}
	HRESULT WINAPI CreateTexture(UINT Width, UINT Height, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle)
	{
		return m_device->CreateTexture(Width, Height, Levels, Usage, Format, Pool, ppTexture, pSharedHandle);
	}
	HRESULT WINAPI CreateVolumeTexture(UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle)
	{
		return m_device->CreateVolumeTexture(Width, Height, Depth, Levels, Usage, Format, Pool, ppVolumeTexture, pSharedHandle);
	}
	HRESULT WINAPI CreateCubeTexture(UINT EdgeLength, UINT Levels, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle)
	{
		return m_device->CreateCubeTexture(EdgeLength, Levels, Usage, Format, Pool, ppCubeTexture, pSharedHandle);
	}
	HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9** ppVertexBuffer, HANDLE* pSharedHandle)
	{
		return m_device->CreateVertexBuffer(Length, Usage, FVF, Pool, ppVertexBuffer, pSharedHandle);
	}
	HRESULT WINAPI CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format, D3DPOOL Pool, IDirect3DIndexBuffer9** ppIndexBuffer, HANDLE* pSharedHandle)
	{
		return m_device->CreateIndexBuffer(Length, Usage, Format, Pool, ppIndexBuffer, pSharedHandle);
	}
	HRESULT WINAPI CreateRenderTarget(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		return m_device->CreateRenderTarget(Width, Height, Format, MultiSample, MultisampleQuality, Lockable, ppSurface, pSharedHandle);
	}
	HRESULT WINAPI CreateDepthStencilSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		return m_device->CreateDepthStencilSurface(Width, Height, Format, MultiSample, MultisampleQuality, Discard, ppSurface, pSharedHandle);
	}
	HRESULT WINAPI UpdateSurface(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestinationSurface, CONST POINT* pDestPoint)
	{
		return m_device->UpdateSurface(pSourceSurface, pSourceRect, pDestinationSurface, pDestPoint);
	}
	HRESULT WINAPI UpdateTexture(IDirect3DBaseTexture9* pSourceTexture, IDirect3DBaseTexture9* pDestinationTexture)
	{
		return m_device->UpdateTexture(pSourceTexture, pDestinationTexture);
	}
	HRESULT WINAPI GetRenderTargetData(IDirect3DSurface9* pRenderTarget, IDirect3DSurface9* pDestSurface)
	{
		return m_device->GetRenderTargetData(pRenderTarget, pDestSurface);
	}
	HRESULT WINAPI GetFrontBufferData(UINT iSwapChain, IDirect3DSurface9* pDestSurface)
	{
		return m_device->GetFrontBufferData(iSwapChain, pDestSurface);
	}
	HRESULT WINAPI StretchRect(IDirect3DSurface9* pSourceSurface, CONST RECT* pSourceRect, IDirect3DSurface9* pDestSurface, CONST RECT* pDestRect, D3DTEXTUREFILTERTYPE Filter)
	{
		return m_device->StretchRect(pSourceSurface, pSourceRect, pDestSurface, pDestRect, Filter);
	}
	HRESULT WINAPI ColorFill(IDirect3DSurface9* pSurface, CONST RECT* pRect, D3DCOLOR color)
	{
		return m_device->ColorFill(pSurface, pRect, color);
	}
	HRESULT WINAPI CreateOffscreenPlainSurface(UINT Width, UINT Height, D3DFORMAT Format, D3DPOOL Pool, IDirect3DSurface9** ppSurface, HANDLE* pSharedHandle)
	{
		return m_device->CreateOffscreenPlainSurface(Width, Height, Format, Pool, ppSurface, pSharedHandle);
	}
	HRESULT WINAPI SetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9* pRenderTarget)
	{
		return m_device->SetRenderTarget(RenderTargetIndex, pRenderTarget);
	}
	HRESULT WINAPI GetRenderTarget(DWORD RenderTargetIndex, IDirect3DSurface9** ppRenderTarget)
	{
		return m_device->GetRenderTarget(RenderTargetIndex, ppRenderTarget);
	}
	HRESULT WINAPI SetDepthStencilSurface(IDirect3DSurface9* pNewZStencil)
	{
		return m_device->SetDepthStencilSurface(pNewZStencil);
	}
	HRESULT WINAPI GetDepthStencilSurface(IDirect3DSurface9** ppZStencilSurface)
	{
		return m_device->GetDepthStencilSurface(ppZStencilSurface);
	}
	HRESULT WINAPI BeginScene()
	{
		OnGrcBeginScene();
		return m_device->BeginScene();
	}
	HRESULT WINAPI EndScene()
	{
		OnGrcEndScene();
		return m_device->EndScene();
	}
	HRESULT WINAPI Clear(DWORD Count, CONST D3DRECT* pRects, DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
	{
		return m_device->Clear(Count, pRects, Flags, Color, Z, Stencil);
	}
	HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* mat)
	{
		return m_device->SetTransform(State, mat);
	}
	HRESULT WINAPI GetTransform(D3DTRANSFORMSTATETYPE State, D3DMATRIX* mat)
	{
		return m_device->GetTransform(State, mat);
	}
	HRESULT WINAPI MultiplyTransform(D3DTRANSFORMSTATETYPE State, CONST D3DMATRIX* mat)
	{
		return m_device->MultiplyTransform(State, mat);
	}
	HRESULT WINAPI SetViewport(CONST D3DVIEWPORT9* pViewport)
	{
		return m_device->SetViewport(pViewport);
	}
	HRESULT WINAPI GetViewport(D3DVIEWPORT9* pViewport)
	{
		return m_device->GetViewport(pViewport);
	}
	HRESULT WINAPI SetMaterial(CONST D3DMATERIAL9* pMaterial)
	{
		return m_device->SetMaterial(pMaterial);
	}
	HRESULT WINAPI GetMaterial(D3DMATERIAL9* pMaterial)
	{
		return m_device->GetMaterial(pMaterial);
	}
	HRESULT WINAPI SetLight(DWORD Index, CONST D3DLIGHT9* Light)
	{
		return m_device->SetLight(Index, Light);
	}
	HRESULT WINAPI GetLight(DWORD Index, D3DLIGHT9* Light)
	{
		return m_device->GetLight(Index, Light);
	}
	HRESULT WINAPI LightEnable(DWORD Index, BOOL Enable)
	{
		return m_device->LightEnable(Index, Enable);
	}
	HRESULT WINAPI GetLightEnable(DWORD Index, BOOL* pEnable)
	{
		return m_device->GetLightEnable(Index, pEnable);
	}
	HRESULT WINAPI SetClipPlane(DWORD Index, CONST float* pPlane)
	{
		return m_device->SetClipPlane(Index, pPlane);
	}
	HRESULT WINAPI GetClipPlane(DWORD Index, float* pPlane)
	{
		return m_device->GetClipPlane(Index, pPlane);
	}
	HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
	{
		return m_device->SetRenderState(State, Value);
	}
	HRESULT WINAPI GetRenderState(D3DRENDERSTATETYPE State, DWORD* pValue)
	{
		return m_device->GetRenderState(State, pValue);
	}
	HRESULT WINAPI CreateStateBlock(D3DSTATEBLOCKTYPE Type, IDirect3DStateBlock9** ppSB)
	{
		return m_device->CreateStateBlock(Type, ppSB);
	}
	HRESULT WINAPI BeginStateBlock()
	{
		return m_device->BeginStateBlock();
	}
	HRESULT WINAPI EndStateBlock(IDirect3DStateBlock9** ppSB)
	{
		return m_device->EndStateBlock(ppSB);
	}
	HRESULT WINAPI SetClipStatus(CONST D3DCLIPSTATUS9* pClipStatus)
	{
		return m_device->SetClipStatus(pClipStatus);
	}
	HRESULT WINAPI GetClipStatus(D3DCLIPSTATUS9* pClipStatus)
	{
		return m_device->GetClipStatus(pClipStatus);
	}
	HRESULT WINAPI GetTexture(DWORD Stage, IDirect3DBaseTexture9** ppTexture)
	{
		return m_device->GetTexture(Stage, ppTexture);
	}
	HRESULT WINAPI SetTexture(DWORD Stage, IDirect3DBaseTexture9* pTexture)
	{
		return m_device->SetTexture(Stage, pTexture);
	}
	HRESULT WINAPI GetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue)
	{
		return m_device->GetTextureStageState(Stage, Type, pValue);
	}
	HRESULT WINAPI SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
	{
		return m_device->SetTextureStageState(Stage, Type, Value);
	}
	HRESULT WINAPI GetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD* pValue)
	{
		return m_device->GetSamplerState(Sampler, Type, pValue);
	}
	HRESULT WINAPI SetSamplerState(DWORD Sampler, D3DSAMPLERSTATETYPE Type, DWORD Value)
	{
		return m_device->SetSamplerState(Sampler, Type, Value);
	}
	HRESULT WINAPI ValidateDevice(DWORD* pNumPasses)
	{
		return m_device->ValidateDevice(pNumPasses);
	}
	HRESULT WINAPI SetPaletteEntries(UINT PaletteNumber, CONST PALETTEENTRY* pEntries)
	{
		return m_device->SetPaletteEntries(PaletteNumber, pEntries);
	}
	HRESULT WINAPI GetPaletteEntries(UINT PaletteNumber, PALETTEENTRY* pEntries)
	{
		return m_device->GetPaletteEntries(PaletteNumber, pEntries);
	}
	HRESULT WINAPI SetCurrentTexturePalette(UINT PaletteNumber)
	{
		return m_device->SetCurrentTexturePalette(PaletteNumber);
	}
	HRESULT WINAPI GetCurrentTexturePalette(UINT* PaletteNumber)
	{
		return m_device->GetCurrentTexturePalette(PaletteNumber);
	}
	HRESULT WINAPI SetScissorRect(CONST RECT* pRect)
	{
		return m_device->SetScissorRect(pRect);
	}
	HRESULT WINAPI GetScissorRect(RECT* pRect)
	{
		return m_device->GetScissorRect(pRect);
	}
	HRESULT WINAPI SetSoftwareVertexProcessing(BOOL bSoftware)
	{
		return m_device->SetSoftwareVertexProcessing(bSoftware);
	}
	BOOL WINAPI GetSoftwareVertexProcessing()
	{
		return m_device->GetSoftwareVertexProcessing();
	}
	HRESULT WINAPI SetNPatchMode(float nSegments)
	{
		return m_device->SetNPatchMode(nSegments);
	}
	float WINAPI GetNPatchMode()
	{
		return m_device->GetNPatchMode();
	}
	HRESULT WINAPI DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
	{
		return m_device->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
	}
	HRESULT WINAPI DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount)
	{
		return m_device->DrawIndexedPrimitive(PrimitiveType, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
	}
	HRESULT WINAPI DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
	{
		return m_device->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
	}
	HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
	{
		return m_device->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
	}
	HRESULT WINAPI ProcessVertices(UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, IDirect3DVertexBuffer9* pDestBuffer, IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags)
	{
		return m_device->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, pDestBuffer, pVertexDecl, Flags);
	}
	HRESULT WINAPI CreateVertexDeclaration(CONST D3DVERTEXELEMENT9* pVertexElements, IDirect3DVertexDeclaration9** ppDecl)
	{
		return m_device->CreateVertexDeclaration(pVertexElements, ppDecl);
	}
	HRESULT WINAPI SetVertexDeclaration(IDirect3DVertexDeclaration9* pDecl)
	{
		return m_device->SetVertexDeclaration(pDecl);
	}
	HRESULT WINAPI GetVertexDeclaration(IDirect3DVertexDeclaration9** ppDecl)
	{
		return m_device->GetVertexDeclaration(ppDecl);
	}
	HRESULT WINAPI SetFVF(DWORD FVF)
	{
		return m_device->SetFVF(FVF);
	}
	HRESULT WINAPI GetFVF(DWORD* pFVF)
	{
		return m_device->GetFVF(pFVF);
	}
	HRESULT WINAPI CreateVertexShader(CONST DWORD* pFunction, IDirect3DVertexShader9** ppShader)
	{
		return m_device->CreateVertexShader(pFunction, ppShader);
	}
	HRESULT WINAPI SetVertexShader(IDirect3DVertexShader9* pShader)
	{
		return m_device->SetVertexShader(pShader);
	}
	HRESULT WINAPI GetVertexShader(IDirect3DVertexShader9** ppShader)
	{
		return m_device->GetVertexShader(ppShader);
	}
	HRESULT WINAPI SetVertexShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
	{
		return m_device->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}
	HRESULT WINAPI GetVertexShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
	{
		return m_device->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}
	HRESULT WINAPI SetVertexShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
	{
		return m_device->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}
	HRESULT WINAPI GetVertexShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
	{
		return m_device->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}
	HRESULT WINAPI SetVertexShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount)
	{
		return m_device->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	}
	HRESULT WINAPI GetVertexShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
	{
		return m_device->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	}
	HRESULT WINAPI SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9* pStreamData, UINT OffsetInBytes, UINT Stride)
	{
		return m_device->SetStreamSource(StreamNumber, pStreamData, OffsetInBytes, Stride);
	}
	HRESULT WINAPI GetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9** ppStreamData, UINT* OffsetInBytes, UINT* pStride)
	{
		return m_device->GetStreamSource(StreamNumber, ppStreamData, OffsetInBytes, pStride);
	}
	HRESULT WINAPI SetStreamSourceFreq(UINT StreamNumber, UINT Divider)
	{
		return m_device->SetStreamSourceFreq(StreamNumber, Divider);
	}
	HRESULT WINAPI GetStreamSourceFreq(UINT StreamNumber, UINT* Divider)
	{
		return m_device->GetStreamSourceFreq(StreamNumber, Divider);
	}
	HRESULT WINAPI SetIndices(IDirect3DIndexBuffer9* pIndexData)
	{
		return m_device->SetIndices(pIndexData);
	}
	HRESULT WINAPI GetIndices(IDirect3DIndexBuffer9** ppIndexData)
	{
		return m_device->GetIndices(ppIndexData);
	}
	HRESULT WINAPI CreatePixelShader(CONST DWORD* pFunction, IDirect3DPixelShader9** ppShader)
	{
		return m_device->CreatePixelShader(pFunction, ppShader);
	}
	HRESULT WINAPI SetPixelShader(IDirect3DPixelShader9* pShader)
	{
		return m_device->SetPixelShader(pShader);
	}
	HRESULT WINAPI GetPixelShader(IDirect3DPixelShader9** ppShader)
	{
		return m_device->GetPixelShader(ppShader);
	}
	HRESULT WINAPI SetPixelShaderConstantF(UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount)
	{
		return m_device->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}
	HRESULT WINAPI GetPixelShaderConstantF(UINT StartRegister, float* pConstantData, UINT Vector4fCount)
	{
		return m_device->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	}
	HRESULT WINAPI SetPixelShaderConstantI(UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount)
	{
		return m_device->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}
	HRESULT WINAPI GetPixelShaderConstantI(UINT StartRegister, int* pConstantData, UINT Vector4iCount)
	{
		return m_device->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	}
	HRESULT WINAPI SetPixelShaderConstantB(UINT StartRegister, CONST BOOL* pConstantData, UINT BoolCount)
	{
		return m_device->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	}
	HRESULT WINAPI GetPixelShaderConstantB(UINT StartRegister, BOOL* pConstantData, UINT BoolCount)
	{
		return m_device->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	}
	HRESULT WINAPI DrawRectPatch(UINT Handle, CONST float* pNumSegs, CONST D3DRECTPATCH_INFO* pRectPatchInfo)
	{
		return m_device->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
	}
	HRESULT WINAPI DrawTriPatch(UINT Handle, CONST float* pNumSegs, CONST D3DTRIPATCH_INFO* pTriPatchInfo)
	{
		return m_device->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
	}
	HRESULT WINAPI DeletePatch(UINT Handle)
	{
		return m_device->DeletePatch(Handle);
	}
	HRESULT WINAPI CreateQuery(D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery)
	{
		return m_device->CreateQuery(Type, ppQuery);
	}
};

static IDirect3D9* (WINAPI* g_origDirect3DCreate9)(UINT);

class IDirect3D9Proxy : public IDirect3D9
{
private:
	LPDIRECT3D9 m_D3D;
public:
	IDirect3D9Proxy(LPDIRECT3D9 d3d) : m_D3D(d3d)
	{
	}

	static LPDIRECT3D9 WINAPI HookCreateDirect3D9(UINT SDKVersion)
	{
		LPDIRECT3D9 pD3D = g_origDirect3DCreate9(SDKVersion);
		if (!pD3D)
		{
			return nullptr;
		}
		return new IDirect3D9Proxy(pD3D);
	}

	HRESULT WINAPI QueryInterface(REFIID riid, void** ppvObj)
	{
		return m_D3D->QueryInterface(riid, ppvObj);
	}

	ULONG WINAPI AddRef()
	{
		return m_D3D->AddRef();
	}

	ULONG WINAPI Release()
	{
		return m_D3D->Release();
	}

	HRESULT WINAPI RegisterSoftwareDevice(void* pInitializeFunction)
	{
		return m_D3D->RegisterSoftwareDevice(pInitializeFunction);
	}

	UINT WINAPI GetAdapterCount()
	{
		return m_D3D->GetAdapterCount();
	}

	HRESULT WINAPI GetAdapterIdentifier(UINT Adapter, DWORD Flags, D3DADAPTER_IDENTIFIER9* pIdentifier)
	{
		return m_D3D->GetAdapterIdentifier(Adapter, Flags, pIdentifier);
	}

	UINT WINAPI GetAdapterModeCount(UINT Adapter, D3DFORMAT Format)
	{
		return m_D3D->GetAdapterModeCount(Adapter, Format);
	}

	HRESULT WINAPI EnumAdapterModes(UINT Adapter, D3DFORMAT Format, UINT Mode, D3DDISPLAYMODE* pMode)
	{
		return m_D3D->EnumAdapterModes(Adapter, Format, Mode, pMode);
	}

	HRESULT WINAPI GetAdapterDisplayMode(UINT Adapter, D3DDISPLAYMODE* pMode)
	{
		return m_D3D->GetAdapterDisplayMode(Adapter, pMode);
	}

	HRESULT WINAPI CheckDeviceType(UINT Adapter, D3DDEVTYPE DevType, D3DFORMAT AdapterFormat, D3DFORMAT BackBufferFormat, BOOL bWindowed)
	{
		return m_D3D->CheckDeviceType(Adapter, DevType, AdapterFormat, BackBufferFormat, bWindowed);
	}

	HRESULT WINAPI CheckDeviceFormat(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, DWORD Usage, D3DRESOURCETYPE RType, D3DFORMAT CheckFormat)
	{
		return m_D3D->CheckDeviceFormat(Adapter, DeviceType, AdapterFormat, Usage, RType, CheckFormat);
	}

	HRESULT WINAPI CheckDeviceMultiSampleType(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SurfaceFormat, BOOL Windowed, D3DMULTISAMPLE_TYPE MultiSampleType, DWORD* pQualityLevels)
	{
		return m_D3D->CheckDeviceMultiSampleType(Adapter, DeviceType, SurfaceFormat, Windowed, MultiSampleType, pQualityLevels);
	}

	HRESULT WINAPI CheckDepthStencilMatch(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT AdapterFormat, D3DFORMAT RenderTargetFormat, D3DFORMAT DepthStencilFormat)
	{
		return m_D3D->CheckDepthStencilMatch(Adapter, DeviceType, AdapterFormat, RenderTargetFormat, DepthStencilFormat);
	}

	HRESULT WINAPI CheckDeviceFormatConversion(UINT Adapter, D3DDEVTYPE DeviceType, D3DFORMAT SourceFormat, D3DFORMAT TargetFormat)
	{
		return m_D3D->CheckDeviceFormatConversion(Adapter, DeviceType, SourceFormat, TargetFormat);
	}

	HRESULT WINAPI GetDeviceCaps(UINT Adapter, D3DDEVTYPE DeviceType, D3DCAPS9* pCaps)
	{
		return m_D3D->GetDeviceCaps(Adapter, DeviceType, pCaps);
	}

	HMONITOR WINAPI GetAdapterMonitor(UINT Adapter)
	{
		return m_D3D->GetAdapterMonitor(Adapter);
	}

	HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, D3DPRESENT_PARAMETERS* pPresentationParameters, IDirect3DDevice9** ppReturnedDeviceInterface)
	{
		HRESULT result = m_D3D->CreateDevice(Adapter, DeviceType, hFocusWindow, BehaviorFlags, pPresentationParameters, ppReturnedDeviceInterface);

		if (SUCCEEDED(result))
		{
			RegisterPrimaryGPU();
			OnGrcCreateDevice();
			*ppReturnedDeviceInterface = new IDirect3D9DeviceProxy(*ppReturnedDeviceInterface);
		}
		return result;
	}
};

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


static HookFunction hookFunction([]()
{
	/*
	// D3D Device Begin Scene
	static hook::inject_call<void, int> beginSceneCB((ptrdiff_t)hook::get_pattern("56 8B F1 E8 ? ? ? ? 80 7C 24 08 00", 3));
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
	*/

	// Replace Direct3DCreate with our own stub device
	g_origDirect3DCreate9 = hook::iat("d3d9.dll", IDirect3D9Proxy::HookCreateDirect3D9, "Direct3DCreate9");

	// Handle pre, post and D3D Reset failures
	hook::jump(hook::get_pattern("83 C0 10 3D 00 10 00 00", 22), PreAndPostD3DReset);
});
