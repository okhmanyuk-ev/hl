#include "channel.h"
#include "encoder.h"
#include "protocol.h"

#include <bzlib.h>

#include <Common/buffer_helpers.h>

using namespace HL;

Channel::Channel(std::shared_ptr<Network::UdpSocket> socket, MessagesHandler readHandler, MessagesHandler writeHandler, FileHandler fileHandler) :
	mSocket(socket),
	mReadHandler(readHandler), 
	mWriteHandler(writeHandler),
	mFileHandler(fileHandler)
{
	mTimer.setInterval(Clock::FromMilliseconds(10));
	mTimer.setCallback(std::bind(&Channel::transmit, this));
}

void Channel::transmit()
{
	mOutgoingSequence++;
		
	bool has_fragments = false;
	bool has_reliable_messages = false;

	if (mIncomingAcknowledgement >= mReliableSequence)
	{
		has_fragments = !mOutgoingFragBuffers.empty();
		has_reliable_messages = !mReliableMessages.empty();
	}

	bool rel = has_fragments || has_reliable_messages;

	if (rel)
		mReliableSequence = mOutgoingSequence;

	int seq = (int)mOutgoingSequence | (has_fragments << 30) | (rel << 31);
	int ack = (int)mIncomingSequence | (mOutgoingReliable << 31);

	BitBuffer msg;
		
	msg.write(seq);
	msg.write(ack);

	if (has_fragments)
	{
		writeFragments(msg);
	}
	else if (has_reliable_messages)
	{
		writeReliableMessages(msg);
	}

	mWriteHandler(msg);

	while (msg.getSize() < 16)
		msg.write<uint8_t>(1); 
		
	Encoder::Munge2((void*)((size_t)msg.getMemory() + 8), msg.getSize() - 8, seq & 0xFF);
		
	Network::Packet pack;
		
	pack.adr = mAddress;
		
	Common::BufferHelpers::WriteToBuffer(msg, pack.buf);

	mSocket->sendPacket(pack);
}

void Channel::writeFragments(BitBuffer& msg)
{
	msg.write<uint8_t>(!mOutgoingFragBuffers.empty());

	uint16_t total = mOutgoingFragBuffers.begin()->buffers.size();
	uint16_t cur = 1;
	uint16_t offset = 0; // will be non zero in filefrag
	uint16_t size = mOutgoingFragBuffers.begin()->buffers.begin()->getSize();

	msg.write<uint16_t>(total);
	msg.write<uint16_t>(cur);
	msg.write<uint16_t>(offset);
	msg.write<uint16_t>(size);

	msg.write<uint8_t>(0); // no file fragments

	// file frag headers here

	const auto& buf = *mOutgoingFragBuffers.begin()->buffers.begin();

	msg.write(buf.getMemory(), buf.getSize());
	// write file frag buf here

	mOutgoingFragBuffers.pop_front();
}

void Channel::writeReliableMessages(BitBuffer& msg)
{
	bool first = true;

	for (auto& message : mReliableMessages)
	{
		if (msg.getSize() > 1024 && !first)
			break;
			
		first = false; 

		Common::BufferHelpers::WriteToBuffer(message, msg);

		mReliableSent++;
	}
}

void Channel::process(BitBuffer& msg)
{
	mIncomingTime = Clock::Now();

	auto seq = msg.read<uint32_t>();
	auto ack = msg.read<uint32_t>();
		
	bool rel = seq >> 31;
	bool relAck = ack >> 31;

	bool frag = seq & (1UL << 30);
	bool security = ack & (1UL << 30);
		
	if (security)
		return;

	Encoder::UnMunge2((void*)((size_t)msg.getMemory() + 8), msg.getSize() - 8, seq & 0xFF);
		
	seq &= ~(1 << 31);
	seq &= ~(1 << 30);

	ack &= ~(1 << 31);
	ack &= ~(1 << 30);

	if (seq < mIncomingSequence)
		return; // out of order packet

	if (seq == mIncomingSequence)
		return; // duplicate packet

	if (seq > mIncomingSequence + 1)
		LOGCF("channel: dropped {} packet(s)", Console::Color::Red, seq - mIncomingSequence);

	mIncomingSequence = seq;
	mIncomingAcknowledgement = ack;

	if (rel)
		mOutgoingReliable = !mOutgoingReliable;

	if (mIncomingAcknowledgement >= mReliableSequence)
	{
		if (relAck != mIncomingReliable)
		{
			mIncomingReliable = relAck;

			//if OutgoingFragments.Count > 0 then
			//{
			//	Inc(OutgoingFragments.List[0].Count);// := OutgoingFragments.First.Count + 1;

			//if OutgoingFragments.First.Count > High(OutgoingFragments.First.Data) then
			//	OutgoingFragments.Delete(0);
			//}

				// filefrag

			while (mReliableSent > 0)
			{
				mReliableMessages.pop_front();
				mReliableSent--;
			}
		}
		else
		{
			mReliableSent = 0;
		}
	}

	if (frag)
		readFragments(msg);

	mReadHandler(msg);
}

void Channel::readFragments(BitBuffer& msg)
{
	size_t size = msg.getSize();

	if (msg.read<uint8_t>())
		readNormalFragments(msg);

	if (msg.read<uint8_t>())
		readFileFragments(msg, size - msg.getSize());
}

