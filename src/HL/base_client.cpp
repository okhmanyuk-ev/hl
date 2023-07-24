#include "base_client.h"

#include "protocol.h"
#include <HL/md5.h>
#include <common/buffer_helpers.h>
#include <common/helpers.h>
//#include <filesystem> // TODO: not working on android

#include "encoder.h"

#include <cassert>
#include <algorithm>

#include <platform/defines.h>
#include <platform/asset.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include "utils.h"
#include <magic_enum.hpp>
#include <graphics/all.h>

using namespace HL;

BaseClient::BaseClient(bool hltv) : Networking(),
	mHLTV(hltv)
{
	CONSOLE->registerCVar("dlogs", { "bool" }, CVAR_GETTER_BOOL(mDlogs), CVAR_SETTER_BOOL(mDlogs));
	CONSOLE->registerCVar("dlogs_events", { "bool" }, CVAR_GETTER_BOOL(mDlogsEvents), CVAR_SETTER_BOOL(mDlogsEvents));
	CONSOLE->registerCVar("dlogs_gmsg", { "bool" }, CVAR_GETTER_BOOL(mDlogsGmsg), CVAR_SETTER_BOOL(mDlogsGmsg));
	CONSOLE->registerCVar("dlogs_temp_ents", { "bool" }, CVAR_GETTER_BOOL(mDlogsTempEnts), CVAR_SETTER_BOOL(mDlogsTempEnts));
	CONSOLE->registerCVar("cl_timeout", { "seconds" }, CVAR_GETTER_FLOAT(mTimeout), CVAR_SETTER_FLOAT(mTimeout));

	CONSOLE->registerCommand("connect", "connect to the server", { "address" }, CMD_METHOD(onConnect));
	CONSOLE->registerCommand("disconnect", "disconnect from server", CMD_METHOD(onDisconnect));
	CONSOLE->registerCommand("retry", "connect to the last server", CMD_METHOD(onRetry));
	CONSOLE->registerCommand("cmd", "send command to server", CMD_METHOD(onCmd));
	CONSOLE->registerCommand("fullserverinfo", "receiving from server", { "text" }, CMD_METHOD(onFullServerInfo));
	CONSOLE->registerCommand("reconnect", CMD_METHOD(onReconnect));

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

void BaseClient::onFrame()
{
	if (mState == State::Challenging)
	{
		auto prev_time = mInitializeConnectionTime.value();
		auto now = Clock::Now();

		if (now - prev_time >= Clock::FromSeconds(2.0f))
		{
			initializeConnection();
			mInitializeConnectionTime = now;
		}
	}
	if (mState >= State::Connecting)
	{
		auto prev_time = mInitializeConnectionTime.value();
		auto now = Clock::Now();

		if (mChannel.has_value())
			prev_time = mChannel.value().getIncomingTime();

		if (now - prev_time >= Clock::FromSeconds(mTimeout))
		{
			disconnect("timed out");
		}
	}
}

#pragma region read
void BaseClient::readConnectionlessPacket(Network::Packet& packet)
{
	Utils::dlog(Common::Helpers::BytesArrayToNiceString(packet.buf.getPositionMemory(), packet.buf.getRemaining()));

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

	assert(mChannel);
	mChannel->process(packet.buf);
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
	
	mChannel.emplace(getSocket(),
		[&](auto& msg) { readRegularMessages(msg); },
		[&](auto& msg) { writeRegularMessages(msg); },
		[&](auto name, auto& msg) { receiveFile(name, msg); });
	
	mChannel->setAddress(mServerAdr.value());

	sky::Log("connection accepted");

	mState = State::Connected;

	sendCommand("new");
}

void BaseClient::readConnectionlessReject(Network::Packet& packet)
{
	auto reason = Common::BufferHelpers::ReadString(packet.buf);
	disconnect(fmt::format("connection rejected ({})", reason));
}

void BaseClient::readRegularMessages(BitBuffer& msg)
{
	std::list<Protocol::Server::Message> history;
	
	while (msg.hasRemaining())
	{
		auto index = msg.read<uint8_t>();

		if (index > 64)
		{
			readRegularGameMessage(msg, index);
			continue;
		}
		auto svc = static_cast<Protocol::Server::Message>(index);

		history.push_back(svc);
	
		switch (svc)
		{
		case Protocol::Server::Message::Bad:
			disconnect("svc_bad");
			return;

		case Protocol::Server::Message::Nop:
			break;

		case Protocol::Server::Message::Disconnect:
			readRegularDisconnect(msg);
			return;

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
		
		case Protocol::Server::Message::Customization:
			readRegularCustomization(msg);
			break;
			/*
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
			// TODO: should disconnect 

			auto history_str = std::accumulate(history.begin(), history.end(), std::string(magic_enum::enum_name(*history.begin())),
				[](auto a, auto b) { return a + ", " + std::string(magic_enum::enum_name(b)); });

			sky::Log(Console::Color::Red, "unknown svc: {}, history: {}", index, history_str);
			return;
		}
	}
}

void BaseClient::readRegularGameMessage(BitBuffer& msg, uint8_t index)
{
	assert(mGameMessages.count(index) != 0);

	if (mGameMessages.count(index) == 0)
		return;

	const auto& gmsg = mGameMessages.at(index);
	auto size = gmsg.size;

	if (size == 255)
		size = msg.read<uint8_t>();

	BitBuffer buf;
	buf.setSize(size);
	msg.read(buf.getMemory(), size);

	if (mDlogsGmsg)
		Utils::dlog("{}", gmsg.name);

	if (mGameMod)
	{
		mGameMod->readMessage(gmsg.name, buf);
	}
}

void BaseClient::receiveFile(std::string_view fileName, BitBuffer& msg)
{
	auto game_dir = mServerInfo.value().game_dir;
    Platform::Asset::Write(game_dir + "/" + std::string(fileName), msg.getMemory(), msg.getSize(), HL_ASSET_STORAGE);
	mDownloadQueue.remove_if([fileName](auto a) { return a == fileName; });
	
	sky::Log("received: \"" + std::string(fileName) + "\", size: " +
		Common::Helpers::BytesToNiceString(msg.getSize()) /*+ ", Left: " + std::to_string(mDownloadQueue.size())*/);
}

void BaseClient::readRegularDisconnect(BitBuffer& msg)
{
	auto reason = Common::BufferHelpers::ReadString(msg);
	disconnect(reason);
}

void BaseClient::readRegularEvent(BitBuffer& msg)
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

		if (mDlogsEvents)
		{
			Utils::dlog("index: {}, packet: {}, entity: {}, fire_time: {}, flags: {}, args: ["
				"origin: {:.0f} {:.0f} {:.0f}, angles: {:.0f} {:.0f} {:.0f}, velocity: {:.0f} {:.0f} {:.0f}]", 
				evt.index, evt.packet_index, evt.entity_index, evt.fire_time, evt.flags, evt.args.origin.x, 
				evt.args.origin.y, evt.args.origin.z, evt.args.angles.x, evt.args.angles.y, evt.args.angles.z, 
				evt.args.velocity.x, evt.args.velocity.y, evt.args.velocity.z);
		}

		if (mEventCallback)
			mEventCallback(evt);
	}

	msg.alignByteBoundary();
}

