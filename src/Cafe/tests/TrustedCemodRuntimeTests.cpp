#include "Common/precompiled.h"

#include "Cafe/HW/Espresso/PPCState.h"
#include "Cafe/HW/Espresso/TrustedCemodRuntime.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/libs/cemuextend/Cex2Host.h"
#include "Cafe/OS/libs/cemuextend/Cex2Owner.h"

#include <array>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <thread>

namespace
{
	[[noreturn]] void CheckFailed(const char* expression, int line)
	{
		std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
		std::abort();
	}
#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

	constexpr std::uint32_t kMemoryBase = 0x10000000U;
	constexpr std::uint32_t kPatchAddress = kMemoryBase + 0x100U;
	constexpr std::uint32_t kAllocationAddress = kMemoryBase + 0x1000U;
	constexpr std::uint32_t kShutdownAddress = kMemoryBase + 0x2000U;
	constexpr std::uint32_t kOriginalInstruction = 0x4e800421U;
	constexpr std::uint64_t kTitleId = 0x0005000012345678ULL;

	std::array<std::uint8_t, 0x4000> g_memory{};
	PPCInterpreter_t g_cpu{};
	PPCInterpreter_t* g_currentCpu{};
	std::function<void(std::uint32_t)> g_shutdownCallback;
	std::uint32_t g_callbackCount[2]{};
	std::uint32_t g_releaseCount{};
	std::uint32_t g_closeCount{};
	std::uint32_t g_closeOwnerCount{};
	bool g_sessionOpen{};
	bool g_insideShutdownCallback{};
	bool g_phase1CallbackReturned{};
	bool g_closeOwnerRanOutsideCallback{};
	bool g_releaseRanOutsideCallback{};
	bool g_releaseSawStoppedOwner{};
	cemuextend_hle::Cex2Owner* g_ownerAtClose{};

	void Write32(std::uint32_t address, std::uint32_t value)
	{
		auto* output = g_memory.data() + address - kMemoryBase;
		output[0] = static_cast<std::uint8_t>(value >> 24);
		output[1] = static_cast<std::uint8_t>(value >> 16);
		output[2] = static_cast<std::uint8_t>(value >> 8);
		output[3] = static_cast<std::uint8_t>(value);
	}

	std::uint32_t Read32(std::uint32_t address)
	{
		const auto* input = g_memory.data() + address - kMemoryBase;
		return (static_cast<std::uint32_t>(input[0]) << 24) |
			(static_cast<std::uint32_t>(input[1]) << 16) |
			(static_cast<std::uint32_t>(input[2]) << 8) | input[3];
	}

	void ResetTestState()
	{
		g_memory.fill(0);
		g_cpu = {};
		g_currentCpu = &g_cpu;
		g_shutdownCallback = {};
		g_callbackCount[0] = 0;
		g_callbackCount[1] = 0;
		g_releaseCount = 0;
		g_closeCount = 0;
		g_closeOwnerCount = 0;
		g_sessionOpen = true;
		g_insideShutdownCallback = false;
		g_phase1CallbackReturned = false;
		g_closeOwnerRanOutsideCallback = false;
		g_releaseRanOutsideCallback = false;
		g_releaseSawStoppedOwner = false;
		g_ownerAtClose = nullptr;
		Write32(kPatchAddress, 0x48000000U);
	}

	void TestShutdownCallbackCanReenterCex2()
	{
		ResetTestState();
		TrustedCemodRuntime runtime;
		const auto handle = runtime.InstallInstanceForTesting(kTitleId,
			kAllocationAddress, kPatchAddress, kOriginalInstruction, kShutdownAddress);

		g_shutdownCallback = [&](std::uint32_t phase) {
			CHECK(phase <= 1);
			++g_callbackCount[phase];
			CHECK(runtime.Size() == 1);
			CHECK(runtime.LatestHandle() == handle);
			auto* owner = runtime.Owner();
			CHECK(owner != nullptr);
			CHECK(!owner->IsStopped());
			CHECK(g_releaseCount == 0);
			bool duplicateResult = true;
			std::thread duplicateUnload([&] {
				duplicateResult = phase == 0 ? runtime.PrepareUnload(handle) :
					runtime.FinishUnload(handle);
			});
			duplicateUnload.join();
			CHECK(!duplicateResult);
			CHECK(!runtime.FinishUnload(handle));
			CHECK(!runtime.PrepareUnload(handle));
			if (phase == 1)
			{
				// CEX2Close相当の再入でTrustedOwnerを再取得してもself-deadlockしない。
				CHECK(cemuextend_hle::Cex2Host::Instance().Close(*owner, 7) == 0);
			}
		};

		CHECK(runtime.PrepareUnload(handle));
		CHECK(Read32(kPatchAddress) == kOriginalInstruction);
		CHECK(g_callbackCount[0] == 1);
		CHECK(runtime.PrepareUnload(handle));
		CHECK(g_callbackCount[0] == 1);
		CHECK(runtime.Owner() != nullptr);
		CHECK(runtime.FinishUnload(handle));
		CHECK(g_callbackCount[1] == 1);
		CHECK(g_closeCount == 1);
		CHECK(g_closeOwnerCount == 1);
		CHECK(!g_sessionOpen);
		CHECK(g_closeOwnerRanOutsideCallback);
		CHECK(g_releaseRanOutsideCallback);
		CHECK(g_phase1CallbackReturned);
		CHECK(g_releaseSawStoppedOwner);
		CHECK(g_releaseCount == 1);
		CHECK(runtime.Size() == 0);
		CHECK(runtime.Owner() == nullptr);
		CHECK(!runtime.PrepareUnload(handle));
		CHECK(!runtime.FinishUnload(handle));
		CHECK(g_releaseCount == 1);
	}

