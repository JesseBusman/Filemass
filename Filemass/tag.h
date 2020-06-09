#pragma once

#include <string>
#include <memory>
#include <vector>
#include <array>
#include <map>
#include <optional>

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
};

std::shared_ptr<Tag> findTagsOfFile(const std::array<char, 32>& fileHash, sqlite3* tagbase_db);

enum class TagQueryType
{
	OR,
	AND,
	XOR,
	NOT,
	HAS_CHILD,
	HAS_DESCENDANT,
	HAS_CHILD_WITH_QUERY,
	HAS_DESCENDANT_WITH_QUERY,
};

class TagQuery
{
	//virtual void Save( std::ostream& ) const = 0;
public:
	TagQueryType type;
	TagQuery(TagQueryType _type);
	void findIn(const std::array<char, 32>& parentHashSum, std::map<std::array<char, 32>, bool>& result, sqlite3* tagbase_db);
	bool matches(const std::array<char, 32>& hashSum, sqlite3* tagbase_db);
	long long quickCount(sqlite3* tagbase_db);
	virtual std::string toString();
};

class TagQuery_Or : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_Or(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString();
};

class TagQuery_And : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_And(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString();
};

class TagQuery_Xor : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_Xor(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString();
};

class TagQuery_Not : public TagQuery
{
public:
	std::shared_ptr<TagQuery> subQuery;
	TagQuery_Not(const std::shared_ptr<TagQuery>& _subQuery);
	virtual std::string toString();
};

class TagQuery_HasTag : public TagQuery
{
public:
	std::array<char, 32> hash;
	std::string tagName;
	TagQuery_HasTag(TagQueryType _type, const std::string& _tagName);
};

class TagQuery_HasChildTag : public TagQuery_HasTag
{
public:
	TagQuery_HasChildTag(const std::string& _tagName);
	virtual std::string toString();
};

class TagQuery_HasDescendantTag : public TagQuery_HasTag
{
public:
	std::string tagName;
	TagQuery_HasDescendantTag(const std::string& _tagName);
	virtual std::string toString();
};

class TagQuery_HasChildTagWithQuery : public TagQuery_HasTag
{
public:
	std::string tagName;
	std::shared_ptr<TagQuery> query;
	TagQuery_HasChildTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query);
	virtual std::string toString();
};

class TagQuery_HasDescendantTagWithQuery : public TagQuery_HasTag
{
public:
	std::string tagName;
	std::shared_ptr<TagQuery> query;
	TagQuery_HasDescendantTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query);
	virtual std::string toString();
};
