#include "stdafx.h"

#include <windows.h>

#include <shared/Detours/src/detours.h>

#include <d3d11.h>

#include <tchar.h>

LONG error = NO_ERROR;
HMODULE hD3d11Dll = NULL;

int g_Override = 0;

HANDLE g_hFbTexture;
HANDLE g_hFbDepthTexture;


typedef HRESULT (STDMETHODCALLTYPE * CreateTexture2D_t)(
	ID3D11Device * This,
	/* [annotation] */
	_In_  const D3D11_TEXTURE2D_DESC *pDesc,
	/* [annotation] */
	_In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	_COM_Outptr_opt_  ID3D11Texture2D **ppTexture2D);

CreateTexture2D_t True_CreateTexture2D;

HRESULT STDMETHODCALLTYPE My_CreateTexture2D(
	ID3D11Device * This,
	/* [annotation] */
	_In_  const D3D11_TEXTURE2D_DESC *pDesc,
	/* [annotation] */
	_In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	_COM_Outptr_opt_  ID3D11Texture2D **ppTexture2D)
{
	if (0 < g_Override) {

		TCHAR myFormat[33];

		_stprintf_s(myFormat, _T("pDesc->Format = 0x%08x"), pDesc->Format);

		MessageBox(NULL, myFormat, _T("My_CreateTexture2D"), MB_OK | MB_ICONINFORMATION);
	}

	switch (g_Override)
	{
	case 1:
		++g_Override;
		return This->OpenSharedResource(g_hFbTexture, __uuidof(ID3D11Texture2D), (void **)ppTexture2D);
	case 2:
		g_Override = 0;
		return This->OpenSharedResource(g_hFbDepthTexture, __uuidof(ID3D11Texture2D), (void **)ppTexture2D);
	}

	return True_CreateTexture2D(
		This,
		pDesc,
		pInitialData,
		ppTexture2D
	);
}

typedef HRESULT(WINAPI * D3D11CreateDevice_t)(
	_In_opt_        IDXGIAdapter        *pAdapter,
	D3D_DRIVER_TYPE     DriverType,
	HMODULE             Software,
	UINT                Flags,
	_In_opt_  const D3D_FEATURE_LEVEL   *pFeatureLevels,
	UINT                FeatureLevels,
	UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext
);

D3D11CreateDevice_t TrueD3D11CreateDevice = NULL;

HRESULT WINAPI MyD3D11CreateDevice(
	_In_opt_        IDXGIAdapter        *pAdapter,
	D3D_DRIVER_TYPE     DriverType,
	HMODULE             Software,
	UINT                Flags,
	_In_opt_  const D3D_FEATURE_LEVEL   *pFeatureLevels,
	UINT                FeatureLevels,
	UINT                SDKVersion,
	_Out_opt_       ID3D11Device        **ppDevice,
	_Out_opt_       D3D_FEATURE_LEVEL   *pFeatureLevel,
	_Out_opt_       ID3D11DeviceContext **ppImmediateContext
) {
	HRESULT result = TrueD3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels, FeatureLevels, SDKVersion, ppDevice, pFeatureLevel, ppImmediateContext);

	if (IS_ERROR(result) || NULL == ppDevice)
	{
		error = E_FAIL;
		return result;
	}

	if (NULL == True_CreateTexture2D)
	{
		void ** pCreateTexture2D = (void **)((*(char **)(*ppDevice)) + sizeof(void *) * 5);

		True_CreateTexture2D = (CreateTexture2D_t)*pCreateTexture2D;

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&(PVOID&)True_CreateTexture2D, My_CreateTexture2D);
		error = DetourTransactionCommit();
	}

	/*
	DWORD oldProtect;

	if (!VirtualProtect(pCreateRenderTargetView, sizeof(void *), PAGE_EXECUTE_READWRITE, &oldProtect))
	{
		error = E_FAIL;
		return result;
	}

	if (NULL != ppImmediateContext) MessageBox(0, _T(";-)"), _T(":-O"), MB_OK | MB_ICONERROR);

	*pCreateRenderTargetView = MyCreateRenderTargetView;

	VirtualProtect(pCreateRenderTargetView, sizeof(void *), oldProtect, NULL);
	*/

	return result;
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		{
			hD3d11Dll = LoadLibrary(_T("d3d11.dll"));

			TrueD3D11CreateDevice = (D3D11CreateDevice_t)GetProcAddress(hD3d11Dll,"D3D11CreateDevice");

			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourAttach(&(PVOID&)TrueD3D11CreateDevice, MyD3D11CreateDevice);
			error = DetourTransactionCommit();

		}
		break;
    case DLL_THREAD_ATTACH:
		break;
    case DLL_THREAD_DETACH:
		break;
	case DLL_PROCESS_DETACH:
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			DetourDetach(&(PVOID&)TrueD3D11CreateDevice, MyD3D11CreateDevice);
			error = DetourTransactionCommit();

			FreeLibrary(hD3d11Dll);
		}
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) bool AfxHookUnityStatus(void) {
	return NO_ERROR == error;
}


extern "C" __declspec(dllexport) void AfxHookUnityBeginCreateRenderTexture(HANDLE fbSharedHandle, HANDLE fbDepthSharedHandle)
{
	g_hFbTexture = fbSharedHandle;
	g_hFbDepthTexture = fbSharedHandle;

	g_Override = 1;
}

extern "C" __declspec(dllexport) void AfxHookUnityEndCreateRenderTexture()
{
	g_Override = 0;
}
