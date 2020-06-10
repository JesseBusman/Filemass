#include <memory>
#include <filesystem>
#include <string>
#include <iostream>
#include <sstream>

#include "path_pattern.h"
#include "util.h"

void pathPatternSyntaxError(const std::string& str, unsigned int pos, const char* msg)
{
	std::stringstream out;
	out << "\r\n" << str << "\r\n";
	for (unsigned int i=0; i<pos; i++) out << ' ';
	out << "^\r\n";
	out << msg << "\r\n";
	exitWithError(out.str());
}
void pathPatternSyntaxError(const std::string& str, unsigned int pos, const std::string& msg)
{
	pathPatternSyntaxError(str, pos, msg.c_str());
}

PathPattern_DirectoriesThatMatch::PathPattern_DirectoriesThatMatch(const std::string& _str, std::shared_ptr<PathPattern> _subPattern, bool _absolutePath):
	str(_str), subPattern(_subPattern), absolutePath(_absolutePath)
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
	//std::cout << "pathSegment_matches_patternSegment(" << pathSegment << ", " << patternSegment << ")\r\n";
	if (*pathSegment == 0x00 && *patternSegment == 0x00) return true;
	else if (*pathSegment == *patternSegment) return pathSegment_matches_patternSegment(pathSegment + 1, patternSegment + 1);
	else if (*patternSegment == '*' && *pathSegment != 0x00) return pathSegment_matches_patternSegment(pathSegment + 1, patternSegment + 1) || pathSegment_matches_patternSegment(pathSegment + 1, patternSegment);
	else return false;
}
bool pathSegment_matches_patternSegment(const std::string& pathSegment, const std::string& patternSegment)
{
	return pathSegment_matches_patternSegment(pathSegment.c_str(), patternSegment.c_str());
}



std::string PathPattern::toString()
{
	throw "default PathPattern::toString should never be called.";
}

std::string PathPattern_DirectoriesThatMatch::toString()
{
	return "dir[" + this->str + "] => " + this->subPattern->toString();
}

std::string PathPattern_Union::toString()
{
	std::string ret = "";
	for (size_t i=0; i<this->patterns.size(); i++)
	{
		if (i != 0) ret += ',';
		ret += this->patterns[i]->toString();
	}
	return ret;
}

std::string PathPattern_FilesThatMatch::toString()
{
	return "file[" + this->str + "]";
}

std::string PathPattern_AlsoCheckSubDirectoriesRecursively::toString()
{
	return "[search all subdirs recursively] => " + this->subPattern->toString();
}




void PathPattern::findFiles(const std::string&, std::function<void(const std::string& path)>)
{
	throw "default PathPattern::findFiles should never be called.";
}

