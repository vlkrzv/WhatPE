#include "stdafx.h"
#include "PeBinaryInfo.h"
#include "Helpers.h"
#include "Metadata.h"

namespace peinfo
{
	void HandleLogicError(bool errorOccurred, const char* message)
	{
		if (errorOccurred)
		{
			throw std::logic_error(message);
		}
	}

	void HandleWin32Error(bool errorOccurred)
	{
		if (errorOccurred)
		{
			throw std::system_error(std::error_code(GetLastError(), std::system_category()));
		}
	}

	void HandleFormatError(bool errorOccurred, const char* message)
	{
		if (errorOccurred)
		{
			throw std::runtime_error(message);
		}
	}

	MappedPeFile::MappedPeFile(std::wstring filePath)
	{
		HANDLE fileHandle = CreateFile(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
		HandleWin32Error(fileHandle == INVALID_HANDLE_VALUE);

		try
		{
			LARGE_INTEGER fileSize{};
			BOOL result = GetFileSizeEx(fileHandle, &fileSize);
			HandleWin32Error(result == FALSE);

			HANDLE fileMappingHandle = CreateFileMapping(fileHandle, nullptr, PAGE_READONLY, 0, 0, nullptr);
			HandleWin32Error(fileMappingHandle == nullptr);

			try
			{
				base_ = MapViewOfFile(fileMappingHandle, FILE_MAP_READ, 0, 0, 0);
				HandleWin32Error(base_ == nullptr);
			}
			catch (...)
			{
				CloseHandle(fileMappingHandle);
				throw;
			}

			result = CloseHandle(fileMappingHandle);
			HandleWin32Error(result == FALSE);
		}
		catch (...)
		{
			CloseHandle(fileHandle);
			throw;
		}

		BOOL result = CloseHandle(fileHandle);
		HandleWin32Error(result == FALSE);
	}

	MappedPeFile::~MappedPeFile()
	{
		if (base_ != nullptr)
		{
			UnmapViewOfFile(base_);
		}
	}

	LPVOID MappedPeFile::GetBaseAddress()
	{
		return base_;
	}

	PeFileInfoExtractor::PeFileInfoExtractor(std::wstring filePath)
		: filePath_(filePath), mappedPeFile_(filePath)
	{
		PIMAGE_DOS_HEADER imageDosHeader = (PIMAGE_DOS_HEADER)mappedPeFile_.GetBaseAddress();
		HandleFormatError(imageDosHeader->e_magic != IMAGE_DOS_SIGNATURE, "No IMAGE_DOS_SIGNATURE present");

		imageNtHeaders_ = (IMAGE_NT_HEADERS_3264*)(((uint8_t*)imageDosHeader) + imageDosHeader->e_lfanew);
		HandleFormatError(imageNtHeaders_->Signature != IMAGE_NT_SIGNATURE, "No IMAGE_NT_SIGNATURE present");
	}

	WORD PeFileInfoExtractor::GetMachine()
	{
		return imageNtHeaders_->FileHeader.Machine;
	}

	DWORD PeFileInfoExtractor::GetTimeDateStamp()
	{
		return imageNtHeaders_->FileHeader.TimeDateStamp;
	}

	WORD PeFileInfoExtractor::GetSubsystem()
	{
		return imageNtHeaders_->OptionalHeader32.Subsystem;
	}

	WORD PeFileInfoExtractor::GetLinkerVersion()
	{
		return MAKEWORD(imageNtHeaders_->OptionalHeader32.MinorLinkerVersion, imageNtHeaders_->OptionalHeader32.MajorLinkerVersion);
	}

	bool PeFileInfoExtractor::IsDll()
	{
		return (imageNtHeaders_->FileHeader.Characteristics & IMAGE_FILE_DLL) != 0;
	}

	bool PeFileInfoExtractor::IsPe32Plus()
	{
		switch (imageNtHeaders_->OptionalHeader32.Magic)
		{
		case IMAGE_NT_OPTIONAL_HDR32_MAGIC:
			return false;
		case IMAGE_NT_OPTIONAL_HDR64_MAGIC:
			return true;
		default:
			throw std::runtime_error("Unsupported IMAGE_OPTIONAL_HEADER.Magic value");
		}
	}

	BuildConfiguration PeFileInfoExtractor::GetBuildConfiguration()
	{
		ClrHeaderInfo clrHeaderInfo = GetClrHeaderInfo();
		if (clrHeaderInfo.IsPresent)
		{
			return clrHeaderInfo.AreOptimizationsDisabled ? BuildConfiguration::Debug : BuildConfiguration::Release;
		}

		return peFileVersionInfoProvider_.GetVersionInfo(filePath_).BuildConfiguration;
	}

	ClrHeaderInfo PeFileInfoExtractor::GetClrHeaderInfo()
	{
		PIMAGE_COR20_HEADER clrHeader;
		if (!TryGetClrHeader(clrHeader))
		{
			return ClrHeaderInfo();
		}

		ClrHeaderInfo clrHeaderInfo;
		clrHeaderInfo.IsPresent = true;
		clrHeaderInfo.Flags = clrHeader->Flags;
		clrHeaderInfo.TargetFramework = GetTargetFramework(clrHeader);
		clrHeaderInfo.AssemblyVersion = GetAssemblyVersion(clrHeader);
		clrHeaderInfo.AreOptimizationsDisabled = AreOptimizationsDisabled(clrHeader);
		return clrHeaderInfo;
	}

	DWORD PeFileInfoExtractor::GetDllCharacteristics()
	{
		return imageNtHeaders_->OptionalHeader32.DllCharacteristics;
	}

	PIMAGE_DATA_DIRECTORY PeFileInfoExtractor::GetDataDirectory()
	{
		return IsPe32Plus() ? imageNtHeaders_->OptionalHeader64.DataDirectory : imageNtHeaders_->OptionalHeader32.DataDirectory;
	}

	PIMAGE_SECTION_HEADER PeFileInfoExtractor::GetSectionHeader()
	{
		return (PIMAGE_SECTION_HEADER)((uint8_t*)GetDataDirectory() + IMAGE_NUMBEROF_DIRECTORY_ENTRIES * sizeof(IMAGE_DATA_DIRECTORY));
	}

	bool PeFileInfoExtractor::TryGetClrHeader(PIMAGE_COR20_HEADER& clrHeader)
	{
		IMAGE_DATA_DIRECTORY clrDirectory = GetDataDirectory()[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR];
		if (clrDirectory.VirtualAddress == 0)
		{
			clrHeader = nullptr;
			return false;
		}

		clrHeader = (IMAGE_COR20_HEADER*)(((uint8_t*)mappedPeFile_.GetBaseAddress()) + RvaToFileOffset(clrDirectory.VirtualAddress));

		return true;
	}

	std::wstring utf8_to_utf16(const std::string &source)
	{
		if (source.empty()) 
		{ 
			return std::wstring(); 
		}
		
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &source[0], (int)source.size(), NULL, 0);
		CheckError(sizeNeeded != 0, "MultiByteToWideChar failed");

		std::wstring result(sizeNeeded, 0);
		auto charsWritten = MultiByteToWideChar(CP_UTF8, 0, &source[0], (int)source.size(), &result[0], sizeNeeded);
		CheckError(charsWritten != 0, "MultiByteToWideChar failed");

		return result;
	}