void BaseClient::readRegularVersion(BitBuffer& msg)
{
	auto version = msg.read<int32_t>();
}

void BaseClient::readRegularSetView(BitBuffer& msg)
{
	int16_t value = msg.read<int16_t>();
}

void BaseClient::readRegularSound(BitBuffer& msg)
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

void BaseClient::readRegularTime(BitBuffer& msg)
{
	mTime = msg.read<float>();
}

void BaseClient::readRegularPrint(BitBuffer& msg)
{
	auto text = Common::BufferHelpers::ReadString(msg);
	sky::Log(text);
}

void BaseClient::readRegularStuffText(BitBuffer& msg)
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

void BaseClient::readRegularAngle(BitBuffer& msg)
{
	float x = Common::BufferHelpers::ReadHiResAngle(msg);
	float y = Common::BufferHelpers::ReadHiResAngle(msg);
	float z = Common::BufferHelpers::ReadHiResAngle(msg);
}

void BaseClient::readRegularServerInfo(BitBuffer& msg)
{
	Protocol::ServerInfo server_info;

	server_info.protocol = msg.read<int32_t>();
	server_info.spawn_count = msg.read<int32_t>();
	server_info.map_crc = msg.read<int32_t>();
	server_info.crc = Common::BufferHelpers::ReadBytesToString(msg, 16);
	server_info.max_players = (int32_t)msg.read<uint8_t>();
	server_info.index = (int32_t)msg.read<uint8_t>();
	server_info.deathmatch = msg.read<uint8_t>() > 0;
	server_info.game_dir = Common::BufferHelpers::ReadString(msg);
	server_info.hostname = Common::BufferHelpers::ReadString(msg);
	server_info.map = Common::BufferHelpers::ReadString(msg);
	server_info.map_list = Common::BufferHelpers::ReadString(msg);
	server_info.vac2 = msg.read<uint8_t>() > 0;

	Encoder::UnMunge3(&server_info.map_crc, 4, (-1 - server_info.index) & 0xFF);

	mServerInfo = server_info;

	Utils::dlog("protocol: {}, spawn_count: {}, map_crc: {}, max_players: {}, index: {}, deathmatch: {}, game_dir: {}, "
		"hostname: {}, map: {}, vac2: {}, map_list: {}", server_info.protocol, server_info.spawn_count, server_info.map_crc,
		server_info.max_players, server_info.index, server_info.deathmatch, server_info.game_dir, server_info.hostname,
		server_info.map, server_info.vac2, server_info.map_list);

	initializeGameEngine();
}

