#pragma once

#include "base_client.h"

#include <Core/engine.h>

namespace HL
{
	class PlayableClient : public BaseClient
	{
	public:
		PlayableClient();
		~PlayableClient();

		/*private: // userinfos // TODO: make better
		std::string mUserInfoDLMax = "512";
		std::string mUserInfoLC = "1";
		std::string mUserInfoLW = "1";
		std::string mUserInfoUpdaterate = "101";
		std::string mUserInfoModel = "gordon";
		std::string mUserInfoRate = "25000";
		std::string mUserInfoTopColor = "0";
		std::string mUserInfoBottomColor = "0";
		std::string mUserInfoName = "Player";
		std::string mUserInfoPassword = "";
		*/

	//protected:
	//	std::vector<std::pair<std::string, std::string>> getProtInfo() override;

	};
}