	std::wstring PeFileInfoExtractor::GetTargetFramework(PIMAGE_COR20_HEADER clrHeader)
	{
		auto metadataStartAddress = AddOffset<void>(mappedPeFile_.GetBaseAddress(), RvaToFileOffset(clrHeader->MetaData.VirtualAddress));

		CustomAttributeReader reader(metadataStartAddress, clrHeader->MetaData.Size);

		auto targetFramework = reader.GetTargetFramework();

		return utf8_to_utf16(targetFramework);
	}

	std::wstring PeFileInfoExtractor::GetAssemblyVersion(PIMAGE_COR20_HEADER clrHeader)
	{
		auto metadataStartAddress = AddOffset<void>(mappedPeFile_.GetBaseAddress(), RvaToFileOffset(clrHeader->MetaData.VirtualAddress));

		CustomAttributeReader reader(metadataStartAddress, clrHeader->MetaData.Size);

		auto assemblyVersion = reader.GetAssemblyVersion();

		return utf8_to_utf16(assemblyVersion);
	}

	bool PeFileInfoExtractor::AreOptimizationsDisabled(PIMAGE_COR20_HEADER clrHeader)
	{
		auto metadataStartAddress = AddOffset<void>(mappedPeFile_.GetBaseAddress(), RvaToFileOffset(clrHeader->MetaData.VirtualAddress));

		CustomAttributeReader reader(metadataStartAddress, clrHeader->MetaData.Size);

		return reader.AreOptimizationsDisabled();
	}

