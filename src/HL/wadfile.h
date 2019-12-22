#pragma once

#include <stdint.h>
#include <string>
#include <string_view>
#include <fstream>
#include <vector>

#include <Common/buffer.h>
#include <Platform/asset.h>

typedef struct wadmiptex_s
{
	char			name[16];
	unsigned		width;
	unsigned		height;
	unsigned		offsets[4];
} wadmiptex_t;

typedef struct wadinfo_s
{
	char identification[4];
	int numlumps;
	int infotableofs;
} wadinfo_t;

typedef struct lumpinfo_s
{
	int filepos;
	int disksize;
	int size;
	char type;
	char compression;
	char pad1;
	char pad2;
	char name[16];
} lumpinfo_t;

typedef struct wadlist_s
{
	bool loaded;
	char wadname[32];
	int wad_numlumps;
	lumpinfo_t * wad_lumps;
	uint8_t * wad_base;
} wadlist_t;

class WADFile
{
public:
	void loadFromFile(const std::string& fileName)
	{
		auto asset = Platform::Asset(fileName);
		
		m_Buffer.write(asset.getMemory(), asset.getSize());
		m_Buffer.toStart();

		// header

		auto header = m_Buffer.read<wadinfo_t>();
		
		m_Buffer.setPosition(header.infotableofs);
		
		for (int i = 0; i < header.numlumps; i++)
		{
			auto lump = m_Buffer.read<lumpinfo_t>();

			m_Lumps.push_back(lump);
		}
	}

	lumpinfo_t* findLump(std::string_view name)
	{
		for (auto& lump : m_Lumps)
		{
			if (lump.name != name)
				continue;

			return &lump;
		}

		return nullptr;
	}

public:
	auto& getLumps() { return m_Lumps; }
	auto& getBuffer() { return m_Buffer; }

private:
	std::vector<lumpinfo_t> m_Lumps;
	Common::Buffer m_Buffer;
};