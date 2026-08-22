#pragma once

namespace peinfo
{
	struct IMAGE_NT_HEADERS_3264 {
		DWORD Signature;
		IMAGE_FILE_HEADER FileHeader;
		union
		{
			IMAGE_OPTIONAL_HEADER32 OptionalHeader32;
			IMAGE_OPTIONAL_HEADER64 OptionalHeader64;
		};		
	};

	void HandleLogicError(bool errorOccurred, const char* message);

	void HandleWin32Error(bool errorOccurred);

	void HandleFormatError(bool errorOccurred, const char* message);

	class MappedPeFile
	{
	public:
		MappedPeFile(std::wstring filePath);
		~MappedPeFile();

		LPVOID GetBaseAddress();

	private:
		LPVOID base_ = nullptr;
	};

	enum class BuildConfiguration
	{
		Unknown,
		Debug,
		Release
	};

	struct PeFileVersionInfo
	{
		PeFileVersionInfo()
			: BuildConfiguration(BuildConfiguration::Unknown)
		{
		}

		BuildConfiguration BuildConfiguration;
	};

	class PeFileVersionInfoProvider
	{
	public:
		PeFileVersionInfo GetVersionInfo(std::wstring filePath);
	};

	struct ClrHeaderInfo
	{
		ClrHeaderInfo()
			: IsPresent(false), Flags(0)
		{
		}

		bool IsPresent;
		DWORD Flags;
		std::wstring TargetFramework;
		std::wstring AssemblyVersion;
		bool AreOptimizationsDisabled;
	};

	class PeFileInfoExtractor
	{
	public:
		PeFileInfoExtractor(std::wstring filePath);

		WORD GetMachine();
		DWORD GetTimeDateStamp();
		WORD GetSubsystem();
		WORD GetLinkerVersion();
		bool IsDll();
		bool IsPe32Plus();
		BuildConfiguration GetBuildConfiguration();
		ClrHeaderInfo GetClrHeaderInfo();
		DWORD GetDllCharacteristics();
		bool IsDeterministicBuild();
		bool HasRichHeader();

	private:
		PIMAGE_DATA_DIRECTORY GetDataDirectory();
		DWORD GetNumberOfRvaAndSizes();
		PIMAGE_SECTION_HEADER GetSectionHeader();
		bool TryGetClrHeader(PIMAGE_COR20_HEADER& clrHeader);
		std::wstring GetTargetFramework(PIMAGE_COR20_HEADER clrHeader);
		std::wstring GetAssemblyVersion(PIMAGE_COR20_HEADER clrHeader);
		bool AreOptimizationsDisabled(PIMAGE_COR20_HEADER clrHeader);
		DWORD RvaToFileOffset(DWORD rva);

		std::wstring filePath_;
		MappedPeFile mappedPeFile_;
		IMAGE_NT_HEADERS_3264* imageNtHeaders_;
		PeFileVersionInfoProvider peFileVersionInfoProvider_;
	};

	struct PeFileFormattedInfoItem
	{
		std::wstring Name;
		std::wstring Value;
		std::wstring Note; // optional; empty when the value needs no further explanation
	};

	struct PeFileFormattedInfoCategory
	{
		std::wstring Name;
		std::vector<PeFileFormattedInfoItem> Items;
	};

	struct PeFileFormattedInfo
	{
		std::vector<PeFileFormattedInfoCategory> Categories;
	};

	class PeFileFormattedInfoExtractor
	{
	public:
		PeFileFormattedInfoExtractor(std::wstring filePath);

		PeFileFormattedInfo Extract();

	private:
		std::wstring GetDescription();
		std::wstring GetMachine();
		std::wstring GetPlatform();
		std::wstring GetTimeDateStamp();
		std::wstring GetSubsystem();
		PeFileFormattedInfoItem GetToolsetItem();
		std::wstring GetConfiguration();
		std::wstring GetDepStatus();
		std::wstring GetAslrStatus();
		std::wstring GetCfgStatus();

		PeFileInfoExtractor peFileInfoExtractor_;
	};
}