	DWORD PeFileInfoExtractor::RvaToFileOffset(DWORD rva)
	{
		PIMAGE_SECTION_HEADER sectionsHeader = GetSectionHeader();

		DWORD fileOffset = 0;
		for (WORD i = 0; i < imageNtHeaders_->FileHeader.NumberOfSections; i++, sectionsHeader++)
		{
			if (rva >= sectionsHeader->VirtualAddress && rva < (sectionsHeader->VirtualAddress + sectionsHeader->SizeOfRawData))
			{
				fileOffset = rva - sectionsHeader->VirtualAddress + sectionsHeader->PointerToRawData;
				break;
			}
		}

		HandleFormatError(fileOffset == 0, "Failed to convert RVA");
		return fileOffset;
	}

	PeFileFormattedInfoExtractor::PeFileFormattedInfoExtractor(std::wstring filePath)
		: peFileInfoExtractor_(filePath)
	{
	}

	PeFileFormattedInfo PeFileFormattedInfoExtractor::Extract()
	{
		std::vector<PeFileFormattedInfoCategory> categories;

		PeFileFormattedInfoItem description = { L"Description", GetDescription() };
		PeFileFormattedInfoCategory generalCategory = { L"General", { description } };
		categories.push_back(generalCategory);

		PeFileFormattedInfoItem buildTime = { L"Build time", GetTimeDateStamp() };
		PeFileFormattedInfoItem configuration = { L"Configuration", GetConfiguration() };
		PeFileFormattedInfoItem platform = { L"Platform", GetPlatform() };
		PeFileFormattedInfoItem toolset = { L"Toolset", GetToolset() };
		PeFileFormattedInfoCategory buildCategory = { L"Build", { buildTime, configuration, platform, toolset } };
		categories.push_back(buildCategory);

		ClrHeaderInfo clrHeaderInfo = peFileInfoExtractor_.GetClrHeaderInfo();
		if (clrHeaderInfo.IsPresent)
		{
			PeFileFormattedInfoItem targetFramework = { L"Target Framework", clrHeaderInfo.TargetFramework };
			PeFileFormattedInfoItem assemblyVersion = { L"Assembly Version", clrHeaderInfo.AssemblyVersion };
			PeFileFormattedInfoCategory dotNetCategory = { L".NET", { targetFramework, assemblyVersion } };
			categories.push_back(dotNetCategory);
		}

		PeFileFormattedInfoItem depStatus = { L"DEP", GetDepStatus() };
		PeFileFormattedInfoItem aslrStatus = { L"ASLR", GetAslrStatus() };
		PeFileFormattedInfoItem cfgStatus = { L"CFG", GetCfgStatus() };
		PeFileFormattedInfoCategory securityCategory = { L"Security", { depStatus, aslrStatus, cfgStatus } };
		categories.push_back(securityCategory);

		return PeFileFormattedInfo { categories };
	}