void BaseClient::readRegularLightStyle(BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();

	if (index + 1 > m_LightStyles.size())
		m_LightStyles.resize(index + 1);

	m_LightStyles[index] = Common::BufferHelpers::ReadString(msg);
}

void BaseClient::readRegularUserInfo(BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto userid = msg.read<int32_t>();
	auto info = Common::BufferHelpers::ReadString(msg);
	uint8_t hash[16];
	msg.read(&hash, 16);

	mPlayerUserInfos[index] = info;

	Utils::dlog("index: {}, userid: {}, info: \"{}\"", index, userid, info);
}

void BaseClient::readRegularDeltaDescription(BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	auto fieldsCount = msg.readBits(16);
	mDelta.add(msg, name, fieldsCount);
	msg.alignByteBoundary();
}

void BaseClient::readRegularClientData(BitBuffer& msg)
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

void BaseClient::readRegularPings(BitBuffer& msg)
{
	while (msg.readBit())
	{
		auto index = msg.readBits(5);
		auto ping = msg.readBits(12);
		auto loss = msg.readBits(7);
	}
	msg.alignByteBoundary();
}

void BaseClient::readRegularEventReliable(BitBuffer& msg)
{
	Protocol::Event evt;

	evt.packet_index = -1;
	evt.entity_index = -1;

	evt.index = msg.readBits(10);

	mDelta.readEvent(msg, evt.args);

	if (msg.readBit())
		evt.fire_time = static_cast<float>(msg.readBits(16)); // TODO: confirm static cast

	msg.alignByteBoundary();

	if (mDlogsEvents)
	{
		Utils::dlog("index: {}, packet: {}, entity: {}, fire_time: {}, flags: {}", evt.index, evt.packet_index,
			evt.entity_index, evt.fire_time, evt.flags);
	}

	if (mEventCallback)
		mEventCallback(evt);
}

void BaseClient::readRegularSpawnBaseline(BitBuffer& msg)
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

	for (uint32_t i = 0; i < msg.readBits(6); i++)
		mDelta.readEntityNormal(msg, mExtraBaselines[i]);

	msg.alignByteBoundary();

	if (mBaselines.size() > 0)
		sky::Log("{} baseline entities received", mBaselines.size());

	if (mExtraBaselines.size() > 0)
		sky::Log("{} extra baseline entities received", mExtraBaselines.size());

	mDeltaEntities = mBaselines;
}

