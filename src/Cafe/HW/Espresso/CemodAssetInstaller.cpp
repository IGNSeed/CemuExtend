#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/CemodAssetInstaller.h"
#include "Cafe/HW/Espresso/CemodPackage.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>

#ifdef _WIN32
#include <Windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

bool ExistingRegularFile(const fs::path& path, std::string& error)
{
	std::error_code code;
	const auto status = fs::symlink_status(path, code);
	if (!code && fs::is_regular_file(status) && !fs::is_symlink(status))
		return true;
	error = code ? fmt::format("cannot inspect existing asset '{}': {}",
		_pathToUtf8(path), code.message()) :
		fmt::format("asset destination '{}' is not a regular file", _pathToUtf8(path));
	return false;
}

bool EnsureDirectory(const fs::path& path, std::string& error)
{
	std::error_code code;
	auto status = fs::symlink_status(path, code);
	if ((!code && status.type() == fs::file_type::not_found) ||
		code == std::errc::no_such_file_or_directory)
	{
		code.clear();
		if (!fs::create_directory(path, code) && code)
		{
			error = fmt::format("cannot create asset directory '{}': {}",
				_pathToUtf8(path), code.message());
			return false;
		}
		status = fs::symlink_status(path, code);
	}
	if (code || !fs::is_directory(status) || fs::is_symlink(status))
	{
		error = code ? fmt::format("cannot inspect asset directory '{}': {}",
			_pathToUtf8(path), code.message()) :
			fmt::format("asset directory '{}' is unsafe", _pathToUtf8(path));
		return false;
	}
	return true;
}

bool EnsureAssetParent(const fs::path& root, std::string_view relative, fs::path& output,
	std::string& error)
{
	fs::path current = root;
	std::size_t begin{};
	for (;;)
	{
		const auto end = relative.find('/', begin);
		const auto component = relative.substr(begin,
			end == std::string_view::npos ? relative.size() - begin : end - begin);
		if (component.empty())
		{
			error = "asset path contains an empty component";
			return false;
		}
		current /= _utf8ToPath(component);
		if (end == std::string_view::npos)
		{
			output = std::move(current);
			return true;
		}
		if (!EnsureDirectory(current, error))
			return false;
		begin = end + 1;
	}
}

#ifdef _WIN32
bool WriteExclusive(const fs::path& path, std::span<const std::byte> bytes, bool& existed,
	std::string& error)
{
	existed = false;
	const HANDLE file = ::CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
		CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (file == INVALID_HANDLE_VALUE)
	{
		const auto code = ::GetLastError();
		if (code == ERROR_FILE_EXISTS || code == ERROR_ALREADY_EXISTS)
		{
			existed = true;
			return true;
		}
		error = fmt::format("cannot create asset '{}': Windows error {}", _pathToUtf8(path), code);
		return false;
	}

	bool success = true;
	std::size_t offset{};
	while (offset < bytes.size())
	{
		const auto remaining = std::min<std::size_t>(bytes.size() - offset,
			std::numeric_limits<DWORD>::max());
		DWORD written{};
		if (!::WriteFile(file, bytes.data() + offset, static_cast<DWORD>(remaining),
			&written, nullptr) || written != remaining)
		{
			error = fmt::format("cannot write asset '{}': Windows error {}",
				_pathToUtf8(path), ::GetLastError());
			success = false;
			break;
		}
		offset += written;
	}
	if (success && !::FlushFileBuffers(file))
	{
		error = fmt::format("cannot flush asset '{}': Windows error {}",
			_pathToUtf8(path), ::GetLastError());
		success = false;
	}
	::CloseHandle(file);
	if (!success)
	{
		std::error_code ignored;
		fs::remove(path, ignored);
	}
	return success;
}
#else
bool WriteExclusive(const fs::path& path, std::span<const std::byte> bytes, bool& existed,
	std::string& error)
{
	existed = false;
	const int file = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0644);
	if (file < 0)
	{
		if (errno == EEXIST)
		{
			existed = true;
			return true;
		}
		error = fmt::format("cannot create asset '{}': {}", _pathToUtf8(path), std::strerror(errno));
		return false;
	}

	bool success = true;
	std::size_t offset{};
	while (offset < bytes.size())
	{
		const auto written = ::write(file, bytes.data() + offset, bytes.size() - offset);
		if (written <= 0)
		{
			error = fmt::format("cannot write asset '{}': {}", _pathToUtf8(path), std::strerror(errno));
			success = false;
			break;
		}
		offset += static_cast<std::size_t>(written);
	}
	if (success && ::fsync(file) != 0)
	{
		error = fmt::format("cannot flush asset '{}': {}", _pathToUtf8(path), std::strerror(errno));
		success = false;
	}
	::close(file);
	if (!success)
	{
		std::error_code ignored;
		fs::remove(path, ignored);
	}
	return success;
}
#endif

} // namespace

bool CemodAssetInstaller::InstallMissing(const CemodPackage& package, std::string& error)
{
	std::error_code code;
	const auto temporaryRoot = fs::temp_directory_path(code);
	if (code)
	{
		error = fmt::format("cannot resolve the host temporary directory: {}", code.message());
		return false;
	}
	return InstallMissing(package, temporaryRoot, error);
}

bool CemodAssetInstaller::InstallMissing(const CemodPackage& package,
	const fs::path& temporaryRoot, std::string& error)
{
	error.clear();
	if (package.assets.empty())
		return true;
	if (package.manifest.modId.empty())
	{
		error = "asset package has no mod_id";
		return false;
	}

	const auto destinationRoot = temporaryRoot / _utf8ToPath(package.manifest.modId);
	if (!EnsureDirectory(destinationRoot, error))
		return false;
	for (const auto& asset : package.assets)
	{
		fs::path destination;
		if (!EnsureAssetParent(destinationRoot, asset.path, destination, error))
			return false;

		std::error_code statusError;
		const auto status = fs::symlink_status(destination, statusError);
		const bool missing =
			(!statusError && status.type() == fs::file_type::not_found) ||
			statusError == std::errc::no_such_file_or_directory;
		if (!statusError && !missing)
		{
			if (!fs::is_regular_file(status) || fs::is_symlink(status))
			{
				error = fmt::format("asset destination '{}' is not a regular file",
					_pathToUtf8(destination));
				return false;
			}
			continue;
		}
		if (!missing)
		{
			error = fmt::format("cannot inspect asset destination '{}': {}",
				_pathToUtf8(destination), statusError.message());
			return false;
		}

		bool existed{};
		if (!WriteExclusive(destination, asset.bytes, existed, error))
			return false;
		if (existed && !ExistingRegularFile(destination, error))
			return false;
	}
	return true;
}
