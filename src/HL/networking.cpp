#include "networking.h"
#include "utils.h"
#include <common/buffer_helpers.h>
#include <common/helpers.h>

using namespace HL;

Networking::Networking(uint16_t port)
{
	mSocket = std::make_shared<Network::UdpSocket>(port);
	mSocket->setReadCallback([this](Network::Packet& packet) { 
		readPacket(packet); 
	});
}

void Networking::readPacket(Network::Packet& packet)
{
	packet.buf.toStart();

	if (packet.buf.getSize() <= 4)
		return;

	switch (packet.buf.read<int32_t>()) 
	{
	case -1:
		readConnectionlessPacket(packet);
		break;

	case -2:
		readSplitPacket(packet);
		break;

	default:
		packet.buf.toStart();
		readRegularPacket(packet);
		break;
	}
}

void Networking::readSplitPacket(Network::Packet& packet)
{
	// https://developer.valvesoftware.com/wiki/Server_queries

	auto index = packet.buf.read<int32_t>();

	auto total = packet.buf.readBits(4);
	auto count = packet.buf.readBits(4);

	Utils::dlog("index: {} ({}/{}), size: {}, data: \"{}\"", index, count + 1, total, packet.buf.getRemaining(), Common::Helpers::BytesArrayToNiceString(packet.buf.getPositionMemory(), packet.buf.getRemaining()));

	std::shared_ptr<SplitBuffer> sb = nullptr;

	if (mSplitBuffers.contains(index))
	{
		sb = mSplitBuffers.at(index);
	}

	if (sb == nullptr)
	{
		sb = std::make_shared<SplitBuffer>();
		sb->frags.resize(total);
		mSplitBuffers.insert({ index, sb });
	}

	sb->time = Clock::Now();

	auto& frag = sb->frags[count];

	// write fragment data to buffer

	frag.buffer.write(packet.buf.getPositionMemory(), packet.buf.getRemaining());
	frag.completed = true;

	// check for completion

	bool completed = std::all_of(sb->frags.begin(), sb->frags.end(), [](const auto& frag) { 
		return frag.completed; 
	});

	if (completed)
	{
		// create new packet and push to readPacket function

		Network::Packet pack;

		pack.adr = packet.adr;

		for (auto& f : sb->frags)
		{
			Common::BufferHelpers::WriteToBuffer(f.buffer, pack.buf);
		}

		pack.buf.toStart();

		readPacket(pack);

		// remove completed split buf

		mSplitBuffers.erase(index);
	}
}

void Networking::sendPacket(Network::Packet& packet)
{
	mSocket->sendPacket(packet);
}

void Networking::sendConnectionlessPacket(Network::Packet& packet)
{
	Network::Packet pack; // TODO: make split packet if buf.size too large

	pack.adr = packet.adr;
	pack.buf.write<int32_t>(-1);

	Common::BufferHelpers::WriteToBuffer(packet.buf, pack.buf);

	Utils::dlog(Common::Helpers::BytesArrayToNiceString(packet.buf.getMemory(), packet.buf.getSize()));

	mSocket->sendPacket(pack);
}