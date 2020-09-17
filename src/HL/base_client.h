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
		void readRegularMessages(Common::BitBuffer& msg);
		void readRegularGameMessage(Common::BitBuffer& msg, uint8_t index);
		void receiveFile(std::string_view fileName, Common::BitBuffer& msg);

		void readRegularDisconnect(Common::BitBuffer& msg);
		void readRegularEvent(Common::BitBuffer& msg);
		void readRegularVersion(Common::BitBuffer& msg);
		void readRegularSetView(Common::BitBuffer& msg);
		void readRegularSound(Common::BitBuffer& msg);
		void readRegularTime(Common::BitBuffer& msg);
		void readRegularPrint(Common::BitBuffer& msg);
		void readRegularStuffText(Common::BitBuffer& msg);
		void readRegularAngle(Common::BitBuffer& msg);
		void readRegularServerInfo(Common::BitBuffer& msg);
		void readRegularLightStyle(Common::BitBuffer& msg);
		void readRegularUserInfo(Common::BitBuffer& msg);
		void readRegularDeltaDescription(Common::BitBuffer& msg);
		void readRegularClientData(Common::BitBuffer& msg);
		void readRegularPings(Common::BitBuffer& msg);
		void readRegularEventReliable(Common::BitBuffer& msg);
		void readRegularSpawnBaseline(Common::BitBuffer& msg);
		void readRegularTempEntity(Common::BitBuffer& msg);
		void readRegularSignonNum(Common::BitBuffer& msg);
		void readRegularStaticSound(Common::BitBuffer& msg);
		void readRegularCDTrack(Common::BitBuffer& msg);
		void readRegularWeaponAnim(Common::BitBuffer& msg);
		void readRegularDecalName(Common::BitBuffer& msg);
		void readRegularRoomType(Common::BitBuffer& msg);
		void readRegularUserMsg(Common::BitBuffer& msg);
		void readRegularPacketEntities(Common::BitBuffer& msg, bool delta);
		void readRegularResourceList(Common::BitBuffer& msg);
		void readRegularMoveVars(Common::BitBuffer& msg);
		void readRegularResourceRequest(Common::BitBuffer& msg);
		void readFileTxferFailed(Common::BitBuffer& msg);
		void readRegularHLTV(Common::BitBuffer& msg);
		void readRegularDirector(Common::BitBuffer& msg);
		void readRegularVoiceInit(Common::BitBuffer& msg);
		void readRegularSendExtraInfo(Common::BitBuffer& msg);
		void readRegularResourceLocation(Common::BitBuffer& msg);
		void readRegularCVarValue(Common::BitBuffer& msg);
		void readRegularCVarValue2(Common::BitBuffer& msg);
#pragma endregion

#pragma region write
	private:
		void writeRegularMessages(Common::BitBuffer& msg);

		void writeRegularMove(Common::BitBuffer& msg);
		void writeRegularDelta(Common::BitBuffer& msg);
		void writeRegularFileConsistency(Common::BitBuffer& msg);
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

		auto& getChannel() const { return mChannel; }

		const auto& getResources() const { return mResources; }

		const auto& getProtInfo() const { return mProtInfo; }
		void setProtInfo(const std::vector<std::pair<std::string, std::string>>& value) { mProtInfo = value; }

		const auto& getCertificate() const { return mCertificate; }
		void setCertificate(const std::vector<uint8_t>& value) { mCertificate = value; }

	private:
		Network::Address mServerAdr;
		State mState;
		std::vector<std::pair<std::string, std::string>> mProtInfo;
		//std::vector<std::pair<std::string, std::string>> mUserInfo; // setinfo
		std::vector<uint8_t> mCertificate = { };

		Channel mChannel;


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

		std::map<int, Protocol::Entity*> mEntities;
		std::map<int, Protocol::Entity> mDeltaEntities;
		std::map<int, Protocol::Entity> mBaselines;
		std::map<int, Protocol::Entity> mExtraBaselines;

		std::map<int, std::string> mPlayerUserInfos;

		bool mHLTV = false;
		

	public:
		void setThinkCallback(ThinkCallback value) { mThinkCallback = value; }

	private:
		ThinkCallback mThinkCallback = nullptr;


		uint8_t mSignonNum = 0;



		uint8_t m_DeltaSequence;



		void verifyResources();

		virtual bool isResourceRequired(const Protocol::Resource& resource);
		virtual int getResourceHash(const Protocol::Resource& resource);

	public:
		void sendCommand(std::string_view command);
	
	public:
		bool isPlayerIndex(int value) const;

	private: // userinfos // TODO: make better
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
		std::list<std::pair<std::string, Console::CVar::Getter>> mUserInfos;
	};
}