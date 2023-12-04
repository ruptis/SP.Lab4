#include "JobSystem.h"

#include "WindowsMinimal.h"

#include "Job.h"
#include "Counter.h"
#include "Log.h"

Js::JobSystem::JobSystem(const Options& options):
	ThreadCount(options.ThreadCount),
	Threads(options.ThreadCount),
	FiberPool(options.FiberCount, FiberWorker, this),
	HighPriorityQueue(options.HighPriorityQueueSize),
	NormalPriorityQueue(options.NormalPriorityQueueSize),
	LowPriorityQueue(options.LowPriorityQueueSize) {}

Js::JobSystem::~JobSystem()
{
	if (Quit.load(std::memory_order_relaxed))
		return;

	Shutdown(true);
}

void Js::JobSystem::Initialize()
{
	if (Initialized.load(std::memory_order_relaxed))
		return;
	Log::Info("JobSystem::Initialize: Initializing\n");

	Threads[0].FromCurrentThread();
	Threads[0].GetTls().ThreadFiber.FromCurrentThread();
	Threads[0].GetTls().ThreadIndex = 0;
	Threads[0].SetAffinity(0);

	Fiber* fiber = nullptr;
	Threads[0].GetTls().CurrentFiberIndex = FiberPool.GetFreeFiber(fiber);
	fiber->SetFunc(FiberMain);


	for (size_t i = 1; i < ThreadCount; ++i)
	{
		Tls& tls = Threads[i].GetTls();
		tls.ThreadIndex = i;

		if (!Threads[i].Create(ThreadWorker, this))
			throw JsException("Failed to start thread");
	}

	InitializedThreads.fetch_add(1, std::memory_order_release);

	while (InitializedThreads.load(std::memory_order_acquire) < ThreadCount)
		_mm_pause();

	Initialized.store(true, std::memory_order_release);
	Log::Info("JobSystem::Initialize: Initialized\n");
}

void Js::JobSystem::Shutdown(const bool blocking)
{
	Log::Info("JobSystem::Shutdown: Shutting down\n");
	if (!Initialized.load(std::memory_order_relaxed))
		return;

	Quit.store(true, std::memory_order_release);
	Log::Info("JobSystem::Shutdown: Waiting for threads to finish\n");

	if (blocking)
	{
		for (size_t i = 1; i < ThreadCount; ++i)
			Threads[i].Join();
	}
}

void Js::JobSystem::AddJob(Job& job, Counter* counter, const JobPriority priority)
{
	Log::Info("JobSystem::AddJob: Adding job\n");
	JobQueue* queue = GetQueue(priority);
	if (queue == nullptr)
		return;
	Log::Info("JobSystem::AddJob: Queue is not null\n");

	job.Initialize(this, counter);
	if (counter != nullptr)
		counter->Initialize(this, 1);

	if (!queue->Enqueue(job))
		throw JsException("Queue is full");

	Log::Info("JobSystem::AddJob: Job added\n");
}

void Js::JobSystem::AddJobs(std::vector<Job>& jobs, Counter* counter, const JobPriority priority)
{
	JobQueue* queue = GetQueue(priority);
	if (queue == nullptr)
		return;

	if (counter != nullptr)
		counter->Initialize(this, jobs.size());

	for (Job& job : jobs)
	{
		job.Initialize(this, counter);

		if (!queue->Enqueue(job))
			throw JsException("Queue is full");
	}
}

void Js::JobSystem::Wait(Counter& counter, const uint32_t targetValue)
{
	Log::Info("JobSystem::Wait: Waiting for counter\n");
	if (counter.GetValue() == targetValue)
		return;

	Tls& tls = GetCurrentTls();
	const auto fiberStored = new std::atomic_bool(false);

	Log::Info("JobSystem::Wait: Adding waiter fiber %d on thread %d\n", tls.CurrentFiberIndex, tls.ThreadIndex);
	if (counter.AddWaiter(tls.CurrentFiberIndex, fiberStored, targetValue))
	{
		delete fiberStored;
		return;
	}

	tls.PreviousFiberIndex = tls.CurrentFiberIndex;
	tls.PreviousFiberDestination = FiberDestination::Waiting;
	tls.PreviousFiberStored = fiberStored;

	Fiber* fiber = nullptr;
	tls.CurrentFiberIndex = FiberPool.GetFreeFiber(fiber);
	Log::Info("JobSystem::Wait: Switching from fiber %d to fiber %d\n", tls.PreviousFiberIndex, tls.CurrentFiberIndex);
	tls.ThreadFiber.SwitchTo(fiber, this);

	Log::Info("JobSystem::Wait: Switched back from fiber %d to fiber %d\n", tls.CurrentFiberIndex,
	          tls.PreviousFiberIndex);
	CleanupPreviousFiber();
}

