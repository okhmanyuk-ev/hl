#pragma once

#include <string>

namespace HL::Utils
{
	std::string GetInfoValue(const std::string& info, const std::string& key)
	{
		auto s = info.substr(info.find(key + "\\") + key.length() + 1);
		return s.substr(0, s.find('\\'));
	}
}