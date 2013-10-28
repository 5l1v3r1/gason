#pragma once

#include <stdint.h>
#include <assert.h>

struct JsonAllocator
{
	struct Zone
	{
		Zone *next;
		char *end;
	};

	Zone *head = nullptr;

	~JsonAllocator();
	void *allocate(size_t n, size_t align = 8);
};

#define JSON_VALUE_PAYLOAD_MASK 0x00007FFFFFFFFFFFULL
#define JSON_VALUE_NAN_MASK 0x7FF8000000000000ULL
#define JSON_VALUE_NULL 0x7FFF800000000000ULL
#define JSON_VALUE_TAG_MASK 0xF
#define JSON_VALUE_TAG_SHIFT 47

enum JsonTag
{
	JSON_TAG_NUMBER,
	JSON_TAG_STRING,
	JSON_TAG_BOOL,
	JSON_TAG_ARRAY,
	JSON_TAG_OBJECT,
	JSON_TAG_NULL = 0xF
};

struct JsonElement;
struct JsonPair;

struct JsonValue
{
	union
	{
		uint64_t i;
		double f;
	} data;

	JsonValue()
	{
		data.i = JSON_VALUE_NULL;
	}

	JsonValue(JsonTag tag, void *p)
	{
		uint64_t x = (uint64_t)p;
		assert((int)tag <= JSON_VALUE_TAG_MASK);
		assert(x <= JSON_VALUE_PAYLOAD_MASK);
		data.i = JSON_VALUE_NAN_MASK | ((uint64_t)tag << JSON_VALUE_TAG_SHIFT) | x;
	}

	explicit JsonValue(double x)
	{
		data.f = x;
	}

	explicit operator bool() const
	{
		return data.i != JSON_VALUE_NULL;
	}

	bool isDouble() const
	{
		return (int64_t)data.i <= (int64_t)JSON_VALUE_NAN_MASK;
	}

	JsonTag getTag() const
	{
		return isDouble() ? JSON_TAG_NUMBER : JsonTag((data.i >> JSON_VALUE_TAG_SHIFT) & JSON_VALUE_TAG_MASK);
	}

	uint64_t getPayload() const
	{
		assert(!isDouble());
		return data.i & JSON_VALUE_PAYLOAD_MASK;
	}

	double toNumber() const
	{
		assert(getTag() == JSON_TAG_NUMBER);
		return data.f;
	}

	bool toBool() const
	{
		assert(getTag() == JSON_TAG_BOOL);
		return (bool)getPayload();
	}

	char *toString() const
	{
		assert(getTag() == JSON_TAG_STRING);
		return (char *)getPayload();
	}

	JsonElement *toElement() const
	{
		assert(getTag() == JSON_TAG_ARRAY);
		return (JsonElement *)getPayload();
	}

	JsonPair *toPair() const
	{
		assert(getTag() == JSON_TAG_OBJECT);
		return (JsonPair *)getPayload();
	}
};

struct JsonElement
{
	JsonElement *next;
	JsonValue value;
};

struct JsonPair
{
	JsonPair *next;
	char *key;
	JsonValue value;
};

enum JsonParseStatus
{
	JSON_PARSE_OK,
	JSON_PARSE_BAD_NUMBER,
	JSON_PARSE_BAD_STRING,
	JSON_PARSE_UNKNOWN_IDENTIFIER,
	JSON_PARSE_OVERFLOW,
	JSON_PARSE_UNDERFLOW,
	JSON_PARSE_MISMATCH_BRACKET,
	JSON_PARSE_UNEXPECTED_CHARACTER
};

JsonParseStatus json_parse(char *str, char **endptr, JsonValue *value, JsonAllocator &allocator);