void Js::JobSystem::CleanupPreviousFiber(Tls* tls)
{
	if (tls == nullptr)
		tls = &GetCurrentTls();

	switch (tls->PreviousFiberDestination)
	{
	case FiberDestination::None:
		break;
	case FiberDestination::Pool:
		FiberPool.ReturnFiber(tls->PreviousFiberIndex);
		tls->PreviousFiberStored = nullptr;
		break;
	case FiberDestination::Waiting:
		tls->PreviousFiberStored->store(true, std::memory_order_relaxed);
		break;
	}

	tls->PreviousFiberIndex = UINT16_MAX;
	tls->PreviousFiberDestination = FiberDestination::None;
}

size_t Js::JobSystem::GetCurrentThreadIndex()
{
	return GetCurrentThread().GetId();
}

Js::Thread& Js::JobSystem::GetCurrentThread()
{
	const size_t id = GetCurrentThreadId();
	for (size_t i = 0; i < ThreadCount; ++i)
	{
		if (Threads[i].GetId() == id)
			return Threads[i];
	}
	return Threads[0];
}

Js::Tls& Js::JobSystem::GetCurrentTls()
{
	return GetCurrentThread().GetTls();
}

Js::JobQueue* Js::JobSystem::GetQueue(const JobPriority priority)
{
	switch (priority)
	{
	case JobPriority::High:
		return &HighPriorityQueue;
	case JobPriority::Normal:
		return &NormalPriorityQueue;
	case JobPriority::Low:
		return &LowPriorityQueue;
	}
	return nullptr;
}

bool Js::JobSystem::TryGetJob(Job& job, Tls* tls)
{
	if (HighPriorityQueue.Dequeue(job))
		return true;

	if (tls == nullptr)
		tls = &GetCurrentTls();

	for (auto it = tls->ReadyFibers.begin(); it != tls->ReadyFibers.end(); ++it)
	{
		const uint16_t fiberIndex = it->first;
		//Log::Info("JobSystem::TryGetJob: Checking fiber %d\n", fiberIndex);

		if (!it->second->load(std::memory_order_relaxed))
		{
			Log::Info("JobSystem::TryGetJob: Fiber %d is not ready\n", fiberIndex);
			continue;
		}

		Log::Info("JobSystem::TryGetJob: Fiber %d is ready\n", fiberIndex);
		delete it->second;
		tls->ReadyFibers.erase(it);

		tls->PreviousFiberIndex = tls->CurrentFiberIndex;
		tls->PreviousFiberDestination = FiberDestination::Pool;
		tls->CurrentFiberIndex = fiberIndex;
		Log::Info("JobSystem::TryGetJob: Switching from fiber %d to fiber %d\n", tls->PreviousFiberIndex,
		          tls->CurrentFiberIndex);

		tls->ThreadFiber.SwitchTo(&FiberPool.GetFiber(fiberIndex), this);
		CleanupPreviousFiber(tls);

		break;
	}

	return NormalPriorityQueue.Dequeue(job) || LowPriorityQueue.Dequeue(job);
}

void Js::JobSystem::ThreadWorker(Thread* thread)
{
	Log::Info("JobSystem::ThreadWorker: Thread worker\n");
	const auto jobSystem = static_cast<JobSystem*>(thread->GetData());
	jobSystem->InitializedThreads.fetch_add(1, std::memory_order_release);

	Tls& tls = thread->GetTls();

	thread->SetAffinity(tls.ThreadIndex);
	tls.ThreadFiber.FromCurrentThread();

	while (!jobSystem->Initialized.load(std::memory_order_acquire))
		_mm_pause();

	Fiber* fiber = nullptr;
	tls.CurrentFiberIndex = jobSystem->FiberPool.GetFreeFiber(fiber);

	Log::Info("JobSystem::ThreadWorker: Switching to fiber\n");
	tls.ThreadFiber.SwitchTo(fiber, jobSystem);
	Log::Info("JobSystem::ThreadWorker: Switched back to fiber\n");
	jobSystem->Shutdown(false);
}

void Js::JobSystem::FiberWorker(Fiber* fiber)
{
	Log::Info("JobSystem::FiberWorker: Fiber worker\n");
	const auto jobSystem = static_cast<JobSystem*>(fiber->GetData());
	jobSystem->CleanupPreviousFiber();

	while (!jobSystem->Quit.load(std::memory_order_acquire))
	{
		Tls& tls = jobSystem->GetCurrentTls();
		Job job;
		if (jobSystem->TryGetJob(job, &tls))
		{
			Log::Info("JobSystem::FiberWorker: Executing job\n");
			job.Execute();
			Log::Info("JobSystem::FiberWorker: Job executed\n");
			continue;
		}

		SwitchToThread();
	}

	assert(fiber->ReturnFiber != nullptr);
	fiber->SwitchBack();
}

void Js::JobSystem::FiberMain(Fiber* fiber)
{
	Log::Info("JobSystem::FiberMain: Fiber main\n");
	const auto jobSystem = static_cast<JobSystem*>(fiber->GetData());
	Tls& tls = jobSystem->GetCurrentTls();
	jobSystem->CleanupPreviousFiber();

	assert(fiber->ReturnFiber != nullptr);
	fiber->SwitchBack();
}
