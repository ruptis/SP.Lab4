#pragma once
#include <atomic>
#include <vector>

#include "Fiber.h"

namespace Js
{
	class FiberPool
	{
	public:
		explicit FiberPool(uint16_t size, Fiber::FiberFunc function, void* data = nullptr);
		~FiberPool() = default;

		uint16_t GetFreeFiber(Fiber*& fiber);
		Fiber& GetFiber(uint16_t index);
		void ReturnFiber(uint16_t index);

	private:
		std::vector<Fiber> Fibers;
		std::vector<std::atomic_bool> FreeFibers;
	};
}
