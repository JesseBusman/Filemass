#include <string>
#include <map>
#include <memory>
#include <vector>
#include <sstream>

#include "json.h"

void writeString(std::stringstream& stream, const std::string& str)
{
	stream << '"';
	for (int i=0; i<str.length(); i++)
	{
		if (str[i] == '\\') stream << "\\\\";
		else if (str[i] == '\"') stream << "\\\"";
		else stream << str[i];
	}
	stream << '"';
}

void JsonValue::write(std::stringstream& stream)
{
	throw 234923849;
}

void JsonValue_String::write(std::stringstream& stream)
{
	writeString(stream, this->str);
}

void JsonValue_Map::write(std::stringstream& stream)
{
	stream << '{';
	bool first = true;
	for (auto const& [key, value] : this->map)
	{
		if (!first) stream << ',';
		first = false;
		writeString(stream, key);
		stream << ':';
		value->write(stream);
	}
	stream << '}';
}
void JsonValue_Map::set(const std::string& key, const std::string& value)
{
	this->map[key] = std::make_shared<JsonValue_String>(value);
}
void JsonValue_Map::set(const std::string& key, std::shared_ptr<JsonValue> value)
{
	this->map[key] = value;
}

void JsonValue_Array::write(std::stringstream& stream)
{
	stream << '[';
	bool first = true;
	for (auto const& value : this->array)
	{
		if (!first) stream << ',';
		first = false;
		value->write(stream);
	}
	stream << ']';
}
