#include "Counter.h"
#include "JobSystem.h"

Js::Counter::Unit Js::Counter::Increment(const Unit value)
{
	const Unit oldValue = Value.fetch_add(value);
	CheckWaiters(oldValue);
	return oldValue;
}

Js::Counter::Unit Js::Counter::Decrement(const Unit value)
{
	const Unit oldValue = Value.fetch_sub(value);
	CheckWaiters(oldValue);
	return oldValue;
}

Js::Counter::Unit Js::Counter::GetValue() const
{
	return Value.load(std::memory_order_seq_cst);
}

bool Js::Counter::AddWaiter(const uint16_t fiberId, std::atomic_bool* isFiberStored, const Unit targetValue)
{
	for (size_t i = 0; i < MAX_WAITERS; ++i)
	{
		bool expected = true;
		if (!std::atomic_compare_exchange_strong_explicit(&FreeWaiters[i], &expected, false, std::memory_order_seq_cst, std::memory_order_relaxed))
			continue;

		const auto slot = &Waiters[i];
		slot->FiberId = fiberId;
		slot->IsFiberStored = isFiberStored;
		slot->TargetValue = targetValue;

		slot->IsInUse.store(false);

		const Unit counter = Value.load(std::memory_order_relaxed);
		if (slot->IsInUse.load(std::memory_order_acquire)) {
			return false;
		}

		if (slot->TargetValue == counter) {
			expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&slot->IsInUse, &expected, true, std::memory_order_seq_cst, std::memory_order_relaxed)) {
				return false;
			}

			FreeWaiters[i].store(true, std::memory_order_release);
			return true;
		}

		return false;
	}

	throw JsException("No free waiter slots");
}

void Js::Counter::Initialize(JobSystem* system, const uint32_t initialValue)
{
	System = system;
	for (auto& freeWaiter : FreeWaiters)
		freeWaiter = true;

	Value.store(initialValue, std::memory_order_relaxed);
}

void Js::Counter::CheckWaiters(const Unit value)
{
	for (size_t i = 0; i < MAX_WAITERS; ++i)
	{
		if (FreeWaiters[i].load(std::memory_order_acquire))
			continue;

		const auto waiter = &Waiters[i];
		if (waiter->IsInUse.load(std::memory_order_acquire))
			continue;

		if (waiter->TargetValue == value)
		{
			bool expected = false;
			if (!std::atomic_compare_exchange_strong_explicit(&waiter->IsInUse, &expected, true,
			                                                  std::memory_order_seq_cst, std::memory_order_relaxed))
				continue;

			System->GetCurrentTls().ReadyFibers.emplace_back(waiter->FiberId, waiter->IsFiberStored);
			FreeWaiters[i].store(true, std::memory_order_release);
		}
	}
}
