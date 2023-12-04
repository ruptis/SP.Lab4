#pragma once

#define LOG_FN(FN_NAME)\
template<class... Args>\
void FN_NAME(const char* format, Args&&... args)\
{\
	char msg[LEN_MSG_BUFFER];\
	sprintf_s(msg, format, args...);\
	##FN_NAME(std::string(msg));\
}
#include <string>

namespace Log
{
	constexpr size_t LEN_MSG_BUFFER = 4096;

	void Info(const std::string& s);
	void Error(const std::string& s);
	void Warning(const std::string& s);

	LOG_FN(Error)
	LOG_FN(Warning)
	LOG_FN(Info)
}
