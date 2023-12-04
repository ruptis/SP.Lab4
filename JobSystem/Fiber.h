#pragma once

namespace Js
{
	class Fiber
	{
	public:
		using FiberFunc = void(*)(Fiber*);

		Fiber();
		Fiber(const Fiber&) = delete;
		~Fiber();

		void FromCurrentThread();

		void SetFunc(FiberFunc func);
		void SetData(void* data);

		void SwitchTo(Fiber* fiber, void* data = nullptr);
		void SwitchBack() const;

		FiberFunc GetFunc() const { return Func; }
		void* GetData() const { return Data; }
		bool IsValid() const { return Handle && Func; }

	private:
		friend class JobSystem;

		void* Handle = nullptr;
		bool ThreadFiber = false;

		Fiber* ReturnFiber = nullptr;

		FiberFunc Func = nullptr;
		void* Data = nullptr;

		explicit Fiber(void* fiber) :
			Handle(fiber) {}
	};
}