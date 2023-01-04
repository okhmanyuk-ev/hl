#pragma once

#include <string>
#include <console/device.h>
#include <console/system.h>
#include <platform/defines.h>

#if !defined(PLATFORM_IOS) && !defined(PLATFORM_MAC) && !defined(PLATFORM_EMSCRIPTEN)
	#include <source_location>
#endif

#include <utility>

#if defined(PLATFORM_IOS)
    #define HL_ASSET_STORAGE Platform::Asset::Storage::Bundle
#else
    #define HL_ASSET_STORAGE Platform::Asset::Storage::Assets
#endif

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
#if defined(PLATFORM_IOS) || defined(PLATFORM_MAC) || defined(PLATFORM_EMSCRIPTEN)
        dlog(std::string text, Args&&... args)
        {
            if (CONSOLE->getCVars().at("dlogs").getGetter()().at(0) != "1")
                return;

            LOGCF(text, Console::Color::DarkGray, args...);
        }
#else
		dlog(std::string text, Args&&... args, const std::source_location& location = std::source_location::current())
		{
			if (CONSOLE->getCVars().at("dlogs").getGetter()().at(0) != "1")
				return;

			LOGCF("[{}] " + text, Console::Color::DarkGray, location.function_name(), args...);
		}
#endif
	};

	template <typename... Args>
	dlog(std::string, Args&&...) -> dlog<Args...>;
}