void BaseClient::readRegularTempEntity(BitBuffer& msg)
{
	auto type = (Protocol::TempEntity)msg.read<uint8_t>();

	if (mDlogsTempEnts)
		Utils::dlog("{}", magic_enum::enum_name(type));

	switch (type)
	{
	case Protocol::TempEntity::BeamPoints:
		readTempEntityBeamPoints(msg);
		break;
	/*case Protocol::TempEntity::BeamEntPoint:;
	case Protocol::TempEntity::GunShot:;*/
	case Protocol::TempEntity::Explosion:
		readTempEntityExplosion(msg);
		break;
	//case Protocol::TempEntity::TarExplosion:;
	case Protocol::TempEntity::Smoke:
		readTempEntitySmoke(msg);
		break;
	/*case Protocol::TempEntity::Tracer:;
	case Protocol::TempEntity::Lightning:;
	case Protocol::TempEntity::BeamEnts:;*/
	case Protocol::TempEntity::Sparks:
		readTempEntitySparks(msg);
		break;
	/*case Protocol::TempEntity::LavaSplash:;
	case Protocol::TempEntity::Teleport:;
	case Protocol::TempEntity::Explosion2:;
	case Protocol::TempEntity::BspDecal:;
	case Protocol::TempEntity::Implosion:;
	case Protocol::TempEntity::SpriteTrail:;*/
	case Protocol::TempEntity::Sprite:
		readTempEntitySprite(msg);
		break;
	/*case Protocol::TempEntity::BeamSprite:;
	case Protocol::TempEntity::BeamTorus:;
	case Protocol::TempEntity::BeamDisk:;
	case Protocol::TempEntity::BeamCylinder:;
	case Protocol::TempEntity::BeamFollow:;*/
	case Protocol::TempEntity::GlowSprite:
		readTempEntityGlowSprite(msg);
		break;
	/*case Protocol::TempEntity::BeamRing:;
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
	case Protocol::TempEntity::BubbleTrail:;*/
	case Protocol::TempEntity::BloodSprite:
		readTempEntityBloodSprite(msg);
		break;
	/*case Protocol::TempEntity::WorldDecal:;
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
	case Protocol::TempEntity::UserTracer:;*/
	default:
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

		int length = TE_LENGTH[(int)type];

		if (length == -1)
		{
			if (type == Protocol::TempEntity::BspDecal)
			{
				msg.seek(8);

				if (msg.read<uint16_t>())
					msg.seek(2);
			}
			else if (type == Protocol::TempEntity::TextMessage)
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
	}
}

void BaseClient::readRegularSignonNum(BitBuffer& msg)
{
	// if peek uint8 < current signon then disconnect

	auto num = msg.read<uint8_t>();
	signon(num);
}

void BaseClient::readRegularStaticSound(BitBuffer& msg)
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

void BaseClient::readRegularCDTrack(BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto loop = msg.read<uint8_t>();
}

void BaseClient::readRegularWeaponAnim(BitBuffer& msg)
{
	auto seq = msg.read<uint8_t>();
	auto group = msg.read<uint8_t>();
}

void BaseClient::readRegularDecalName(BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	auto name = Common::BufferHelpers::ReadString(msg);

	sky::Log(Console::Color::Red, "DecalIndex: " + std::to_string(index) + ", DecalName: " + name);
}

void BaseClient::readRegularRoomType(BitBuffer& msg)
{
	auto room = msg.read<int16_t>();

	sky::Log(Console::Color::Red, "RoomType: " + std::to_string(room));
}

void BaseClient::readRegularUserMsg(BitBuffer& msg)
{
	auto index = msg.read<uint8_t>();
	
	mGameMessages[index].size = msg.read<uint8_t>();

	char name[16];
	msg.read(&name, 16);
	
	mGameMessages[index].name = name;
}

void BaseClient::readRegularPacketEntities(BitBuffer& msg, bool delta)
{
	if (mSignonNum == 1)
	{
		signon(2);
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

		mDeltaSequence = mChannel->getIncomingSequence() & 0xFF; // TODO: only in delta ?

		while (msg.read<uint16_t>() != 0)
		{
			msg.seek(-2);

			bool remove = msg.readBit();

			readEntityIndex(index);
				
			if (remove)
			{
				if (mEntities.count(index) == 0)
				{
					sky::Log(Console::Color::Red, "trying to delete non existing entity " + std::to_string(index));
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
				sky::Log("using extra baseline {} for entity {}", extra_index, index);
				entity = mExtraBaselines[extra_index];
			}

			readDeltaEntity(index, entity, custom);
		}

		if (mEntities.size() != count)
			sky::Log(Console::Color::Red, "entities size mismatch: have {}, must be {}", mEntities.size(), count);
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
				sky::Log("using extra baseline {} for entity {}", extra_index, index);
				entity = mExtraBaselines[extra_index];
			}
			else if (msg.readBit())
			{
				auto base_index = msg.readBits(6);
				if (mBaselines.count(base_index) > 0)
				{
					sky::Log("using baseline {} for entity {}", base_index, index);
					entity = mBaselines[base_index];
				}
				else
				{
					sky::Log(Console::Color::Red, "want use baseline {} for entity {}, but baseline not found", base_index, index);
				}
			}

			readDeltaEntity(index, entity, custom);
		}

		msg.read<uint16_t>();
	}

	msg.alignByteBoundary();
}

