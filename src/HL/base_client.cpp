#include "base_client.h"

#include "protocol.h"
#include <HL/md5.h>
#include <Common/buffer_helpers.h>

//#include <filesystem> // TODO: not working on android

#include <Common/size_converter.h>

#include "encoder.h"

#include <cassert>
#include <algorithm>

#include <Platform/defines.h>
#include <Platform/asset.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

using namespace HL;

BaseClient::BaseClient(bool hltv) : Networking(),
	mHLTV(hltv),
	mChannel(getSocket(),
		[&](Common::BitBuffer& msg) { readRegularMessages(msg); },
		[&](Common::BitBuffer& msg) { writeRegularMessages(msg); },
		[&](std::string_view name, Common::BitBuffer& msg) { receiveFile(name, msg); })
{
	CONSOLE->registerCommand("connect", "connect to the server", { "address" },
		CMD_METHOD(onConnect));

	CONSOLE->registerCommand("disconnect", "disconnect from server",
		CMD_METHOD(onDisconnect));

	CONSOLE->registerCommand("cmd", "send command to server",
		CMD_METHOD(onCmd));

	CONSOLE->registerCommand("fullserverinfo", "receiving from server", { "text" },
		CMD_METHOD(onFullServerInfo));

	/*

	TODO: original hltv can send userinfos:

	20:14:52 - SV_ConnectionLessPacket : connect 48 199149200 "\prot\2\unique\-1\raw\
		861078331b85a424935805ca54f82891" "\name\HLTV Proxy\cl_lw\1\cl_lc\1\*hltv\1\
		rate\10000\cl_updaterate\20\hspecs\0\hslots\0\hdelay\30"#10

	*/

	addUserInfo("cl_dlmax", "Client Maximal Fragment Size", 
		CVAR_GETTER(mUserInfoDLMax),
		CVAR_SETTER(mUserInfoDLMax = CON_ARG(0)));

	addUserInfo("cl_lc", "Client Lag Compensation", 
		CVAR_GETTER(mUserInfoLC),
		CVAR_SETTER(mUserInfoLC = CON_ARG(0)));

	addUserInfo("cl_lw", "Client Weapon Prediction",
		CVAR_GETTER(mUserInfoLW),
		CVAR_SETTER(mUserInfoLW = CON_ARG(0)));

	addUserInfo("cl_updaterate", "Client Update Rate", 
		CVAR_GETTER(mUserInfoUpdaterate),
		CVAR_SETTER(mUserInfoUpdaterate = CON_ARG(0)));

	addUserInfo("model", "Client Model", 
		CVAR_GETTER(mUserInfoModel),
		CVAR_SETTER(mUserInfoModel = CON_ARG(0)));

	addUserInfo("rate", "Client Rate",  
		CVAR_GETTER(mUserInfoRate),
		CVAR_SETTER(mUserInfoRate = CON_ARG(0)));

	addUserInfo("topcolor", "Client Top Color", 
		CVAR_GETTER(mUserInfoTopColor),
		CVAR_SETTER(mUserInfoTopColor = CON_ARG(0)));

	addUserInfo("bottomcolor", "Client Bottom Color", 
		CVAR_GETTER(mUserInfoBottomColor),
		CVAR_SETTER(mUserInfoBottomColor = CON_ARG(0)));

	addUserInfo("name", "Client Name", 
		CVAR_GETTER(mUserInfoName),
		CVAR_SETTER(mUserInfoName = CON_ARG(0)));

	addUserInfo("password", "Client Password", 
		CVAR_GETTER(mUserInfoPassword),
		CVAR_SETTER(mUserInfoPassword = CON_ARG(0)));
}
BaseClient::~BaseClient()
{
	//
}

#pragma region read
void BaseClient::readConnectionlessPacket(Network::Packet& packet)
{
	switch (static_cast<Protocol::Server::ConnectionlessPacket>(packet.buf.read<uint8_t>()))
	{
	case Protocol::Server::ConnectionlessPacket::Print:
		break;

	case Protocol::Server::ConnectionlessPacket::Challenge:
		readConnectionlessChallenge(packet);
		break;

	case Protocol::Server::ConnectionlessPacket::Accepted:
		readConnectionlessAccepted(packet);
		break;

	case Protocol::Server::ConnectionlessPacket::Password:
		break;

	case Protocol::Server::ConnectionlessPacket::Reject:
		readConnectionlessReject(packet);
		break;

	case Protocol::Server::ConnectionlessPacket::Info_Old:
		break;

	case Protocol::Server::ConnectionlessPacket::Info_New:
		break;

	case Protocol::Server::ConnectionlessPacket::Players:
		break;

	case Protocol::Server::ConnectionlessPacket::Rules:
		break;
	}
}

void BaseClient::readRegularPacket(Network::Packet& packet)
{
	if (mState < State::Connected)
		return;

	if (mServerAdr != packet.adr)
		return;

	mChannel.process(packet.buf);
}

void BaseClient::readConnectionlessChallenge(Network::Packet& packet)
{
	if (mState != State::Challenging)
		return;

	if (mServerAdr != packet.adr)
		return;

	Network::Packet pack;

	pack.adr = packet.adr;

	auto challenge = Console::System::MakeTokensFromString(Common::BufferHelpers::ReadString(packet.buf))[1];

	auto ss = "connect " + std::to_string(Protocol::Version) + " " + challenge + " \"";

	for (auto& s : getProtInfo())
		ss += "\\" + s.first + "\\" + s.second;

	ss += "\" \"";

	for (auto& [name, callback] : mUserInfos)
	{
		auto value = callback()[0];

		if (value.empty())
			continue;

		ss += "\\" + name + "\\" + value;
	}

	ss += "\"";
		
	Common::BufferHelpers::WriteString(pack.buf, ss);
		
	pack.buf.write(mCertificate.data(), mCertificate.size());
		
	sendConnectionlessPacket(pack);

	mState = State::Connecting;
}

