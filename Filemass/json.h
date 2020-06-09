#pragma once

#include <string>
#include <map>
#include <memory>
#include <vector>

class JsonValue
{
public:
	virtual void write(std::stringstream& stream);
};

class JsonValue_String : public JsonValue
{
public:
	std::string str;
	JsonValue_String(std::string _str):
		str(_str)
	{
	}
	virtual void write(std::stringstream& stream);
};

class JsonValue_Map : public JsonValue
{
public:
	std::map<std::string, std::shared_ptr<JsonValue>> map;
	virtual void write(std::stringstream& stream);
	void set(const std::string& key, const std::string& value);
	void set(const std::string& key, std::shared_ptr<JsonValue> value);
};

class JsonValue_Array : public JsonValue
{
public:
	std::vector<std::shared_ptr<JsonValue>> array;
	virtual void write(std::stringstream& stream);
};