void PathPattern_DirectoriesThatMatch::findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback)
{
	if (DEBUGGING) std::cout << "PathPattern_DirectoriesThatMatch in " << baseDirectory << " on " << this->str << "\r\n";
	if (this->absolutePath)
	{
		if (DEBUGGING) std::cout << "PathPattern_DirectoriesThatMatch::findFiles: " << this->str << " is an absolute path!\r\n";
		this->subPattern->findFiles(this->str, callback);
	}
	else
	{
		if (!std::filesystem::exists(baseDirectory))
		{
			exitWithError("Directory " + baseDirectory + " does not exist!");
			return;
		}

		if (!std::filesystem::is_directory(baseDirectory))
		{
			exitWithError(baseDirectory + " is not a directory!");
			return;
		}

		if (this->str == "." || this->str == "..")
		{
			if (baseDirectory.back() != '/' && baseDirectory.back() != '\\') this->subPattern->findFiles(baseDirectory + "/" + this->str, callback);
			else this->subPattern->findFiles(baseDirectory + this->str, callback);
		}
		else
		{
			for (const auto& entry : std::filesystem::directory_iterator(baseDirectory))
			{
				if (!entry.is_directory()) continue;

				const std::string& dir = entry.path().filename().string();
				if (DEBUGGING) std::cout << dir << "\r\n";
				if (pathSegment_matches_patternSegment(dir, this->str))
				{
					if (baseDirectory.back() != '/' && baseDirectory.back() != '\\') this->subPattern->findFiles(baseDirectory + "/" + dir, callback);
					else this->subPattern->findFiles(baseDirectory + dir, callback);
				}
			}
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
	if (DEBUGGING) std::cout << "PathPattern_FilesThatMatch::findFiles(" << baseDirectory << ") on " << this->str << "\r\n";

	if (!std::filesystem::exists(baseDirectory))
	{
		exitWithError("Directory " + baseDirectory + " does not exist!");
	}

	if (!std::filesystem::is_directory(baseDirectory))
	{
		exitWithError(baseDirectory + " is not a directory!");
	}
	
	for (const auto& entry : std::filesystem::directory_iterator(baseDirectory))
	{
		if (!entry.is_regular_file()) continue;

		const std::string& file = entry.path().filename().string();
		if (DEBUGGING) std::cout << file << "\r\n";

		if (pathSegment_matches_patternSegment(file, this->str))
		{
			if (baseDirectory.back() != '/' && baseDirectory.back() != '\\') callback(baseDirectory + "/" + file);
			else callback(baseDirectory + file);
		}
	}
}

void PathPattern_AlsoCheckSubDirectoriesRecursively::findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback)
{
	this->subPattern->findFiles(baseDirectory, callback);
	for (const auto& entry : std::filesystem::recursive_directory_iterator(baseDirectory))
	{
		if (entry.is_directory())
		{
			this->subPattern->findFiles(entry.path().string(), callback);
		}
	}
}


std::string readPatternPathSegment(const std::string& str, unsigned int& pos)
{
	if (DEBUGGING) std::cout << "readPatternPathSegment(" << str.substr(pos) << ")\r\n";

	const unsigned int startPos = pos;
	while (pos < str.length() && str[pos] != '\\' && str[pos] != '/' && str[pos] != ',')
	{
		pos++;
	}

	if (pos == startPos) pathPatternSyntaxError(str, pos, "Expected file or directory name");

	return str.substr(startPos, pos-startPos);
}

std::shared_ptr<PathPattern> _parsePathPattern(const std::string& str, unsigned int& pos, bool isFirstSegment)
{
	if (DEBUGGING) std::cout << "_parsePathPattern(" << str.substr(pos) << ")\r\n";

	bool absolutePath = false;
	std::string absolutePathPrefix;
	
	if (isFirstSegment)
	{
		if (str[pos] == '/')
		{
			absolutePath = true;
			absolutePathPrefix = str[pos];
			pos++;
		}
		else
		{
			int startPos = pos;
			absolutePathPrefix = readPatternPathSegment(str, pos);
			if (DEBUGGING) std::cout << "absolutePathPrefix=" << absolutePathPrefix << "\r\n";
			char slashAfterAbsolutePathPrefix;
			if (str[pos] == '/' || str[pos] == '\\')
			{
				slashAfterAbsolutePathPrefix = str[pos];
				pos++;
			}
			else
			{
				pathPatternSyntaxError(str, pos, "Expected path delimiter: / or \\");
				return nullptr;
			}

			absolutePathPrefix += slashAfterAbsolutePathPrefix;

			if (std::filesystem::path(absolutePathPrefix).is_absolute())
			{
				if (DEBUGGING) std::cout << "'" << absolutePathPrefix << "' is absolute!\r\n";
				absolutePath = true;
			}
			else
			{
				if (DEBUGGING) std::cout << "'" << absolutePathPrefix << "' is not absolute!\r\n";
				pos = startPos;
			}
		}
	}

	std::string patternSegment = readPatternPathSegment(str, pos);

	if (pos == str.length() || str[pos] == ',')
	{
		if (absolutePath) return std::make_shared<PathPattern_DirectoriesThatMatch>(absolutePathPrefix, std::make_shared<PathPattern_FilesThatMatch>(patternSegment), true);
		else return std::make_shared<PathPattern_FilesThatMatch>(patternSegment);
	}
	else
	{
		if (str[pos] == '/' || str[pos] == '\\')
		{
			pos++;
			bool isRecursiveSearch = false;
			if (str[pos] == '/' || str[pos] == '\\')
			{
				pos++;
				isRecursiveSearch = true;
				
			}
			std::shared_ptr<PathPattern> sub = _parsePathPattern(str, pos, false);
			if (isRecursiveSearch) sub = std::make_shared<PathPattern_AlsoCheckSubDirectoriesRecursively>(sub);
			if (absolutePath) return std::make_shared<PathPattern_DirectoriesThatMatch>(absolutePathPrefix + "/" + patternSegment, sub, true);
			else return std::make_shared<PathPattern_DirectoriesThatMatch>(patternSegment, sub, false);
		}
		else
		{
			pathPatternSyntaxError(str, pos, "Expected path delimiter: / or \\");
			return nullptr;
		}
	}
}

std::shared_ptr<PathPattern> parsePathPattern(const std::string& pattern)
{
	if (DEBUGGING) std::cout << "parsePathPattern(" << pattern << ")\r\n";

	std::vector<std::shared_ptr<PathPattern>> patterns;
	unsigned int pos = 0;
	while (true)
	{
		patterns.push_back(_parsePathPattern(pattern, pos, true));
		if (pos >= pattern.length()) break;
		if (pattern[pos] != ',') pathPatternSyntaxError(pattern, pos, std::string("Unexpected character in path pattern: '") + pattern[pos] + "'");
		pos++;
	}

	if (DEBUGGING) std::cout << "parsePathPattern(" << pattern << ") done!\r\n";

	if (patterns.size() == 1) return patterns[0];
	else return std::make_shared<PathPattern_Union>(std::move(patterns));
}