void BaseClient::readConnectionlessAccepted(Network::Packet& packet)
{
	if (mState != State::Connecting)
		return;

	if (mServerAdr != packet.adr)
		return;

	mChannel.setActive(true);
	mChannel.setAddress(mServerAdr);

	CONSOLE_DEVICE->writeLine("connection accepted");

	mState = State::Connected;

	sendCommand("new");
}

void BaseClient::readConnectionlessReject(Network::Packet& packet)
{
	CONSOLE_DEVICE->writeLine("connection rejected, reason: " + Common::BufferHelpers::ReadString(packet.buf));
}

void BaseClient::readRegularMessages(Common::BitBuffer& msg)
{
	while (msg.hasRemaining())
	{
		auto index = msg.read<uint8_t>();
			
		if (index > 64)
		{
			readRegularGameMessage(msg, index);
			continue;
		}

		switch (static_cast<Protocol::Server::Message>(index))
		{
		//case Protocol::Server::Message::Bad:
			// disconnect
			//	break;

		case Protocol::Server::Message::Nop:
			break;

		case Protocol::Server::Message::Disconnect:
			readRegularDisconnect(msg);
			break;

		case Protocol::Server::Message::Event:
			readRegularEvent(msg);
			break;
		
		case Protocol::Server::Message::Version:
			readRegularVersion(msg);
			break;

		case Protocol::Server::Message::SetView:
			readRegularSetView(msg);
			break;

		case Protocol::Server::Message::Sound:
			readRegularSound(msg);
			break;

		case Protocol::Server::Message::Time:
			readRegularTime(msg);
			break;

		case Protocol::Server::Message::Print:
			readRegularPrint(msg);
			break;

		case Protocol::Server::Message::StuffText:
			readRegularStuffText(msg);
			break;

		case Protocol::Server::Message::SetAngle:
			readRegularAngle(msg);
			break;

		case Protocol::Server::Message::ServerInfo:
			readRegularServerInfo(msg);
			break;

		case Protocol::Server::Message::LightStyle:
			readRegularLightStyle(msg);
			break;

		case Protocol::Server::Message::UpdateUserinfo:
			readRegularUserInfo(msg);
			break;

		case Protocol::Server::Message::DeltaDescription:
			readRegularDeltaDescription(msg);
			break;

		case Protocol::Server::Message::ClientData:
			readRegularClientData(msg);
			break;

		/*
		case Protocol::Server::Message::StopSound:
		*/
			
		case Protocol::Server::Message::Pings:
			readRegularPings(msg);
			break;

		/*
		case Protocol::Server::Message::Particle:
		case Protocol::Server::Message::Damage:
		case Protocol::Server::Message::SpawnStatic:
		*/

		case Protocol::Server::Message::EventReliable:
			readRegularEventReliable(msg);
			break;

		case Protocol::Server::Message::SpawnBaseline:
			readRegularSpawnBaseline(msg);
			break;

		case Protocol::Server::Message::TempEntity:
			readRegularTempEntity(msg);
			break;

		/*
		case Protocol::Server::Message::SetPause:
		*/

		case Protocol::Server::Message::SignonNum:
			readRegularSignonNum(msg);
			break;

		/*
		case Protocol::Server::Message::CenterPrint:
		case Protocol::Server::Message::KilledMonster:
		case Protocol::Server::Message::FoundSecret:
		*/

		case Protocol::Server::Message::SpawnStaticSound:
			readRegularStaticSound(msg);
			break;
		/*
		case Protocol::Server::Message::Intermission:
		case Protocol::Server::Message::Finale:
		*/

		case Protocol::Server::Message::CDTrack:
			readRegularCDTrack(msg);
			break;

		/*
		case Protocol::Server::Message::Restore:
		case Protocol::Server::Message::CutScene:
		*/
				
		case Protocol::Server::Message::WeaponAnim:
			readRegularWeaponAnim(msg);
			break;

		case Protocol::Server::Message::DecalName:
			readRegularDecalName(msg);
			break;

		case Protocol::Server::Message::RoomType:
			readRegularRoomType(msg);
			break;

		/*case Protocol::Server::Message::AddAngle:
		*/

		case Protocol::Server::Message::NewUserMsg:
			readRegularUserMsg(msg);
			break;

		case Protocol::Server::Message::PacketEntities:
			readRegularPacketEntities(msg, false);
			break;

		case Protocol::Server::Message::DeltaPacketEntities:
			readRegularPacketEntities(msg, true);
			break;

		case Protocol::Server::Message::Choke:
			break;

		case Protocol::Server::Message::ResourceList:
			readRegularResourceList(msg);
			break;

		case Protocol::Server::Message::NewMoveVars:
			readRegularMoveVars(msg);
			break;

		case Protocol::Server::Message::ResourceRequest:
			readRegularResourceRequest(msg);
			break;
		/*
		case Protocol::Server::Message::Customization:
		case Protocol::Server::Message::CrosshairAngle:
		case Protocol::Server::Message::SoundFade:		*/

		case Protocol::Server::Message::FileTxferFailed: 
			readFileTxferFailed(msg);
			break;
		
		case Protocol::Server::Message::HLTV:
			readRegularHLTV(msg);
			break;
			
		case Protocol::Server::Message::Director:
			readRegularDirector(msg);
			break;
				
		case Protocol::Server::Message::VoiceInit:
			readRegularVoiceInit(msg);
			break;

		/*
		case Protocol::Server::Message::VoiceData:
		*/
			
		case Protocol::Server::Message::SendExtraInfo:
			readRegularSendExtraInfo(msg);
			break;
		/*
		case Protocol::Server::Message::TimeScale:
		*/

		case Protocol::Server::Message::ResourceLocation:
			readRegularResourceLocation(msg);
			break;

		case Protocol::Server::Message::SendCVarValue:
			readRegularCVarValue(msg);
			break;

		case Protocol::Server::Message::SendCVarValue2:
			readRegularCVarValue2(msg);
			break;

		default:
			// disconnect here, parsing error
			CONSOLE_DEVICE->writeLine("unknown svc: " + std::to_string(index), Console::Color::Red);
			return;
		}
	}
}

