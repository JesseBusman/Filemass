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

std::shared_ptr<Tag> findTagsOfFile(const std::array<char, 32>& fileHash, sqlite3* tagbase_db)
{
	std::shared_ptr<Tag> ret = std::make_shared<Tag>(ZERO_HASH, fileHash, fileHash, fileHash);
	std::map<std::array<char, 32>, std::shared_ptr<Tag>> hash_sum__to__tag;
	std::map<std::array<char, 32>, std::vector<std::shared_ptr<Tag>>> parent_hash_sum__to__child_tags;

	sqlite3_stmt* stmt = p(
		tagbase_db,
		"SELECT parent_hash_sum, hash_sum, _this_hash FROM edges WHERE _file_hash=?"
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
		
		auto tag = std::make_shared<Tag>(parent_hash_sum, hash_sum, _this_hash, fileHash);
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
		for (int i=0; i<subtags.size(); i++)
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

	std::cout << bytes_to_hex(*thisHash) << "->addTo(" << bytes_to_hex(destParentHashSum) << ")\r\n";

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
			std::cout << "\r\n" << stepResult;
			std::cout << "\r\nsqlite3_step did not return SQLITE_DONE on query INSERT INTO hashed_data\r\n";
			std::cout << sqlite3_errmsg(tagbase_db) << "\r\n";
			exit(1);
			throw 1;
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
		std::cout << "\r\n" << stepResult;
		std::cout << "\r\nsqlite3_step did not return SQLITE_DONE on query INSERT INTO EDGES\r\n";
		std::cout << sqlite3_errmsg(tagbase_db) << "\r\n";
		exit(1);
		throw 1;
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

bool TagQuery::matches(const std::array<char, 32>& hashSum, sqlite3* tagbase_db)
{
	//std::cout << "TagQuery::";
	if (this->type == TagQueryType::HAS_CHILD)
	{
		const std::array<char, 32>& searchHash = ((TagQuery_HasChildTag*)this)->hash;
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT parent_hash_sum FROM edges WHERE _this_hash=? AND parent_hash_sum=? LIMIT 1"
		);
		sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
		sqlite3_bind_blob(stmt, 2, hashSum.data(), 32, SQLITE_STATIC);
		int stepResult = sqlite3_step(stmt);
		bool ret;
		if (stepResult == SQLITE_ROW) ret = true;
		else if (stepResult == SQLITE_DONE) ret = false;
		else
		{
			throw "Query error in matches TagQueryType::HAS_CHILD";
		}
		return ret;
	}
	else if (this->type == TagQueryType::HAS_CHILD_WITH_QUERY)
	{
		const std::array<char, 32>& searchHash = ((TagQuery_HasChildTag*)this)->hash;
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT hash_sum FROM edges WHERE _this_hash=? AND parent_hash_sum=?"
		);
		sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
		sqlite3_bind_blob(stmt, 2, hashSum.data(), 32, SQLITE_STATIC);
		int stepResult;
		std::vector<std::array<char, 32>> temp;
		while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			temp.push_back(sqlite3_column_32chars(stmt, 0));
		}
		if (stepResult != SQLITE_DONE && stepResult != SQLITE_ROW)
		{
			throw "Query error in matches TagQueryType::HAS_CHILD_WITH_QUERY";
		}

		for (std::array<char, 32> hash_sum : temp)
		{
			if (((TagQuery_HasChildTagWithQuery*)this)->query->matches(hash_sum, tagbase_db)) return true;
		}

		return false;
	}
	else if (this->type == TagQueryType::HAS_DESCENDANT)
	{
		const std::array<char, 32>& searchHash = ((TagQuery_HasChildTag*)this)->hash;

		{
			sqlite3_stmt* stmt = p(
				tagbase_db,
				"SELECT parent_hash_sum FROM edges WHERE _this_hash=? AND parent_hash_sum=? LIMIT 1"
			);
			sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 2, hashSum.data(), 32, SQLITE_STATIC);
			int stepResult = sqlite3_step(stmt);
			bool ret;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else throw "Query error in matches TagQueryType::HAS_CHILD";
			if (ret == true) return true;
		}

		{
			sqlite3_stmt* stmt = p(
				tagbase_db,
				"SELECT _grandparent_hash_sum FROM edges WHERE _this_hash=? AND _grandparent_hash_sum=? LIMIT 1"
			);
			sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 2, hashSum.data(), 32, SQLITE_STATIC);
			int stepResult = sqlite3_step(stmt);
			bool ret;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else throw "Query error in matches TagQueryType::HAS_CHILD";
			if (ret == true) return true;
		}
		
		{
			sqlite3_stmt* stmt = p(
				tagbase_db,
				"SELECT e1._grandparent_hash_sum "
				"FROM edges AS e1 "
				"INNER JOIN edges AS e2 ON e1.hash_sum=e2._grandparent_hash_sum "
				"WHERE e1._grandparent_hash_sum=? AND (e2._grandparent_hash_sum=? OR e2.parent_hash_sum=? OR e2.hash_sum=?) "
				"LIMIT 1"
			);
			sqlite3_bind_blob(stmt, 1, hashSum.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 2, searchHash.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 3, searchHash.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 4, searchHash.data(), 32, SQLITE_STATIC);
			int stepResult = sqlite3_step(stmt);
			bool ret;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else throw "Query error in matches TagQueryType::HAS_CHILD";
			if (ret == true) return true;
		}
		
		// TODO search at distances > 5
		return false;
	}
	else if (this->type == TagQueryType::NOT)
	{
		return ! ((TagQuery_Not*)this)->subQuery->matches(hashSum, tagbase_db);
	}
	else if (this->type == TagQueryType::OR)
	{
		for (std::shared_ptr<TagQuery> subQuery : ((TagQuery_Or*)this)->operands)
		{
			if (subQuery->matches(hashSum, tagbase_db)) return true;
		}
		return false;
	}
	else if (this->type == TagQueryType::AND)
	{
		for (std::shared_ptr<TagQuery> subQuery : ((TagQuery_And*)this)->operands)
		{
			if (!subQuery->matches(hashSum, tagbase_db)) return false;
		}
		return true;
	}
	else if (this->type == TagQueryType::XOR)
	{
		bool ret = false;
		for (std::shared_ptr<TagQuery> subQuery : ((TagQuery_Xor*)this)->operands)
		{
			if (subQuery->matches(hashSum, tagbase_db)) ret = !ret;
		}
		return ret;
	}
	else
	{
		throw "TagQuery::matches unimplemented for " + std::to_string((int)this->type);
	}
}