void BaseClient::readRegularResourceList(BitBuffer& msg)
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
			Encoder::UnMunge(&resource.reserved, 32, mServerInfo.value().spawn_count);
			resource.flags |= Protocol::RES_RESERVED;
		}
	}

	mConfirmationRequired = msg.readBit();

	if (mConfirmationRequired)
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

	sky::Log("{} resources received", mResources.size());

	// fix sound directory

	for (auto& resource : mResources)
	{
		if (resource.type != Protocol::Resource::Type::Sound)
			continue;
			
		resource.name = "sound/" + resource.name;
	}

	mResourcesVerifying = true;
	mResourcesVerified = false;

	verifyResources();
}

void BaseClient::readRegularMoveVars(BitBuffer& msg)
{
	auto movevars = Protocol::MoveVars{};

	movevars.gravity = msg.read<float>();
	movevars.stop_speed = msg.read<float>();
	movevars.max_speed = msg.read<float>();
	movevars.spectator_max_speed = msg.read<float>();
	movevars.accelerate = msg.read<float>();
	movevars.air_accelerate = msg.read<float>();
	movevars.water_accelerate = msg.read<float>();
	movevars.friction = msg.read<float>();
	movevars.edge_friction = msg.read<float>();
	movevars.water_friction = msg.read<float>();
	movevars.ent_gravity = msg.read<float>();
	movevars.bounce = msg.read<float>();
	movevars.step_size = msg.read<float>();
	movevars.max_velocity = msg.read<float>();
	movevars.z_max = msg.read<float>();
	movevars.wave_height = msg.read<float>();
	movevars.foot_steps = msg.read<uint8_t>();
	movevars.roll_angle = msg.read<float>();
	movevars.roll_speed = msg.read<float>();
	movevars.sky_color.r = msg.read<float>();
	movevars.sky_color.g = msg.read<float>();
	movevars.sky_color.b = msg.read<float>();
	movevars.sky_vec.x = msg.read<float>();
	movevars.sky_vec.y = msg.read<float>();
	movevars.sky_vec.z = msg.read<float>();
	movevars.sky_name = Common::BufferHelpers::ReadString(msg);

	mMoveVars = movevars;
}

void BaseClient::readRegularResourceRequest(BitBuffer& msg)
{
	auto spawncount = msg.read<int32_t>();
	auto range = msg.read<int32_t>(); // rehlds says that it will always 0 (wtf is it?)

	auto count = 0;

	BitBuffer bf;
	bf.write<uint8_t>((uint8_t)Protocol::Client::Message::ResourceList);
	bf.write<int16_t>(count);

	sky::Log("{} resources sent", count);

	mChannel->addReliableMessage(bf);
	mChannel->fragmentateReliableBuffer(512, false);

	/*
	BF.WriteUInt8(CLC_RESOURCELIST);
	BF.WriteInt16(Length(MyResources));

	for I := 0 to Length(MyResources) - 1 do
		with MyResources[I] do
		begin
			BF.WriteLStr(Name);
			BF.WriteUInt8(RType);
			BF.WriteInt16(I);
			BF.WriteInt32(Size);
			BF.WriteUInt8(Flags);

			if Flags and RES_CUSTOM > 0 then
				BF.Write(MD5, SizeOf(MD5));

			Debug(T, [ToString], RESOURCELIST_LOG_LEVEL);
		end;
	*/
}

void BaseClient::readRegularCustomization(BitBuffer& msg) 
{
	auto index = msg.read<uint8_t>();

	auto resource = Protocol::Resource();
	auto type = msg.read<uint8_t>(); // resource.type enum
	resource.name = Common::BufferHelpers::ReadString(msg);
	resource.index = msg.read<int16_t>();
	resource.size = msg.read<int32_t>();
	resource.flags = msg.read<uint8_t>();
	
	if (resource.flags & Protocol::RES_CUSTOM)
	{
		msg.seek(16);
	}

	Utils::dlog("index: {}, type: {}, name: \"{}\", size: {}, flags: {}", index, 
		type, resource.name, resource.size, resource.flags);
}

