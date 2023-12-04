#pragma once
#include <cassert>
#include <atomic>

namespace Js
{
	template <typename T>
	class Queue
	{
	public:
		explicit Queue(const size_t bufferSize);

		~Queue();

		Queue(Queue const&) = delete;
		void operator =(Queue const&) = delete;

		bool Enqueue(T const& data);

		bool Dequeue(T& data);

	private:
		struct Cell
		{
			std::atomic<size_t> Sequence;
			T Data;
		};

		static constexpr size_t CACHELINE_SIZE = 64;
		typedef char CachelinePad[CACHELINE_SIZE];

		CachelinePad Pad0;
		Cell* Buffer;
		size_t BufferMask;
		CachelinePad Pad1;
		std::atomic<size_t> EnqueuePos;
		CachelinePad Pad2;
		std::atomic<size_t> DequeuePos;
		CachelinePad Pad3;
	};

	template <typename T>
	Queue<T>::Queue(const size_t bufferSize) : Pad0{}, Buffer(new Cell[bufferSize])
	                                           , BufferMask(bufferSize - 1), Pad1{}, Pad2{}, Pad3{}
	{
		assert((bufferSize >= 2) && ((bufferSize & (bufferSize - 1)) == 0));
		for (size_t i = 0; i != bufferSize; i += 1)
			Buffer[i].Sequence.store(i, std::memory_order_relaxed);
		EnqueuePos.store(0, std::memory_order_relaxed);
		DequeuePos.store(0, std::memory_order_relaxed);
	}

	template <typename T>
	Queue<T>::~Queue()
	{
		delete[] Buffer;
	}

	template <typename T>
	bool Queue<T>::Enqueue(T const& data)
	{
		Cell* cell;
		size_t pos = EnqueuePos.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &Buffer[pos & BufferMask];
			const size_t seq = cell->Sequence.load(std::memory_order_acquire);
			const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos);
			if (dif == 0)
			{
				if (EnqueuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = EnqueuePos.load(std::memory_order_relaxed);
		}

		cell->Data = data;
		cell->Sequence.store(pos + 1, std::memory_order_release);

		return true;
	}

	template <typename T>
	bool Queue<T>::Dequeue(T& data)
	{
		Cell* cell;
		size_t pos = DequeuePos.load(std::memory_order_relaxed);
		for (;;)
		{
			cell = &Buffer[pos & BufferMask];
			const size_t seq = cell->Sequence.load(std::memory_order_acquire);
			const intptr_t dif = static_cast<intptr_t>(seq) - static_cast<intptr_t>(pos + 1);
			if (dif == 0)
			{
				if (DequeuePos.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed))
					break;
			}
			else if (dif < 0)
				return false;
			else
				pos = DequeuePos.load(std::memory_order_relaxed);
		}

		data = cell->Data;
		cell->Sequence.store(pos + BufferMask + 1, std::memory_order_release);

		return true;
	}
}
