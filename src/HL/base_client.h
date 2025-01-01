#pragma once

#include "networking.h"
#include <console/device.h>
#include <common/console_commands.h>
#include <console/system.h>
#include <vector>
#include <map>
#include "channel.h"
#include "delta.h"
#include "encoder.h"
#include <cstdint>
#include "gamemod.h"

namespace HL
{
	class BaseClient : public Networking,
		public Common::FrameSystem::Frameable
	{
	public:
		enum class State
		{
			Disconnected,
			Challenging,
			Connecting,
			Connected,
			GameStarted // signon 2, after entities start receiving
		};

	public:
		BaseClient(bool hltv = false);
		~BaseClient();

	public:
		void onFrame() override;

	protected:
		void readConnectionlessPacket(Network::Packet& packet) override;
		void readRegularPacket(Network::Packet& packet) override;

	private:
		void readConnectionlessChallenge(Network::Packet& packet);
		void readConnectionlessAccepted(Network::Packet& packet);
		void readConnectionlessReject(Network::Packet& packet);

	private:
		void readRegularMessages(sky::BitBuffer& msg);
		void readRegularGameMessage(sky::BitBuffer& msg, uint8_t index);
		void receiveFile(std::string_view fileName, sky::BitBuffer& msg);

		void readRegularDisconnect(sky::BitBuffer& msg);
		void readRegularEvent(sky::BitBuffer& msg);
		void readRegularVersion(sky::BitBuffer& msg);
		void readRegularSetView(sky::BitBuffer& msg);
		void readRegularSound(sky::BitBuffer& msg);
		void readRegularTime(sky::BitBuffer& msg);
		void readRegularPrint(sky::BitBuffer& msg);
		void readRegularStuffText(sky::BitBuffer& msg);
		void readRegularAngle(sky::BitBuffer& msg);
		void readRegularServerInfo(sky::BitBuffer& msg);
		void readRegularLightStyle(sky::BitBuffer& msg);
		void readRegularUserInfo(sky::BitBuffer& msg);
		void readRegularDeltaDescription(sky::BitBuffer& msg);
		void readRegularClientData(sky::BitBuffer& msg);
		void readRegularPings(sky::BitBuffer& msg);
		void readRegularEventReliable(sky::BitBuffer& msg);
		void readRegularSpawnBaseline(sky::BitBuffer& msg);
		void readRegularTempEntity(sky::BitBuffer& msg);
		void readRegularSignonNum(sky::BitBuffer& msg);
		void readRegularStaticSound(sky::BitBuffer& msg);
		void readRegularCDTrack(sky::BitBuffer& msg);
		void readRegularWeaponAnim(sky::BitBuffer& msg);
		void readRegularDecalName(sky::BitBuffer& msg);
		void readRegularRoomType(sky::BitBuffer& msg);
		void readRegularUserMsg(sky::BitBuffer& msg);
		void readRegularPacketEntities(sky::BitBuffer& msg, bool delta);
		void readRegularResourceList(sky::BitBuffer& msg);
		void readRegularMoveVars(sky::BitBuffer& msg);
		void readRegularResourceRequest(sky::BitBuffer& msg);
		void readRegularCustomization(sky::BitBuffer& msg);
		void readFileTxferFailed(sky::BitBuffer& msg);
		void readRegularHLTV(sky::BitBuffer& msg);
		void readRegularDirector(sky::BitBuffer& msg);
		void readRegularVoiceInit(sky::BitBuffer& msg);
		void readRegularSendExtraInfo(sky::BitBuffer& msg);
		void readRegularResourceLocation(sky::BitBuffer& msg);
		void readRegularCVarValue(sky::BitBuffer& msg);
		void readRegularCVarValue2(sky::BitBuffer& msg);

		void readTempEntityBeamPoints(sky::BitBuffer& msg);
		void readTempEntityExplosion(sky::BitBuffer& msg);
		void readTempEntitySmoke(sky::BitBuffer& msg);
		void readTempEntitySparks(sky::BitBuffer& msg);
		void readTempEntityGlowSprite(sky::BitBuffer& msg);
		void readTempEntitySprite(sky::BitBuffer& msg);
		void readTempEntityBloodSprite(sky::BitBuffer& msg);

	private:
		void writeRegularMessages(sky::BitBuffer& msg);

		void writeRegularMove(sky::BitBuffer& msg);
		void writeRegularDelta(sky::BitBuffer& msg);
		void writeRegularFileConsistency(sky::BitBuffer& msg);

	private:
		void onConnect(CON_ARGS);
		void onDisconnect(CON_ARGS);
		void onRetry(CON_ARGS);
		void onCmd(CON_ARGS);
		void onFullServerInfo(CON_ARGS);
		void onReconnect(CON_ARGS);

	public:
		auto getState() const { return mState; }
		const auto& getEntities() const { return mEntities; }
		const auto& getBaselines() const { return mBaselines; }
		const auto& getClientData() const { return mClientData; }
		const auto& getPlayerUserInfos() const { return mPlayerUserInfos; }
		const auto& getChannel() const { return mChannel; }
		const auto& getResources() const { return mResources; }
		const auto& getGameMod() const { return mGameMod; }
		const auto& getServerInfo() const { return mServerInfo; }
		const auto& getMoveVars() const { return mMoveVars; }

		const auto& getProtInfo() const { return mProtInfo; }
		void setProtInfo(const std::map<std::string, std::string>& value) { mProtInfo = value; }

		const auto& getCertificate() const { return mCertificate; }
		void setCertificate(const std::vector<uint8_t>& value) { mCertificate = value; }

	private:
		std::optional<Network::Address> mServerAdr; 
		State mState = State::Disconnected;
		std::map<std::string, std::string> mProtInfo;
		std::vector<uint8_t> mCertificate = { };
		std::optional<Channel> mChannel;
		std::list<std::string> mDownloadQueue;
		bool mResourcesVerifying = false;
		bool mResourcesVerified = false;
		bool mConfirmationRequired = false;
		Delta mDelta;
		std::map<uint8_t, Protocol::GameMessage> mGameMessages;
		std::shared_ptr<GameMod> mGameMod;
		float mTime = 0.0f; // svc_time
		Protocol::ClientData mClientData = {}; // svc_clientdata
		std::vector<std::string> m_LightStyles; // svc_lightstyles
		std::vector<Protocol::WeaponData> m_WeaponData; // svc_clientdata
		std::vector<Protocol::Resource> mResources;
		std::map<int, Protocol::Entity*> mEntities; // TODO: shared ptr
		std::map<int, Protocol::Entity> mDeltaEntities;
		std::map<int, Protocol::Entity> mBaselines;
		std::map<int, Protocol::Entity> mExtraBaselines;
		std::map<int, std::string> mPlayerUserInfos;
		bool mHLTV = false;
		std::optional<Protocol::ServerInfo> mServerInfo;
		std::optional<Protocol::MoveVars> mMoveVars;
		uint8_t mSignonNum = 0;
		uint8_t mDeltaSequence;
		std::optional<Clock::TimePoint> mInitializeConnectionTime;
		float mTimeout = 30.0f;

	public:
		using ThinkCallback = std::function<void(Protocol::UserCmd&)>;
		using DisconnectCallback = std::function<void(const std::string& reason)>;
		using IsResourceRequiredCallback = std::function<bool(const Protocol::Resource& resource)>;
		using GameEngineInitializedCallback = std::function<void()>;
		using GameInitializedCallback = std::function<void()>;

	public:
		void setThinkCallback(ThinkCallback value) { mThinkCallback = value; }
		void setDisconnectCallback(DisconnectCallback value) { mDisconnectCallback = value; }
		void setResourceRequiredCallback(IsResourceRequiredCallback callback) { mIsResourceRequiredCallback = callback; }
		void setGameEngineInitializedCallback(GameEngineInitializedCallback value) { mGameEngineInitializedCallback = value; }
		void setGameInitializedCallback(GameInitializedCallback value) { mGameInitializedCallback = value; }

	private:
		ThinkCallback mThinkCallback = nullptr;
		DisconnectCallback mDisconnectCallback = nullptr;
		IsResourceRequiredCallback mIsResourceRequiredCallback = nullptr;
		GameEngineInitializedCallback mGameEngineInitializedCallback = nullptr;
		GameInitializedCallback mGameInitializedCallback = nullptr;

	private:
		void verifyResources();

		virtual bool isResourceRequired(const Protocol::Resource& resource);
		virtual int getResourceHash(const Protocol::Resource& resource);

		void signon(uint8_t num);
		void initializeConnection();

	protected:
		virtual void initializeGameEngine();
		virtual void initializeGame();
		virtual void resetGameResources();

	public:
		void sendCommand(const std::string& command);
		void connect(const Network::Address& address);
		void disconnect(const std::string& reason);
		bool isPlayerIndex(int value) const;
		std::optional<HL::Protocol::Resource> findModel(int model_index) const;

	private: // userinfos
		std::string mUserInfoDLMax = "512";
		std::string mUserInfoLC = "1";
		std::string mUserInfoLW = "1";
		std::string mUserInfoUpdaterate = "101";
		std::string mUserInfoModel = "gordon";
		std::string mUserInfoRate = "25000";
		std::string mUserInfoTopColor = "0"; // TODO: not in hltv
		std::string mUserInfoBottomColor = "0"; // TODO: not in hltv
		std::string mUserInfoName = "Player";
		std::string mUserInfoPassword = "";

	protected:
		void addUserInfo(const std::string& name, const std::string& description, 
			Console::CVar::Getter getter, Console::CVar::Setter setter);

	private:
		std::map<std::string, Console::CVar::Getter> mUserInfos;

	public:
		using EventCallback = std::function<void(const Protocol::Event& evt)>;
		using BeamPointsCallback = std::function<void(const glm::vec3& start, const glm::vec3& end, uint8_t lifetime, const glm::vec4& color)>;
		using BloodSpriteCallback = std::function<void(const glm::vec3& origin)>;
		using SparksCallback = std::function<void(const glm::vec3& origin)>;
		using GlowSpriteCallback = std::function<void(const glm::vec3& origin, int model_index)>;
		using SpriteCallback = std::function<void(const glm::vec3& origin, int model_index)>;
		using SmokeCallback = std::function<void(const glm::vec3& origin, int model_index)>;
		using ExplosionCallback = std::function<void(const glm::vec3& origin, int model_index)>;

	public:
		void setEventCallback(EventCallback value) { mEventCallback = value; }
		void setBeamPointsCallback(BeamPointsCallback value) { mBeamPointsCallback = value; }
		void setBloodSpriteCallback(BloodSpriteCallback value) { mBloodSpriteCallback = value; }
		void setSparksCallback(SparksCallback value) { mSparksCallback = value; }
		void setGlowSpriteCallback(GlowSpriteCallback value) { mGlowSpriteCallback = value; }
		void setSpriteCallback(SpriteCallback value) { mSpriteCallback = value; }
		void setSmokeCallback(SmokeCallback value) { mSmokeCallback = value; }
		void setExplosionCallback(ExplosionCallback value) { mExplosionCallback = value; }

	private:
		EventCallback mEventCallback = nullptr;
		BeamPointsCallback mBeamPointsCallback = nullptr;
		BloodSpriteCallback mBloodSpriteCallback = nullptr;
		SparksCallback mSparksCallback = nullptr;
		GlowSpriteCallback mGlowSpriteCallback = nullptr;
		SpriteCallback mSpriteCallback = nullptr;
		SmokeCallback mSmokeCallback = nullptr;
		ExplosionCallback mExplosionCallback = nullptr;

	private:
		bool mDlogs = true;
		bool mDlogsEvents = true;
		bool mDlogsGmsg = true;
		bool mDlogsTempEnts = true;
	};
}