	std::wstring PeFileFormattedInfoExtractor::GetDescription()
	{
		WORD subsystem = peFileInfoExtractor_.GetSubsystem();

		if (subsystem == IMAGE_SUBSYSTEM_NATIVE)
		{
			return L"Windows kernel component or driver";
		}

		if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_GUI || subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
		{
			if (peFileInfoExtractor_.IsDll())
			{
				return L"Windows Dynamic-Link Library (DLL)";
			}

			if (subsystem == IMAGE_SUBSYSTEM_WINDOWS_CUI)
			{
				return L"Windows Console Application (EXE)";
			}

			return L"Windows Application (EXE)";
		}

		if (subsystem == IMAGE_SUBSYSTEM_UNKNOWN)
		{
			return L"Portable Executable Image";
		}

		return GetSubsystem();		
	}

	std::wstring PeFileFormattedInfoExtractor::GetMachine()
	{
		WORD machine = peFileInfoExtractor_.GetMachine();
		switch (machine)
		{
		case IMAGE_FILE_MACHINE_I386:
			return L"i386";
		case IMAGE_FILE_MACHINE_AMD64:
			return L"AMD64";
		case IMAGE_FILE_MACHINE_IA64:
			return L"IA-64";
		case IMAGE_FILE_MACHINE_R3000:
			return L"MIPS R3000";
		case IMAGE_FILE_MACHINE_R4000:
			return L"MIPS R4000";
		case IMAGE_FILE_MACHINE_R10000:
			return L"MIPS R10000";
		case IMAGE_FILE_MACHINE_WCEMIPSV2:
			return L"MIPS WCE v2";
		case IMAGE_FILE_MACHINE_ALPHA:
			return L"DEC Alpha / Alpha AXP";
		case IMAGE_FILE_MACHINE_SH3:
			return L"SH3";
		case IMAGE_FILE_MACHINE_SH3DSP:
			return L"SH3DSP";
		case IMAGE_FILE_MACHINE_SH3E:
			return L"SH3E";
		case IMAGE_FILE_MACHINE_SH4:
			return L"SH4";
		case IMAGE_FILE_MACHINE_SH5:
			return L"SH5";
		case IMAGE_FILE_MACHINE_ARM:
			return L"ARM";
		case IMAGE_FILE_MACHINE_THUMB:
			return L"ARM Thumb/Thumb-2";
		case IMAGE_FILE_MACHINE_ARMNT:
			return L"ARM Thumb-2";
		case IMAGE_FILE_MACHINE_ARM64:
			return L"ARM64";
		case IMAGE_FILE_MACHINE_AM33:
			return L"AM33";
		case IMAGE_FILE_MACHINE_POWERPC:
			return L"POWERPC";
		case IMAGE_FILE_MACHINE_POWERPCFP:
			return L"POWERPCFP";
		case IMAGE_FILE_MACHINE_MIPS16:
			return L"MIPS16";
		case IMAGE_FILE_MACHINE_ALPHA64:
			return L"ALPHA64";
		case IMAGE_FILE_MACHINE_MIPSFPU:
			return L"MIPSFPU";
		case IMAGE_FILE_MACHINE_MIPSFPU16:
			return L"MIPSFPU16";
		case IMAGE_FILE_MACHINE_TRICORE:
			return L"TriCore";
		case IMAGE_FILE_MACHINE_CEF:
			return L"CEF";
		case IMAGE_FILE_MACHINE_EBC:
			return L"EFI Byte Code";
		case IMAGE_FILE_MACHINE_M32R:
			return L"M32R";
		case IMAGE_FILE_MACHINE_CEE:
			return L"CEE";
		default:
			return L"Unknown";
		}
	}

