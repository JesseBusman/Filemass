#include <memory>
#include <string>
#include <iostream>
#include <vector>
#include <stack>
#include <sstream>

#include "tag.h"
#include "util.h"
#include "tag_parser.h"
#include "tag_query.h"

void tagListSyntaxError(const std::string& str, int pos1, int pos2, const char* msg)
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

void tagListSyntaxError(const std::string& str, int pos, const char* msg)
{
	tagListSyntaxError(str, pos, -1, msg);
}

void tagListSyntaxError(const std::string& str, int pos, const std::string& msg)
{
	tagListSyntaxError(str, pos, msg.c_str());
}

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
				if (newTag == nullptr) tagListSyntaxError(str, i, "Syntax error in tag list: Unexpected character [");
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

	if (lastNonWhitespace == -1) tagListSyntaxError(str, startPos, "Syntax error: expected tag name");

	//std::cout << "end: startPos=" << startPos << " lastNonWhitespace=" << lastNonWhitespace << "\r\n";

	return str.substr(startPos, lastNonWhitespace - startPos + 1); //std::string_view(&str.c_str()[startPos], lastNonWhitespace - startPos + 1);
}

std::string repeat(char c, int n)
{
	char* ret = new char[n+1];
	ret[n] = 0x00;
	for (int i=0; i<n; i++) ret[i] = c;
	return std::string(ret);
}
