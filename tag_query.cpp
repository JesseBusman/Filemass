#include <array>
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <climits>

#include "util.h"
#include "sqlite3.h"
#include "tag_query.h"
#include "sha256.h"

bool TagQuery::matches(const std::array<char, 32>& hashSum, sqlite3* tagbase_db) const
{
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
		bool ret = false;
		if (stepResult == SQLITE_ROW) ret = true;
		else if (stepResult == SQLITE_DONE) ret = false;
		else
		{
			exitWithError("Query error in matches TagQueryType::HAS_CHILD " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
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
			exitWithError("Query error in matches TagQueryType::HAS_CHILD_WITH_QUERY " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
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
			bool ret = false;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else exitWithError("Query error in matches #1 TagQueryType::HAS_DESCENDANT " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
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
			bool ret = false;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else exitWithError("Query error in matches #2 TagQueryType::HAS_DESCENDANT " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
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
			bool ret = false;
			if (stepResult == SQLITE_ROW) ret = true;
			else if (stepResult == SQLITE_DONE) ret = false;
			else exitWithError("Query error in matches #3 TagQueryType::HAS_DESCENDANT " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
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

void TagQuery::findIn(const std::array<char, 32>& parentHashSum, std::map<std::array<char, 32>, bool>& result, sqlite3* tagbase_db) const
{
	if (this->type == TagQueryType::HAS_CHILD)
	{
		std::array<char, 32> searchHash = ((TagQuery_HasChildTag*)this)->hash;
		if (DEBUGGING) std::cout << "TagQueryType::HAS_CHILD findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
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
		if (stepResult != SQLITE_DONE) exitWithError("Query error in findIn(..) on HAS_CHILD: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
	}
	else if (this->type == TagQueryType::HAS_DESCENDANT)
	{
		if (parentHashSum == ZERO_HASH)
		{
			std::array<char, 32> searchHash = ((TagQuery_HasDescendantTag*)this)->hash;
			if (DEBUGGING) std::cout << "TagQueryType::HAS_DESCENDANT findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
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
			if (stepResult != SQLITE_DONE) exitWithError("Query error in findIn(..) on HAS_DESCENDANT: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
		}
		else
		{
			throw "Not implemented: finding subtags with a descendant";
		}
	}
	else if (this->type == TagQueryType::NOT)
	{
		std::shared_ptr<TagQuery> sub = ((TagQuery_Not*)this)->subQuery;
		if (sub->type == TagQueryType::AND)
		{
			// !(a && b)    =>   (!a) || (!b)
			for (auto operand : std::dynamic_pointer_cast<TagQuery_And>(sub)->operands)
			{
				TagQuery_Not(operand).findIn(parentHashSum, result, tagbase_db);
			}
		}
		else if (sub->type == TagQueryType::OR)
		{
			// !(a || b)    =>   (!a) && (!b)
			std::vector<std::shared_ptr<TagQuery>> invertedOperands;
			for (auto operand : std::dynamic_pointer_cast<TagQuery_Or>(sub)->operands)
			{
				invertedOperands.push_back(std::make_shared<TagQuery_Not>(operand));
			}
			TagQuery_And(invertedOperands).findIn(parentHashSum, result, tagbase_db);
		}
		else if (parentHashSum == ZERO_HASH)
		{
			if (sub->type == TagQueryType::HAS_DESCENDANT)
			{
				if (DEBUGGING) std::cout << "running findFiles on !~\r\n";

				sqlite3_stmt* stmt = p(
					tagbase_db,
					"SELECT DISTINCT e._file_hash FROM edges AS e WHERE NOT EXISTS(SELECT e2._file_hash FROM edges AS e2 WHERE e2._file_hash=e._file_hash AND e2._this_hash=? LIMIT 1)"
				);

				sqlite3_bind_blob(stmt, 1, std::dynamic_pointer_cast<TagQuery_HasDescendantTag>(sub)->hash.data(), 32, SQLITE_STATIC);
				
				int stepResult;
				while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
				{
					result[sqlite3_column_32chars(stmt, 0)] = true;
				}
				if (stepResult != SQLITE_DONE) throw "ERROR IN QUERY 9849845";
			}
			else if (sub->type == TagQueryType::HAS_CHILD)
			{
				sqlite3_stmt* stmt = p(
					tagbase_db,
					"SELECT DISTINCT e._file_hash FROM edges AS e WHERE NOT EXISTS(SELECT e2._file_hash FROM edges AS e2 WHERE e2.parent_hash_sum=e._file_hash AND e2._this_hash=? LIMIT 1)"
				);
				
				sqlite3_bind_blob(stmt, 1, std::dynamic_pointer_cast<TagQuery_HasChildTag>(sub)->hash.data(), 32, SQLITE_STATIC);
				
				int stepResult;
				while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
				{
					result[sqlite3_column_32chars(stmt, 0)] = true;
				}
				if (stepResult != SQLITE_DONE) throw "ERROR IN QUERY 938458934";
			}
			else if (sub->type == TagQueryType::HAS_CHILD_WITH_QUERY)
			{
				if (DEBUGGING) std::cout << "running findFiles on !test[hallo]\r\n";
				// this = !test[hallo]
				// sub  = test[hallo]
				
				
				// First, fetch all files that don't have a 'test'
				{
					sqlite3_stmt* stmt = p(
						tagbase_db,
						"SELECT DISTINCT e._file_hash FROM edges AS e WHERE NOT EXISTS(SELECT e2._file_hash FROM edges AS e2 WHERE e2.parent_hash_sum=e._file_hash AND e2._this_hash=? LIMIT 1)"
					);
					
					sqlite3_bind_blob(stmt, 1, std::dynamic_pointer_cast<TagQuery_HasChildTagWithQuery>(sub)->hash.data(), 32, SQLITE_STATIC);
					
					int stepResult;
					while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
					{
						if (DEBUGGING) std::cout << "Found a file without test!\r\n";
						result[sqlite3_column_32chars(stmt, 0)] = true;
					}
					if (stepResult != SQLITE_DONE) exitWithError("Query #1 error in findIn(..) on NOT > HAS_CHILD_WITH_QUERY: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
				}
				
				// Now, fetch all files that do have a 'test'
				{
					sqlite3_stmt* stmt = p(
						tagbase_db,
						"SELECT parent_hash_sum, hash_sum FROM edges WHERE _this_hash=?"
					);
					sqlite3_bind_blob(stmt, 1, std::dynamic_pointer_cast<TagQuery_HasChildTagWithQuery>(sub)->hash.data(), 32, SQLITE_STATIC);
					
					int stepResult;
					std::vector<std::pair<std::array<char, 32>, std::array<char, 32>>> temp;
					while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
					{
						std::cout << "Found a tag with test as parent!\r\n";
						temp.push_back({sqlite3_column_32chars(stmt, 0), sqlite3_column_32chars(stmt, 1)});
					}
					if (stepResult != SQLITE_DONE) exitWithError("Query #2 error in findIn(..) on NOT > HAS_CHILD_WITH_QUERY: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
					
					
					for (std::pair<std::array<char, 32>, std::array<char, 32>> tt : temp)
					{
						std::cout << "Found a file with test! " << bytes_to_hex(tt.first) << " " << bytes_to_hex(tt.second) << "\r\n";
						if (!std::dynamic_pointer_cast<TagQuery_HasChildTagWithQuery>(sub)->query->matches(tt.second, tagbase_db))
						{
							result[tt.first] = true;
						}
					}
				}
			}
			else
			{
				throw "Not implemented: finding files with negative query of type " + std::to_string((int)sub->type);
			}
		}
		else
		{
			throw "Not implemented: finding subtags with negative query";
		}
	}
	else if (this->type == TagQueryType::HAS_CHILD_WITH_QUERY)
	{
		std::array<char, 32> searchHash = ((TagQuery_HasChildTagWithQuery*)this)->hash;
		std::shared_ptr<TagQuery> query = ((TagQuery_HasChildTagWithQuery*)this)->query;
		
		if (DEBUGGING) std::cout << "TagQueryType::HAS_CHILD_WITH_QUERY findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
		
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
		if (stepResult != SQLITE_DONE) exitWithError("Query error in findIn(..) on HAS_CHILD_WITH_QUERY: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
		
		for (std::pair<std::array<char, 32>, std::array<char, 32>> tt : temp)
		{
			if (query->matches(tt.second, tagbase_db))
			{
				result[tt.first] = true;
			}
		}
	}
	else if (this->type == TagQueryType::HAS_DESCENDANT_WITH_QUERY)
	{
		if (parentHashSum == ZERO_HASH)
		{
			std::array<char, 32> searchHash = ((TagQuery_HasChildTagWithQuery*)this)->hash;
			std::shared_ptr<TagQuery> query = ((TagQuery_HasChildTagWithQuery*)this)->query;
			
			if (DEBUGGING) std::cout << "TagQueryType::HAS_CHILD_WITH_QUERY findIn(" << bytes_to_hex(parentHashSum) << ") searchHash=" << bytes_to_hex(searchHash) << "\r\n";
			
			sqlite3_stmt* stmt = p(
				tagbase_db,
				"SELECT _file_hash, hash_sum FROM edges WHERE _this_hash=?"
			);
			sqlite3_bind_blob(stmt, 1, searchHash.data(), 32, SQLITE_STATIC);
			sqlite3_bind_blob(stmt, 2, parentHashSum.data(), 32, SQLITE_STATIC);
			int stepResult;
			std::vector<std::pair<std::array<char, 32>, std::array<char, 32>>> temp;
			while ((stepResult = sqlite3_step(stmt)) == SQLITE_ROW)
			{
				temp.push_back({sqlite3_column_32chars(stmt, 0), sqlite3_column_32chars(stmt, 1)});
			}
			if (stepResult != SQLITE_DONE) exitWithError("Query error in findIn(..) on HAS_DESCENDANT_WITH_QUERY: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
			
			for (std::pair<std::array<char, 32>, std::array<char, 32>> tt : temp)
			{
				if (query->matches(tt.second, tagbase_db))
				{
					result[tt.first] = true;
				}
			}
		}
		else
		{
			exitWithError("Not implemented: HAS_DESCENDANT_WITH_QUERY on subtags");
		}
	}
	else if (this->type == TagQueryType::OR)
	{
		for (std::shared_ptr<TagQuery> subQuery : ((TagQuery_Or*)this)->operands)
		{
			this->findIn(parentHashSum, result, tagbase_db);
		}
	}
	else if (this->type == TagQueryType::XOR)
	{
		auto t = ((TagQuery_Xor*)this);
		std::map<std::array<char, 32>, bool> temp1;
		t->operands[0]->findIn(parentHashSum, temp1, tagbase_db);
		
		for (unsigned int i=1; i<t->operands.size(); i++)
		{
			std::map<std::array<char, 32>, bool> temp2;
			t->operands[i]->findIn(parentHashSum, temp2, tagbase_db);
			for (const auto& [key, value] : temp2)
			{
				if (value == true)
				{
					temp1[key] ^= true;
				}
			}
		}
		
		for (const auto& [key, value] : temp1)
		{
			if (value == true) result[key] = true;
		}
	}
	else if (this->type == TagQueryType::AND)
	{
		const auto& operands = ((TagQuery_And*)this)->operands;
		
		std::map<int, long long> operand_to_count;
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
		long long operand0cardinality = operand_to_count.begin()->second;
		long long operand1cardinality = (operand_to_count.begin()++)->second;
		
		if (DEBUGGING) std::cout << "TagQueryType::AND: operand0cardinality=" << operand0cardinality << " operand1cardinality=" << operand1cardinality << "\r\n";
		
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

long long TagQuery::quickCount(sqlite3* tagbase_db) const
{
	if (this->type == TagQueryType::HAS_CHILD || this->type == TagQueryType::HAS_DESCENDANT || this->type == TagQueryType::HAS_CHILD_WITH_QUERY)
	{
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT COUNT(_this_hash) FROM edges WHERE _this_hash=?"
		);
		sqlite3_bind_blob(stmt, 1, ((TagQuery_HasTag*)this)->hash.data(), 32, SQLITE_STATIC);
		int stepResult = sqlite3_step(stmt);
		if (stepResult != SQLITE_ROW) exitWithError("Query error in quickCount(..) on HAS_CHILD/HAS_DESCENDANT/HAS_CHILD_WITH_QUERY: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
		return sqlite3_column_int64(stmt, 0);
	}
	else if (this->type == TagQueryType::OR)
	{
		long long ret = 0;
		for (std::shared_ptr<TagQuery> operand : ((TagQuery_Or*)this)->operands)
		{
			ret += operand->quickCount(tagbase_db);
		}
		return ret;
	}
	else if (this->type == TagQueryType::XOR)
	{
		long long ret = 0;
		for (std::shared_ptr<TagQuery> operand : ((TagQuery_Xor*)this)->operands)
		{
			ret += operand->quickCount(tagbase_db);
		}
		return ret;
	}
	else if (this->type == TagQueryType::AND)
	{
		long long ret = LLONG_MAX;
		for (std::shared_ptr<TagQuery> operand : ((TagQuery_And*)this)->operands)
		{
			long long count = operand->quickCount(tagbase_db);
			if (count < ret) ret = count;
		}
		return ret;
	}
	else if (this->type == TagQueryType::NOT)
	{
		sqlite3_stmt* stmt = p(
			tagbase_db,
			"SELECT COUNT(_this_hash) FROM edges"
		);
		int stepResult = sqlite3_step(stmt);
		if (stepResult != SQLITE_ROW) exitWithError("Query error in quickCount(..) on NOT: " + std::to_string(stepResult) + sqlite3_errmsg(tagbase_db));
		return sqlite3_column_int64(stmt, 0) - ((TagQuery_Not*)this)->subQuery->quickCount(tagbase_db);
	}
	else
	{
		throw "patatje joppiesaus" + std::to_string((int)this->type);
	}
}

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
	SHA256 ctx;
	ctx.update((const unsigned char*)_tagName.data(), _tagName.length());
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

std::string TagQuery::toString() const
{
	return "UNKNOWN_QUERY";
}

std::string TagQuery_Or::toString() const
{
	std::string ret = "";
	for (size_t i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " | ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_And::toString() const
{
	std::string ret = "";
	for (size_t i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " & ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_Xor::toString() const
{
	std::string ret = "";
	for (size_t i=0; i<this->operands.size(); i++)
	{
		if (i != 0) ret += " ^ ";
		ret += "(" + this->operands[i]->toString() + ")";
	}
	return ret;
}

std::string TagQuery_Not::toString() const
{
	return "! " + this->subQuery->toString();
}

std::string TagQuery_HasChildTag::toString() const
{
	return std::string(this->tagName);
}

std::string TagQuery_HasDescendantTag::toString() const
{
	return "~ " + std::string(this->tagName);
}

std::string TagQuery_HasChildTagWithQuery::toString() const
{
	if (DEBUGGING) std::cout << "tagname=" << this->tagName << " length=" << this->tagName.length() << "\r\n";
	return std::string(this->tagName) + "[" + this->query->toString() + "]";
}

std::string TagQuery_HasDescendantTagWithQuery::toString() const
{
	return "~ " + std::string(this->tagName) + "[" + this->query->toString() + "]";
}
