#pragma once

#include <string>
#include <memory>
#include <vector>
#include <array>
#include <map>
#include <optional>

class JsonValue_Map;

struct sqlite3;

class Tag
{
public:
	std::optional<std::array<char, 32>> parentHashSum;
	std::optional<std::array<char, 32>> hashSum;
	std::optional<std::array<char, 32>> thisHash;
	std::optional<std::array<char, 32>> fileHash;
	std::optional<std::string> name;
	std::vector<std::shared_ptr<Tag>> subtags;

	Tag(const std::string& _name);
	Tag(const std::array<char, 32>& parentHashSum, const std::array<char, 32>& hashSum, const std::array<char, 32>& thisHash, const std::array<char, 32>& fileHash);
	void debugPrint(int depth=0);
	void addTo(const std::array<char, 32>& parentHashSum, const std::array<char, 32>& destGrandParentHashSum, const std::array<char, 32>& destFileHash, sqlite3* tagbase_db, bool insideTransaction);
	void removeFrom(const std::array<char, 32>& parentHashSum, sqlite3* tagbase_db);
	std::string toString();
	std::shared_ptr<JsonValue_Map> toJSON(); 
};

std::shared_ptr<Tag> findTagsOfFile(const std::array<char, 32>& fileHash, sqlite3* tagbase_db);
