#include "gamemod.h"

using namespace HL;

// GameMod

glm::vec3 GameMod::getPlayerColor(int index) const
{
	return Graphics::Color::Yellow;
}

void GameMod::readMessage(const std::string& name, BitBuffer& msg)
{
	if (!mReadCallbacks.contains(name))
		return;

	mReadCallbacks.at(name)(msg);
}

void GameMod::addReadCallback(const std::string& name, ReadMessageCallback callback)
{
	mReadCallbacks.insert({ name, callback });
}

// CounterStrike

CounterStrike::CounterStrike()
{
	addReadCallback("ReqState", [](auto& msg) {
		//sendCommand("VModEnable 1");
	});

	addReadCallback("TeamInfo", [this](auto& msg) {
		auto player_id = msg.read<uint8_t>();
		auto team = Common::BufferHelpers::ReadString(msg);
		mTeamInfo[player_id] = magic_enum::enum_cast<Team>(team).value_or(Team::UNASSIGNED);
	});
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

CounterStrike::Team CounterStrike::getPlayerTeam(int index) const
{
	if (!mTeamInfo.contains(index))
		return Team::UNASSIGNED;

	return mTeamInfo.at(index);
}