	void TestMissingCpuDoesNotAdvanceLifecycle()
	{
		ResetTestState();
		TrustedCemodRuntime runtime;
		const auto handle = runtime.InstallInstanceForTesting(kTitleId,
			kAllocationAddress, kPatchAddress, kOriginalInstruction, kShutdownAddress);
		g_currentCpu = nullptr;
		CHECK(!runtime.PrepareUnload(handle));
		CHECK(Read32(kPatchAddress) == 0x48000000U);
		CHECK(g_releaseCount == 0);
		g_currentCpu = &g_cpu;
		CHECK(runtime.PrepareUnload(handle));
		CHECK(runtime.FinishUnload(handle));
		CHECK(g_releaseCount == 1);
	}

	void TestVersionOneInstanceHasNoShutdownCallback()
	{
		ResetTestState();
		TrustedCemodRuntime runtime;
		const auto handle = runtime.InstallInstanceForTesting(kTitleId,
			kAllocationAddress, kPatchAddress, kOriginalInstruction, 0);
		g_currentCpu = nullptr;
		CHECK(runtime.PrepareUnload(handle));
		CHECK(runtime.PrepareUnload(handle));
		CHECK(runtime.FinishUnload(handle));
		CHECK(g_callbackCount[0] == 0);
		CHECK(g_callbackCount[1] == 0);
		CHECK(g_releaseCount == 1);
		CHECK(g_closeOwnerCount == 1);
	}
}

PPCInterpreter_t* PPCInterpreter_getCurrentInstance()
{
	return g_currentCpu;
}

std::uint8_t* memory_base{};

void memory_writeU32(std::uint32_t address, std::uint32_t value)
{
	Write32(address, value);
}

PPCInterpreter_t* PPCCore_executeCallbackInternal(std::uint32_t)
{
	CHECK(g_currentCpu != nullptr);
	const auto phase = g_currentCpu->gpr[3];
	CHECK(phase <= 1);
	g_insideShutdownCallback = true;
	if (g_shutdownCallback)
		g_shutdownCallback(phase);
	g_insideShutdownCallback = false;
	if (phase == 1)
		g_phase1CallbackReturned = true;
	g_currentCpu->gpr[3] = 0;
	return g_currentCpu;
}

void PPCRecompiler_invalidateRange(std::uint32_t, std::uint32_t) {}

std::uint8_t* memory_getPointerFromVirtualOffset(std::uint32_t address)
{
	CHECK(address >= kMemoryBase);
	CHECK(address - kMemoryBase < g_memory.size());
	return g_memory.data() + address - kMemoryBase;
}

std::uint32_t memory_getVirtualOffsetFromPointer(void* pointer)
{
	const auto* bytes = static_cast<std::uint8_t*>(pointer);
	if (bytes < g_memory.data() || bytes >= g_memory.data() + g_memory.size())
		return 0;
	return kMemoryBase + static_cast<std::uint32_t>(bytes - g_memory.data());
}

bool memory_isAddressRangeAccessible(MPTR address, std::uint32_t size)
{
	return address >= kMemoryBase && address - kMemoryBase <= g_memory.size() &&
		size <= g_memory.size() - (address - kMemoryBase);
}

sint32 RPLLoader_GetModuleCount()
{
	return 0;
}

RPLModule** RPLLoader_GetModuleList()
{
	return nullptr;
}

MEMPTR<void> RPLLoader_AllocateCodeCaveMem(std::uint32_t, std::uint32_t)
{
	return MEMPTR<void>{kAllocationAddress};
}

void RPLLoader_ReleaseCodeCaveMem(MEMPTR<void>)
{
	++g_releaseCount;
	g_releaseRanOutsideCallback = !g_insideShutdownCallback;
	g_releaseSawStoppedOwner = g_ownerAtClose != nullptr && g_ownerAtClose->IsStopped();
}

namespace cemuextend_hle
{
	struct Cex2Host::Impl {};

	Cex2Host::Cex2Host() : m_impl(std::make_unique<Impl>()) {}
	Cex2Host::~Cex2Host() = default;

	Cex2Host& Cex2Host::Instance()
	{
		static Cex2Host host;
		return host;
	}

	std::int32_t Cex2Host::Close(Cex2Owner& owner, std::uint32_t)
	{
		CHECK(!owner.IsStopped());
		CHECK(g_insideShutdownCallback);
		CHECK(g_sessionOpen);
		g_sessionOpen = false;
		++g_closeCount;
		return 0;
	}

	void Cex2Host::CloseOwner(Cex2Owner& owner)
	{
		CHECK(!owner.IsStopped());
		g_ownerAtClose = &owner;
		g_closeOwnerRanOutsideCallback = !g_insideShutdownCallback;
		++g_closeOwnerCount;
		g_sessionOpen = false;
	}

	void Cex2Host::PermissionsChanged(Cex2Owner&, std::uint32_t) {}
}

int main()
{
	TestShutdownCallbackCanReenterCex2();
	TestMissingCpuDoesNotAdvanceLifecycle();
	TestVersionOneInstanceHasNoShutdownCallback();
	return 0;
}
