#pragma once

#include <string>
#include <array>
#include <map>
#include <memory>
#include <vector>

#include "sqlite3.h"

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
public:
	TagQueryType type;
	TagQuery(TagQueryType _type);
	void findIn(const std::array<char, 32>& parentHashSum, std::map<std::array<char, 32>, bool>& result, sqlite3* tagbase_db) const;
	bool matches(const std::array<char, 32>& hashSum, sqlite3* tagbase_db) const;
	long long quickCount(sqlite3* tagbase_db) const;
	virtual std::string toString() const;
};

class TagQuery_Or : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_Or(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString() const;
};

class TagQuery_And : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_And(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString() const;
};

class TagQuery_Xor : public TagQuery
{
public:
	std::vector<std::shared_ptr<TagQuery>> operands;
	TagQuery_Xor(const std::vector<std::shared_ptr<TagQuery>>& _operands);
	virtual std::string toString() const;
};

class TagQuery_Not : public TagQuery
{
public:
	std::shared_ptr<TagQuery> subQuery;
	TagQuery_Not(const std::shared_ptr<TagQuery>& _subQuery);
	virtual std::string toString() const;
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
	virtual std::string toString() const;
};

class TagQuery_HasDescendantTag : public TagQuery_HasTag
{
public:
	TagQuery_HasDescendantTag(const std::string& _tagName);
	virtual std::string toString() const;
};

class TagQuery_HasChildTagWithQuery : public TagQuery_HasTag
{
public:
	std::shared_ptr<TagQuery> query;
	TagQuery_HasChildTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query);
	virtual std::string toString() const;
};

class TagQuery_HasDescendantTagWithQuery : public TagQuery_HasTag
{
public:
	std::shared_ptr<TagQuery> query;
	TagQuery_HasDescendantTagWithQuery(const std::string& _tagName, const std::shared_ptr<TagQuery>& _query);
	virtual std::string toString() const;
};
