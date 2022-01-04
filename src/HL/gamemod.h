#pragma once

#include <shared/all.h>

namespace HL
{
	class GameMod
	{
	public:
		using ReadMessageCallback = std::function<void(BitBuffer&)>;
	
	public:
		virtual glm::vec3 getPlayerColor(int index) const;
		void readMessage(const std::string& name, BitBuffer& msg);
		void addReadCallback(const std::string& name, ReadMessageCallback callback);

	private:
		std::map<std::string, ReadMessageCallback> mReadCallbacks;
	};

	class CounterStrike : public GameMod
	{
	public:
		enum class Team
		{
			UNASSIGNED,
			TERRORIST,
			CT,
			SPECTATOR
		};

	public:
		CounterStrike();

	public:
		glm::vec3 getPlayerColor(int index) const override;
		Team getPlayerTeam(int index) const;

	private:
		std::map<uint8_t, Team> mTeamInfo;
	};
}