#include "FiberPool.h"

Js::FiberPool::FiberPool(const uint16_t size, const Fiber::FiberFunc function, void* data):
	Fibers(size),
	FreeFibers(size)
{
	for (uint16_t i = 0; i < size; i++)
	{
		Fibers[i].SetFunc(function);
		Fibers[i].SetData(data);
		FreeFibers[i].store(true, std::memory_order_relaxed);
	}
}

uint16_t Js::FiberPool::GetFreeFiber(Fiber*& fiber)
{
	for (;;)
	{
		for (uint16_t i = 0; i < Fibers.size(); i++)
		{
			if (!FreeFibers[i].load(std::memory_order_relaxed) ||
				!FreeFibers[i].load(std::memory_order_acquire))
			{
				continue;
			}

			bool expected = true;
			if (std::atomic_compare_exchange_weak_explicit(&FreeFibers[i], &expected, false, std::memory_order_release,
			                                               std::memory_order_relaxed))
			{
				fiber = &Fibers[i];
				return i;
			}
		}
	}
}

Js::Fiber& Js::FiberPool::GetFiber(const uint16_t index)
{
	return Fibers[index];
}

void Js::FiberPool::ReturnFiber(const uint16_t index)
{
	FreeFibers[index].store(true, std::memory_order_release);
}
