#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <map>
#include <array>
#include <string>

#include "sha256.h"
#include "util.h"
#include "sqlite3.h"
#include "tag.h"
#include "json.h"

std::shared_ptr<Tag> findTagsOfFile(const std::array<char, 32>& fileHash, sqlite3* tagbase_db)
{
	std::shared_ptr<Tag> ret = std::make_shared<Tag>(ZERO_HASH, fileHash, fileHash, fileHash);
	std::map<std::array<char, 32>, std::shared_ptr<Tag>> hash_sum__to__tag;
	std::map<std::array<char, 32>, std::vector<std::shared_ptr<Tag>>> parent_hash_sum__to__child_tags;

	sqlite3_stmt* stmt = p(
		tagbase_db,
		"SELECT e.parent_hash_sum, e.hash_sum, e._this_hash, hd.data FROM edges AS e LEFT JOIN hashed_data AS hd ON e._this_hash=hd.hash WHERE e._file_hash=?"
	);
	sqlite3_bind_blob(stmt, 1, fileHash.data(), 32, SQLITE_STATIC);
	int stepResult;
	while (true)
	{
		stepResult = sqlite3_step(stmt);
		if (stepResult == SQLITE_DONE) break;
		else if (stepResult != SQLITE_ROW)
		{
			throw "sql query error in findTagsOfFile";
		}

		std::array<char, 32> parent_hash_sum = sqlite3_column_32chars(stmt, 0);
		std::array<char, 32> hash_sum = sqlite3_column_32chars(stmt, 1);
		std::array<char, 32> _this_hash = sqlite3_column_32chars(stmt, 2);
		std::string tag_name = std::string((const char*)sqlite3_column_blob(stmt, 3), sqlite3_column_bytes(stmt, 3));
		
		auto tag = std::make_shared<Tag>(parent_hash_sum, hash_sum, _this_hash, fileHash);
		tag->name = tag_name;

		if (parent_hash_sum == fileHash)
		{
			ret->subtags.push_back(tag);
		}
		else
		{
			// If we already saw the parent of this tag, add it to its parent's subtags
			if (hash_sum__to__tag.find(parent_hash_sum) != hash_sum__to__tag.end())
			{
				hash_sum__to__tag[parent_hash_sum]->subtags.push_back(tag);
			}

			// Otherwise, cache it
			else
			{
				parent_hash_sum__to__child_tags[parent_hash_sum].push_back(tag);
			}
		}
		
		// Add this tag to the lookup map
		hash_sum__to__tag[hash_sum] = tag;
		
		// If we already saw children of this tag, move them to this tag's subtags
		std::swap(tag->subtags, parent_hash_sum__to__child_tags[hash_sum]);
		parent_hash_sum__to__child_tags.erase(hash_sum);
	}

	if (stepResult != SQLITE_DONE) throw "ERROR stepResult != SQLITE_DONE";
	
	return ret;
}

Tag::Tag(const std::array<char, 32>& parentHashSum, const std::array<char, 32>& hashSum, const std::array<char, 32>& thisHash, const std::array<char, 32>& fileHash):
	parentHashSum(parentHashSum),
	hashSum(hashSum),
	thisHash(thisHash),
	fileHash(fileHash)
{
}

Tag::Tag(const std::string& _name):
	name(_name)
{
	thisHash.emplace(ZERO_HASH);
	SHA256 hasher;
	hasher.update((const unsigned char*)_name.data(), _name.length());
	hasher.final((unsigned char*)thisHash->data());
}

std::string Tag::toString()
{
	std::string ret;
	if (this->name.has_value()) ret = *this->name;
	else if (this->thisHash.has_value()) ret = bytes_to_hex(*this->thisHash);
	else ret = "??";
	if (this->subtags.size() >= 1)
	{
		ret += '[';
		for (size_t i=0; i<this->subtags.size(); i++)
		{
			if (i != 0) ret += ',';
			ret += this->subtags[i]->toString();
		}
		ret += ']';
	}
	return ret;
}

std::shared_ptr<JsonValue_Map> Tag::toJSON()
{
	std::shared_ptr<JsonValue_Map> map = std::make_shared<JsonValue_Map>();
	if (this->name.has_value()) map->set("name", *this->name);
	else if (this->thisHash.has_value()) map->set("hash", bytes_to_hex(*this->thisHash));
	else throw 123;

	if (this->subtags.size() >= 1)
	{
		std::shared_ptr<JsonValue_Array> array = std::make_shared<JsonValue_Array>();
		for (auto subtag : this->subtags)
		{
			array->array.push_back(subtag->toJSON());
		}
		map->set("subtags", array);
	}
	return map;
}

