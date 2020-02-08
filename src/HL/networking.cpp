#include "networking.h"
#include <Common/buffer_helpers.h>

namespace HL
{
	Networking::Networking(uint16_t port) : 
		mSocket(NETWORK->createSocket(port))
	{
		mSocket->setReadCallback([this](Network::Packet& packet) { 
			readPacket(packet); 
		});
	}

	Networking::~Networking()
	{
		NETWORK->destroySocket(mSocket);
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

		SplitBuffer* sb = nullptr;

		// search split buffer

		for (auto& splitBuf : mSplitBuffers)
		{
			if (splitBuf->index != index)
				continue;

			sb = splitBuf;
			break;
		}

		// allocate new buffer if we cannot found one

		if (sb == nullptr)
		{
			sb = new SplitBuffer();

			sb->index = index;
			sb->frags.resize(total);

			mSplitBuffers.push_back(sb);
		}

		sb->time = Clock::Now();

		auto& frag = sb->frags[count];

		// write fragment data to buffer

		frag.buffer.write(packet.buf.getMemory(), packet.buf.getRemaining());

		// check for completion

		bool completed = true;

		for (auto& f : sb->frags)
		{
			if (f.completed)
				continue;

			completed = false;
			break;
		}

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

			mSplitBuffers.remove(sb);

			delete sb;
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

		mSocket->sendPacket(pack);
	}
}