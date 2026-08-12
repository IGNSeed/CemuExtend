#include "Cafe/HW/Espresso/GuestExecutionQuiescence.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>

namespace {

[[noreturn]] void CheckFailed(const char* expression, int line)
{
	std::cerr << "CHECK failed at line " << line << ": " << expression << '\n';
	std::abort();
}
#define CHECK(condition) do { if (!(condition)) CheckFailed(#condition, __LINE__); } while (false)

void WaitUntil(const std::function<bool()>& predicate)
{
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
	while (!predicate())
	{
		CHECK(std::chrono::steady_clock::now() < deadline);
		std::this_thread::yield();
	}
}

void WaitsForExistingExecution()
{
	GuestExecutionQuiescence quiescence;
	std::promise<void> entered;
	std::promise<void> release;
	auto releaseFuture = release.get_future();
	std::thread worker([&] {
		quiescence.Enter();
		entered.set_value();
		releaseFuture.wait();
		quiescence.Leave();
	});
	entered.get_future().wait();

	std::atomic_bool callbackRan{};
	std::thread coordinator([&] {
		quiescence.RunQuiesced([&] {
			CHECK(quiescence.ActiveCount() == 0);
			callbackRan = true;
		});
	});
	WaitUntil([&] { return quiescence.IsQuiescing(); });
	CHECK(!callbackRan.load());
	release.set_value();
	worker.join();
	coordinator.join();
	CHECK(callbackRan.load());
}

void BlocksNewExecutionUntilCallbackReturns()
{
	GuestExecutionQuiescence quiescence;
	std::promise<void> callbackEntered;
	std::promise<void> releaseCallback;
	auto releaseFuture = releaseCallback.get_future();
	std::thread coordinator([&] {
		quiescence.RunQuiesced([&] {
			callbackEntered.set_value();
			releaseFuture.wait();
		});
	});
	callbackEntered.get_future().wait();

	std::atomic_bool guestEntered{};
	std::thread guest([&] {
		quiescence.Enter();
		guestEntered = true;
		quiescence.Leave();
	});
	std::this_thread::yield();
	CHECK(!guestEntered.load());
	releaseCallback.set_value();
	coordinator.join();
	guest.join();
	CHECK(guestEntered.load());
}

void RestoresCallingGuestLease()
{
	GuestExecutionQuiescence quiescence;
	quiescence.Enter();
	quiescence.RunQuiesced([&] {
		CHECK(quiescence.ActiveCount() == 0);
	}, true);
	CHECK(quiescence.ActiveCount() == 1);
	quiescence.Leave();
}

} // namespace

int main()
{
	WaitsForExistingExecution();
	BlocksNewExecutionUntilCallbackReturns();
	RestoresCallingGuestLease();
	return 0;
}