void TagQuery::findIn(const std::array<char, 32>& parentHashSum, std::map<std::array<char, 32>, bool>& result, sqlite3* tagbase_db)
{
	if (this->type == TagQueryType::HAS_CHILD)
	{
		std::array<char, 32> searchHash = ((TagQuery_HasChildTag*)this)->hash;
		//std::cout << "TagQueryType::HAS_CHILD findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT parent_hash_sum FROM edges WHERE _this_hash=? AND _grandparent_hash_sum=?"
		);
		sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
		sqlite3_bind_blob(stmt, 2, parentHashSum.data(), 32, SQLITE_STATIC);
		int stepResult;
		while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			std::array<char, 32> parent_hash_sum = sqlite3_column_32chars(stmt, 0);
			result[parent_hash_sum] = true;
		}
		if (stepResult != SQLITE_DONE) throw "ERROR IN QUERY 948348984593";
	}
	else if (this->type == TagQueryType::HAS_DESCENDANT)
	{
		if (parentHashSum == ZERO_HASH)
		{
			std::array<char, 32> searchHash = ((TagQuery_HasChildTag*)this)->hash;
			//std::cout << "TagQueryType::HAS_DESCENDANT findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
			sqlite3_stmt* stmt = p(
				tagbase_db,
				"SELECT DISTINCT _file_hash FROM edges WHERE _this_hash=?"
			);
			sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
			int stepResult;
			while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
			{
				std::array<char, 32> file_hash = sqlite3_column_32chars(stmt, 0);
				result[file_hash] = true;
			}
			if (stepResult != SQLITE_DONE) throw "ERROR IN QUERY 36225562";
		}
		else
		{
			throw "Not implemented: finding subtags with a descendant";
		}
	}
	else if (this->type == TagQueryType::HAS_CHILD)
	{

	}
	else if (this->type == TagQueryType::HAS_CHILD_WITH_QUERY)
	{
		std::array<char, 32> searchHash = ((TagQuery_HasChildTagWithQuery*)this)->hash;
		std::shared_ptr<TagQuery> query = ((TagQuery_HasChildTagWithQuery*)this)->query;

		//std::cout << "TagQueryType::HAS_CHILD_WITH_QUERY findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";

		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT parent_hash_sum, hash_sum FROM edges WHERE _this_hash=? AND _grandparent_hash_sum=?"
		);
		sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
		sqlite3_bind_blob(stmt, 2, parentHashSum.data(), 32, SQLITE_STATIC);
		int stepResult;
		std::vector<std::pair<std::array<char, 32>, std::array<char, 32>>> temp;
		while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			temp.push_back({sqlite3_column_32chars(stmt, 0), sqlite3_column_32chars(stmt, 1)});
		}
		if (stepResult != SQLITE_DONE) throw "ERROR IN QUERY 92349832";

		for (std::pair<std::array<char, 32>, std::array<char, 32>> tt : temp)
		{
			if (query->matches(tt.second, tagbase_db))
			{
				result[tt.first] = true;
			}
		}
	}
	else if (this->type == TagQueryType::OR)
	{
		for (std::shared_ptr<TagQuery> subQuery : ((TagQuery_Or*)this)->operands)
		{
			this->findIn(parentHashSum, result, tagbase_db);
		}
	}
	else if (this->type == TagQueryType::AND)
	{
		const auto& operands = ((TagQuery_And*)this)->operands;

		std::map<int, long> operand_to_count;
		{
			int i = 0;
			for (std::shared_ptr<TagQuery> subQuery : operands)
			{
				operand_to_count[i] = subQuery->quickCount(tagbase_db);
				i++;
			}
		}

		std::shared_ptr<TagQuery> operand0 = operands[operand_to_count.begin()->first];
		std::shared_ptr<TagQuery> operand1 = operands[(operand_to_count.begin()++)->first];
		int operand0cardinality = operand_to_count.begin()->second;
		int operand1cardinality = (operand_to_count.begin()++)->second;

		//std::cout << "TagQueryType::AND: operand0cardinality=" << operand0cardinality << " operand1cardinality=" << operand1cardinality << "\r\n";

		//if (operand0cardinality < 1000)
		{
			std::map<std::array<char, 32>, bool> matchingOperand0;

			operand0->findIn(parentHashSum, matchingOperand0, tagbase_db);

			for (const auto& [yo, _] : matchingOperand0)
			{
				bool allMatch = true;
				auto i = operand_to_count.begin();
				i++;
				while (i != operand_to_count.end())
				{
					const auto& [operandIndex, _] = *i;
					if (!operands[operandIndex]->matches(yo, tagbase_db)) { allMatch = false; break; }
					i++;
				}
				if (allMatch)
				{
					result[yo] = true;
				}
			}
		}
		/*else
		{
		}*/


	}
	else
	{
		throw "findFiles(): unimplemented for tag query type " + std::to_string((int)this->type);
	}
}