	// http://weblog.ikvm.net/2011/11/14/ManagedPEFileTypes.aspx
	std::wstring PeFileFormattedInfoExtractor::GetPlatform()
	{
		if (peFileInfoExtractor_.IsPe32Plus())
		{
			return L"64-bit (" + GetMachine() + L")";
		}

		ClrHeaderInfo clrHeaderInfo = peFileInfoExtractor_.GetClrHeaderInfo();
		if (clrHeaderInfo.IsPresent && IsFlagSet(clrHeaderInfo.Flags, COMIMAGE_FLAGS_ILONLY))
		{
			if (IsFlagSet(clrHeaderInfo.Flags, COMIMAGE_FLAGS_32BITPREFERRED))
			{
				return L"Any CPU 32-bit preferred";
			}

			if (!IsFlagSet(clrHeaderInfo.Flags, COMIMAGE_FLAGS_32BITREQUIRED))
			{
				return L"Any CPU";
			}			
		}

		return L"32-bit (" + GetMachine() + L")";
	}

	std::wstring PeFileFormattedInfoExtractor::GetTimeDateStamp()
	{
		time_t timeDateStamp = peFileInfoExtractor_.GetTimeDateStamp();
		std::tm tm{};
		errno_t e = localtime_s(&tm, &timeDateStamp);
		if (e != 0)
		{
			return L"INVALID";
		}

		std::wostringstream stream;
		stream.imbue(std::locale(""));
		stream << std::put_time(&tm, L"%x %X");
		return stream.str();
	}

	std::wstring PeFileFormattedInfoExtractor::GetSubsystem()
	{
		WORD subsystem = peFileInfoExtractor_.GetSubsystem();
		switch (subsystem)
		{
		case IMAGE_SUBSYSTEM_NATIVE:
			return L"Native";
		case IMAGE_SUBSYSTEM_WINDOWS_GUI:
			return L"Windows GUI";
		case IMAGE_SUBSYSTEM_WINDOWS_CUI:
			return L"Windows Console";
		case IMAGE_SUBSYSTEM_OS2_CUI:
			return L"OS/2 Console";
		case IMAGE_SUBSYSTEM_POSIX_CUI:
			return L"POSIX Console";
		case IMAGE_SUBSYSTEM_NATIVE_WINDOWS:
			return L"Native Win9x driver";
		case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:
			return L"Windows CE GUI";
		case IMAGE_SUBSYSTEM_EFI_APPLICATION:
			return L"EFI Application";
		case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
			return L"EFI Boot Service Driver";
		case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
			return L"EFI Runtime Driver";
		case IMAGE_SUBSYSTEM_EFI_ROM:
			return L"EFI ROM";
		case IMAGE_SUBSYSTEM_XBOX:
			return L"XBOX";
		case IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION:
			return L"Windows BOOT APPLICATION";
		case IMAGE_SUBSYSTEM_XBOX_CODE_CATALOG:
			return L"XBOX Code Catalog";
		case IMAGE_SUBSYSTEM_UNKNOWN:
		default:
			return L"Unknown";
		}
	}

	// https://dev.to/yumetodo/list-of-mscver-and-mscfullver-8nd
	std::wstring PeFileFormattedInfoExtractor::GetToolset()
	{
		WORD linkerVersion = peFileInfoExtractor_.GetLinkerVersion();
		std::wstring toolsetName = L"(Unknown)";
		switch (linkerVersion)
		{
		case 0x0E15:
			toolsetName = L"Visual Studio 2019 (16.1.2-16.3.2)";
			break;
		case 0x0E14:
			toolsetName = L"Visual Studio 2019 (16.0.0)";
			break;
		case 0x0E10:
			toolsetName = L"Visual Studio 2017 (15.9.0-15.9.11)";
			break;
		case 0x0E0F:
			toolsetName = L"Visual Studio 2017 (15.8.0)";
			break;
		case 0x0E0E:
			toolsetName = L"Visual Studio 2017 (15.7.1-15.7.5)";
			break;
		case 0x0E0D:
			toolsetName = L"Visual Studio 2017 (15.6.0-15.6.7)";
			break;
		case 0x0E0C:
			toolsetName = L"Visual Studio 2017 (15.5.2-15.5.7)";
			break;
		case 0x0E0B:
			toolsetName = L"Visual Studio 2017 (15.3.3-15.4.5)";
			break;
		case 0x0E0A:
			toolsetName = L"Visual Studio 2017 (15.0-15.2)";
			break;
		case 0x0E00:
			toolsetName = L"Visual Studio 2015";
			break;
		case 0x0C00:
			toolsetName = L"Visual Studio 2013";
			break;
		case 0x0B00:
			toolsetName = L"Visual Studio 2012";
			break;
		case 0x0A00:
			toolsetName = L"Visual Studio 2010";
			break;
		case 0x0900:
			toolsetName = L"Visual Studio 2008";
			break;
		case 0x0800:
			toolsetName = L"Visual Studio 2005";
			break;
		case 0x070A:
			toolsetName = L"Visual Studio .NET 2003";
			break;
		case 0x0700:
			toolsetName = L"Visual Studio .NET 2002";
			break;
		case 0x0600:
			toolsetName = L"Visual Studio 6.0";
			break;
		case 0x0500:
			toolsetName = L"Visual Studio 5.0";
			break;
		case 0x3000:
			toolsetName = L"Visual Studio C#";
			break;
		}

		auto versionString = L"v" + std::to_wstring(HIBYTE(linkerVersion)) + L"." + std::to_wstring(LOBYTE(linkerVersion));
		return versionString + L" (" + toolsetName + L")";
	}

