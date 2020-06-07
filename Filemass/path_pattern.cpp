#include <memory>
#include <filesystem>
#include <string>

#include "path_pattern.h"

PathPattern_DirectoriesThatMatch::PathPattern_DirectoriesThatMatch(const std::string& _str, std::shared_ptr<PathPattern> _subPattern):
	str(_str), subPattern(_subPattern)
{
}

PathPattern_Union::PathPattern_Union(std::vector<std::shared_ptr<PathPattern>>&& _patterns):
	patterns(_patterns)
{
}

PathPattern_FilesThatMatch::PathPattern_FilesThatMatch(const std::string& _str):
	str(_str)
{
}

PathPattern_AlsoCheckSubDirectoriesRecursively::PathPattern_AlsoCheckSubDirectoriesRecursively(std::shared_ptr<PathPattern> _subPattern):
	subPattern(_subPattern)
{
}

bool pathSegment_matches_patternSegment(const char* pathSegment, const char* patternSegment)
{
	if (*pathSegment == *patternSegment) return true;
	else if (*patternSegment == '*') return pathSegment_matches_patternSegment(pathSegment, patternSegment + 1) || pathSegment_matches_patternSegment(pathSegment + 1, patternSegment + 1);
	else return false;
}
bool pathSegment_matches_patternSegment(const std::string& pathSegment, const std::string& patternSegment)
{
	return pathSegment_matches_patternSegment(pathSegment.c_str(), patternSegment.c_str());
}



void PathPattern_DirectoriesThatMatch::findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback)
{
	for (const auto& entry : std::filesystem::directory_iterator(baseDirectory))
	{
		if (!entry.is_directory()) continue;

		const std::string& dir = entry.path().stem().string();
		if (pathSegment_matches_patternSegment(dir, this->str))
		{
			this->subPattern->findFiles(baseDirectory + "/" + dir, callback);
		}
	}
}

void PathPattern_Union::findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback)
{
	for (const auto& pattern : this->patterns)
	{
		pattern->findFiles(baseDirectory, callback);
	}
}

void PathPattern_FilesThatMatch::findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback)
{
	for (const auto& entry : std::filesystem::directory_iterator(baseDirectory))
	{
		if (!entry.is_regular_file()) continue;

		const std::string& file = entry.path().stem().string();
		if (pathSegment_matches_patternSegment(file, this->str))
		{
			callback(baseDirectory + "/" + file);
		}
	}
}


std::string readPatternPathSegment(const std::string& str, int& pos)
{
	const int startPos = pos;
	while (str[pos] != '\\' && str[pos] != '/' && str[pos] != ',')
	{
		pos++;
	}

	if (pos == startPos) throw "readPatternPathSegment failed";

	return str.substr(startPos, pos-startPos);
}

std::shared_ptr<PathPattern> _parsePathPattern(const std::string& str, int& pos)
{
	bool absolutePath = false;
	std::string absolutePathPrefix;

	if (str[pos] == '/')
	{
		absolutePath = true;
		absolutePathPrefix = "/";
		pos++;
	}

	if (str[pos+1] == ':' && (str[pos+2] == '/' || str[pos+2] == '\\'))
	{
		absolutePath = true;
		absolutePathPrefix = str.substr(0, 3);
		pos += 3;
	}

	std::string patternSegment = readPatternPathSegment(str, pos);


	/*
	/hello
	hello
	*/
	if (pos == str.length() || str[pos] == ',')
	{
		if (absolutePath) return std::make_shared<PathPattern_DirectoriesThatMatch>(absolutePathPrefix, std::make_shared<PathPattern_FilesThatMatch>(patternSegment));
		else return std::make_shared<PathPattern_FilesThatMatch>(patternSegment);
	}
	else
	{
		/*
		/hello/bla
		hello/bla
		*/
		if (str[pos] == '/' || str[pos] == '\\')
		{
			pos++;
			std::shared_ptr<PathPattern> sub = _parsePathPattern(str, pos);
			if (str[pos] == '/' || str[pos] == '\\')
			{
				pos++;
				sub = std::make_shared<PathPattern_AlsoCheckSubDirectoriesRecursively>(sub);
			}
			if (absolutePath) return std::make_shared<PathPattern_DirectoriesThatMatch>(absolutePathPrefix + patternSegment, sub);
			else return std::make_shared<PathPattern_DirectoriesThatMatch>(patternSegment, sub);
		}
		else
		{
			throw "wtffff 923848934 " + str + " " + std::to_string(pos);
		}
	}
}

std::shared_ptr<PathPattern> parsePathPattern(const std::string& pattern)
{
	std::vector<std::shared_ptr<PathPattern>> patterns;
	int pos = 0;
	while (true)
	{
		patterns.push_back(_parsePathPattern(pattern, pos));
		if (pos >= pattern.length()) break;
		if (pattern[pos] != ',') throw "Unexpected char in pattern " + pattern + " " + pattern[pos];
		pos++;
	}
	if (patterns.size() == 1) return patterns[0];
	else return std::make_shared<PathPattern_Union>(std::move(patterns));
}
