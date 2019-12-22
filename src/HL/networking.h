#pragma once

#include <Core/engine.h>
#include <Network/packet.h>
#include <Network/system.h>
#include <Common/bitbuffer.h>

#include <vector>

namespace HL
{
	class Networking 
	{
	public:
		Networking(uint16_t port = 0);
		~Networking();

	protected:
		virtual void readConnectionlessPacket(Network::Packet& packet) = 0;
		virtual void readRegularPacket(Network::Packet& packet) = 0;

	private:
		void readPacket(Network::Packet& packet);
		void readSplitPacket(Network::Packet& packet);

	protected:
		void sendPacket(Network::Packet& packet);
		void sendConnectionlessPacket(Network::Packet& packet);

	protected:
		auto& getSocket() { return mSocket; }

	private:
		Network::Socket& mSocket;

	private:
		struct Fragment
		{
			Common::BitBuffer buffer;
			bool completed = false;
		};

		struct SplitBuffer
		{
			Clock::TimePoint time;
			int32_t index;
			std::vector<Fragment> frags;
		};

		std::list<SplitBuffer*> mSplitBuffers;
	};
}