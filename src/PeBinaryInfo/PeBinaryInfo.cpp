// PeBinaryInfo.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "../PeBinaryInfoLib/PeBinaryInfo.h"

using namespace peinfo;

int wmain(int argc, wchar_t* argv[])
{
	if (argc < 2)
	{
		return 1;
	}

	try
	{
		::CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);

		PeFileFormattedInfoExtractor peInfoExtractor(argv[1]);
		PeFileFormattedInfo peInfo = peInfoExtractor.Extract();

		std::wcout << "File: " << argv[1] << std::endl;

		for (const auto& category : peInfo.Categories)
		{
			std::wcout << L"========== " << category.Name << L" ==========" << std::endl;

			for (const auto& item : category.Items)
			{
				std::wcout << item.Name << L": " << item.Value << std::endl;
				if (!item.Note.empty())
				{
					std::wcout << L"    (" << item.Note << L")" << std::endl;
				}
			}
		}

		//std::wcout << "Machine: " << peInfo.Machine << std::endl;

		//std::wcout << "Architecture: " << peInfo.Architecture << std::endl;

		//std::wcout << "TimeDateStamp: " << peInfo.TimeDateStamp << std::endl;

		//std::wcout << "Subsystem: " << peInfo.Subsystem << std::endl;

		//std::wcout << "Configuration: " << peInfo.Configuration << std::endl;

		return 0;
	}
	catch (const std::exception& e)
	{
		std::cout << "Exception: " << e.what() << std::endl;
		return 1;
	}
}
