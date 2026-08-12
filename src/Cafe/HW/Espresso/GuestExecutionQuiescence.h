#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>

class GuestExecutionQuiescence
{
public:
	void Enter();
	void Leave();
	void RunQuiesced(const std::function<void()>& callback,
		bool resumeCurrentLease = false);

	[[nodiscard]] std::uint32_t ActiveCount() const;
	[[nodiscard]] bool IsQuiescing() const;

private:
	static constexpr std::uint32_t kQuiescingBit = 0x80000000U;
	static constexpr std::uint32_t kActiveCountMask = ~kQuiescingBit;

	std::atomic_uint32_t m_state{};
	std::mutex m_operationMutex;
	std::mutex m_waitMutex;
	std::condition_variable m_stateChanged;
};
