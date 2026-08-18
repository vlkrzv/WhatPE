// PeBinaryInfoShellExt.cpp : Implementation of CPeBinaryInfoShellExt

#include "stdafx.h"
#include "PeBinaryInfoShellExt.h"
#include "dllmain.h"
#include "../PeBinaryInfoLib/PeBinaryInfo.h"

// CPeBinaryInfoShellExt

STDMETHODIMP CPeBinaryInfoShellExt::Initialize(
	LPCITEMIDLIST pidlFolder,
	LPDATAOBJECT pDataObj,
	HKEY hProgID)
{
	FORMATETC fmt = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
	STGMEDIUM stg = { TYMED_HGLOBAL };
	HDROP hDrop;

	// Look for CF_HDROP data in the data object. If there
	// is no such data, return an error back to Explorer.
	if (FAILED(pDataObj->GetData(&fmt, &stg)))
		return E_INVALIDARG;

	// Get a pointer to the actual data.
	hDrop = (HDROP)GlobalLock(stg.hGlobal);

	// Make sure it worked.
	if (NULL == hDrop)
		return E_INVALIDARG;

	// Sanity check – make sure there is at least one filename.
	UINT uNumFiles = DragQueryFile(hDrop, 0xFFFFFFFF, NULL, 0);
	HRESULT hr = S_OK;

	if (1 != uNumFiles)
	{
		GlobalUnlock(stg.hGlobal);
		ReleaseStgMedium(&stg);
		return E_INVALIDARG;
	}

	// Get the name of the first file and store it in our
	// member variable m_szFile.
	WCHAR file[MAX_PATH + 1]{};
	if (0 == DragQueryFile(hDrop, 0, file, MAX_PATH))
		hr = E_INVALIDARG;

	m_filePath = file;

	GlobalUnlock(stg.hGlobal);
	ReleaseStgMedium(&stg);

	return hr;
}

INT_PTR CALLBACK PropPageDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bRet = FALSE;

	switch (uMsg)
	{
	case WM_SHOWWINDOW:
		{
			if (wParam != FALSE && lParam == 0)
			{
				RECT r{};
				GetClientRect(hwnd, &r);

				HWND hwndPeInfoListView = GetDlgItem(hwnd, IDC_LIST);

				MoveWindow(hwndPeInfoListView, 0 + 10, 0 + 12, r.right - 20, r.bottom - 24, TRUE);
			}
		}
		break;

	case WM_CTLCOLORDLG:
		return (INT_PTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	case WM_DESTROY:
		{
			HBRUSH dlgBrush = (HBRUSH)GetWindowLongPtr(hwnd, GWLP_USERDATA);
			if (dlgBrush != nullptr)
			{
				DeleteObject(dlgBrush);
				SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
			}
		}
		break;

	case WM_INITDIALOG:
		{
			bRet = TRUE;

			HBRUSH dlgBrush = CreateSolidBrush(RGB(255, 255, 255));
			SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)dlgBrush);

			PROPSHEETPAGE*  ppsp = (PROPSHEETPAGE*)lParam;

			peinfo::PeFileFormattedInfo* p = (peinfo::PeFileFormattedInfo*)ppsp->lParam;

			HWND hwndPeInfoListView = GetDlgItem(hwnd, IDC_LIST);
			
			LV_COLUMN lvColumn{};
			lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;
			lvColumn.cx = 100;

			lvColumn.pszText = L"Property";
			ListView_InsertColumn(hwndPeInfoListView, 0, &lvColumn);

			lvColumn.pszText = L"Value";
			ListView_InsertColumn(hwndPeInfoListView, 1, &lvColumn);

			for (int i = 0; i < p->Categories.size(); ++i)
			{
				const auto& category = p->Categories.at(i);

				LVGROUP lvGroup;
				lvGroup.cbSize = sizeof(LVGROUP);
				lvGroup.mask = LVGF_HEADER | LVGF_GROUPID;

				lvGroup.pszHeader = const_cast<LPWSTR>(category.Name.c_str());
				lvGroup.iGroupId = i;
				ListView_InsertGroup(hwndPeInfoListView, -1, &lvGroup);

				for (int j = 0; j < category.Items.size(); ++j)
				{
					const auto& item = category.Items[j];

					LVITEM lvItem{};
					lvItem.mask = LVIF_TEXT | LVIF_GROUPID;
					lvItem.iGroupId = i;

					lvItem.iSubItem = 0;
					lvItem.pszText = const_cast<LPWSTR>(item.Name.c_str());
					ListView_InsertItem(hwndPeInfoListView, &lvItem);

					lvItem.mask = LVIF_TEXT;
					lvItem.iSubItem = 1;
					lvItem.pszText = const_cast<LPWSTR>(item.Value.c_str());
					ListView_SetItem(hwndPeInfoListView, &lvItem);
				}
			}

			ListView_EnableGroupView(hwndPeInfoListView, TRUE); // todo check ret
			ListView_SetColumnWidth(hwndPeInfoListView, 0, LVSCW_AUTOSIZE);
			ListView_SetColumnWidth(hwndPeInfoListView, 1, LVSCW_AUTOSIZE);
		}
		break;
	}

	return bRet;
}

UINT CALLBACK PropPageCallbackProc(HWND hwnd, UINT uMsg, LPPROPSHEETPAGE ppsp)
{
	if (PSPCB_RELEASE == uMsg)
	{
		delete (peinfo::PeFileFormattedInfo*)ppsp->lParam;
	}

	return 1;
}

STDMETHODIMP CPeBinaryInfoShellExt::AddPages(LPFNADDPROPSHEETPAGE lpfnAddPageProc, LPARAM lParam)
{
	peinfo::PeFileFormattedInfo peFileFormattedInfo;
	try
	{
		peinfo::PeFileFormattedInfoExtractor peFileFormattedInfoExtractor(m_filePath);
		peFileFormattedInfo = peFileFormattedInfoExtractor.Extract();
	}
	catch (const std::exception&)
	{
		return E_UNEXPECTED;
	}
	
	PROPSHEETPAGE  psp{};
	HPROPSHEETPAGE hPage;

	psp.dwSize = sizeof(PROPSHEETPAGE);
	psp.dwFlags = PSP_USEREFPARENT | PSP_USETITLE | PSP_USECALLBACK;
	psp.hInstance = _AtlBaseModule.GetResourceInstance();
	psp.pszTemplate = MAKEINTRESOURCE(IDD_PROPPAGE_PEINFO);
	psp.pszTitle = L"PE Image";
	psp.pfnDlgProc = PropPageDlgProc;
	psp.lParam = (LPARAM)new peinfo::PeFileFormattedInfo(peFileFormattedInfo);
	psp.pfnCallback = PropPageCallbackProc;
	psp.pcRefParent = (UINT*)&_AtlModule.m_nLockCnt;

	hPage = CreatePropertySheetPage(&psp);

	if (NULL != hPage)
	{
		// Call the shell's callback function, so it adds the page to
		// the property sheet.
		if (!lpfnAddPageProc(hPage, lParam))
		{
			DestroyPropertySheetPage(hPage);
		}
	}

	return 1;
}