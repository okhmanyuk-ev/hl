#include "gamemod.h"

using namespace HL;

// GameMod

glm::vec3 GameMod::getPlayerColor(int index) const
{
	return Graphics::Color::Yellow;
}

bool GameMod::isPlayerAlive(int index) const
{
	return true;
}

void GameMod::readMessage(const std::string& name, sky::BitBuffer& msg)
{
	if (!mReadCallbacks.contains(name))
		return;

	mReadCallbacks.at(name)(msg);
}

void GameMod::addReadCallback(const std::string& name, ReadMessageCallback callback)
{
	mReadCallbacks.insert({ name, callback });
}

void GameMod::sendCommand(const std::string& cmd)
{
	mSendCommandCallback(cmd);
}

// DummyGameMod

DummyGameMod::DummyGameMod()
{
	addReadCallback("ReqState", [this](auto& msg) {
		sendCommand("VModEnable 1");
	});
}

// CounterStrike

CounterStrike::CounterStrike()
{
	/*
	AddCallback("AmmoX", ReadAmmoX)
	AddCallback("WeaponList", ReadWeaponList)*/
	addReadCallback("ReqState", [this](auto& msg) {
		sendCommand("VModEnable 1");
	});
    /*
	AddCallback("SayText", ReadSayText)
	AddCallback("MOTD", ReadMOTD)
	*/
	addReadCallback("TeamInfo", [this](sky::BitBuffer& msg) {
		auto player_id = msg.read<uint8_t>();
		auto team = sky::bitbuffer_helpers::ReadString(msg);
		mTeamInfo[player_id] = magic_enum::enum_cast<Team>(team).value_or(Team::UNASSIGNED);
	});
	/*
	AddCallback("ScoreInfo", ReadScoreInfo)
	AddCallback("TextMsg", ReadTextMsg)
	AddCallback("ServerName", ReadServerName)
	AddCallback("Money", ReadMoney)
	AddCallback("TeamScore", ReadTeamScore)
	*/
	addReadCallback("ScoreAttrib", [this](sky::BitBuffer& msg) {
		auto player_id = msg.read<uint8_t>();
		auto status = msg.read<uint8_t>();
		mScoreStatus[player_id] = status;
	});
	/*
	AddCallback("StatusIcon", ReadStatusIcon)
	AddCallback("StatusValue", ReadStatusValue)
	AddCallback("FlashBat", ReadFlashBat)
	AddCallback("CurWeapon", ReadCurWeapon)
	AddCallback("Health", ReadHealth)
	AddCallback("RoundTime", ReadRoundTime)
	AddCallback("SetFOV", ReadSetFOV)
	AddCallback("Location", ReadLocation)
	AddCallback("WeapPickup", ReadWeapPickup)
	AddCallback("AmmoPickup", ReadAmmoPickup)
	AddCallback("ScreenFade", ReadScreenFade)
	AddCallback("Damage", ReadDamage)
	AddCallback("DeathMsg", ReadDeathMsg)
	AddCallback("HostagePos", ReadHostagePos)
	AddCallback("Battery", ReadBattery)
	AddCallback("BarTime", ReadBarTime)
	AddCallback("HudTextArgs", ReadHudTextArgs)
	AddCallback("HideWeapon", ReadHideWeapon)
	AddCallback("SendAudio", ReadSendAudio)
	AddCallback("Geiger", ReadGeiger)
	*/
	addReadCallback("Radar", [this](sky::BitBuffer& msg) {
		auto player_id = msg.read<uint8_t>();
		auto x = sky::bitbuffer_helpers::ReadCoord(msg);
		auto y = sky::bitbuffer_helpers::ReadCoord(msg);
		auto z = sky::bitbuffer_helpers::ReadCoord(msg);
		mRadar[player_id] = { x, y, z };
	});
	/*
	AddCallback("ShowMenu", ReadShowMenu)
	AddCallback("Train", ReadTrain)
	AddCallback("ResetHUD", ReadResetHUD)
	AddCallback("InitHUD", ReadInitHUD)
	AddCallback("GameMode", ReadGameMode)
	AddCallback("ViewMode", ReadViewMode)
	AddCallback("ShadowIdx", ReadShadowIdx)
	AddCallback("AllowSpec", ReadAllowSpec)
	AddCallback("ItemStatus", ReadItemStatus)
	AddCallback("ForceCam", ReadForceCam)
	AddCallback("Crosshair", ReadCrosshair)
	AddCallback("Spectator", ReadSpectator)
	AddCallback("NVGToggle", ReadNVGToggle)
	AddCallback("VGUIMenu", ReadVGUIMenu)
	AddCallback("ADStop", ReadADStop)
	AddCallback("BotVoice", ReadBotVoice)
	AddCallback("Scenario", ReadScenario)
	AddCallback("ClCorpse", ReadClCorpse)
	AddCallback("Brass", ReadBrass)
	AddCallback("BombDrop", ReadBombDrop)
	AddCallback("BombPickup", ReadBombPickup)
	AddCallback("ShowTimer", ReadShowTimer)
	AddCallback("SpecHealth", ReadSpecHealth)
	AddCallback("StatusText", ReadStatusText)
	AddCallback("ReloadSound", ReadReloadSound)
	AddCallback("Account", ReadAccount)
	AddCallback("HealthInfo", ReadHealthInfo)
	*/
}

glm::vec3 CounterStrike::getPlayerColor(int index) const
{
	auto team = getPlayerTeam(index);

	if (team == Team::TERRORIST)
		return Graphics::Color::Red;
	else if (team == Team::CT)
		return Graphics::Color::Blue;
	else
		return Graphics::Color::DarkGray;
}

bool CounterStrike::isPlayerAlive(int index) const
{
	if (!mScoreStatus.contains(index))
		return true;

	auto status = mScoreStatus.at(index);

	return (status & SCORE_STATUS_DEAD) == 0;
}

CounterStrike::Team CounterStrike::getPlayerTeam(int index) const
{
	if (!mTeamInfo.contains(index))
		return Team::UNASSIGNED;

	return mTeamInfo.at(index);
}

std::optional<glm::vec3> CounterStrike::getPlayerRadarCoord(int index) const
{
	if (!mRadar.contains(index))
		return std::nullopt;

	return mRadar.at(index);
}
