#pragma once
#include <functional>

#include "Counter.h"
#include "JobSystem.h"

namespace Js
{
	enum class JobPriority;
	class JobSystem;

	class Job final
	{
	public:
		Job() = default;
		Job(std::function<void(JobSystem&, void*)> function, void* data = nullptr);

		std::function<void(JobSystem&, void*)> Function;
		void* Data = nullptr;

	private:
		friend class JobSystem;

		JobSystem* System = nullptr;
		Counter* Counter = nullptr;

		void Initialize(JobSystem* system, Js::Counter* counter);

		void Execute() const;
	};
}
