#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>

enum class JsonValueType
{
	STRING,
	MAP,
	ARRAY
};

class JsonValue
{
public:
	JsonValueType type;
	JsonValue(JsonValueType _type);
	virtual void write(std::stringstream& stream, int depth=0, bool prefixSpaces=true);
};

class JsonValue_String : public JsonValue
{
public:
	std::string str;
	JsonValue_String(std::string _str);
	virtual void write(std::stringstream& stream, int depth=0, bool prefixSpaces=true);
};

class JsonValue_Map : public JsonValue
{
public:
	std::map<std::string, std::shared_ptr<JsonValue>> map;
	JsonValue_Map();
	virtual void write(std::stringstream& stream, int depth=0, bool prefixSpaces=true);
	void set(const std::string& key, const std::string& value);
	void set(const std::string& key, std::shared_ptr<JsonValue> value);
};

class JsonValue_Array : public JsonValue
{
public:
	std::vector<std::shared_ptr<JsonValue>> array;
	JsonValue_Array();
	virtual void write(std::stringstream& stream, int depth=0, bool prefixSpaces=true);
};
