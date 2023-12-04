#pragma once
#include <atomic>
#include <cstdint>
#include <array>

#include "JSException.h"

namespace Js
{
	class JobSystem;

	class Counter
	{
	public:
		Counter() = default;
		~Counter() = default;

	private:
		friend class JobSystem;
		friend class Job;

		using Unit = uint32_t;

		struct WaiterFiber
		{
			uint16_t FiberId = 0;
			std::atomic_bool* IsFiberStored = nullptr;
			Unit TargetValue = 0;
			std::atomic_bool IsInUse{true};
		};

		Unit Increment(Unit value = 1);
		Unit Decrement(Unit value = 1);

		Unit GetValue() const;

		bool AddWaiter(uint16_t fiberId, std::atomic_bool* isFiberStored, Unit targetValue);
		void Initialize(JobSystem* system, uint32_t initialValue = 0);
		void CheckWaiters(Unit value);

		constexpr static size_t MAX_WAITERS = 16;
		std::array<WaiterFiber, MAX_WAITERS> Waiters;
		std::array<std::atomic_bool, MAX_WAITERS> FreeWaiters;

		std::atomic<Unit> Value{0};
		JobSystem* System = nullptr;
	};
}
