#include "JobSystem.h"

#include "WindowsMinimal.h"

#include "Job.h"
#include "Counter.h"

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

	Fiber* fiber = nullptr;
	Threads[0].GetTls().CurrentFiberIndex = FiberPool.GetFreeFiber(fiber);

	Threads[0].FromCurrentThread();
	Threads[0].SetAffinity(0);


	for (size_t i = 1; i < ThreadCount; ++i)
	{
		Tls& tls = Threads[i].GetTls();
		tls.ThreadIndex = i;

		if (!Threads[i].Create(ThreadWorker, this))
			throw JsException("Failed to start thread");
	}

	Initialized.store(true, std::memory_order_release);
}

void Js::JobSystem::Shutdown(const bool blocking)
{
	if (!Initialized.load(std::memory_order_relaxed))
		return;

	Quit.store(true, std::memory_order_release);

	if (blocking)
	{
		for (size_t i = 1; i < ThreadCount; ++i)
			Threads[i].Join();
	}
}

void Js::JobSystem::AddJob(Job& job, Counter* counter, const JobPriority priority)
{
	JobQueue* queue = GetQueue(priority);
	if (queue == nullptr)
		return;

	job.Initialize(this, counter);
	if (counter != nullptr)
		counter->Initialize(this, 1);

	if (!queue->Enqueue(job))
		throw JsException("Queue is full");
}

void Js::JobSystem::AddJobs(std::vector<Job>& jobs, Counter* counter, const JobPriority priority)
{
	JobQueue* queue = GetQueue(priority);
	if (queue == nullptr)
		return;

	counter->Initialize(this);

	for (Job& job : jobs)
	{
		job.Initialize(this, counter);
		counter->Increment();

		if (!queue->Enqueue(job))
			throw JsException("Queue is full");
	}
}

void Js::JobSystem::Wait(Counter& counter, const uint32_t targetValue)
{
	if (counter.GetValue() == targetValue)
		return;

	Tls& tls = GetCurrentTls();
	const auto fiberStored = new std::atomic_bool(false);

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
	tls.ThreadFiber.SwitchTo(fiber, this);

	CleanupPreviousFiber(&GetCurrentTls());
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
		break;
	case FiberDestination::Waiting:
		tls->PreviousFiberStored->store(true, std::memory_order_relaxed);
		break;
	}

	tls->Cleanup();
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

		if (!it->second->load(std::memory_order_relaxed))
			continue;

		delete it->second;
		tls->ReadyFibers.erase(it);

		tls->PreviousFiberIndex = tls->CurrentFiberIndex;
		tls->PreviousFiberDestination = FiberDestination::Pool;
		tls->CurrentFiberIndex = fiberIndex;

		tls->ThreadFiber.SwitchTo(&FiberPool.GetFiber(fiberIndex), this);
		CleanupPreviousFiber(tls);

		break;
	}

	const bool result = NormalPriorityQueue.Dequeue(job) || LowPriorityQueue.Dequeue(job);

	return result;
}

void Js::JobSystem::ThreadWorker(Thread* thread)
{
	const auto jobSystem = static_cast<JobSystem*>(thread->GetData());
	Tls& tls = thread->GetTls();

	thread->SetAffinity(tls.ThreadIndex);
	tls.ThreadFiber.FromCurrentThread();

	while (!jobSystem->Initialized.load(std::memory_order_acquire))
		_mm_pause();

	Fiber* fiber = nullptr;
	tls.CurrentFiberIndex = jobSystem->FiberPool.GetFreeFiber(fiber);

	tls.ThreadFiber.SwitchTo(fiber, jobSystem);
}

void Js::JobSystem::FiberWorker(Fiber* fiber)
{
	const auto jobSystem = static_cast<JobSystem*>(fiber->GetData());
	Tls& tls = jobSystem->GetCurrentTls();
	jobSystem->CleanupPreviousFiber(&tls);

	while (!jobSystem->Quit.load(std::memory_order_acquire))
	{
		Job job;
		if (jobSystem->TryGetJob(job, &tls))
		{
			job.Execute();
			continue;
		}

		_mm_pause();
	}

	assert(fiber->ReturnFiber != nullptr);
	fiber->SwitchBack();
}
