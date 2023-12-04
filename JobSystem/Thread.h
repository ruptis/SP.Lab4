#pragma once
#include <cstdint>
#include <condition_variable>

#include "Tls.h"

namespace Js
{
	class Thread final
	{
	public:
		using ThreadFunc = void(*)(Thread*);

		Thread() = default;
		Thread(const Thread&) = delete;
		~Thread() = default;

		bool Create(ThreadFunc func, void* data = nullptr);
		void SetAffinity(size_t affinity) const;
		void Join() const;
		void FromCurrentThread();

		ThreadFunc GetFunc() const { return Func; }
		Tls& GetTls() { return Tls; }
		size_t GetId() const { return Id; }
		void* GetData() const { return Data; }

		void WaitForReady();

	private:
		void* Handle = nullptr;
		uint32_t Id = UINT32_MAX;

		std::condition_variable IdReceived;
		std::mutex IdMutex;

		ThreadFunc Func = nullptr;
		void* Data = nullptr;
		Tls Tls;

		bool IsCreated() const { return Id != UINT32_MAX; }
	};
}