void Channel::readNormalFragments(BitBuffer& msg)
{
	auto sequence = msg.read<int32_t>();
	auto offset = msg.read<int16_t>();
	auto size = msg.read<int16_t>();

	int index = sequence << 16;
	int count = sequence >> 16;
	int total = sequence & 0xFFFF;

	// mConsole->writeLine("normal fragment " + std::to_string(index) + " (" + std::to_string(count) + "/" + std::to_string(total) + ")");

	std::shared_ptr<FragBuffer> fb = nullptr;
		
	// search frag buffer

	if (mNormalFragBuffers.contains(index))
	{
		fb = mNormalFragBuffers.at(index);
	}

	// allocate new buffer if we cannot found one

	if (fb == nullptr)
	{
		fb = std::make_shared<FragBuffer>();
		fb->frags.resize(total);
		mNormalFragBuffers.insert({ index, fb });
	}

	fb->time = Clock::Now();

	auto& frag = fb->frags[count - 1];

	// write fragment data to buffer

	size_t skip;

	if (msg.read<uint8_t>())
		skip = 9;
	else
		skip = 1;

	msg.seek(-1);

	frag.buffer.write((void*)((size_t)msg.getMemory() + msg.getPosition() + skip + offset), size);
	frag.completed = true;

	// remove frag data from msg

	size_t wpos = msg.getPosition() + skip + offset;
	size_t rpos = wpos + size;

	memmove((void*)((size_t)msg.getMemory() + wpos), (void*)((size_t)msg.getMemory() + rpos), 
		msg.getSize() - rpos);

	msg.setSize(msg.getSize() - size);

	// check for completion

	bool completed = true;

	for (auto& f : fb->frags)
	{
		if (f.completed)
			continue;

		completed = false;
		break;
	}

	if (completed)
	{
		BitBuffer buf;

		for (auto& f : fb->frags)
		{
			Common::BufferHelpers::WriteToBuffer(f.buffer, buf);
		}

		buf.toStart();
			
		if (Common::BufferHelpers::ReadString(buf) == "BZ2")
		{
			uint32_t uncompressedSize = 65536;

			BitBuffer uncompressed;

			uncompressed.setSize(uncompressedSize);
				
			BZ2_bzBuffToBuffDecompress((char*)uncompressed.getMemory(), &uncompressedSize, 
				(char*)((size_t)buf.getMemory() + buf.getPosition()), 
				(unsigned int)(buf.getSize() - buf.getPosition()), 1, 0);

			buf.clear();
			buf.write(uncompressed.getMemory(), uncompressedSize);
		}

		// read just completed fragbuf as normal messages
		
		buf.toStart();
		mReadHandler(buf);
			
		// remove completed fragbuf

		mNormalFragBuffers.erase(index);
	}
}

void Channel::readFileFragments(BitBuffer& msg, size_t normalSize)
{
	auto sequence = msg.read<int32_t>();
	auto offset = msg.read<int16_t>() - (int16_t)normalSize; // !!!
	auto size = msg.read<int16_t>();

	int index = sequence << 16;
	int count = sequence >> 16;
	int total = sequence & 0xFFFF;

	//mConsole->writeLine("file fragment " + std::to_string(index) + " (" + std::to_string(count) + "/" + std::to_string(total) + ")");

	std::shared_ptr<FragBuffer> fb = nullptr;

	if (mFileFragBuffers.contains(index))
	{
		fb = mFileFragBuffers.at(index);
	}

	// allocate new buffer if we cannot found one

	if (fb == nullptr)
	{
		fb = std::make_shared<FragBuffer>();
		fb->frags.resize(total);
		mFileFragBuffers.insert({ index, fb });
	}

	fb->time = Clock::Now();

	auto& frag = fb->frags[count - 1];

	// write fragment to buffer

	frag.buffer.write((void*)((size_t)msg.getMemory() + msg.getPosition() + offset), size);
	frag.completed = true;

	// remove frag data from msg

	size_t wpos = msg.getPosition() + offset;
	size_t rpos = wpos + size;

	memmove((void*)((size_t)msg.getMemory() + wpos), (void*)((size_t)msg.getMemory() + rpos),
		msg.getSize() - rpos);

	msg.setSize(msg.getSize() - size);

	// check for completion

	bool completed = true;

	for (auto& f : fb->frags)
	{
		if (f.completed)
			continue;

		completed = false;
		break;
	}

	if (completed)
	{
		//std::cout << total << " file fragments completed" << std::endl;

		BitBuffer buf;

		for (auto& f : fb->frags)
		{
			Common::BufferHelpers::WriteToBuffer(f.buffer, buf);
		}

		buf.toStart();

		auto fileName = Common::BufferHelpers::ReadString(buf);
		bool compressed = Common::BufferHelpers::ReadString(buf) == "bz2";
		uint32_t size = buf.read<uint32_t>();

		BitBuffer filebuf;

		filebuf.setSize(size);

		if (compressed)
		{
			BZ2_bzBuffToBuffDecompress((char*)filebuf.getMemory(), &size,
				(char*)((size_t)buf.getMemory() + buf.getPosition()), 
				(unsigned int)(buf.getSize() - buf.getPosition()), 1, 0);
		}
		else
		{
			for (int i = 0; i < static_cast<int>(size); i++)
				filebuf.write<uint8_t>(buf.read<uint8_t>());
		}

		mFileHandler(fileName, filebuf);

		mFileFragBuffers.erase(index);
	}
}

void Channel::addReliableMessage(BitBuffer& msg)
{
	auto bf = BitBuffer();

	Common::BufferHelpers::WriteToBuffer(msg, bf);

	mReliableMessages.push_back(bf);
}

void Channel::fragmentateReliableBuffer(int fragment_size, bool compression)
{
	assert(!mReliableMessages.empty());

	OutgoingFragBuffer frag_buf;
	frag_buf.buffers.push_back(*mReliableMessages.begin());
	mOutgoingFragBuffers.push_back(frag_buf);

	mReliableMessages.pop_front();
}