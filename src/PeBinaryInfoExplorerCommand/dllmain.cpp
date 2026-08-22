// dllmain.cpp : Implementation of DllMain and the standard COM server DLL exports.
//
// This DLL is never registered via the registry - it's activated purely through the MSIX package
// manifest (com:SurrogateServer, hosted out-of-process by dllhost.exe), which resolves the CLSID
// declared in Guids.h directly to this DLL's package-relative path.

#include "stdafx.h"
#include "Module.h"

CExplorerCommandModule _AtlModule;

HMODULE g_hModule = nullptr;

extern "C" BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
	{
		g_hModule = hInstance;
	}

	return _AtlModule.DllMain(dwReason, lpReserved);
}

STDAPI DllCanUnloadNow(void)
{
	return _AtlModule.DllCanUnloadNow();
}

_Check_return_
STDAPI DllGetClassObject(_In_ REFCLSID rclsid, _In_ REFIID riid, _Outptr_ LPVOID* ppv)
{
	return _AtlModule.DllGetClassObject(rclsid, riid, ppv);
}
