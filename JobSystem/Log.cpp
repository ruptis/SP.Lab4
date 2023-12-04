#include "Log.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <locale>
#include <iomanip>
#include <algorithm>

#include <fcntl.h>
#include <io.h>

#include <Windows.h>


enum
{
	MaxConsoleLines = 500
};

namespace Log
{
	using namespace std;


	void InitConsole()
	{
		// src: https://stackoverflow.com/a/46050762/2034041
		//void RedirectIOToConsole() 
		{
			//Create a console for this application
			AllocConsole();

			// Get STDOUT handle
			HANDLE ConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
			int SystemOutput = _open_osfhandle(reinterpret_cast<intptr_t>(ConsoleOutput), _O_TEXT);
			std::FILE* COutputHandle = _fdopen(SystemOutput, "w");

			// Get STDERR handle
			HANDLE ConsoleError = GetStdHandle(STD_ERROR_HANDLE);
			int SystemError = _open_osfhandle(reinterpret_cast<intptr_t>(ConsoleError), _O_TEXT);
			std::FILE* CErrorHandle = _fdopen(SystemError, "w");

			// Get STDIN handle
			HANDLE ConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
			int SystemInput = _open_osfhandle(reinterpret_cast<intptr_t>(ConsoleInput), _O_TEXT);
			std::FILE* CInputHandle = _fdopen(SystemInput, "r");

			//make cout, wcout, cin, wcin, wcerr, cerr, wclog and clog point to console as well
			ios::sync_with_stdio(true);

			// Redirect the CRT standard input, output, and error handles to the console
			freopen_s(&CInputHandle, "CONIN$", "r", stdin);
			freopen_s(&COutputHandle, "CONOUT$", "w", stdout);
			freopen_s(&CErrorHandle, "CONOUT$", "w", stderr);

			//Clear the error state for each of the C++ standard stream objects. We need to do this, as
			//attempts to access the standard streams before they refer to a valid target will cause the
			//iostream objects to enter an error state. In versions of Visual Studio after 2005, this seems
			//to always occur during startup regardless of whether anything has been read from or written to
			//the console or not.
			std::wcout.clear();
			std::cout.clear();
			std::wcerr.clear();
			std::cerr.clear();
			std::wcin.clear();
			std::cin.clear();
		}
	}

	std::string GetCurrentTimeAsString()
	{
		const std::time_t now = std::time(0);
		std::tm tmNow;	// current time
		localtime_s(&tmNow, &now);

		// YYYY-MM-DD_HH-MM-SS
		std::stringstream ss;
		ss << (tmNow.tm_year + 1900) << "_"
			<< std::setfill('0') << std::setw(2) << tmNow.tm_mon + 1 << "_"
			<< std::setfill('0') << std::setw(2) << tmNow.tm_mday << "-"
			<< std::setfill('0') << std::setw(2) << tmNow.tm_hour << ":"
			<< std::setfill('0') << std::setw(2) << tmNow.tm_min << ":"
			<< std::setfill('0') << std::setw(2) << tmNow.tm_sec;
		return ss.str();
	}
	std::string GetCurrentTimeAsStringWithBrackets() { return "[" + GetCurrentTimeAsString() + "]"; }

	void Error(const std::string& s)
	{
		std::string err = GetCurrentTimeAsStringWithBrackets() + "  [ERROR]\t: ";
		err += s + "\n";
		OutputDebugStringA(err.c_str());
		cout << err; 
	}

	void Warning(const std::string& s)
	{
		std::string warn = GetCurrentTimeAsStringWithBrackets() + "[WARNING]\t: ";
		warn += s + "\n";
		OutputDebugStringA(warn.c_str());
		cout << warn;
	}

	void Info(const std::string& s)
	{
		const std::string info = GetCurrentTimeAsStringWithBrackets() + "   [INFO]\t: " + s + "\n";
		OutputDebugStringA(info.c_str());
		cout << info;
	}
}
