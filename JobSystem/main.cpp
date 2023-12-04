#include <iostream>
#include <vector>
#include <algorithm>
#include <string>
#include <algorithm>
#include <fstream>

#include "JobSystem.h"
#include "Job.h"
#include "WindowsMinimal.h"

struct DivideAndSortData
{
	uint32_t PartCount;
	std::vector<std::string> Strings;
};

void DivideAndSort(Js::JobSystem& jobSystem, void* data)
{
	auto* jobData = static_cast<DivideAndSortData*>(data);

	if (jobData->PartCount == 1)
	{
		std::sort(jobData->Strings.begin(), jobData->Strings.end());
		return;
	}

	DivideAndSortData firstPartData;
	firstPartData.PartCount = jobData->PartCount / 2;
	firstPartData.Strings.assign(jobData->Strings.begin(), jobData->Strings.begin() + jobData->Strings.size() / 2);

	DivideAndSortData secondPartData;
	secondPartData.PartCount = jobData->PartCount - jobData->PartCount / 2;
	secondPartData.Strings.assign(jobData->Strings.begin() + jobData->Strings.size() / 2, jobData->Strings.end());

	Js::Job firstPartJob{DivideAndSort, &firstPartData};
	Js::Job secondPartJob{DivideAndSort, &secondPartData};
	auto jobs = std::vector<Js::Job>{firstPartJob, secondPartJob};

	Js::Counter counter;

	jobSystem.AddJobs(jobs, &counter);

	jobSystem.Wait(counter, 0);

	std::merge(firstPartData.Strings.begin(), firstPartData.Strings.end(), secondPartData.Strings.begin(),
	           secondPartData.Strings.end(), jobData->Strings.begin());
}

void ReadFile(const char* str, std::vector<std::string>& vector)
{
	std::ifstream file(str);
	std::string line;
	while (std::getline(file, line))
	{
		vector.push_back(line);
	}
}

void WriteToFile(const char* str, const std::vector<std::string>& vector)
{
	std::ofstream file(str);
	for (const auto& line : vector)
	{
		file << line << std::endl;
	}
}

int main()
{
	Js::JobSystem jobSystem;
	jobSystem.Initialize();

	std::vector<std::string> strings;
	ReadFile("strings.txt", strings);

	std::cout << "Sorting " << strings.size() << " strings" << std::endl;

	DivideAndSortData data;
	data.PartCount = 16;
	std::copy(strings.begin(), strings.end(), std::back_inserter(data.Strings));

	Js::Job job{DivideAndSort, &data};
	Js::Counter counter;

	jobSystem.AddJob(job, &counter);

	jobSystem.Wait(counter, 0);

	jobSystem.Shutdown(true);

	std::cout << "Sorted strings: " << std::is_sorted(data.Strings.begin(), data.Strings.end()) << std::endl;

	WriteToFile("sorted_strings.txt", data.Strings);
}