long TagQuery::quickCount(sqlite3* tagbase_db)
{
	if (this->type == TagQueryType::HAS_CHILD || this->type == TagQueryType::HAS_DESCENDANT)
	{
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT COUNT(_this_hash) FROM edges WHERE _this_hash=?"
		);
		sqlite3_bind_blob(stmt, 1, ((TagQuery_HasTag*)this)->hash.data(), 32, SQLITE_STATIC);
		if (sqlite3_step(stmt) != SQLITE_ROW) throw "not row :((((";
		long ret = sqlite3_column_int64(stmt, 0);
		return ret;
	}
	else if (this->type == TagQueryType::OR)
	{
		long ret = 0;
		for (std::shared_ptr<TagQuery> operand : ((TagQuery_Or*)this)->operands)
		{
			ret += operand->quickCount(tagbase_db);
		}
		return ret;
	}
	else
	{
		throw "patatje joppiesaus";
	}
}

/*
int TagQuery::countTags(sqlite3* tagbase_db)
{
}

int TagQuery::findTags(const std::array<char, 32>& fileHash, sqlite3* tagbase_db)
{
}
*/

TagQuery::TagQuery(TagQueryType _type):
	type(_type)
{
}

TagQuery_Or::TagQuery_Or(const std::vector<std::shared_ptr<TagQuery>>& _operands):
	TagQuery(TagQueryType::OR),
	operands(_operands)
{
}

