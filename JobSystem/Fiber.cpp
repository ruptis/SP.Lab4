#include "Fiber.h"

#include <windows.h>

#include "JSException.h"

namespace
{
	void LaunchFiber(Js::Fiber* fiber)
	{
		const Js::Fiber::FiberFunc func = fiber->GetFunc();
		if (func == nullptr)
			throw Js::JsException("Fiber function is null");

		func(fiber);
	}
}

Js::Fiber::Fiber()
{
	Handle = CreateFiber(524288, reinterpret_cast<LPFIBER_START_ROUTINE>(LaunchFiber), this);
	ThreadFiber = false;
}

Js::Fiber::~Fiber()
{
	if (Handle && !ThreadFiber)
		DeleteFiber(Handle);
}

void Js::Fiber::FromCurrentThread()
{
	if (Handle && !ThreadFiber)
		DeleteFiber(Handle);

	Handle = ConvertThreadToFiber(nullptr);
	ThreadFiber = true;
}

void Js::Fiber::SetFunc(const FiberFunc func)
{
	if (func == nullptr)
		throw JsException("Fiber function is null");

	Func = func;
}

void Js::Fiber::SetData(void* data) { Data = data; }

void Js::Fiber::SwitchTo(Fiber* fiber, void* data)
{
	if (Handle == nullptr || fiber->Handle == nullptr)
		throw JsException("Fiber is not created");

	fiber->Data = data;
	fiber->ReturnFiber = this;

	SwitchToFiber(fiber->Handle);
}

void Js::Fiber::SwitchBack() const
{
	if (ReturnFiber == nullptr || ReturnFiber->Handle == nullptr)
		throw JsException("Unable to switch back to fiber");

	SwitchToFiber(ReturnFiber->Handle);
}
