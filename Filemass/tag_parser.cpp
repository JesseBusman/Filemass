#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <stack>

#include "tag.h"
#include "tag_parser.h"

std::vector<std::shared_ptr<Tag>> parseTag(std::string str)
{
	enum {
		INSIDE_TAG_NAME,
		AFTER_TAG_NAME
	} tagParserState = INSIDE_TAG_NAME;

	int currentTagNameStart = 0;
	int currentTagNameLength = -1;

	std::shared_ptr<Tag> baseTag = std::make_shared<Tag>("");
	std::vector<std::shared_ptr<Tag>> stack { baseTag };

	for (size_t i=0; i<=str.length(); i++)
	{
		char c = str.c_str()[i];
		if (tagParserState == INSIDE_TAG_NAME)
		{
			if (c == '[' || c == ']' || c == ',' || c == 0x00)
			{
				currentTagNameLength = i - currentTagNameStart;
				tagParserState = AFTER_TAG_NAME;
				i--;
				continue;
			}
			else
			{
				//std::cout << "Added character " << c << " to tag\r\n";
			}
		}
		else if (tagParserState == AFTER_TAG_NAME)
		{
			std::shared_ptr<Tag> newTag;
			if (currentTagNameLength != -1)
			{
				newTag = std::make_shared<Tag>(str.substr(currentTagNameStart, currentTagNameLength)); //std::string_view(&str.c_str()[currentTagNameStart], currentTagNameLength));
				stack[stack.size()-1]->subtags.push_back(newTag);
				//std::cout << "after_tag_name newTag=" << newTag->name << " c=" << c << "\r\n";

				currentTagNameLength = -1;
			}
					
			if (c == '[')
			{
				if (newTag == nullptr) throw "Syntax error in tags: Unexpected character [";
				stack.push_back(newTag);
			}
			else if (c == ']')
			{
				stack.pop_back();
			}
			else if (c == ',')
			{
			}
			else if (c == 0x00)
			{
				break;
			}
			else
			{
				//std::cout << "New tag name starts with " << c << "\r\n";
				tagParserState = INSIDE_TAG_NAME;
				currentTagNameStart = i;
				currentTagNameLength = 0;
				//std::cout << "\r\nSyntax error in tag list: Unexpected character " << c << "\r\n";
				//exit(1);
				//return 1;
			}
		}
	}

	//std::cout << "Parsed " << baseTag->subtags.size() << " tags:\r\n";
	baseTag->debugPrint();

	return baseTag->subtags;
}

std::string readTagName(const std::string& str, int& pos)
{
	//std::cout << "readTagName(" << str.substr(pos) << ")\r\n";

	int startPos = pos;
	int lastNonWhitespace = -1;
	
	while (pos < str.size() && str[pos] != ',' && str[pos] != '[' && str[pos] != ']' && str[pos] != '&' && str[pos] != '|' && str[pos] != '^' && str[pos] != '!' && str[pos] != '~' && str[pos] != ')' && str[pos] != '(')
	{
		//std::cout << "char " << pos << " is '" << str[pos] << "'=" << ((int)str[pos]) << "\r\n";
		if (str[pos] == ' ' || str[pos] == '\r' || str[pos] == '\t' || str[pos] == '\n')
		{
			if (pos == startPos)
			{
				pos++;
				startPos++;
			}
			else
			{
				pos++;
			}
		}
		else
		{
			lastNonWhitespace = pos;
			pos++;
		}
	}

	if (lastNonWhitespace == -1) throw "Failed to parse tag query: expected tag name at pos " + std::to_string(startPos) + " in:\r\n" + str;

	//std::cout << "end: startPos=" << startPos << " lastNonWhitespace=" << lastNonWhitespace << "\r\n";

	return str.substr(startPos, lastNonWhitespace - startPos + 1); //std::string_view(&str.c_str()[startPos], lastNonWhitespace - startPos + 1);
}

void skipWhitespace(const std::string& str, int& pos)
{
	while (str[pos] == '\r' || str[pos] == '\n' || str[pos] == '\t' || str[pos] == ' ')
	{
		pos++;
	}

	//std::cout << "after skipWhitespace(" << str.substr(pos) << ")\r\n";
}

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


