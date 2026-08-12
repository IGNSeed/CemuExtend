#include "Cafe/HW/Espresso/GuestExecutionQuiescence.h"

#include <cassert>

void GuestExecutionQuiescence::Enter()
{
	auto state = m_state.load(std::memory_order_seq_cst);
	for (;;)
	{
		while ((state & kQuiescingBit) != 0)
		{
			std::unique_lock lock(m_waitMutex);
			m_stateChanged.wait(lock, [this] {
				return (m_state.load(std::memory_order_seq_cst) & kQuiescingBit) == 0;
			});
			state = m_state.load(std::memory_order_seq_cst);
		}
		assert((state & kActiveCountMask) != kActiveCountMask);
		if (m_state.compare_exchange_weak(state, state + 1U,
			std::memory_order_seq_cst, std::memory_order_seq_cst))
			return;
	}
}

void GuestExecutionQuiescence::Leave()
{
	const auto previous = m_state.fetch_sub(1U, std::memory_order_seq_cst);
	assert((previous & kActiveCountMask) != 0);
	if ((previous & kActiveCountMask) == 1U)
		m_stateChanged.notify_all();
}

void GuestExecutionQuiescence::RunQuiesced(const std::function<void()>& callback,
	bool resumeCurrentLease)
{
	if (!callback)
		return;

	// guest callback 自身から要求された場合は、operation lock を待つ前に
	// 自coreのleaseを外す。他coreのquiescence要求との相互待ちを防ぐためである。
	if (resumeCurrentLease)
		Leave();

	std::unique_lock operationLock(m_operationMutex);
	const auto previous = m_state.fetch_or(kQuiescingBit, std::memory_order_seq_cst);
	assert((previous & kQuiescingBit) == 0);

	{
		std::unique_lock waitLock(m_waitMutex);
		m_stateChanged.wait(waitLock, [this] {
			return (m_state.load(std::memory_order_seq_cst) & kActiveCountMask) == 0;
		});
	}

	auto resume = [this, resumeCurrentLease] {
		// quiescence中はactive countが必ず0である。callerがguest threadなら
		// stop bitの解除とcaller leaseの復元を単一storeで公開する。
		m_state.store(resumeCurrentLease ? 1U : 0U, std::memory_order_seq_cst);
		m_stateChanged.notify_all();
	};

	try
	{
		callback();
	}
	catch (...)
	{
		resume();
		throw;
	}
	resume();
}

std::uint32_t GuestExecutionQuiescence::ActiveCount() const
{
	return m_state.load(std::memory_order_seq_cst) & kActiveCountMask;
}

bool GuestExecutionQuiescence::IsQuiescing() const
{
	return (m_state.load(std::memory_order_seq_cst) & kQuiescingBit) != 0;
}
