#pragma once

#include "Cafe/HW/Espresso/CemodPackage.h"
#include "Cafe/HW/Espresso/ModExecutionContext.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace cemuextend_hle { class Cex2Owner; }

class TrustedCemodRuntime
{
public:
	TrustedCemodRuntime();
	~TrustedCemodRuntime();

	[[nodiscard]] std::optional<std::uint64_t> Load(CemodPackage package,
		std::uint32_t titlePermissions, const ModServicePermissions& services,
		std::string& error);
	[[nodiscard]] bool PrepareUnload(std::uint64_t handle);
	[[nodiscard]] bool FinishUnload(std::uint64_t handle);
	[[nodiscard]] std::optional<std::uint64_t> LatestHandle() const;
	void AbandonAllForTitleShutdown();
	void UpdatePermissions(std::uint32_t permissions, const ModServicePermissions& services);

	[[nodiscard]] cemuextend_hle::Cex2Owner* Owner();
	[[nodiscard]] std::size_t Size() const;

private:
	[[nodiscard]] bool Release(std::uint64_t handle);

	struct Impl;
	std::unique_ptr<Impl> m_impl;
};