std::string repeat(char c, int n)
{
	char* ret = new char[n+1];
	ret[n] = 0x00;
	for (int i=0; i<n; i++) ret[i] = c;
	return std::string(ret);
}

std::shared_ptr<TagQuery> _parseTagQuery(const std::string& str, int& pos, int depth);

std::shared_ptr<TagQuery> parseTagQueryWithinNot(const std::string& str, int& pos, int depth)
{
	//std::cout << "parseTagQueryWithinNot(" << str.substr(pos) << ")\r\n";

	skipWhitespace(str, pos);

	// TagQueryType::WITHIN_NOT = ( TAG_QUERY )
	if (str[pos] == '(')
	{
		pos++;

		std::shared_ptr<TagQuery> ret = _parseTagQuery(str, pos, depth+1);
		skipWhitespace(str, pos);

		if (str[pos] != ')') throw "Syntax error in tag query: unmatched opening bracket, at character " + std::to_string(pos) + " '" + str[pos] + "' " + std::to_string((int)str[pos]);
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
			pos++;
			skipWhitespace(str, pos);
			std::shared_ptr<TagQuery> query = _parseTagQuery(str, pos, depth+1);
			skipWhitespace(str, pos);

			if (str[pos] != ']') throw "Syntax in tag query: [ unmatched with ]";
			pos++;

			// TagQueryType::WITHIN_NOT = ~ TAG_NAME [ TAG_QUERY ]
			if (tilda)
			{
				//std::cout << "parseTagQueryWithinNot returning TagQuery_HasDescendantTagWithQuery(" << tagName << ")\r\n";
				return std::make_shared<TagQuery_HasDescendantTagWithQuery>(tagName, query);
			}

			// TagQueryType::WITHIN_NOT = TAG_NAME [ TAG_QUERY ]
			else
			{
				//std::cout << "parseTagQueryWithinNot returning TagQuery_HasChildTagWithQuery(" << tagName << ")\r\n";
				return std::make_shared<TagQuery_HasChildTagWithQuery>(tagName, query);
			}
		}

		// TagQueryType::WITHIN_NOT = ~ TAG_NAME
		else if (tilda)
		{
			//std::cout << "parseTagQueryWithinNot returning TagQuery_HasDescendantTag(" << tagName << ")\r\n";
			return std::make_shared<TagQuery_HasDescendantTag>(tagName);
		}

		// TagQueryType::WITHIN_NOT = TAG_NAME
		else
		{
			//std::cout << "parseTagQueryWithinNot returning TagQuery_HasChildTag(" << tagName << ")\r\n";
			return std::make_shared<TagQuery_HasChildTag>(tagName);
		}
	}
}
std::shared_ptr<TagQuery> parseTagQueryWithinAnd(const std::string& str, int& pos, int depth)
{
	//std::cout << "parseTagQueryWithinAnd(" << str.substr(pos) << ")\r\n";

	skipWhitespace(str, pos);
	if (str[pos] == '!')
	{
		pos++;
		std::shared_ptr<TagQuery> subQuery = parseTagQueryWithinNot(str, pos, depth+1);
		//std::cout << "parseTagQueryWithinAnd returning TagQuery_Not\r\n";
		return std::make_shared<TagQuery_Not>(subQuery);
	}
	else
	{
		//std::cout << "parseTagQueryWithinAnd calling parseTagQueryWithinNot\r\n";
		return parseTagQueryWithinNot(str, pos, depth+1);
	}
}
std::shared_ptr<TagQuery> parseTagQueryWithinXor(const std::string& str, int& pos, int depth)
{
	//std::cout << "parseTagQueryWithinXor(" << str.substr(pos) << ")\r\n";

	std::vector<std::shared_ptr<TagQuery>> operands;
	skipWhitespace(str, pos);
	operands.push_back(parseTagQueryWithinAnd(str, pos, depth+1));
	skipWhitespace(str, pos);

	//std::cout << "parseTagQueryWithinXor parsed one operand, remaining: " << str.substr(pos) << "\r\n";

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
	//std::cout << "parseTagQueryWithinOr(" << str.substr(pos) << ")\r\n";

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
	//std::cout << "_parseTagQuery(" << str.substr(pos) << ")\r\n";

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
		throw std::string("Unexpected string after tag query: ") + str.substr(pos);
	}
	return ret;
}
