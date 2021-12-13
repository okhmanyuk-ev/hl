#pragma once

#include "networking.h"
#include <Console/device.h>
#include <Common/console_commands.h>
#include <Network/address.h>
#include <Console/system.h>
#include <vector>
#include <map>
#include "channel.h"
#include "delta.h"
#include "encoder.h"

namespace HL
{
	class BaseClient : public Networking
	{
	public:
		enum class State
		{
			Disconnected,
			Challenging,
			Connecting,
			Connected
		};

		using ThinkCallback = std::function<void(Protocol::UserCmd&)>;
		using DisconnectCallback = std::function<void(const std::string& reason)>;
		using ReadGameMessageCallback = std::function<void(const std::string& name, void* memory, size_t size)>;
		using IsResourceRequiredCallback = std::function<bool(const Protocol::Resource& resource)>;

	public:
		BaseClient(bool hltv = false);
		~BaseClient();
	
#pragma region read
	protected:
		void readConnectionlessPacket(Network::Packet& packet) override;
		void readRegularPacket(Network::Packet& packet) override;

	private:
		void readConnectionlessChallenge(Network::Packet& packet);
		void readConnectionlessAccepted(Network::Packet& packet);
		void readConnectionlessReject(Network::Packet& packet);

	private:
		void readRegularMessages(BitBuffer& msg);
		void readRegularGameMessage(BitBuffer& msg, uint8_t index);
		void receiveFile(std::string_view fileName, BitBuffer& msg);

		void readRegularDisconnect(BitBuffer& msg);
		void readRegularEvent(BitBuffer& msg);
		void readRegularVersion(BitBuffer& msg);
		void readRegularSetView(BitBuffer& msg);
		void readRegularSound(BitBuffer& msg);
		void readRegularTime(BitBuffer& msg);
		void readRegularPrint(BitBuffer& msg);
		void readRegularStuffText(BitBuffer& msg);
		void readRegularAngle(BitBuffer& msg);
		void readRegularServerInfo(BitBuffer& msg);
		void readRegularLightStyle(BitBuffer& msg);
		void readRegularUserInfo(BitBuffer& msg);
		void readRegularDeltaDescription(BitBuffer& msg);
		void readRegularClientData(BitBuffer& msg);
		void readRegularPings(BitBuffer& msg);
		void readRegularEventReliable(BitBuffer& msg);
		void readRegularSpawnBaseline(BitBuffer& msg);
		void readRegularTempEntity(BitBuffer& msg);
		void readRegularSignonNum(BitBuffer& msg);
		void readRegularStaticSound(BitBuffer& msg);
		void readRegularCDTrack(BitBuffer& msg);
		void readRegularWeaponAnim(BitBuffer& msg);
		void readRegularDecalName(BitBuffer& msg);
		void readRegularRoomType(BitBuffer& msg);
		void readRegularUserMsg(BitBuffer& msg);
		void readRegularPacketEntities(BitBuffer& msg, bool delta);
		void readRegularResourceList(BitBuffer& msg);
		void readRegularMoveVars(BitBuffer& msg);
		void readRegularResourceRequest(BitBuffer& msg);
		void readFileTxferFailed(BitBuffer& msg);
		void readRegularHLTV(BitBuffer& msg);
		void readRegularDirector(BitBuffer& msg);
		void readRegularVoiceInit(BitBuffer& msg);
		void readRegularSendExtraInfo(BitBuffer& msg);
		void readRegularResourceLocation(BitBuffer& msg);
		void readRegularCVarValue(BitBuffer& msg);
		void readRegularCVarValue2(BitBuffer& msg);
#pragma endregion

#pragma region write
	private:
		void writeRegularMessages(BitBuffer& msg);

		void writeRegularMove(BitBuffer& msg);
		void writeRegularDelta(BitBuffer& msg);
		void writeRegularFileConsistency(BitBuffer& msg);
#pragma endregion

	private: // svc_serverinfo // TODO: make as structure
		int32_t mSpawnCount;
		int32_t mMapCrc;
		int32_t mMaxPlayers;
		
	public:
		auto getIndex() const { return mIndex; }
		const auto& getHostname() const { return mHostname; }
		const auto& getMap() const { return mMap; }
		const auto& getGameDir() const { return mGameDir; }

	private:
		int32_t mIndex;
		std::string mHostname;
		std::string mMap;
		std::string mGameDir;
	
	private:
		void onConnect(CON_ARGS);
		void onDisconnect(CON_ARGS);
		void onRetry(CON_ARGS);
		void onCmd(CON_ARGS);
		void onFullServerInfo(CON_ARGS);

	//protected:
	//	virtual std::vector<std::pair<std::string, std::string>> getProtInfo() = 0;

	//protected:
	//	virtual std::vector<uint8_t> generateCertificate() { return { }; } // TODO: add setCertificate(), add mCertificate, add getCertificate()

	public:
		auto getState() const { return mState; }
	//	auto& getUserInfo() { return mUserInfo; }
		const auto& getEntities() const { return mEntities; }
		const auto& getBaselines() const { return mBaselines; }
		const auto& getClientData() const { return mClientData; }
		const auto& getPlayerUserInfos() const { return mPlayerUserInfos; }

		const auto& getChannel() const { return mChannel; }

		const auto& getResources() const { return mResources; }

		const auto& getProtInfo() const { return mProtInfo; }
		void setProtInfo(const std::map<std::string, std::string>& value) { mProtInfo = value; }

		const auto& getCertificate() const { return mCertificate; }
		void setCertificate(const std::vector<uint8_t>& value) { mCertificate = value; }

	private:
		std::optional<Network::Address> mServerAdr; 
		State mState = State::Disconnected;
		std::map<std::string, std::string> mProtInfo;
		//std::vector<std::pair<std::string, std::string>> mUserInfo; // setinfo
		std::vector<uint8_t> mCertificate = { };

		std::optional<Channel> mChannel;


		std::list<std::string> mDownloadQueue;
		bool m_ResourcesVerifying = false;
		bool m_ResourcesVerified = false;
		bool m_ConfirmationRequired = false;





		Delta mDelta;
		std::map<uint8_t, Protocol::GameMessage> mGameMessages;

	public:
		void setReadGameMessageCallback(ReadGameMessageCallback callback) { mReadGameMessageCallback = callback; }
		void setResourceRequiredCallback(IsResourceRequiredCallback callback) { mIsResourceRequiredCallback = callback; }

	private:
		ReadGameMessageCallback mReadGameMessageCallback = nullptr;
		IsResourceRequiredCallback mIsResourceRequiredCallback = nullptr;

	private:
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
		

	public:
		void setThinkCallback(ThinkCallback value) { mThinkCallback = value; }
		void setDisconnectCallback(DisconnectCallback value) { mDisconnectCallback = value; }

	private:
		ThinkCallback mThinkCallback = nullptr;
		DisconnectCallback mDisconnectCallback = nullptr;

		uint8_t mSignonNum = 0;



		uint8_t mDeltaSequence;



		void verifyResources();

		virtual bool isResourceRequired(const Protocol::Resource& resource);
		virtual int getResourceHash(const Protocol::Resource& resource);

		void signon(uint8_t num);

	public:
		void sendCommand(const std::string& command);
		void connect(const Network::Address& address);
		void disconnect(const std::string& reason);
	
	public:
		bool isPlayerIndex(int value) const;

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
	};
}