TagQuery_And::TagQuery_And(const std::vector<std::shared_ptr<TagQuery>>& _operands):
	TagQuery(TagQueryType::AND),
	operands(_operands)
{
}

TagQuery_Xor::TagQuery_Xor(const std::vector<std::shared_ptr<TagQuery>>& _operands):
	TagQuery(TagQueryType::XOR),
	operands(_operands)
{
}

TagQuery_Not::TagQuery_Not(const std::shared_ptr<TagQuery>& _subQuery):
	TagQuery(TagQueryType::NOT),
	subQuery(_subQuery)
{
}

TagQuery_HasTag::TagQuery_HasTag(TagQueryType _type, const std::string& _tagName):
	TagQuery(_type),
	tagName(_tagName)
{
	//std::cout << "TagQuery_HasTag(" << _tagName << ") length=" << _tagName.length() << "\r\n";
	SHA256 ctx;
	ctx.update((unsigned char*)_tagName.data(), _tagName.length());
	ctx.final((unsigned char*)this->hash.data());
}

TagQuery_HasChildTag::TagQuery_HasChildTag(const std::string& _tagName):
	TagQuery_HasTag(TagQueryType::HAS_CHILD, _tagName)
{
}

TagQuery_HasDescendantTag::TagQuery_HasDescendantTag(const std::string& _tagName):
	TagQuery_HasTag(TagQueryType::HAS_DESCENDANT, _tagName)
{
}

TagQuery_HasChildTagWithQuery::TagQuery_HasChildTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query):
	TagQuery_HasTag(TagQueryType::HAS_CHILD_WITH_QUERY, _tagName),
	query(_query)
{
}

TagQuery_HasDescendantTagWithQuery::TagQuery_HasDescendantTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query):
	TagQuery_HasTag(TagQueryType::HAS_DESCENDANT_WITH_QUERY, _tagName),
	query(_query)
{
}

std::string TagQuery::toString()
{
	return "UNKNOWN_QUERY";
}

std::string TagQuery_Or::toString()
{
	std::string ret = "";
	for (int i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " | ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_And::toString()
{
	std::string ret = "";
	for (int i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " & ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_Xor::toString()
{
	std::string ret = "";
	for (int i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " ^ ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_Not::toString()
{
	return "! " + this->subQuery->toString();
}

std::string TagQuery_HasChildTag::toString()
{
	return std::string(this->tagName);
}

std::string TagQuery_HasDescendantTag::toString()
{
	return "~ " + std::string(this->tagName);
}

std::string TagQuery_HasChildTagWithQuery::toString()
{
	return std::string(this->tagName) + "[" + this->query->toString() + "]";
}

std::string TagQuery_HasDescendantTagWithQuery::toString()
{
	return "~ " + std::string(this->tagName) + "[" + this->query->toString() + "]";
}
