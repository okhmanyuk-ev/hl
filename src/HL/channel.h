#pragma once

// TODO: add timeouts for frag buffers

#include <Core/engine.h>
#include <Common/bitbuffer.h>
#include <Common/timer.h>
#include <Network/system.h>
#include <Console/device.h>
#include <vector>

namespace HL
{
	class Channel
	{
	public:
		typedef std::function<void(Common::BitBuffer& msg)> MessagesHandler;
		typedef std::function<void(const std::string& name, Common::BitBuffer& buf)> FileHandler;

	public:
		Channel(std::shared_ptr<Network::UdpSocket> socket, MessagesHandler readHandler, MessagesHandler writeHandler, FileHandler fileHandler);
		~Channel();

	public:
		void clear();
		void process(Common::BitBuffer& msg);
		void addReliableMessage(Common::BitBuffer& msg);
		void createNormalFragments(); // fragmentate a reliable buffer

	private:
		void transmit();
		void writeReliableMessages(Common::BitBuffer& msg);
		void readFragments(Common::BitBuffer& msg);
		void readNormalFragments(Common::BitBuffer& msg);
		void readFileFragments(Common::BitBuffer& msg, size_t normalSize);

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
		bool isActive() const { return mActive; }
		void setActive(bool value) { mActive = value; }

		auto getIncomingSequence() const { return mIncomingSequence; }
		auto getIncomingAcknowledgement() const { return mIncomingAcknowledgement; }
		auto getIncomingReliable() const { return mIncomingReliable; }
		auto getIncomingTime() const { return mIncomingTime; }

		auto getOutgoingSequence() const { return mOutgoingSequence; }
		auto getOutgoingReliable() const { return mOutgoingReliable; }
		
		auto getLatency() const { return mLatency; }

	private:
		bool mActive = false;

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
		std::list<Common::BitBuffer*> mReliableMessages;

		struct Fragment
		{
			Common::BitBuffer buffer;
			bool completed = false;
		};

		struct FragBuffer
		{
			Clock::TimePoint time;
			int32_t index;
			std::vector<Fragment> frags;
		};

		std::list<FragBuffer*> mNormalFragBuffers;
		std::list<FragBuffer*> mFileFragBuffers;
	};
}