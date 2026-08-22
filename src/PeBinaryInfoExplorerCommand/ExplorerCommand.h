#pragma once

#include "Guids.h"

class ATL_NO_VTABLE CExplorerCommandVerb :
	public CComObjectRootEx<CComSingleThreadModel>,
	public CComCoClass<CExplorerCommandVerb, &__uuidof(WhatPEExplorerCommandClass)>,
	public IExplorerCommand
{
public:
	CExplorerCommandVerb()
	{
	}

	DECLARE_NOT_AGGREGATABLE(CExplorerCommandVerb)
	DECLARE_NO_REGISTRY()

	BEGIN_COM_MAP(CExplorerCommandVerb)
		COM_INTERFACE_ENTRY(IExplorerCommand)
	END_COM_MAP()

	// IExplorerCommand
	IFACEMETHODIMP GetTitle(_In_opt_ IShellItemArray* items, _Outptr_result_nullonfailure_ LPWSTR* name);
	IFACEMETHODIMP GetIcon(_In_opt_ IShellItemArray* items, _Outptr_result_nullonfailure_ LPWSTR* icon);
	IFACEMETHODIMP GetToolTip(_In_opt_ IShellItemArray* items, _Outptr_result_nullonfailure_ LPWSTR* infoTip);
	IFACEMETHODIMP GetCanonicalName(_Out_ GUID* guidCommandName);
	IFACEMETHODIMP GetState(_In_opt_ IShellItemArray* items, _In_ BOOL okToBeSlow, _Out_ EXPCMDSTATE* cmdState);
	IFACEMETHODIMP Invoke(_In_opt_ IShellItemArray* items, _In_opt_ IBindCtx* bindCtx);
	IFACEMETHODIMP GetFlags(_Out_ EXPCMDFLAGS* flags);
	IFACEMETHODIMP EnumSubCommands(_Outptr_result_nullonfailure_ IEnumExplorerCommand** enumCommands);

private:
	std::wstring GetTargetExePath();
};

OBJECT_ENTRY_AUTO(__uuidof(WhatPEExplorerCommandClass), CExplorerCommandVerb)
