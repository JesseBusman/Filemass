#include <string>
#include <map>
#include <memory>
#include <vector>
#include <sstream>
#include <iostream>

#include "json.h"

void writeString(std::stringstream& stream, const std::string& str)
{
	stream << '"';
	for (size_t i=0; i<str.length(); i++)
	{
		if (str[i] == '\\') stream << "\\\\";
		else if (str[i] == '\"') stream << "\\\"";
		else stream << str[i];
	}
	stream << '"';
}

void writeSpaces(std::stringstream& stream, int n)
{
	for (int i=0; i<n; i++) stream << ' ';
}

void JsonValue::write(std::stringstream&, int, bool)
{
	throw 234923849;
}

JsonValue::JsonValue(JsonValueType _type):
	type(_type)
{
}

JsonValue_String::JsonValue_String(std::string _str):
	JsonValue(JsonValueType::STRING),
	str(_str)
{
}

JsonValue_Map::JsonValue_Map():
	JsonValue(JsonValueType::MAP)
{
}

JsonValue_Array::JsonValue_Array():
	JsonValue(JsonValueType::ARRAY)
{
}


void JsonValue_String::write(std::stringstream& stream, int, bool)
{
	writeString(stream, this->str);
}

void JsonValue_Map::write(std::stringstream& stream, int depth, bool prefixSpaces)
{
	if (prefixSpaces) writeSpaces(stream, depth); stream << "{\r\n";
	bool first = true;
	size_t n = 0;
	for (auto const& [key, value] : this->map)
	{
		n++;
		if (!first) stream << ",\r\n";
		first = false;
		writeSpaces(stream, depth + 2); writeString(stream, key); stream << ": ";
		value->write(stream, depth + 2, false);
		if ((value->type == JsonValueType::ARRAY || value->type == JsonValueType::MAP) && n < this->map.size()) stream << "\r\n";
	}
	if (!first) stream << "\r\n";
	writeSpaces(stream, depth); stream << "}";
}
void JsonValue_Map::set(const std::string& key, const std::string& value)
{
	this->map[key] = std::make_shared<JsonValue_String>(value);
}
void JsonValue_Map::set(const std::string& key, std::shared_ptr<JsonValue> value)
{
	this->map[key] = value;
}

void JsonValue_Array::write(std::stringstream& stream, int depth, bool prefixSpaces)
{
	if (prefixSpaces) writeSpaces(stream, depth); stream << "[\r\n";
	for (auto const& value : this->array)
	{
		value->write(stream, depth + 2);
		if (&value != &this->array.back()) stream << ',';
		if (value->type == JsonValueType::ARRAY || value->type == JsonValueType::MAP) stream << "\r\n";
	}
	writeSpaces(stream, depth); stream << "]";
}
