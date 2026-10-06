#pragma once

#include <vector>

// 구운 결과를 Cache/ 밑에 통째로 눕혔다가 다음 실행에서 되읽는다.
// signature 는 구울 때 쓴 재료의 지문이다. 재료가 바뀌면 지문이 안 맞아 저절로 다시 굽는다
bool LoadCachedBlob(const char* name, uint64 signature, std::vector<uint8>* out);
bool SaveCachedBlob(const char* name, uint64 signature, const std::vector<uint8>& data);

uint64 HashBytes(const void* data, size_t byte_count, uint64 seed);
uint64 HashValue(uint64 value, uint64 seed);

// 바이트 뭉치에 순서대로 밀어 넣는다
struct BlobWriter
{
	std::vector<uint8> bytes;

	void WriteRaw(const void* data, size_t byte_count);
	void WriteInt(int32 value);
	void WriteUint(uint32 value);
	void WriteFloat(float value);
	void WriteByte(uint8 value);
};

// 읽다가 범위를 넘으면 failed 가 서고 그 뒤 읽기는 전부 0 을 준다
struct BlobReader
{
	const std::vector<uint8>* bytes = nullptr;
	size_t cursor = 0;
	bool failed = false;

	bool ReadRaw(void* target, size_t byte_count);
	int32 ReadInt();
	uint32 ReadUint();
	float ReadFloat();
	uint8 ReadByte();
};
