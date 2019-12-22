#pragma once

#include "base_client.h"

#include <Core/engine.h>

namespace HL
{
	class HLTVClient : public BaseClient
	{
	public:
		HLTVClient();
		~HLTVClient();

	//protected:
	//	std::vector<std::pair<std::string, std::string>> getProtInfo() override;

	private:
		std::string mUserInfoHLTV = "1";
		std::string mUserInfoHSpecs = "0";
		std::string mUserInfoHSlots = "0";
		std::string mUserInfoHDelay = "30";
	};
}