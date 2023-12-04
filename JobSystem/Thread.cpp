#include "Thread.h"
#include <windows.h>

#include "JSException.h"

namespace
{
	void WINAPI LaunchThread(void* data)
	{
		const auto thread = static_cast<Js::Thread*>(data);
		const Js::Thread::ThreadFunc func = thread->GetFunc();

		if (func == nullptr)
			throw Js::JsException("Thread function is null");

		thread->WaitForReady();
		func(thread);
	}
}

bool Js::Thread::Create(const ThreadFunc func, void* data)
{
	Handle = nullptr;
	Id = UINT32_MAX;
	Func = func;
	Data = data;
	IdReceived.notify_all();

	{
		std::lock_guard<std::mutex> lock(IdMutex);
		Handle = CreateThread(nullptr, 524288, reinterpret_cast<LPTHREAD_START_ROUTINE>(LaunchThread), this, 0,
		                      reinterpret_cast<DWORD*>(&Id));
	}

	return IsCreated();
}

void Js::Thread::SetAffinity(const size_t affinity) const
{
	if (!IsCreated())
		throw JsException("Thread is not created");

	const auto mask = 1ull << affinity;
	if (SetThreadAffinityMask(Handle, mask) == 0)
		throw JsException("Failed to set thread affinity");
}

void Js::Thread::Join() const
{
	if (!IsCreated())
		throw JsException("Thread is not created");

	WaitForSingleObject(Handle, INFINITE);
}

void Js::Thread::FromCurrentThread()
{
	Handle = GetCurrentThread();
	Id = GetCurrentThreadId();
}

void Js::Thread::WaitForReady()
{
	{
		std::lock_guard<std::mutex> lock(IdMutex);
		if (IsCreated())
			return;
	}

	std::mutex mutex;

	std::unique_lock<std::mutex> lock(mutex);
	IdReceived.wait(lock);
}
