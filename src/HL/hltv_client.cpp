#include "hltv_client.h"

namespace HL
{
	HLTVClient::HLTVClient() : BaseClient(true)
	{
		//getUserInfo().push_back({ "*hltv", "1" }); // TODO: make via setinfo
		//getUserInfo().push_back({ "hspecs", "0" });
		//getUserInfo().push_back({ "hslots", "0" });
		//getUserInfo().push_back({ "hdelay", "30" });
		
		//getUserInfo().push_back({ "name", "HLTV Proxy" });

		// SEE addUserInfo method to uncomment prev lines
		// and see how it is was used in base client

		//addUserInfo()
		
		addUserInfo("*hltv", "asasa",
			CVAR_GETTER(mUserInfoHLTV),
			CVAR_SETTER(mUserInfoHLTV = CON_ARG(0)));

		addUserInfo("hspecs", "asasa",
			CVAR_GETTER(mUserInfoHSpecs),
			CVAR_SETTER(mUserInfoHSpecs = CON_ARG(0)));

		addUserInfo("hslots", "asasa",
			CVAR_GETTER(mUserInfoHSlots),
			CVAR_SETTER(mUserInfoHSlots = CON_ARG(0)));

		addUserInfo("hdelay", "asasa",
			CVAR_GETTER(mUserInfoHDelay),
			CVAR_SETTER(mUserInfoHDelay = CON_ARG(0)));

		setProtInfo({ 
			{ "prot", "2" }, 
			{ "unique", "-1" }, 
			{ "raw", "861078331b85a424935805ca54f82891" } 
		});
	}

	HLTVClient::~HLTVClient()
	{
		//
	}

	//std::vector<std::pair<std::string, std::string>> HLTVClient::getProtInfo()
	//{
			/*
		  Execute('setinfo *hltv 1');
		  Execute('setinfo hspecs 0');
		  Execute('setinfo hslots 0');
		  Execute('setinfo hdelay 30');
			
		  Execute('name "HLTV Proxy"');

		  ProtInfo.Add(TAlias.Create('prot', '2'));
		  ProtInfo.Add(TAlias.Create('unique', '-1'));
		  ProtInfo.Add(TAlias.Create('raw', GenerateCDKey));
		*/

	//	return {
	//		{ "prot", "2" },
	//		{ "unique", "-1" },
	//		{ "raw", "aaaabbbbccccddddeeeeffffaaaabbbb" },
	//	};
	//}
}