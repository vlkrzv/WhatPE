#include "stdafx.h"
#include "ExplorerCommand.h"
#include <shlwapi.h>
#include <strsafe.h>

#pragma comment(lib, "shlwapi.lib")

IFACEMETHODIMP CExplorerCommandVerb::GetTitle(IShellItemArray* /*items*/, LPWSTR* name)
{
	return SHStrDupW(L"What PE?", name);
}

IFACEMETHODIMP CExplorerCommandVerb::GetIcon(IShellItemArray* /*items*/, LPWSTR* icon)
{
	*icon = nullptr;
	return E_NOTIMPL;
}

IFACEMETHODIMP CExplorerCommandVerb::GetToolTip(IShellItemArray* /*items*/, LPWSTR* infoTip)
{
	*infoTip = nullptr;
	return E_NOTIMPL;
}

IFACEMETHODIMP CExplorerCommandVerb::GetCanonicalName(GUID* guidCommandName)
{
	*guidCommandName = GUID_NULL;
	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::GetState(IShellItemArray* items, BOOL /*okToBeSlow*/, EXPCMDSTATE* cmdState)
{
	DWORD count = 0;
	if (items != nullptr && SUCCEEDED(items->GetCount(&count)) && count == 1)
	{
		*cmdState = ECS_ENABLED;
	}
	else
	{
		*cmdState = ECS_HIDDEN;
	}

	return S_OK;
}

extern HMODULE g_hModule;

std::wstring CExplorerCommandVerb::GetTargetExePath()
{
	WCHAR modulePath[MAX_PATH]{};
	if (0 == GetModuleFileNameW(g_hModule, modulePath, MAX_PATH))
	{
		return std::wstring();
	}

	PathRemoveFileSpecW(modulePath);   // .../PeBinaryInfoExplorerCommand
	PathRemoveFileSpecW(modulePath);   // package root

	std::wstring targetPath = modulePath;
	targetPath += L"\\PeBinaryInfoDialog\\WhatPE.Dialog.exe";

	return targetPath;
}

IFACEMETHODIMP CExplorerCommandVerb::Invoke(IShellItemArray* items, IBindCtx* /*bindCtx*/)
{
	if (items == nullptr)
	{
		return S_OK;
	}

	CComPtr<IShellItem> item;
	HRESULT hr = items->GetItemAt(0, &item);
	if (FAILED(hr))
	{
		return S_OK;
	}

	CComHeapPtr<wchar_t> filePath;
	hr = item->GetDisplayName(SIGDN_FILESYSPATH, &filePath);
	if (FAILED(hr))
	{
		return S_OK;
	}

	std::wstring targetExe = GetTargetExePath();
	if (targetExe.empty())
	{
		return S_OK;
	}

	WCHAR commandLine[2 * MAX_PATH + 8]{};
	StringCchPrintfW(commandLine, ARRAYSIZE(commandLine), L"\"%s\" \"%s\"", targetExe.c_str(), (LPCWSTR)filePath);

	STARTUPINFOW si{ sizeof(si) };
	PROCESS_INFORMATION pi{};

	if (CreateProcessW(targetExe.c_str(), commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi))
	{
		CloseHandle(pi.hThread);
		CloseHandle(pi.hProcess);
	}

	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::GetFlags(EXPCMDFLAGS* flags)
{
	*flags = ECF_DEFAULT;
	return S_OK;
}

IFACEMETHODIMP CExplorerCommandVerb::EnumSubCommands(IEnumExplorerCommand** enumCommands)
{
	*enumCommands = nullptr;
	return E_NOTIMPL;
}
