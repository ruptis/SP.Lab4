#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <algorithm>

#include "JobSystem.h"
#include "Job.h"
#include "WindowsMinimal.h"

// divide strings into 12 parts, sort each part, merge all parts into one sorted vector

struct MergeData
{
	std::vector<std::string> FirstPart;
	std::vector<std::string> SecondPart;
	std::vector<std::string> SortedStrings;
};

void SortAndMerge(Js::JobSystem& jobSystem, void* data)
{
	auto* jobData = static_cast<MergeData*>(data);
	std::merge(jobData->FirstPart.begin(), jobData->FirstPart.end(), jobData->SecondPart.begin(),
	           jobData->SecondPart.end(), jobData->SortedStrings.begin());
}

struct DivideAndSortData
{
	uint32_t PartCount;
	std::vector<std::string> Strings;
	std::vector<std::string> SortedStrings;
};

void DivideAndSort(Js::JobSystem& jobSystem, void* data)
{
	auto* jobData = static_cast<DivideAndSortData*>(data);

	OutputDebugStringA(("Sorting " + std::to_string(jobData->Strings.size()) + " strings\n").c_str());

	if (jobData->PartCount == 1)
	{
		std::sort(jobData->Strings.begin(), jobData->Strings.end());
		OutputDebugStringA(("Finished sorting " + std::to_string(jobData->Strings.size()) + " strings\n").c_str());
		return;
	}

	DivideAndSortData firstPartData;
	firstPartData.PartCount = jobData->PartCount / 2;
	std::copy_n(jobData->Strings.begin(), jobData->Strings.size() / 2,
	            std::back_inserter(firstPartData.Strings));
	DivideAndSortData secondPartData;
	secondPartData.PartCount = jobData->PartCount - jobData->PartCount / 2;
	std::copy(jobData->Strings.begin() + jobData->Strings.size() / 2, jobData->Strings.end(),
	          std::back_inserter(secondPartData.Strings));

	const Js::Job firstPartJob(DivideAndSort, &firstPartData);
	const Js::Job secondPartJob(DivideAndSort, &secondPartData);
	std::vector<Js::Job> dependencies = {firstPartJob, secondPartJob};

	Js::Counter counter;

	jobSystem.AddJobs(dependencies, &counter);
	OutputDebugStringA(("Waiting for " + std::to_string(jobData->Strings.size()) + " strings to be sorted\n").c_str());
	jobSystem.Wait(counter, 0);

	MergeData mergeData;
	mergeData.FirstPart = std::move(firstPartData.SortedStrings);
	mergeData.SecondPart = std::move(secondPartData.SortedStrings);

	Js::Job mergeJob(SortAndMerge, &mergeData);

	Js::Counter mergeCounter;

	jobSystem.AddJob(mergeJob, &mergeCounter);
	OutputDebugStringA(("Waiting for " + std::to_string(jobData->Strings.size()) + " strings to be merged\n").c_str());
	jobSystem.Wait(mergeCounter, 0);

	jobData->SortedStrings = std::move(mergeData.SortedStrings);
	OutputDebugStringA(("Finished sorting and merging " + std::to_string(jobData->Strings.size()) + " strings\n").c_str());
}

int main()
{
	Js::JobSystem jobSystem;
	jobSystem.Initialize();

	std::vector<std::string> strings;
	strings.reserve(1024);
	for (int i = 0; i < 1024; ++i)
	{
		strings.push_back(std::to_string(rand()));
	}

	std::cout << "Sorting " << strings.size() << " strings" << std::endl;

	DivideAndSortData data;
	std::copy(strings.begin(), strings.end(), std::back_inserter(data.Strings));
	data.PartCount = 12;

	Js::Job job(DivideAndSort, &data);

	jobSystem.AddJob(job);

	std::cout << "Waiting for job to finish" << std::endl;

	while (data.SortedStrings.empty())
	{
		Sleep(1);
	}

	jobSystem.Shutdown(true);
}