	std::wstring PeFileFormattedInfoExtractor::GetConfiguration()
	{
		BuildConfiguration configuration = peFileInfoExtractor_.GetBuildConfiguration();
		switch (configuration)
		{
		case BuildConfiguration::Debug:
			return L"Debug";
		case BuildConfiguration::Release:
			return L"Release";
		default:
			return L"Unknown";
		}
	}

	std::wstring PeFileFormattedInfoExtractor::GetDepStatus()
	{
		return IsFlagSet(peFileInfoExtractor_.GetDllCharacteristics(), IMAGE_DLLCHARACTERISTICS_NX_COMPAT)
			? L"Yes"
			: L"No";
	}

	std::wstring PeFileFormattedInfoExtractor::GetAslrStatus()
	{
		if (!IsFlagSet(peFileInfoExtractor_.GetDllCharacteristics(), IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE))
		{
			return L"No";
		}

		if (IsFlagSet(peFileInfoExtractor_.GetDllCharacteristics(), IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA))
		{
			return L"Yes (High Entropy)";
		}
		
		return L"Yes";
	}
	
	std::wstring PeFileFormattedInfoExtractor::GetCfgStatus()
	{
		return IsFlagSet(peFileInfoExtractor_.GetDllCharacteristics(), IMAGE_DLLCHARACTERISTICS_GUARD_CF)
			? L"Yes"
			: L"No";
	}

	PeFileVersionInfo PeFileVersionInfoProvider::GetVersionInfo(std::wstring filePath)
	{
		PeFileVersionInfo versionInfo;

		DWORD versionInfoSize = GetFileVersionInfoSize(filePath.c_str(), nullptr);
		if (versionInfoSize == 0)
		{
			return versionInfo;
		}

		std::vector<std::uint8_t> buffer(versionInfoSize);
		BOOL result = GetFileVersionInfo(filePath.c_str(), 0, versionInfoSize, buffer.data());
		if (result == FALSE)
		{
			return versionInfo;
		}

		VS_FIXEDFILEINFO* pFixedFileInfo = nullptr;
		UINT fixedFileInfoSize = 0;
		result = VerQueryValue(buffer.data(), L"\\", (LPVOID*)&pFixedFileInfo, &fixedFileInfoSize);
		if (result == FALSE || fixedFileInfoSize == 0)
		{
			return versionInfo;
		}

		if (pFixedFileInfo->dwFileFlagsMask & VS_FF_DEBUG)
		{
			versionInfo.BuildConfiguration = ((pFixedFileInfo->dwFileFlags & VS_FF_DEBUG) != 0) 
				? BuildConfiguration::Debug 
				: BuildConfiguration::Release;
		}

		return versionInfo;
	}
}
