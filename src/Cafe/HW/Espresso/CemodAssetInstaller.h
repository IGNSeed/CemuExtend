#pragma once

#include <filesystem>
#include <string>

struct CemodPackage;

class CemodAssetInstaller
{
public:
	[[nodiscard]] static bool InstallMissing(const CemodPackage& package,
		std::string& error);
	[[nodiscard]] static bool InstallMissing(const CemodPackage& package,
		const std::filesystem::path& temporaryRoot, std::string& error);
};
