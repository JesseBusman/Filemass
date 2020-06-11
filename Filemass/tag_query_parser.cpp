#include <string>
#include <sstream>
#include <iostream>

#include "util.h"
#include "tag_query_parser.h"

std::string readTagName(const std::string& str, int& pos);

/*
GRAMMAR:
TAG_QUERY = 
	TagQueryType::WITHIN_OR ( | TagQueryType::WITHIN_OR )*

TagQueryType::WITHIN_OR =
	TagQueryType::WITHIN_XOR ( | TagQueryType::WITHIN_XOR )*

TagQueryType::WITHIN_XOR =
	TagQueryType::WITHIN_AND ( | TagQueryType::WITHIN_AND )*

TagQueryType::WITHIN_AND =
	TagQueryType::WITHIN_NOT
	! TagQueryType::WITHIN_NOT

TagQueryType::WITHIN_NOT =
	TAG_NAME
	~ TAG_NAME
	TAG_NAME [ TAG_QUERY ]
	~ TAG_NAME [ TAG_QUERY ]
	( TAG_QUERY )
*/

void skipWhitespace(const std::string& str, int& pos)
{
	while (str[pos] == '\r' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == ' ')
	{
		pos++;
	}

	if (DEBUGGING) std::cout << "[Tag Query Parser] after skipWhitespace(" << str.substr(pos) << ")\r\n";
}

void tagQuerySyntaxError(const std::string& str, int pos1, int pos2, const char* msg)
{
	if (pos2 == pos1) pos2 = -1;
	else if (pos2 != -1 && pos1 > pos2) std::swap(pos1, pos2);
	std::stringstream out;
	out << "\r\n" << str << "\r\n";
	for (int i=0; i<pos1; i++) out << ' ';
	out << '^';
	if (pos2 != -1)
	{
		for (int i=0; i<pos2-pos1-1; i++) out << ' ';
		out << '^';
	}
	out << "\r\n";
	out << msg << "\r\n";
	exitWithError(out.str());
}

void tagQuerySyntaxError(const std::string& str, int pos, const char* msg)
{
	tagQuerySyntaxError(str, pos, -1, msg);
}

void tagQuerySyntaxError(const std::string& str, int pos, const std::string& msg)
{
	tagQuerySyntaxError(str, pos, msg.c_str());
}

std::shared_ptr<TagQuery> _parseTagQuery(const std::string& str, int& pos, int depth);

std::shared_ptr<TagQuery> parseTagQueryWithinNot(const std::string& str, int& pos, int depth)
{
	if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinNot(" << str.substr(pos) << ")\r\n";

	skipWhitespace(str, pos);

	// TagQueryType::WITHIN_NOT = ( TAG_QUERY )
	if (str[pos] == '(')
	{
		int openingBracketPos = pos;
		pos++;

		std::shared_ptr<TagQuery> ret = _parseTagQuery(str, pos, depth+1);
		skipWhitespace(str, pos);

		if (str[pos] != ')') tagQuerySyntaxError(str, openingBracketPos, pos, "Syntax error in tag query: ( not closed with )");
		pos++;

		return ret;
	}
	
	else
	{
		bool tilda = false;
		if (str[pos] == '~')
		{
			pos++;
			tilda = true;

			skipWhitespace(str, pos);
		}
		
		std::string tagName = readTagName(str, pos);

		skipWhitespace(str, pos);
		
		if (str[pos] == '[')
		{
			int openingBracketPos = pos;
			pos++;
			skipWhitespace(str, pos);
			std::shared_ptr<TagQuery> query = _parseTagQuery(str, pos, depth+1);
			skipWhitespace(str, pos);

			if (str[pos] != ']') tagQuerySyntaxError(str, openingBracketPos, pos, "Syntax error in tag query: [ not closed with ]");
			pos++;

			// TagQueryType::WITHIN_NOT = ~ TAG_NAME [ TAG_QUERY ]
			if (tilda)
			{
				if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinNot returning TagQuery_HasDescendantTagWithQuery(" << tagName << ")\r\n";
				return std::make_shared<TagQuery_HasDescendantTagWithQuery>(tagName, query);
			}

			// TagQueryType::WITHIN_NOT = TAG_NAME [ TAG_QUERY ]
			else
			{
				if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinNot returning TagQuery_HasChildTagWithQuery(" << tagName << ")\r\n";
				return std::make_shared<TagQuery_HasChildTagWithQuery>(tagName, query);
			}
		}

		// TagQueryType::WITHIN_NOT = ~ TAG_NAME
		else if (tilda)
		{
			if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinNot returning TagQuery_HasDescendantTag(" << tagName << ")\r\n";
			return std::make_shared<TagQuery_HasDescendantTag>(tagName);
		}

		// TagQueryType::WITHIN_NOT = TAG_NAME
		else
		{
			if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinNot returning TagQuery_HasChildTag(" << tagName << ")\r\n";
			return std::make_shared<TagQuery_HasChildTag>(tagName);
		}
	}
}