void BaseClient::readFileTxferFailed(BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	sky::Log(Console::Color::Red, "failed to download file: " + name);
}

void BaseClient::readRegularHLTV(BitBuffer& msg)
{
	sky::Log(Console::Color::Red, "SVC_HLTV was received");
}

void BaseClient::readRegularDirector(BitBuffer& msg)
{
	msg.seek(msg.read<uint8_t>());

	//mConsoleDevice.writeLine("SVC_DIRECTOR was received", Console::Color::Red);
}

void BaseClient::readRegularVoiceInit(BitBuffer& msg)
{
	auto codec = Common::BufferHelpers::ReadString(msg);
	auto quality = msg.read<uint8_t>(); // if protocol > 46

	Utils::dlog("codec: \"{}\", quality: {}", codec, quality);
}

void BaseClient::readRegularSendExtraInfo(BitBuffer& msg)
{
	auto fallbackDir = Common::BufferHelpers::ReadString(msg);
	auto allowCheats = msg.read<uint8_t>();
}

void BaseClient::readRegularResourceLocation(BitBuffer& msg)
{
	auto downloadUrl = Common::BufferHelpers::ReadString(msg);
}

void BaseClient::readRegularCVarValue(BitBuffer& msg)
{
	auto name = Common::BufferHelpers::ReadString(msg);
	sky::Log(Console::Color::Red, "SVC_SENDCVARVALUE: " + name);
}

void BaseClient::readRegularCVarValue2(BitBuffer& msg)
{
	auto id = msg.read<uint32_t>();
	auto name = Common::BufferHelpers::ReadString(msg);
	sky::Log(Console::Color::Red, "SVC_SENDCVARVALUE2: " + std::to_string(id) + ", " + name);
}

void BaseClient::readTempEntityBeamPoints(BitBuffer& msg)
{
	glm::vec3 start;
	start.x = Common::BufferHelpers::ReadCoord(msg);
	start.y = Common::BufferHelpers::ReadCoord(msg);
	start.z = Common::BufferHelpers::ReadCoord(msg);

	glm::vec3 end;
	end.x = Common::BufferHelpers::ReadCoord(msg);
	end.y = Common::BufferHelpers::ReadCoord(msg);
	end.z = Common::BufferHelpers::ReadCoord(msg);

	auto texture = msg.read<int16_t>();

	msg.read<uint8_t>(); // start frame
	msg.read<uint8_t>(); // framerate

	auto lifetime = msg.read<uint8_t>();

	auto width = msg.read<uint8_t>();
	auto noise = msg.read<uint8_t>();

	auto r = msg.read<uint8_t>();
	auto g = msg.read<uint8_t>();
	auto b = msg.read<uint8_t>();
	auto a = msg.read<uint8_t>();

	auto speed = msg.read<uint8_t>();

	if (mBeamPointsCallback)
		mBeamPointsCallback(start, end, lifetime, Graphics::Color::ToNormalized(r, g, b, a));
}

void BaseClient::readTempEntityExplosion(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);

	auto model_index = msg.read<int16_t>();

	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?

	if (mExplosionCallback)
		mExplosionCallback(origin, model_index);
}

void BaseClient::readTempEntitySmoke(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);

	auto model_index = msg.read<int16_t>();

	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?

	if (mSmokeCallback)
		mSmokeCallback(origin, model_index);
}

void BaseClient::readTempEntitySparks(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);

	if (mSparksCallback)
		mSparksCallback(origin);
}

void BaseClient::readTempEntityGlowSprite(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);

	auto model_index = msg.read<int16_t>();
	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?

	if (mGlowSpriteCallback)
		mGlowSpriteCallback(origin, model_index);
}

void BaseClient::readTempEntitySprite(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);

	auto model_index = msg.read<int16_t>();

	msg.read<uint8_t>(); // ?
	msg.read<uint8_t>(); // ?

	if (mSpriteCallback)
		mSpriteCallback(origin, model_index);
}

