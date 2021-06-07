#pragma once

// TODO: add timeouts for frag buffers

#include <core/engine.h>
#include <common/bitbuffer.h>
#include <common/timer.h>
#include <network/system.h>
#include <console/device.h>
#include <vector>

namespace HL
{
	class Channel
	{
	public:
		using MessagesHandler = std::function<void(BitBuffer& msg)>;
		using FileHandler = std::function<void(const std::string& name, BitBuffer& buf)>;

	public:
		Channel(std::shared_ptr<Network::UdpSocket> socket, MessagesHandler readHandler, MessagesHandler writeHandler, FileHandler fileHandler);

	public:
		void process(BitBuffer& msg);
		void addReliableMessage(BitBuffer& msg);
		void createNormalFragments(); // fragmentate a reliable buffer

	private:
		void transmit();
		void writeReliableMessages(BitBuffer& msg);
		void readFragments(BitBuffer& msg);
		void readNormalFragments(BitBuffer& msg);
		void readFileFragments(BitBuffer& msg, size_t normalSize);

	private:
		MessagesHandler mReadHandler;
		MessagesHandler mWriteHandler;
		FileHandler mFileHandler;

	public:
		auto getSocket() { return mSocket; }

		auto getAddress() const { return mAddress; }
		void setAddress(Network::Address value) { mAddress = value; }

	private:
		std::shared_ptr<Network::UdpSocket> mSocket;
		Network::Address mAddress;
		
	public:
		auto getIncomingSequence() const { return mIncomingSequence; }
		auto getIncomingAcknowledgement() const { return mIncomingAcknowledgement; }
		auto getIncomingReliable() const { return mIncomingReliable; }
		auto getIncomingTime() const { return mIncomingTime; }

		auto getOutgoingSequence() const { return mOutgoingSequence; }
		auto getOutgoingReliable() const { return mOutgoingReliable; }
		
		auto getLatency() const { return mLatency; }

	private:
		uint32_t mIncomingSequence = 0;
		uint32_t mIncomingAcknowledgement = 0;
		bool mIncomingReliable = false;
		Clock::TimePoint mIncomingTime = Clock::Now();

		uint32_t mOutgoingSequence = 0;
		bool mOutgoingReliable = false;
		
		uint32_t mReliableSequence = 0;

		Clock::Duration mLatency = Clock::Duration::zero();
		uint32_t mLatencySequence = 0;
		Clock::TimePoint mLatencyTime = Clock::Now();
		bool mLatencyReady = true;

		int mReliableSent = 0;

		Common::Timer mTimer;

	private:
		std::list<BitBuffer> mReliableMessages;

		struct Fragment
		{
			BitBuffer buffer;
			bool completed = false;
		};

		struct FragBuffer
		{
			Clock::TimePoint time;
			int32_t index;
			std::vector<Fragment> frags;
		};

		std::list<std::shared_ptr<FragBuffer>> mNormalFragBuffers;
		std::list<std::shared_ptr<FragBuffer>> mFileFragBuffers;
	};
}