void Tag::debugPrint(int depth)
{
	auto printSpaces = [](int n){for (int i=0; i<n; i++)std::cout << ' ';};

	printSpaces(depth);
	std::cout << "[\r\n";

	printSpaces(depth+2);
	if (name.has_value()) std::cout << *name << ",\r\n";
	else std::cout << "?,\r\n";

	printSpaces(depth+2);
	if (parentHashSum.has_value())	std::cout << "parent:   " << bytes_to_hex(*parentHashSum) << ",\r\n";
	else std::cout << "parent: ?,\r\n";

	printSpaces(depth+2);
	if (hashSum.has_value())		std::cout << "hashsum:  " << bytes_to_hex(*hashSum) << ",\r\n";
	else std::cout << "hashsum: ?,\r\n";

	printSpaces(depth+2);
	if (thisHash.has_value())		std::cout << "hash:     " << bytes_to_hex(*thisHash) << ",\r\n";
	else std::cout << "hash: ?,\r\n";

	printSpaces(depth+2);
	if (fileHash.has_value())		std::cout << "filehash: " << bytes_to_hex(*fileHash) << ",\r\n";
	else std::cout << "filehash: ?,\r\n";

	if (subtags.size() != 0)
	{
		for (size_t i=0; i<subtags.size(); i++)
		{
			//if (i != 0) { std::cout << ','; }
			subtags[i]->debugPrint(depth+2);
		}
	}

	printSpaces(depth);
	std::cout << "]\r\n";

	if (depth == 0) std::cout << "\r\n";
}
void Tag::removeFrom(const std::array<char, 32>& destParentHashSum, sqlite3* tagbase_db)
{
	std::cout << bytes_to_hex(*thisHash) << "->removeFrom(" << bytes_to_hex(destParentHashSum) << ")\r\n";

	sqlite3_stmt* stmt = p(
		tagbase_db,
		"DELETE FROM edges WHERE parent_hash_sum=? AND _this_hash=? LIMIT 1"
	);

	std::array<char, 32> hashSum = non_commutative__non_associative__hash_sum(destParentHashSum, *this->thisHash);

	sqlite3_bind_blob(stmt, 1, destParentHashSum.data(), 32, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, this->thisHash->data(), 32, SQLITE_STATIC);

	int stepResult = sqlite3_step(stmt);

	if (stepResult != SQLITE_DONE)
	{
		std::cout << "\r\n" << stepResult;
		std::cout << "\r\nsqlite3_step did not return SQLITE_DONE on query DELETE FROM edges\r\n";
		std::cout << sqlite3_errmsg(tagbase_db) << "\r\n";
		exit(1);
		throw 1;
	}
	
	for (auto subtag : this->subtags)
	{
		subtag->removeFrom(hashSum, tagbase_db);
	}
}
void Tag::addTo(const std::array<char, 32>& destParentHashSum, const std::array<char, 32>& destGrandParentHashSum, const std::array<char, 32>& destFileHash, sqlite3* tagbase_db, bool insideTransaction)
{
	if (!insideTransaction)
	{
		q(tagbase_db, "BEGIN TRANSACTION");
	}

	if (DEBUGGING) std::cout << bytes_to_hex(*thisHash) << "->addTo(" << bytes_to_hex(destParentHashSum) << ")\r\n";

	if (this->name->length() < 65536)
	{
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"INSERT OR IGNORE INTO hashed_data (hash, data) VALUES(?, ?)"
		);

		sqlite3_bind_blob(stmt, 1, this->thisHash->data(), 32, SQLITE_STATIC);
		sqlite3_bind_blob(stmt, 2, this->name->c_str(), this->name->length(), SQLITE_STATIC);
		
		int stepResult = sqlite3_step(stmt);

		if (stepResult != SQLITE_DONE)
		{
			exitWithError(std::to_string(stepResult) +  " < sqlite3_step did not return SQLITE_DONE on query INSERT OR IGNORE INTO hashed_data: " + sqlite3_errmsg(tagbase_db));
		}
	}




	sqlite3_stmt* stmt = p(
		tagbase_db,
		"INSERT OR IGNORE INTO edges (parent_hash_sum, _this_hash, hash_sum, _file_hash, _grandparent_hash_sum) VALUES(?, ?, ?, ?, ?)"
	);

	std::array<char, 32> hashSum = non_commutative__non_associative__hash_sum(destParentHashSum, *this->thisHash);

	sqlite3_bind_blob(stmt, 1, destParentHashSum.data(), 32, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 2, this->thisHash->data(), 32, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 3, hashSum.data(), 32, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 4, destFileHash.data(), 32, SQLITE_STATIC);
	sqlite3_bind_blob(stmt, 5, destGrandParentHashSum.data(), 32, SQLITE_STATIC);

	int stepResult = sqlite3_step(stmt);

	if (stepResult != SQLITE_DONE)
	{
		exitWithError(std::to_string(stepResult) +  " < sqlite3_step did not return SQLITE_DONE on query INSERT OR IGNORE INTO edges: " + sqlite3_errmsg(tagbase_db));
	}
	
	for (auto subtag : this->subtags)
	{
		subtag->addTo(hashSum, destParentHashSum, destFileHash, tagbase_db, true);
	}

	if (!insideTransaction)
	{
		q(tagbase_db, "COMMIT");
	}
}
