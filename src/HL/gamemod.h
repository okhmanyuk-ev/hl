#pragma once

#include <shared/all.h>

namespace HL
{
	class GameMod
	{
	public:
		using ReadMessageCallback = std::function<void(BitBuffer&)>;
		using SendCommandCallback = std::function<void(const std::string&)>;

	public:
		virtual glm::vec3 getPlayerColor(int index) const;
		virtual bool isPlayerAlive(int index) const;

	public:
		void readMessage(const std::string& name, BitBuffer& msg);
		void addReadCallback(const std::string& name, ReadMessageCallback callback);
		void setSendCommandCallback(SendCommandCallback value) { mSendCommandCallback = value; }

	protected:
		void sendCommand(const std::string& cmd);

	private:
		std::map<std::string, ReadMessageCallback> mReadCallbacks;
		SendCommandCallback mSendCommandCallback = nullptr;
	};

#define SCORE_STATUS_NONE       0
#define SCORE_STATUS_DEAD       1 << 0
#define SCORE_STATUS_BOMB       1 << 1
#define SCORE_STATUS_VIP        1 << 2
#define SCORE_STATUS_DEFKIT     1 << 3

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
		bool isPlayerAlive(int index) const override;

	public:
		Team getPlayerTeam(int index) const;
		std::optional<glm::vec3> getPlayerRadarCoord(int index) const;

	private:
		std::map<uint8_t, Team> mTeamInfo;
		std::map<uint8_t, glm::vec3> mRadar;
		std::map<uint8_t, uint8_t> mScoreStatus;
	};
}