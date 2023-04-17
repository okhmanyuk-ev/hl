#pragma once

#include <common/bitbuffer.h>
#include <common/buffer_helpers.h>
#include <string>

#include "studio.h"

#include <vector>
#include <platform/asset.h>

class MDLFile
{
public:
	void loadFromFile(const std::string& fileName)
	{
		auto asset = Platform::Asset(fileName);
		mBuffer.write(asset.getMemory(), asset.getSize());
	};

public:
	auto getHeader() { return (studiohdr_t*)mBuffer.getMemory(); }
	
private:
	BitBuffer mBuffer;
};
