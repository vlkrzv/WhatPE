#include "stdafx.h"
#include "Dialog.h"

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
	int argc = 0;
	LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	if (argv == nullptr || argc < 2)
	{
		MessageBoxW(nullptr, L"No file specified.", L"WhatPE", MB_OK | MB_ICONERROR);
		return 1;
	}

	std::wstring filePath = argv[1];
	LocalFree(argv);

	try
	{
		::CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

		peinfo::PeFileFormattedInfoExtractor extractor(filePath);
		peinfo::PeFileFormattedInfo peInfo = extractor.Extract();

		ShowPeInfoDialog(hInstance, filePath, peInfo);
	}
	catch (const std::exception& e)
	{
		std::string message = std::string("Could not read this file: ") + e.what();
		MessageBoxA(nullptr, message.c_str(), "WhatPE", MB_OK | MB_ICONERROR);
		return 1;
	}

	return 0;
}
