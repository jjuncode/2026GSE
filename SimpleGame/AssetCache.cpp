#include "stdafx.h"
#include "AssetCache.h"

#include <cstring>
#include <direct.h>
#include <fstream>
#include <iostream>
#include <string>

namespace
{
	const char* CACHE_DIRECTORY = "Cache";

	// 앞 네 글자로 남의 파일을 걸러내고, 판 번호로 옛 형식을 걸러낸다
	const uint32 BLOB_MAGIC = 0x43455347u;	// 'GSEC'
	const uint32 BLOB_VERSION = 1u;

	struct BlobHeader
	{
		uint32 magic = 0;
		uint32 version = 0;
		uint64 signature = 0;
		uint64 byte_count = 0;
	};

	std::string MakeCachePath(const char* name)
	{
		std::string path = CACHE_DIRECTORY;
		path += "/";
		path += name;
		path += ".bin";

		return path;
	}

	void EnsureCacheDirectory()
	{
		_mkdir(CACHE_DIRECTORY);
	}
}

uint64 HashBytes(const void* data, size_t byte_count, uint64 seed)
{
	// FNV-1a. 지문만 필요하고 충돌은 재료가 통째로 같을 때나 걱정할 일이라 이걸로 충분하다
	uint64 hash = seed;
	const uint8* cursor = static_cast<const uint8*>(data);

	for (size_t index = 0; index < byte_count; ++index)
	{
		hash ^= static_cast<uint64>(cursor[index]);
		hash *= 1099511628211ull;
	}

	return hash;
}

uint64 HashValue(uint64 value, uint64 seed)
{
	return HashBytes(&value, sizeof(value), seed);
}

bool LoadCachedBlob(const char* name, uint64 signature, std::vector<uint8>* out)
{
	const std::string path = MakeCachePath(name);
	std::ifstream file(path.c_str(), std::ios::binary);

	if (file.is_open() == false)
	{
		return false;
	}

	BlobHeader header;
	file.read(reinterpret_cast<char*>(&header), sizeof(header));

	if (file.gcount() != static_cast<std::streamsize>(sizeof(header)))
	{
		return false;
	}

	if (header.magic != BLOB_MAGIC || header.version != BLOB_VERSION)
	{
		return false;
	}

	// 재료가 바뀌었으면 여기서 어긋난다. 옛 파일은 그냥 덮어쓰게 둔다
	if (header.signature != signature)
	{
		return false;
	}

	// 잘린 파일을 읽으면 뒤쪽이 쓰레기로 남는다
	if (header.byte_count > (1ull << 30))
	{
		return false;
	}

	out->assign(static_cast<size_t>(header.byte_count), 0);

	if (header.byte_count > 0)
	{
		file.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(header.byte_count));

		if (file.gcount() != static_cast<std::streamsize>(header.byte_count))
		{
			out->clear();
			return false;
		}
	}

	return true;
}

bool SaveCachedBlob(const char* name, uint64 signature, const std::vector<uint8>& data)
{
	EnsureCacheDirectory();

	const std::string path = MakeCachePath(name);
	std::ofstream file(path.c_str(), std::ios::binary | std::ios::trunc);

	if (file.is_open() == false)
	{
		std::cout << "[ AssetCache - SaveCachedBlob ] : cannot open " << path << "\n";
		return false;
	}

	BlobHeader header;
	header.magic = BLOB_MAGIC;
	header.version = BLOB_VERSION;
	header.signature = signature;
	header.byte_count = static_cast<uint64>(data.size());

	file.write(reinterpret_cast<const char*>(&header), sizeof(header));

	if (data.empty() == false)
	{
		file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
	}

	return file.good();
}

void BlobWriter::WriteRaw(const void* data, size_t byte_count)
{
	const uint8* cursor = static_cast<const uint8*>(data);
	bytes.insert(bytes.end(), cursor, cursor + byte_count);
}

void BlobWriter::WriteInt(int32 value)
{
	WriteRaw(&value, sizeof(value));
}

void BlobWriter::WriteUint(uint32 value)
{
	WriteRaw(&value, sizeof(value));
}

void BlobWriter::WriteFloat(float value)
{
	WriteRaw(&value, sizeof(value));
}

void BlobWriter::WriteByte(uint8 value)
{
	bytes.push_back(value);
}

bool BlobReader::ReadRaw(void* target, size_t byte_count)
{
	if (bytes == nullptr || failed)
	{
		return false;
	}

	if (cursor + byte_count > bytes->size())
	{
		failed = true;
		return false;
	}

	std::memcpy(target, bytes->data() + cursor, byte_count);
	cursor += byte_count;

	return true;
}

int32 BlobReader::ReadInt()
{
	int32 value = 0;
	ReadRaw(&value, sizeof(value));

	return value;
}

uint32 BlobReader::ReadUint()
{
	uint32 value = 0;
	ReadRaw(&value, sizeof(value));

	return value;
}

float BlobReader::ReadFloat()
{
	float value = 0.f;
	ReadRaw(&value, sizeof(value));

	return value;
}

uint8 BlobReader::ReadByte()
{
	uint8 value = 0;
	ReadRaw(&value, sizeof(value));

	return value;
}
