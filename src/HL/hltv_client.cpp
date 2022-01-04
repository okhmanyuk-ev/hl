#include "hltv_client.h"
#include <common/buffer_helpers.h>
#include <magic_enum.hpp>

using namespace HL;

HLTVClient::HLTVClient() : BaseClient(true)
{	
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

	CONSOLE->execute("name 'HLTV Proxy'");
}

HLTVClient::~HLTVClient()
{
	//
}

void HLTVClient::initializeGame()
{
	BaseClient::initializeGame();

	sendCommand("spectate");
}
