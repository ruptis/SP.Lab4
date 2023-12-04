#pragma once
#include <exception>
#include <string>

namespace Js
{
	class JsException : public std::exception
	{
	public:
		JsException() noexcept = default;
		explicit JsException(std::string message) noexcept : Message(std::move(message)) {}
		JsException(const JsException&) noexcept = default;
		~JsException() noexcept override = default;

		const char* what() const noexcept override { return Message.c_str(); }

	private:
		std::string Message;
	};
}
