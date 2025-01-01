#pragma once

#include <core/engine.h>
#include <network/system.h>
#include <common/bitbuffer.h>
#include <map>

namespace HL
{
	class Networking
	{
	public:
		Networking(uint16_t port = 0);

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
		auto getSocket() { return mSocket; }

	private:
		std::shared_ptr<Network::UdpSocket> mSocket;

	private:
		struct Fragment
		{
			sky::BitBuffer buffer;
			bool completed = false;
		};

		struct SplitBuffer
		{
			Clock::TimePoint time;
			std::vector<Fragment> frags;
		};

		std::map<int32_t, std::shared_ptr<SplitBuffer>> mSplitBuffers;
	};
}