void BaseClient::readRegularGameMessage(Common::BitBuffer& msg, uint8_t index)
{
	assert(mGameMessages.count(index) != 0);

	if (mGameMessages.count(index) == 0)
		return;

	const auto& gmsg = mGameMessages.at(index);
	auto size = gmsg.size;

	if (size == 255)
		size = msg.read<uint8_t>();

	Common::BitBuffer buf;
	buf.setSize(size);
	msg.read(buf.getMemory(), size);

	if (mReadGameMessageCallback)
	{
		mReadGameMessageCallback(gmsg.name, buf.getMemory(), buf.getSize());
	}
}

void BaseClient::receiveFile(std::string_view fileName, Common::BitBuffer& msg)
{
	Platform::Asset::Write(mGameDir + "/" + std::string(fileName), msg.getMemory(), msg.getSize());
	mDownloadQueue.remove_if([fileName](auto a) { return a == fileName; });

	CONSOLE_DEVICE->writeLine("received: \"" + std::string(fileName) + "\", size: " +
		Common::SizeConverter::ToString(msg.getSize()) /*+ ", Left: " + std::to_string(mDownloadQueue.size())*/);
}

void BaseClient::readRegularDisconnect(Common::BitBuffer& msg)
{
	auto reason = Common::BufferHelpers::ReadString(msg);
	CONSOLE_DEVICE->writeLine(reason);
}

void BaseClient::readRegularEvent(Common::BitBuffer& msg)
{
	auto count = msg.readBits(5);

	for (uint32_t i = 0; i < count; i++)
	{
		Protocol::Event evt;

		evt.packet_index = -1;
		evt.entity_index = -1;

		evt.index = msg.readBits(10);

		if (msg.readBit()) 
		{
			evt.packet_index = msg.readBits(11);

			if (msg.readBit())
				mDelta.readEvent(msg, evt.args);
		}

		if (msg.readBit())
			evt.fire_time = static_cast<float>(msg.readBits(16));

		// onEvent(evt); // TODO: do it
	}

	msg.alignByteBoundary();
}

void BaseClient::readRegularVersion(Common::BitBuffer& msg)
{
	auto version = msg.read<int32_t>();
}

void BaseClient::readRegularSetView(Common::BitBuffer& msg)
{
	int16_t value = msg.read<int16_t>();
}

void BaseClient::readRegularSound(Common::BitBuffer& msg)
{
	Protocol::Sound sound;

	sound.volume = Protocol::VOL_NORM;
	sound.attenuation = Protocol::ATTN_NORM;
	sound.pitch = Protocol::PITCH_NORM;

	sound.flags = msg.readBits(9);

	if (sound.flags & Protocol::SND_VOLUME) 
		sound.volume = msg.readBits(8);

	if (sound.flags & Protocol::SND_ATTN)
			sound.attenuation = static_cast<float>(msg.readBits(8)); // TODO: confirm static cast

	sound.channel = msg.readBits(3); // if chan = 6 then static sound ? (from ida)
	sound.entity = msg.readBits(11);

	if (sound.flags & Protocol::SND_LONG_INDEX)
		sound.index = msg.readBits(16);
	else
		sound.index = msg.readBits(8);

	Common::BufferHelpers::ReadBitVec3(msg, sound.origin);

	if (sound.flags & Protocol::SND_PITCH)
		sound.pitch = msg.readBits(8);
		  
	msg.alignByteBoundary();

	// onSound(sound); // TODO: event
}

void BaseClient::readRegularTime(Common::BitBuffer& msg)
{
	mTime = msg.read<float>();
}

void BaseClient::readRegularPrint(Common::BitBuffer& msg)
{
	auto text = Common::BufferHelpers::ReadString(msg);
	CONSOLE_DEVICE->writeLine(text);
}

void BaseClient::readRegularStuffText(Common::BitBuffer& msg)
{
	auto text = Common::BufferHelpers::ReadString(msg);
	auto cmds = Console::System::ParseCommandLine(text);

	for (const auto& cmd : cmds)
	{
		auto args = Console::System::MakeTokensFromString(cmd);

		if (args.size() == 0)
			continue;

		auto name = args[0];

		std::transform(name.begin(), name.end(), name.begin(), tolower);

		if (name.empty())
			continue;

		bool shouldFeedback = CONSOLE->getCommands().count(name) == 0 && 
			CONSOLE->getAliases().count(name) == 0 && CONSOLE->getCVars().count(name) == 0;

		if (shouldFeedback)
			sendCommand(cmd);
		else
			CONSOLE->execute(cmd);
	}
}

