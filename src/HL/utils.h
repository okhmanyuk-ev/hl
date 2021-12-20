#pragma once

#include <string>
#include <console/device.h>
#include <console/system.h>
#include <source_location>
#include <utility>

namespace HL::Utils
{
	inline std::string GetInfoValue(const std::string& info, const std::string& key)
	{
		auto s = info.substr(info.find(key + "\\") + key.length() + 1);
		return s.substr(0, s.find('\\'));
	}

	template <typename... Args>
	struct dlog
	{
		dlog(std::string text, Args&&... args, const std::source_location& location = std::source_location::current())
		{
			if (CONSOLE->getCVars().at("dlogs").getGetter()().at(0) != "1")
				return;

			LOGCF("[{}] " + text, Console::Color::DarkGray, location.function_name(), args...);
		}
	};

	template <typename... Args>
	dlog(std::string, Args&&...) -> dlog<Args...>;
}