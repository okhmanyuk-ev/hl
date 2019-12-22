#include "playable_client.h"


namespace HL
{
	PlayableClient::PlayableClient() : BaseClient()
	{
		setProtInfo({ { "prot", "3" }, { "unique", "-1" }, { "raw", "steam" }, { "cdkey", "a14d12f2b1c001518c3837f477cbe0f7" } });
	}

	PlayableClient::~PlayableClient()
	{
		//
	}

	//std::vector<std::pair<std::string, std::string>> PlayableClient::getProtInfo()
	//{
	//	return {
	//		{ "prot", "3" },
	//		{ "unique", "-1" },
	//		{ "raw", "steam" },
	//		{ "cdkey", "a14d12f2b1c001518c3837f477cbe0f7" }
	//	};
	//}
}