void BaseClient::readRegularAngle(Common::BitBuffer& msg)
{
	float x = Common::BufferHelpers::ReadHiResAngle(msg);
	float y = Common::BufferHelpers::ReadHiResAngle(msg);
	float z = Common::BufferHelpers::ReadHiResAngle(msg);
}

void BaseClient::readRegularServerInfo(Common::BitBuffer& msg)
{
	int protocol = msg.read<int32_t>(); // must be Protocol.Version
	mSpawnCount = msg.read<int32_t>();
	mMapCrc = msg.read<int32_t>();

	uint8_t crc[16];
	msg.read(&crc, 16);

	mMaxPlayers = (int32_t)msg.read<uint8_t>();

	// SetLength(Players, MaxPlayers);

	mIndex = (int32_t)msg.read<uint8_t>();

	Encoder::UnMunge3(&mMapCrc, 4, (-1 - mIndex) & 0xFF);

	msg.read<uint8_t>(); // UInt((coop.Value = 0) and (deathmatch.Value <> 0))

	mGameDir = Common::BufferHelpers::ReadString(msg);
	mHostname = Common::BufferHelpers::ReadString(msg);
	mMap = Common::BufferHelpers::ReadString(msg);
	auto mapList = Common::BufferHelpers::ReadString(msg);

	msg.seek(1);

	/*if not (Protocol in [46..48]) then
	begin
		FinalizeConnection('Server is using unsupported protocol version: ' + Protocol.ToString);
		Exit;
	end;
	*/

	//...InitializeGameEngine;
	//Delta.Initialize;
	//Engine.Execute('sendres');

	// onGameInitialize(); // TODO: was uncommented

	sendCommand("sendres");
}

void BaseClient::readRegularLightStyle(Common::BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();

	if (index + 1 > m_LightStyles.size())
		m_LightStyles.resize(index + 1);

	m_LightStyles[index] = Common::BufferHelpers::ReadString(msg);
}

void BaseClient::readRegularUserInfo(Common::BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto userId = msg.read<int32_t>();
	auto info = Common::BufferHelpers::ReadString(msg);
	uint8_t hash[16];
	msg.read(&hash, 16);

	mPlayerUserInfos[index] = info;
}

void BaseClient::readRegularDeltaDescription(Common::BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	auto fieldsCount = msg.readBits(16);
	mDelta.add(msg, name, fieldsCount);
	msg.alignByteBoundary();
}

void BaseClient::readRegularClientData(Common::BitBuffer& msg)
{
	if (mHLTV)
		return;

	if (msg.readBit())
		msg.read<uint8_t>(); // delta sequence

	mDelta.readClientData(msg, mClientData);

	while (msg.readBit())
	{
		auto index = msg.readBits(6); // 5 bits if protocol < 47

		if (index + 1 > m_WeaponData.size())
			m_WeaponData.resize(index + 1);

		mDelta.readWeaponData(msg, m_WeaponData[index]);
	}

	msg.alignByteBoundary();
}

void BaseClient::readRegularPings(Common::BitBuffer& msg)
{
	while (msg.readBit())
	{
		auto index = msg.readBits(5);
		auto ping = msg.readBits(12);
		auto loss = msg.readBits(7);
	}
	msg.alignByteBoundary();
}

void BaseClient::readRegularEventReliable(Common::BitBuffer& msg)
{
	Protocol::Event evt;

	evt.packet_index = -1;
	evt.entity_index = -1;

	evt.index = msg.readBits(10);

	mDelta.readEvent(msg, evt.args);

	if (msg.readBit())
		evt.fire_time = static_cast<float>(msg.readBits(16)); // TODO: confirm static cast

	msg.alignByteBoundary();

	// onEvent(evt);
}

void BaseClient::readRegularSpawnBaseline(Common::BitBuffer& msg)
{
	while (msg.read<uint16_t>() != 0xFFFF)
	{
		msg.seek(-2);

		auto index = msg.readBits(11);
		auto& entity = mBaselines[index];

		if (msg.readBits(2) & (int)Protocol::EntityType::Beam)
			mDelta.readEntityCustom(msg, entity);
		else if (isPlayerIndex(index))
			mDelta.readEntityPlayer(msg, entity);
		else
			mDelta.readEntityNormal(msg, entity);
	}

	//mExtraBaselines.resize(msg.readBits(6));

	//for (auto& extra : mExtraBaselines)
	//	mDelta.readEntityNormal(msg, extra);

	for (int i = 0; i < msg.readBits(6); i++)
		mDelta.readEntityNormal(msg, mExtraBaselines[i]);

	msg.alignByteBoundary();

	if (mBaselines.size() > 0)
		CONSOLE_DEVICE->writeLine(std::to_string(mBaselines.size()) + " baseline entities received");

	if (mExtraBaselines.size() > 0)
		CONSOLE_DEVICE->writeLine(std::to_string(mExtraBaselines.size()) + " extra baseline entities received");

	mDeltaEntities = mBaselines;
}

