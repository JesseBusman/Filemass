#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>

enum class JsonValueType
{
	STRING,
	INTEGER,
	MAP,
	ARRAY
};

class JsonValue
{
public:
	JsonValueType type;
	JsonValue(JsonValueType _type);
	virtual void write(std::ostream& stream, int depth=0, bool prefixSpaces=true) const;
};

class JsonValue_String : public JsonValue
{
public:
	std::string str;
	JsonValue_String(std::string _str);
	virtual void write(std::ostream& stream, int depth=0, bool prefixSpaces=true) const;
};

class JsonValue_Integer : public JsonValue
{
public:
	long long i;
	JsonValue_Integer(long long _i);
	virtual void write(std::ostream& stream, int depth=0, bool prefixSpaces=true) const;
};

class JsonValue_Map : public JsonValue
{
public:
	std::map<std::string, std::shared_ptr<JsonValue>> map;
	JsonValue_Map();
	virtual void write(std::ostream& stream, int depth=0, bool prefixSpaces=true) const;
	void set(const std::string& key, const std::string& value);
	void set(const std::string& key, long long value);
	void set(const std::string& key, std::shared_ptr<JsonValue> value);
};

class JsonValue_Array : public JsonValue
{
public:
	std::vector<std::shared_ptr<JsonValue>> array;
	JsonValue_Array();
	virtual void write(std::ostream& stream, int depth=0, bool prefixSpaces=true) const;
};