std::shared_ptr<TagQuery> parseTagQueryWithinAnd(const std::string& str, int& pos, int depth)
{
	if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinAnd(" << str.substr(pos) << ")\r\n";

	skipWhitespace(str, pos);
	if (str[pos] == '!')
	{
		pos++;
		std::shared_ptr<TagQuery> subQuery = parseTagQueryWithinNot(str, pos, depth+1);
		if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinAnd returning TagQuery_Not\r\n";
		return std::make_shared<TagQuery_Not>(subQuery);
	}
	else
	{
		if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinAnd calling parseTagQueryWithinNot\r\n";
		return parseTagQueryWithinNot(str, pos, depth+1);
	}
}

std::shared_ptr<TagQuery> parseTagQueryWithinXor(const std::string& str, int& pos, int depth)
{
	if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinXor(" << str.substr(pos) << ")\r\n";

	std::vector<std::shared_ptr<TagQuery>> operands;
	skipWhitespace(str, pos);
	operands.push_back(parseTagQueryWithinAnd(str, pos, depth+1));
	skipWhitespace(str, pos);

	if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinXor parsed one operand, remaining: " << str.substr(pos) << "\r\n";

	while (str[pos] == '&')
	{
		pos++;
		operands.push_back(parseTagQueryWithinAnd(str, pos, depth+1));
		skipWhitespace(str, pos);
	}

	if (operands.size() == 1)
	{
		return operands[0];
	}
	else
	{
		return std::make_shared<TagQuery_And>(operands);
	}
}

std::shared_ptr<TagQuery> parseTagQueryWithinOr(const std::string& str, int& pos, int depth)
{
	if (DEBUGGING) std::cout << "[Tag Query Parser] parseTagQueryWithinOr(" << str.substr(pos) << ")\r\n";

	std::vector<std::shared_ptr<TagQuery>> operands;
	skipWhitespace(str, pos);
	operands.push_back(parseTagQueryWithinXor(str, pos, depth+1));
	skipWhitespace(str, pos);
	while (str[pos] == '^')
	{
		pos++;
		operands.push_back(parseTagQueryWithinXor(str, pos, depth+1));
		skipWhitespace(str, pos);
	}

	if (operands.size() == 1)
	{
		return operands[0];
	}
	else
	{
		return std::make_shared<TagQuery_Xor>(operands);
	}
}

std::shared_ptr<TagQuery> _parseTagQuery(const std::string& str, int& pos, int depth)
{
	if (DEBUGGING) std::cout << "[Tag Query Parser] _parseTagQuery(" << str.substr(pos) << ")\r\n";

	std::vector<std::shared_ptr<TagQuery>> operands;
	skipWhitespace(str, pos);
	operands.push_back(parseTagQueryWithinOr(str, pos, depth+1));
	skipWhitespace(str, pos);
	while (str[pos] == '|')
	{
		pos++;
		operands.push_back(parseTagQueryWithinOr(str, pos, depth+1));
		skipWhitespace(str, pos);
	}

	if (operands.size() == 1)
	{
		return operands[0];
	}
	else
	{
		return std::make_shared<TagQuery_Or>(operands);
	}
}

std::shared_ptr<TagQuery> parseTagQuery(const std::string& str)
{
	int pos = 0;
	std::shared_ptr<TagQuery> ret = _parseTagQuery(str, pos, 0);
	skipWhitespace(str, pos);
	if (pos != str.size())
	{
		tagQuerySyntaxError(str, pos, "Syntax error in tag query: Unexpected character");
	}
	return ret;
}