void BaseClient::readRegularTempEntity(Common::BitBuffer& msg)
{
	static const int TE_MAX = 128;

	static const int TE_LENGTH[TE_MAX] =
	{
		24, 20,  6, 11,  6, 10, 12, 17, 16,  6,  6,  6,  8, -1,  9, 19,
		-2, 10, 16, 24, 24, 24, 10, 11, 16, 19, -2, 12, 16, -1, 19, 17,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2,
		-2, -2, -2,  2, 10, 14, 12, 14,  9,  5, 17, 13, 24,  9, 17,  7,
		10, 19, 19, 12,  7,  7,  9, 16, 18,  6, 10, 13,  7,  1, 18, 15,
	};

	int type = (int)msg.read<uint8_t>();

	int length = TE_LENGTH[type];

	if (length == -1)
	{
		if (type == (int)Protocol::TempEntity::BspDecal)
		{
			msg.seek(8);

			if (msg.read<uint16_t>())
				msg.seek(2);
		}
		else if (type == (int)Protocol::TempEntity::TextMessage)
		{
			msg.seek(5);

			if (msg.read<uint8_t>() == 2)
				msg.seek(2);

			msg.seek(14);
			Common::BufferHelpers::ReadString(msg);
		}
	}
	else
	{
		msg.seek(length);
	}

	return;

	switch ((Protocol::TempEntity)msg.read<uint8_t>())
	{
	case Protocol::TempEntity::BeamPoints:;
	case Protocol::TempEntity::BeamEntPoint:;
	case Protocol::TempEntity::GunShot:;
	case Protocol::TempEntity::Explosion:;
	case Protocol::TempEntity::TarExplosion:;
	case Protocol::TempEntity::Smoke:;
	case Protocol::TempEntity::Tracer:;
	case Protocol::TempEntity::Lightning:;
	case Protocol::TempEntity::BeamEnts:;
	case Protocol::TempEntity::Sparks:;
	case Protocol::TempEntity::LavaSplash:;
	case Protocol::TempEntity::Teleport:;
	case Protocol::TempEntity::Explosion2:;
	case Protocol::TempEntity::BspDecal:;
	case Protocol::TempEntity::Implosion:;
	case Protocol::TempEntity::SpriteTrail:;
	case Protocol::TempEntity::Sprite:;
	case Protocol::TempEntity::BeamSprite:;
	case Protocol::TempEntity::BeamTorus:;
	case Protocol::TempEntity::BeamDisk:;
	case Protocol::TempEntity::BeamCylinder:;
	case Protocol::TempEntity::BeamFollow:;
	case Protocol::TempEntity::GlowSprite:;
	case Protocol::TempEntity::BeamRing:;
	case Protocol::TempEntity::StreakSplash:;
	case Protocol::TempEntity::DLight:;
	case Protocol::TempEntity::ELight:;
	case Protocol::TempEntity::TextMessage:;
	case Protocol::TempEntity::Line:;
	case Protocol::TempEntity::Box:;
	case Protocol::TempEntity::KillBeam:;
	case Protocol::TempEntity::LargeFunnel:;
	case Protocol::TempEntity::BloodStream:;
	case Protocol::TempEntity::ShowLine:;
	case Protocol::TempEntity::Blood:;
	case Protocol::TempEntity::Decal:;
	case Protocol::TempEntity::Fizz:;
	case Protocol::TempEntity::Model:;
	case Protocol::TempEntity::ExplodeModel:;
	case Protocol::TempEntity::BreakModel:;
	case Protocol::TempEntity::GunshotDecal:;
	case Protocol::TempEntity::SpriteSpray:;
	case Protocol::TempEntity::ArmorRicochet:;
	case Protocol::TempEntity::PlayerDecal:;
	case Protocol::TempEntity::Bubbles:;
	case Protocol::TempEntity::BubbleTrail:;
	case Protocol::TempEntity::BloodSprite:;
	case Protocol::TempEntity::WorldDecal:;
	case Protocol::TempEntity::WorldDecalHigh:;
	case Protocol::TempEntity::DecalHigh:;
	case Protocol::TempEntity::Projectile:;
	case Protocol::TempEntity::Spray:;
	case Protocol::TempEntity::PlayerSprites:;
	case Protocol::TempEntity::ParticleBurst:;
	case Protocol::TempEntity::FireField:;
	case Protocol::TempEntity::PlayerAttachment:;
	case Protocol::TempEntity::KillPlayerAttachments:;
	case Protocol::TempEntity::MultiGunshot:;
	case Protocol::TempEntity::UserTracer:;
	}
}

void BaseClient::readRegularSignonNum(Common::BitBuffer& msg)
{
	// if peek uint8 < current signon then disconnect

	mSignonNum = msg.read<uint8_t>();

	if (mSignonNum == 1)
		sendCommand("sendents");
}

void BaseClient::readRegularStaticSound(Common::BitBuffer& msg)
{
	Protocol::Sound sound;
		
	sound.origin[0] = Common::BufferHelpers::ReadCoord(msg);
	sound.origin[1] = Common::BufferHelpers::ReadCoord(msg);
	sound.origin[2] = Common::BufferHelpers::ReadCoord(msg);

	sound.index = msg.read<int16_t>();
	sound.volume = msg.read<uint8_t>();
	sound.attenuation = msg.read<uint8_t>();
	sound.entity = msg.read<int16_t>();
	sound.pitch = msg.read<uint8_t>();
	sound.flags = msg.read<uint8_t>();

	// onSound(sound); // TODO: it is ambient sound, lua code need to know this
}

void BaseClient::readRegularCDTrack(Common::BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto loop = msg.read<uint8_t>();
}

void BaseClient::readRegularWeaponAnim(Common::BitBuffer& msg)
{
	auto seq = msg.read<uint8_t>();
	auto group = msg.read<uint8_t>();
}

void BaseClient::readRegularDecalName(Common::BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto name = Common::BufferHelpers::ReadString(msg);

	CONSOLE_DEVICE->writeLine("DecalIndex: " + std::to_string(index) + ", DecalName: " + name, Console::Color::Red);
}

