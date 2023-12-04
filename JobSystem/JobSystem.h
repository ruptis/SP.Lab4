#pragma once
#include <thread>

#include "FiberPool.h"
#include "Queue.h"
#include "Thread.h"
#include "Tls.h"

namespace Js
{
	class Counter;
	class Job;

	enum class JobPriority
	{
		High,
		Normal,
		Low
	};

	using JobQueue = Queue<Job>;

	struct Options
	{
		Options() : ThreadCount(std::thread::hardware_concurrency()) {}
		~Options() = default;

		size_t ThreadCount;
		uint16_t FiberCount = 160;

		size_t LowPriorityQueueSize = 4096;
		size_t NormalPriorityQueueSize = 2048;
		size_t HighPriorityQueueSize = 1024;
	};

	class JobSystem
	{
	public:
		explicit JobSystem(const Options& options = Options());
		JobSystem(const JobSystem&) = delete;
		JobSystem(JobSystem&&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;
		JobSystem& operator=(JobSystem&&) = delete;
		~JobSystem();

		void Initialize();
		void Shutdown(bool blocking);

		void AddJob(Job& job, Counter* counter = nullptr, const JobPriority priority = JobPriority::Normal);
		void AddJobs(std::vector<Job>& jobs, Counter* counter = nullptr, const JobPriority priority = JobPriority::Normal);

		void Wait(Counter& counter, const uint32_t targetValue);

		size_t GetThreadCount() const { return ThreadCount; }

	private:
		friend class Counter;

		std::atomic_bool Initialized{false};
		std::atomic<size_t> RunningThreads{0};
		std::atomic_bool Quit{false};

		size_t ThreadCount;
		std::vector<Thread> Threads;

		FiberPool FiberPool;

		void CleanupPreviousFiber(Tls* tls);

		size_t GetCurrentThreadIndex();
		Thread& GetCurrentThread();
		Tls& GetCurrentTls();

		JobQueue HighPriorityQueue;
		JobQueue NormalPriorityQueue;
		JobQueue LowPriorityQueue;

		JobQueue* GetQueue(JobPriority priority);
		bool TryGetJob(Job& job, Tls* tls);

		static void ThreadWorker(Thread* thread);
		static void FiberWorker(Fiber* fiber);
	};
}