void BaseClient::readTempEntityBloodSprite(BitBuffer& msg)
{
	glm::vec3 origin;
	origin.x = Common::BufferHelpers::ReadCoord(msg);
	origin.y = Common::BufferHelpers::ReadCoord(msg);
	origin.z = Common::BufferHelpers::ReadCoord(msg);
	auto blood_spray = msg.read<int16_t>();
	auto blood_drop = msg.read<int16_t>();
	auto color = msg.read<uint8_t>();
	auto amount = msg.read<uint8_t>();

	if (mBloodSpriteCallback)
		mBloodSpriteCallback(origin);
}

#pragma endregion

#pragma region write
void BaseClient::writeRegularMessages(BitBuffer& msg)
{
	if (mResourcesVerifying && mDownloadQueue.size() == 0)
	{
		mResourcesVerified = true;
		mResourcesVerifying = false;

		if (mConfirmationRequired)
			writeRegularFileConsistency(msg);
		else
			sky::Log("confirmation of resources isn't required");

		auto crc = mServerInfo.value().map_crc;
		auto spawn_count = mServerInfo.value().spawn_count;
		Encoder::Munge2(&crc, 4, (-1 - spawn_count) & 0xFF);
		sendCommand(fmt::format("spawn {} {}", spawn_count, crc));

		mChannel->fragmentateReliableBuffer(128, false);
	}

	if ((mResourcesVerified || mResourcesVerifying) && !mHLTV)
	{
		writeRegularMove(msg);
	}

	if (mSignonNum == 2)
	{
		writeRegularDelta(msg);
	}
}

void BaseClient::writeRegularMove(BitBuffer& msg)
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
	
	if (mThinkCallback && mSignonNum == 2)
		mThinkCallback(userCmd);
	
	mDelta.writeUserCmd(msg, userCmd, old);

	msg.alignByteBoundary();

	*(uint8_t*)((size_t)msg.getMemory() + o + 1) = (uint8_t)(msg.getSize() - o - 3);
		
	*(uint8_t*)((size_t)msg.getMemory() + o + 2) = (uint8_t)Encoder::BlockSequenceCRCByte(
		(void*)((size_t)msg.getMemory() + o + 3), msg.getSize() - o - 3, mChannel->getOutgoingSequence());
		
	Encoder::Munge((void*)((size_t)msg.getMemory() + o + 3), msg.getSize() - o - 3,
		mChannel->getOutgoingSequence());
}
	
void BaseClient::writeRegularDelta(BitBuffer& msg)
{
	msg.write<uint8_t>((uint8_t)Protocol::Client::Message::Delta);
	msg.write<uint8_t>(mDeltaSequence);
}

void BaseClient::writeRegularFileConsistency(BitBuffer& msg)
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

	sky::Log("{} resources confirmed", c);
}
#pragma endregion

void BaseClient::onConnect(CON_ARGS)
{
	Network::Address address;

	try
	{
		address = Network::Address(CON_ARG(0));
	}
	catch (const std::exception& e)
	{
		sky::Log(e.what());
		return;
	}

	if (address.port == 0)
		address.port = Protocol::DefaultServerPort;

	connect(address);
}

void BaseClient::onDisconnect(CON_ARGS)
{
	if (mState <= State::Disconnected)
	{
		sky::Log("cannot disconnect, not connected");
		return;
	}
	if (mChannel)
	{
		sendCommand("dropclient");
		mChannel->transmit();
	}
	disconnect("client sent drop");
}

void BaseClient::onRetry(CON_ARGS)
{
	if (!mServerAdr.has_value())
	{
		sky::Log("cannot retry, no connection was made");
		return;
	}
	connect(mServerAdr.value());
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

void BaseClient::onReconnect(CON_ARGS)
{
	if (mState < State::Connected)
	{
		sky::Log("cannot reconnect, not connected");
		return;
	}
	resetGameResources();
	mState = State::Connected;
	sendCommand("new");
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
		mChannel->fragmentateReliableBuffer();
	}
}

bool BaseClient::isResourceRequired(const Protocol::Resource& resource)
{
	auto game_dir = mServerInfo.value().game_dir;

    if (Platform::Asset::Exists(game_dir + '/' + resource.name, HL_ASSET_STORAGE))
		return false;

    if (Platform::Asset::Exists("valve/" + resource.name, HL_ASSET_STORAGE))
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
	auto game_dir = mServerInfo.value().game_dir;
	auto asset = Platform::Asset(game_dir + '/' + resource.name);

	MD5 md5;

	md5.update((unsigned char*)asset.getMemory(), asset.getSize());
	md5.finalize();

	return *(int*)md5.getdigest();
}