void BaseClient::readRegularRoomType(Common::BitBuffer& msg)
{
	auto room = msg.read<int16_t>();

	CONSOLE_DEVICE->writeLine("RoomType: " + std::to_string(room), Console::Color::Red);
}

void BaseClient::readRegularUserMsg(Common::BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	
	mGameMessages[index].size = msg.read<uint8_t>();

	char name[16];
	msg.read(&name, 16);
	
	mGameMessages[index].name = name;
}

void BaseClient::readRegularPacketEntities(Common::BitBuffer& msg, bool delta)
{
	if (mSignonNum == 1)
	{
		mSignonNum = 2;

		if (mHLTV)
		{
			sendCommand("spectate");
		}
	}

	auto readDeltaEntity = [this, &msg](int index, Protocol::Entity& entity, bool custom) {
		if (custom)
			mDelta.readEntityCustom(msg, entity);
		else if (isPlayerIndex(index))
			mDelta.readEntityPlayer(msg, entity);
		else
			mDelta.readEntityNormal(msg, entity);
	};

	auto readEntityIndex = [&msg](uint32_t& index) {
		if (msg.readBit())
			index = msg.readBits(11);
		else
			index += msg.readBits(6);
	};

	auto count = msg.read<uint16_t>();

	uint32_t index = 0;

	if (delta)
	{
		auto mask = msg.read<uint8_t>();

		m_DeltaSequence = mChannel.getIncomingSequence() & 0xFF; // TODO: only in delta ?

		while (msg.read<uint16_t>() != 0)
		{
			msg.seek(-2);

			bool remove = msg.readBit();

			readEntityIndex(index);
				
			if (remove)
			{
				if (mEntities.count(index) == 0)
				{
					CONSOLE_DEVICE->writeLine("trying to delete non existing entity " + std::to_string(index), Console::Color::Red);
				}
				mEntities.erase(index);
				continue;
			}

			auto& entity = mDeltaEntities[index];
			mEntities[index] = &entity;

			bool custom = msg.readBit();

			if (!mExtraBaselines.empty() && msg.readBit()) // TODO: confirm in tfc (does it has in delta packet)
			{
				auto extra_index = msg.readBits(6);
				assert(mExtraBaselines.count(extra_index) > 0);
				CONSOLE_DEVICE->writeLine("using extra baseline " + std::to_string(extra_index) +
					" for entity " + std::to_string(index));
				entity = mExtraBaselines[extra_index];
			}

			readDeltaEntity(index, entity, custom);
		}

		if (mEntities.size() != count)
			CONSOLE_DEVICE->writeLine("entities size mismatch: have " + std::to_string(mEntities.size()) + ", must be " + std::to_string(count), Console::Color::Red);
	}
	else
	{
		mEntities.clear();

		for (int i = 0; i < count; i++)
		{
			if (msg.readBit())
				index += 1;
			else
				readEntityIndex(index);

			auto& entity = mDeltaEntities[index];
			mEntities[index] = &entity;

			bool custom = msg.readBit();

			if (!mExtraBaselines.empty() && msg.readBit()) // TODO: confirm in tfc
			{
				auto extra_index = msg.readBits(6);
				assert(mExtraBaselines.count(extra_index) > 0);
				CONSOLE_DEVICE->writeLine("using extra baseline " + std::to_string(extra_index) +
					" for entity " + std::to_string(index));
				entity = mExtraBaselines[extra_index];
			}
			else if (msg.readBit())
			{
				auto base_index = msg.readBits(6);
				assert(mBaselines.count(base_index) > 0);
				CONSOLE_DEVICE->writeLine("using baseline " + std::to_string(base_index) +
					" for entity " + std::to_string(index));
				entity = mBaselines[base_index];
			}

			readDeltaEntity(index, entity, custom);
		}

		msg.read<uint16_t>();
	}

	msg.alignByteBoundary();
}

void BaseClient::readRegularResourceList(Common::BitBuffer& msg)
{
	mResources.resize(msg.readBits(12));

	for (auto& resource : mResources)
	{
		resource.type = static_cast<Protocol::Resource::Type>(msg.readBits(4));
		resource.name = Common::BufferHelpers::ReadString(msg);
		resource.index = msg.readBits(12);
		resource.size = msg.readBits(24);
		resource.flags = msg.readBits(3);

		if (resource.flags & Protocol::RES_CUSTOM)
			msg.read(&resource.hash, 16);

		if (msg.readBit())
		{
			msg.read(&resource.reserved, 32);
			Encoder::UnMunge(&resource.reserved, 32, mSpawnCount);
			resource.flags |= Protocol::RES_RESERVED;
		}
	}

	m_ConfirmationRequired = msg.readBit();

	if (m_ConfirmationRequired)
	{
		uint32_t index = 0;

		while (msg.readBit())
		{
			if (msg.readBit())
				index += msg.readBits(5);
			else
				index = msg.readBits(10);
			
			mResources[index].flags |= Protocol::RES_CHECKFILE;
		}
	}

	msg.alignByteBoundary();

	CONSOLE_DEVICE->writeLine(std::to_string(mResources.size()) + " resources received");

	// fix sound directory

	for (auto& resource : mResources)
	{
		if (resource.type != Protocol::Resource::Type::Sound)
			continue;
			
		resource.name = "sound/" + resource.name;
	}

	m_ResourcesVerifying = true;
	m_ResourcesVerified = false;

	verifyResources();
}

