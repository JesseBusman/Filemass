#pragma once

#include <memory>
#include <vector>
#include <string>

class PathPattern
{
	virtual void findFiles(const std::string& baseDirectory, void (*callback)(const std::string& path));
};

class PathPattern_DirectoriesThatMatch : public PathPattern
{
public:

};

class PathPattern_DirectoriesThatMatch : public PathPattern
{
public:
	std::string str;
	int amountOfWildcardsInStr;
	std::shared_ptr<PathPattern> pattern;
	PathPattern_DirectoriesThatMatch(const std::string& _str, std::shared_ptr<PathPattern> _pattern):
		str(_str), pattern(_pattern)
	{
		amountOfWildcardsInStr = 0;
		for (int i=0; i<str.length(); i++)
		{
			if (str[i] == '*') amountOfWildcardsInStr++;
		}
	}
	virtual void findFiles(const std::string& baseDirectory, void (*callback)(const std::string& path));
};

class PathPattern_Union : public PathPattern
{
public:
	std::vector<std::shared_ptr<PathPattern>> patterns;
	PathPattern_Union(const std::vector<std::shared_ptr<PathPattern>>& _patterns):
		patterns(_patterns)
	{
	}
	virtual void findFiles(const std::string& baseDirectory, void (*callback)(const std::string& path));
};

class PathPattern_FilesThatMatch : public PathPattern
{
public:
	std::string str;
	int amountOfWildcardsInStr;
	PathPattern_FilesThatMatch(const std::string& _str):
		str(_str)
	{
		amountOfWildcardsInStr = 0;
		for (int i=0; i<str.length(); i++)
		{
			if (str[i] == '*') amountOfWildcardsInStr++;
		}
	}
	virtual void findFiles(const std::string& baseDirectory, void (*callback)(const std::string& path));
};

class PathPattern_AlsoCheckSubDirectoriesRecursively : public PathPattern
{
public:
	virtual void findFiles(const std::string& baseDirectory, void (*callback)(const std::string& path));
};

std::shared_ptr<PathPattern> parsePathPattern(const std::string& pattern)
{

	std::shared_ptr<PathPattern> currentPatternBase = nullptr;
	std::shared_ptr<PathPattern> currentPattern = nullptr;
	std::string buffer = "";
	for (int i=0; i<pattern.length; i++)
	{
		char c = pattern[i];
		if (currentPatternBase == nullptr)
		{

		}
	}
	
}
