#pragma once
#include <atomic>
#include <cstdint>
#include <utility>
#include <vector>

#include "Fiber.h"

namespace Js
{
	enum class FiberDestination : uint8_t
	{
		None,
		Waiting,
		Pool
	};

	struct Tls
	{
		Tls() = default;
		~Tls() = default;

		void Cleanup()
		{
			PreviousFiberIndex = UINT16_MAX;
			PreviousFiberStored = nullptr;
			PreviousFiberDestination = FiberDestination::None;
		}

		size_t ThreadIndex = SIZE_MAX;

		Fiber ThreadFiber;

		uint16_t CurrentFiberIndex = UINT16_MAX;

		uint16_t PreviousFiberIndex = UINT16_MAX;
		std::atomic_bool* PreviousFiberStored = nullptr;
		FiberDestination PreviousFiberDestination = FiberDestination::None;

		std::vector<std::pair<uint16_t, std::atomic_bool*>> ReadyFibers;
	};
}