void BaseClient::readRegularMoveVars(Common::BitBuffer& msg)
{
	float gravity = msg.read<float>();
	float stopSpeed = msg.read<float>();
	float maxSpeed = msg.read<float>();
	float spectatorMaxSpeed = msg.read<float>();
	float accelerate = msg.read<float>();
	float airAccelerate = msg.read<float>();
	float waterAccelerate = msg.read<float>();
	float friction = msg.read<float>();
	float edgeFriction = msg.read<float>();
	float waterFriction = msg.read<float>();
	float entGravity = msg.read<float>();
	float bounce = msg.read<float>();
	float stepSize = msg.read<float>();
	float maxVelocity = msg.read<float>();
	float zMax = msg.read<float>();
	float waveHeight = msg.read<float>();
	uint8_t footSteps = msg.read<uint8_t>();
	float rollAngle = msg.read<float>();
	float rollSpeed = msg.read<float>();
	float skyColorR = msg.read<float>();
	float skyColorG = msg.read<float>();
	float skyColorB = msg.read<float>();
	float skyVecX = msg.read<float>();
	float skyVecY = msg.read<float>();
	float skyVecZ = msg.read<float>();
	std::string skyName = Common::BufferHelpers::ReadString(msg);
}

void BaseClient::readRegularResourceRequest(Common::BitBuffer& msg)
{
	int spawncount = msg.read<int32_t>();
	msg.read<int32_t>(); // rehlds says that it will always 0

	// send clc_resourcelist here

	/*ServerNum: = MSG.Read<Int32>;

	if ServerNum = ServerInfo.SpawnCount then
	begin
		Range : = MSG.Read<Int32>;

		if Range = 0 then
		begin
			WriteRegularResourceList;
		Channel.CreateFragments(BF.ToArray, DEFAULT_FRAGMENT_SIZE, False);
	BF.Clear;
	end
	else
		raise Exception.Create('custom resource list request out of range (' + Range.ToString + ')');
	end
	else
		raise Exception.Create('Index (' + ServerNum.ToString + ') <> SpawnCount (' + ServerInfo.SpawnCount.ToString + ')');
		*/
}

void BaseClient::readFileTxferFailed(Common::BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	CONSOLE_DEVICE->writeLine("failed to download file: " + name, Console::Color::Red);
}

void BaseClient::readRegularHLTV(Common::BitBuffer& msg)
{
	CONSOLE_DEVICE->writeLine("SVC_HLTV was received", Console::Color::Red);
}

void BaseClient::readRegularDirector(Common::BitBuffer& msg)
{
	msg.seek(msg.read<uint8_t>());

	//mConsoleDevice.writeLine("SVC_DIRECTOR was received", Console::Color::Red);
}

void BaseClient::readRegularVoiceInit(Common::BitBuffer& msg)
{
	auto codec = Common::BufferHelpers::ReadString(msg);
	auto quality = msg.read<uint8_t>(); // if protocol > 46

	CONSOLE_DEVICE->writeLine("voice codec: " + codec, Console::Color::Red);
}

void BaseClient::readRegularSendExtraInfo(Common::BitBuffer& msg)
{
	auto fallbackDir = Common::BufferHelpers::ReadString(msg);
	auto allowCheats = msg.read<uint8_t>();
}

void BaseClient::readRegularResourceLocation(Common::BitBuffer& msg)
{
	auto downloadUrl = Common::BufferHelpers::ReadString(msg);
}

void BaseClient::readRegularCVarValue(Common::BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	CONSOLE_DEVICE->writeLine("SVC_SENDCVARVALUE: " + name, Console::Color::Red);
}

void BaseClient::readRegularCVarValue2(Common::BitBuffer& msg)
{
	auto id = msg.read<uint32_t>();
	auto name = Common::BufferHelpers::ReadString(msg);
	CONSOLE_DEVICE->writeLine("SVC_SENDCVARVALUE2: " + std::to_string(id) + ", " + name, Console::Color::Red);
}

#pragma endregion

#pragma region write
void BaseClient::writeRegularMessages(Common::BitBuffer& msg)
{
	if (m_ResourcesVerifying && mDownloadQueue.size() == 0)
	{
		m_ResourcesVerified = true;
		m_ResourcesVerifying = false;

		if (m_ConfirmationRequired)
			writeRegularFileConsistency(msg);
		else
			CONSOLE_DEVICE->writeLine("confirmation of resources isn't required");

		int crc = mMapCrc;
		Encoder::Munge2(&crc, 4, (-1 - mSpawnCount) & 0xFF);
		sendCommand("spawn " + std::to_string(mSpawnCount) + " " + std::to_string(crc));

		mChannel.createNormalFragments(/*128 bytes per fragment, without compression*/);
	}

	if (mSignonNum == 2)
	{
		if (!mHLTV)
		{
			writeRegularMove(msg);
		}
		writeRegularDelta(msg);
	}
}

void BaseClient::writeRegularMove(Common::BitBuffer& msg)
{
	size_t o = msg.getSize();

	msg.write<uint8_t>((uint8_t)Protocol::Client::Message::Move);
	msg.write<uint8_t>(0); // size
	msg.write<uint8_t>(0); // checksum
	msg.write<uint8_t>(0); // flags; send net_drops or bad clc_delta count ? 8th bit is voiceloopback flag, munge starts from here
	msg.write<uint8_t>(0); // backup count
	msg.write<uint8_t>(1); // cmd count

	Protocol::UserCmd old;
	memset(&old, 0, sizeof(old));
		
	HL::Protocol::UserCmd userCmd;
	memset(&userCmd, 0, sizeof(userCmd));
	
	if (mThinkCallback)
		mThinkCallback(userCmd);
	
	mDelta.writeUserCmd(msg, userCmd, old);

	msg.alignByteBoundary();

	*(uint8_t*)((size_t)msg.getMemory() + o + 1) = (uint8_t)(msg.getSize() - o - 3);
		
	*(uint8_t*)((size_t)msg.getMemory() + o + 2) = (uint8_t)Encoder::BlockSequenceCRCByte(
		(void*)((size_t)msg.getMemory() + o + 3), msg.getSize() - o - 3, mChannel.getOutgoingSequence());
		
	Encoder::Munge((void*)((size_t)msg.getMemory() + o + 3), msg.getSize() - o - 3,
		mChannel.getOutgoingSequence());
}
	