void BaseClient::signon(uint8_t num)
{
	Utils::dlog("{}", num);
	mSignonNum = num;
	if (mSignonNum == 1)
	{
		sendCommand("sendents");
	}
	else if (mSignonNum == 2)
	{
		initializeGame();
	}
}

void BaseClient::sendCommand(const std::string& command)
{
	if (mState < State::Connected)
	{
		sky::Log("cannot forward \"" + command + "\", not connected");
		return;
	}

	BitBuffer buf;

	buf.write<uint8_t>((uint8_t)Protocol::Client::Message::StringCmd);
	Common::BufferHelpers::WriteString(buf, command);

	mChannel->addReliableMessage(buf);

	sky::Log("forward \"" + command + "\"");
}		

void BaseClient::connect(const Network::Address& address)
{
	if (mState != State::Disconnected)
	{
		sky::Log("cannot connect, already connected");
		return;
	}

	mServerAdr = address;
	mState = State::Challenging;
	initializeConnection();
	mInitializeConnectionTime = Clock::Now();
}

void BaseClient::disconnect(const std::string& reason)
{
	mState = State::Disconnected;
	mChannel.reset();

	mResourcesVerifying = false;
	mResourcesVerified = false;
	mConfirmationRequired = false;
	mDownloadQueue.clear();
	mDelta.clear();
	mGameMessages.clear();
	mTime = 0.0f;
	m_LightStyles.clear();
	m_WeaponData.clear();
	mSignonNum = 0;
	mDeltaSequence = 0;
	mMoveVars.reset();
	mGameMod.reset();
	mInitializeConnectionTime.reset();

	resetGameResources();

	sky::Log("disconnected, reason: \"" + reason + "\"");

	if (mDisconnectCallback)
		mDisconnectCallback(reason);
}

void BaseClient::initializeConnection()
{
	assert(mState == State::Challenging);
	Network::Packet packet;
	packet.adr = mServerAdr.value();
	Common::BufferHelpers::WriteString(packet.buf, "getchallenge steam");
	sendConnectionlessPacket(packet);
	sky::Log("initializing connection to " + mServerAdr.value().toString());
}

bool BaseClient::isPlayerIndex(int value) const
{
	auto max_players = mServerInfo.value().max_players;
	return value >= 1 && value <= max_players;
}

std::optional<HL::Protocol::Resource> BaseClient::findModel(int model_index) const
{
	auto result = std::find_if(mResources.cbegin(), mResources.cend(), [model_index](const auto& res) {
		return res.index == model_index && res.type == HL::Protocol::Resource::Type::Model;
	});

	if (result == mResources.cend())
		return std::nullopt;
	else
		return *result;
}

void BaseClient::addUserInfo(const std::string& name, const std::string& description,
	Console::CVar::Getter getter, Console::CVar::Setter setter)
{
	CONSOLE->registerCVar(name, description, { "str" },
		getter, setter);

	mUserInfos.insert({ name, getter });
}

void BaseClient::initializeGameEngine()
{
	// - delta descriptions received
	// - serverinfo received

	if (mGameEngineInitializedCallback)
		mGameEngineInitializedCallback();

	sendCommand("sendres");

	auto gamedir = mServerInfo.value().game_dir;

	if (gamedir == "cstrike" || gamedir == "czero")
		mGameMod = std::make_shared<CounterStrike>();

	if (!mGameMod)
	{
		mGameMod = std::make_shared<DummyGameMod>();
	}

	mGameMod->setSendCommandCallback([this](const auto& cmd) {
		sendCommand(cmd);
	});
}

void BaseClient::initializeGame()
{
	// - signon 2
	// - starts receiving entities
	// - can be spawned in world
	// - vgui menus starts from here (such as joining team, joining class)

	if (mGameInitializedCallback)
		mGameInitializedCallback();

/*	sendCommand("specmode 3");
	sendCommand("specmode 3");
	sendCommand("unpause \n");
	sendCommand("unpause \n");
	sendCommand("unpause \n");
	sendCommand("unpause \n");*/

	mState = State::GameStarted;

}

void BaseClient::resetGameResources()
{
	mEntities.clear();
	mServerInfo.reset();
	mResources.clear();
	mDeltaEntities.clear();
	mBaselines.clear();
	mExtraBaselines.clear();
}