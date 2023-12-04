#include "Job.h"

#include <utility>

Js::Job::Job(std::function<void(JobSystem&, void*)> function, void* data) :
Function(std::move(function)), Data(data){}

void Js::Job::Initialize(JobSystem* system, Js::Counter* counter)
{
	System = system;
	Counter = counter;
}

void Js::Job::Execute() const
{
	Function(*System, Data);

	if (Counter)
		Counter->Decrement();
}
