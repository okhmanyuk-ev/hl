#include "channel.h"
#include "encoder.h"
#include "protocol.h"
#include "utils.h"
#include <bzlib.h>

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

void Channel::onFrame()
{
	STATS_INDICATE_GROUP("netchan_seq", "in seq", getIncomingSequence());
	STATS_INDICATE_GROUP("netchan_seq", "out seq", getOutgoingSequence());
	STATS_INDICATE_GROUP("netchan_rel", "in rel", getIncomingReliable());
	STATS_INDICATE_GROUP("netchan_rel", "out rel", getOutgoingReliable());
	STATS_INDICATE_GROUP("netchan", "latency", Clock::ToMilliseconds(getLatency()));
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
		writeFragments(msg);
	
	if (has_reliable_messages)
		writeReliableMessages(msg);
	
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
	auto has_frag_buf = !mOutgoingFragBuffers.empty();

	msg.write<uint8_t>(has_frag_buf ? 1 : 0);

	if (has_frag_buf)
	{
		const auto& frag_buf = *mOutgoingFragBuffers.begin();

		uint16_t total = frag_buf.total;
		uint16_t cur = total - frag_buf.buffers.size() + 1;
		uint16_t offset = 0; // will be non zero in filefrag
		uint16_t size = frag_buf.buffers.begin()->getSize();

		msg.write<uint16_t>(total);
		msg.write<uint16_t>(cur);
		msg.write<uint16_t>(offset);
		msg.write<uint16_t>(size);

		Utils::dlog("{}/{}, size: {}", cur, total, size);
	}

	auto has_file_frag_buf = false;

	msg.write<uint8_t>(has_file_frag_buf ? 1 : 0); // no file fragments

	if (has_file_frag_buf)
	{
		// file frag headers here
	}

	const auto& buf = *mOutgoingFragBuffers.begin()->buffers.begin();

	msg.write(buf.getMemory(), buf.getSize());
	// write file frag buf here
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
		sky::Log(Console::Color::Red, "channel: dropped {} packet(s)", seq - mIncomingSequence);

	mIncomingSequence = seq;
	mIncomingAcknowledgement = ack;

	if (rel)
		mOutgoingReliable = !mOutgoingReliable;

	if (mIncomingAcknowledgement >= mReliableSequence)
	{
		if (relAck != mIncomingReliable)
		{
			mIncomingReliable = relAck;

			if (!mOutgoingFragBuffers.empty())
			{
				auto& frags_buffer = *mOutgoingFragBuffers.begin();
				if (!frags_buffer.buffers.empty())
				{
					frags_buffer.buffers.pop_front();
				}
				
				int total = frags_buffer.total;
				int count = total - frags_buffer.buffers.size();
				int index = total << 16;
				int percent = static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0f);
				STATS_INDICATE_GROUP("netchan_frag", fmt::format("out frag {}", index), fmt::format("{}/{} ({}%)", count, total, percent));
				
				if (frags_buffer.buffers.empty())
				{
					mOutgoingFragBuffers.pop_front();
				}
			}

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
	
	Utils::dlog("index: {} ({}/{}), offset: {}, size: {}", index, count, total, offset, size);
	
	int percent = static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0f);
	STATS_INDICATE_GROUP("netchan_frag", fmt::format("in frag {}", index), fmt::format("{}/{} ({}%)", count, total, percent));

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

	bool completed = std::all_of(fb->frags.begin(), fb->frags.end(), [](const auto& frag) { 
		return frag.completed; 
	});
	
	if (completed)
	{
		BitBuffer buf;

		for (auto& f : fb->frags)
		{
			Common::BufferHelpers::WriteToBuffer(f.buffer, buf);
		}

		buf.toStart();

		Utils::dlog("fragments completed (size: {})", buf.getSize());
		
		if (buf.getRemaining() >= 3 && Common::BufferHelpers::ReadString(buf) == "BZ2")
		{
			uint32_t dst_len = 65536;
			BitBuffer dst_buf;
			dst_buf.setSize(dst_len);
			
			auto dst = (char*)dst_buf.getMemory();
			auto src = (char*)((size_t)buf.getMemory() + buf.getPosition());
			auto src_len = (unsigned int)(buf.getSize() - buf.getPosition());

			BZ2_bzBuffToBuffDecompress(dst, &dst_len, src, src_len, 1, 0);
			
			buf.clear();
			buf.write(dst_buf.getMemory(), dst_len);

			Utils::dlog("decompress {} -> {}", src_len, dst_len);
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
	auto offset = msg.read<int16_t>();
	auto size = msg.read<int16_t>();

	int index = sequence << 16;
	int count = sequence >> 16;
	int total = sequence & 0xFFFF;

	Utils::dlog("index: {} ({}/{}), offset: {}, size: {}", index, count, total, offset, size);

	int percent = static_cast<int>((static_cast<float>(count) / static_cast<float>(total)) * 100.0f);
	STATS_INDICATE_GROUP("netchan_frag", fmt::format("in frag {}", index), fmt::format("{}/{} ({}%)", count, total, percent));

	offset -= (int16_t)normalSize; // !!!

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

	bool completed = std::all_of(fb->frags.begin(), fb->frags.end(), [](const auto& frag) {
		return frag.completed;
	});

	if (completed)
	{
		//std::cout << total << " file fragments completed" << std::endl;

		BitBuffer buf;

		for (auto& f : fb->frags)
		{
			Common::BufferHelpers::WriteToBuffer(f.buffer, buf);
		}

		buf.toStart();

		Utils::dlog("fragments completed (size: {})", buf.getSize());

		auto fileName = Common::BufferHelpers::ReadString(buf);
		bool compressed = Common::BufferHelpers::ReadString(buf) == "bz2";
		uint32_t size = buf.read<uint32_t>();

		BitBuffer filebuf;

		filebuf.setSize(size);

		if (compressed)
		{
			auto dst = (char*)filebuf.getMemory();
			auto src = (char*)((size_t)buf.getMemory() + buf.getPosition());
			auto src_len = (unsigned int)(buf.getSize() - buf.getPosition());

			BZ2_bzBuffToBuffDecompress(dst, &size, src, src_len, 1, 0);

			Utils::dlog("decompress {} -> {}", src_len, size);
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

void Channel::fragmentateReliableBuffer(int fragment_size, bool compress)
{
	assert(!mReliableMessages.empty());

	BitBuffer msg;

	for (const auto& reliable : mReliableMessages)
	{
		msg.write(reliable.getMemory(), reliable.getSize());
	}

	if (compress)
	{
		unsigned int dst_len = 65535;

		BitBuffer temp_buf;
		temp_buf.setSize(dst_len);

		auto dst = (char*)temp_buf.getMemory();
		auto src = (char*)msg.getMemory();
		auto src_len = msg.getSize();
		auto res = BZ2_bzBuffToBuffCompress(dst, &dst_len, src, src_len, 9, 0, 30); // TODO: wtf is last three arguments?

		msg.clear();
		Common::BufferHelpers::WriteString(msg, "BZ2");
		msg.write(temp_buf.getMemory(), dst_len);

		Utils::dlog("compress {} -> {}", src_len, dst_len);
	}

	mReliableMessages.clear();

	OutgoingFragBuffer frag_buf;

	msg.toStart();

	while (msg.hasRemaining())
	{
		BitBuffer temp_buf;

		for (int i = 0; i < fragment_size; i++)
		{
			if (!msg.hasRemaining())
				break;

			temp_buf.write<uint8_t>(msg.read<uint8_t>());
		}

		frag_buf.buffers.push_back(temp_buf);
	}

	frag_buf.total = frag_buf.buffers.size();
	mOutgoingFragBuffers.push_back(frag_buf);

	Utils::dlog("{} fragments created", frag_buf.total);
}