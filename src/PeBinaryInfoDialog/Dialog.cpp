#include "stdafx.h"
#include "Dialog.h"
#include "Resource.h"
#include <commctrl.h>

#pragma comment(lib, "comctl32.lib")

namespace
{
	struct DialogState
	{
		const peinfo::PeFileFormattedInfo* PeInfo;
		std::wstring FilePath;
		std::vector<HWND> TabListViews;
	};

	std::wstring GetFileNameFromPath(const std::wstring& filePath)
	{
		size_t pos = filePath.find_last_of(L"\\/");
		return (pos == std::wstring::npos) ? filePath : filePath.substr(pos + 1);
	}

	void PopulateListView(HWND hwndListView, const peinfo::PeFileFormattedInfoCategory& category)
	{
		LV_COLUMN lvColumn{};
		lvColumn.mask = LVCF_TEXT | LVCF_WIDTH;

		lvColumn.cx = 140;
		lvColumn.pszText = const_cast<LPWSTR>(L"Property");
		ListView_InsertColumn(hwndListView, 0, &lvColumn);

		lvColumn.cx = 140;
		lvColumn.pszText = const_cast<LPWSTR>(L"Value");
		ListView_InsertColumn(hwndListView, 1, &lvColumn);

		lvColumn.cx = 220;
		lvColumn.pszText = const_cast<LPWSTR>(L"Note");
		ListView_InsertColumn(hwndListView, 2, &lvColumn);

		for (size_t i = 0; i < category.Items.size(); ++i)
		{
			const auto& item = category.Items[i];

			LVITEM lvItem{};
			lvItem.mask = LVIF_TEXT;
			lvItem.iItem = static_cast<int>(i);

			lvItem.iSubItem = 0;
			lvItem.pszText = const_cast<LPWSTR>(item.Name.c_str());
			ListView_InsertItem(hwndListView, &lvItem);

			lvItem.iSubItem = 1;
			lvItem.pszText = const_cast<LPWSTR>(item.Value.c_str());
			ListView_SetItem(hwndListView, &lvItem);

			if (!item.Note.empty())
			{
				lvItem.iSubItem = 2;
				lvItem.pszText = const_cast<LPWSTR>(item.Note.c_str());
				ListView_SetItem(hwndListView, &lvItem);
			}
		}

		ListView_SetColumnWidth(hwndListView, 0, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(hwndListView, 1, LVSCW_AUTOSIZE_USEHEADER);
		ListView_SetColumnWidth(hwndListView, 2, LVSCW_AUTOSIZE_USEHEADER);
	}

	void LayoutTabContent(HWND hwndTab, HWND hwndListView)
	{
		RECT rect{};
		GetClientRect(hwndTab, &rect);
		TabCtrl_AdjustRect(hwndTab, FALSE, &rect);
		MapWindowPoints(hwndTab, GetParent(hwndTab), reinterpret_cast<POINT*>(&rect), 2);

		MoveWindow(hwndListView, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}

	void CreateTabsAndListViews(HWND hwndDlg, DialogState* state)
	{
		HWND hwndTab = GetDlgItem(hwndDlg, IDC_TAB);

		for (size_t i = 0; i < state->PeInfo->Categories.size(); ++i)
		{
			const auto& category = state->PeInfo->Categories[i];

			TCITEM tcItem{};
			tcItem.mask = TCIF_TEXT;
			tcItem.pszText = const_cast<LPWSTR>(category.Name.c_str());
			TabCtrl_InsertItem(hwndTab, static_cast<int>(i), &tcItem);

			HWND hwndListView = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				WC_LISTVIEWW,
				L"",
				WS_CHILD | WS_TABSTOP | LVS_REPORT | LVS_SHOWSELALWAYS,
				0, 0, 0, 0,
				hwndTab,
				nullptr,
				reinterpret_cast<HINSTANCE>(GetWindowLongPtr(hwndDlg, GWLP_HINSTANCE)),
				nullptr);

			ListView_SetExtendedListViewStyle(hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			PopulateListView(hwndListView, category);
			LayoutTabContent(hwndTab, hwndListView);

			state->TabListViews.push_back(hwndListView);
		}

		if (!state->TabListViews.empty())
		{
			ShowWindow(state->TabListViews[0], SW_SHOW);
			for (size_t i = 1; i < state->TabListViews.size(); ++i)
			{
				ShowWindow(state->TabListViews[i], SW_HIDE);
			}
		}
	}

	INT_PTR CALLBACK MainDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_INITDIALOG:
			{
				DialogState* state = reinterpret_cast<DialogState*>(lParam);
				SetWindowLongPtr(hwndDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));

				std::wstring title = L"WhatPE - " + GetFileNameFromPath(state->FilePath);
				SetWindowTextW(hwndDlg, title.c_str());

				CreateTabsAndListViews(hwndDlg, state);
				return TRUE;
			}

		case WM_NOTIFY:
			{
				LPNMHDR nmhdr = reinterpret_cast<LPNMHDR>(lParam);
				if (nmhdr->idFrom == IDC_TAB && nmhdr->code == TCN_SELCHANGE)
				{
					DialogState* state = reinterpret_cast<DialogState*>(GetWindowLongPtr(hwndDlg, GWLP_USERDATA));
					HWND hwndTab = GetDlgItem(hwndDlg, IDC_TAB);
					int selected = TabCtrl_GetCurSel(hwndTab);

					for (size_t i = 0; i < state->TabListViews.size(); ++i)
					{
						ShowWindow(state->TabListViews[i], (static_cast<int>(i) == selected) ? SW_SHOW : SW_HIDE);
					}
				}
			}
			return TRUE;

		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_CLOSE || LOWORD(wParam) == IDCANCEL)
			{
				EndDialog(hwndDlg, 0);
				return TRUE;
			}
			return FALSE;

		case WM_CLOSE:
			EndDialog(hwndDlg, 0);
			return TRUE;
		}

		return FALSE;
	}
}

void ShowPeInfoDialog(HINSTANCE hInstance, const std::wstring& filePath, const peinfo::PeFileFormattedInfo& peInfo)
{
	INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES };
	InitCommonControlsEx(&icc);

	DialogState state;
	state.PeInfo = &peInfo;
	state.FilePath = filePath;

	DialogBoxParamW(hInstance, MAKEINTRESOURCEW(IDD_MAIN), nullptr, MainDlgProc, reinterpret_cast<LPARAM>(&state));
}
