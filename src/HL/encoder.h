#pragma once

#include <stdint.h>

namespace HL
{
	class Encoder
	{
	private:
		static void MungeInternal(void* data, size_t size, int seq, const uint8_t table[]);
		static void UnMungeInternal(void* data, size_t size, int seq, const uint8_t table[]);

	public:
		static void Munge(void* data, size_t size, int seq);
		static void UnMunge(void* data, size_t size, int seq);
		static void Munge2(void* data, size_t size, int seq);
		static void UnMunge2(void* data, size_t size, int seq);
		static void Munge3(void* data, size_t size, int seq);
		static void UnMunge3(void* data, size_t size, int seq);

	public:
		typedef unsigned int CRC32_t;

		static void CRC32_Init(CRC32_t *pulCRC);
		static CRC32_t CRC32_Final(CRC32_t pulCRC);
		static void CRC32_ProcessByte(CRC32_t *pulCRC, unsigned char ch);
		static void CRC32_ProcessBuffer(CRC32_t *pulCRC, void *pBuffer, int nBuffer);

		static uint8_t BlockSequenceCRCByte(void* data, int len, int seq);
	};
}