void BaseClient::writeRegularDelta(Common::BitBuffer& msg)
{
	msg.write<uint8_t>((uint8_t)Protocol::Client::Message::Delta);
	msg.write<uint8_t>(m_DeltaSequence);
}

void BaseClient::writeRegularFileConsistency(Common::BitBuffer& msg)
{
	size_t o = msg.getSize();

	msg.write<uint8_t>((uint8_t)Protocol::Client::Message::FileConsistency);
	msg.write<uint16_t>(0); // size

	int c = 0;
		
	for (int i = 0; i < mResources.size(); i++)
	{
		auto resource = mResources[i];

		if (!(resource.flags & Protocol::RES_CHECKFILE))
			continue;

		c++;

		msg.writeBit(true);
		msg.writeBits(i, 12);

		if (resource.flags & Protocol::RES_RESERVED) // thanks, valve
		{			
			msg.write((void*)((size_t)&resource.reserved + 1), 24);
		}
		else
		{
			msg.write<int32_t>(getResourceHash(resource));
		}
	}

	msg.writeBit(false);
	msg.alignByteBoundary();

	CONSOLE_DEVICE->writeLine(std::to_string(c) + " resources confirmed");
}
#pragma endregion



void BaseClient::onConnect(CON_ARGS)
{
	Network::Packet packet;

	try
	{
		packet.adr = Network::Address(CON_ARG(0));
	}
	catch (const std::exception& e)
	{
		CONSOLE_DEVICE->writeLine(e.what());
		return;
	}

	if (packet.adr.port == 0)
		packet.adr.port = Protocol::DefaultServerPort;

	mServerAdr = packet.adr;

	Common::BufferHelpers::WriteString(packet.buf, "getchallenge steam");
		
	sendConnectionlessPacket(packet);

	CONSOLE_DEVICE->writeLine("initializing connection to " + packet.adr.toString());

	mState = State::Challenging;
}

void BaseClient::onDisconnect(CON_ARGS)
{
	//
}

void BaseClient::onCmd(CON_ARGS)
{
	if (CON_ARGS_COUNT < 1)
		return;

	sendCommand(CON_ARG(0));
}

void BaseClient::onFullServerInfo(CON_ARGS)
{
	if (CON_ARGS_COUNT < 1)
		return;

	auto text = CON_ARG(0);
}






























void BaseClient::verifyResources()
{
	for (auto& resource : mResources)
	{
		if (resource.type != Protocol::Resource::Type::Model && 
			resource.type != Protocol::Resource::Type::Sound &&
			resource.type != Protocol::Resource::Type::Generic)
			continue;

		if (resource.type == Protocol::Resource::Type::Model && resource.name[0] == '*')
			continue;

		if (!isResourceRequired(resource))
			continue;

		mDownloadQueue.push_back(resource.name);
		sendCommand("dlfile " + resource.name);
	}

	mChannel.createNormalFragments();
}

bool BaseClient::isResourceRequired(const Protocol::Resource& resource)
{
	if (Platform::Asset::Exists(mGameDir + '/' + resource.name))
		return false;

	if (Platform::Asset::Exists("valve/" + resource.name))
		return false;

	if (mIsResourceRequiredCallback != nullptr && mIsResourceRequiredCallback(resource))
		return true;

	// resources with flag RES_CHECKFILE need to be confirmed via CLC_FILECONSISTENCY

	if (!(resource.flags & Protocol::RES_CHECKFILE))
		return false;

	// we not need to download file with reserved consistency, thanks valve

	if (resource.flags & Protocol::RES_RESERVED)
		return false;
	
	return true;
}

int BaseClient::getResourceHash(const Protocol::Resource& resource)
{
	auto asset = Platform::Asset(mGameDir + '/' + resource.name);

	MD5 md5;

	md5.update((unsigned char*)asset.getMemory(), asset.getSize());
	md5.finalize();

	return *(int*)md5.getdigest();
}

void BaseClient::sendCommand(std::string_view command)
{
	if (mState < State::Connected)
	{
		CONSOLE_DEVICE->writeLine("cannot send \"" + std::string(command) + "\", not connected");
		return;
	}

	Common::BitBuffer buf;

	buf.write<uint8_t>((uint8_t)Protocol::Client::Message::StringCmd);
	Common::BufferHelpers::WriteString(buf, command);

	mChannel.addReliableMessage(buf);

	CONSOLE_DEVICE->writeLine("forward \"" + std::string(command) + "\"");
}

bool BaseClient::isPlayerIndex(int value) const
{
	return value >= 1 && value <= mMaxPlayers;
}

void BaseClient::addUserInfo(const std::string& name, const std::string& description,
	Console::CVar::Getter getter, Console::CVar::Setter setter)
{
	CONSOLE->registerCVar(name, description, { "str" },
		getter, setter);

	mUserInfos.push_back({ name, getter });
}
