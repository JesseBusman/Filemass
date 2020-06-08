#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>

// aa/bb/cc.txt --> PathPattern_DirectoriesThatMatch[aa] => PathPattern_DirectoriesThatMatch[bb] => PathPattern_FilesThatMatch[cc.txt]

class PathPattern
{
public:
	virtual void findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback);
	virtual std::string toString();
};

class PathPattern_DirectoriesThatMatch : public PathPattern
{
public:
	std::string str;
	std::shared_ptr<PathPattern> subPattern;
	PathPattern_DirectoriesThatMatch(const std::string& _str, std::shared_ptr<PathPattern> _subPattern);
	virtual void findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback);
	virtual std::string toString();
};

class PathPattern_Union : public PathPattern
{
public:
	std::vector<std::shared_ptr<PathPattern>> patterns;
	PathPattern_Union(std::vector<std::shared_ptr<PathPattern>>&& _patterns);
	virtual void findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback);
	virtual std::string toString();
};

class PathPattern_FilesThatMatch : public PathPattern
{
public:
	std::string str;
	PathPattern_FilesThatMatch(const std::string& _str);
	virtual void findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback);
	virtual std::string toString();
};

class PathPattern_AlsoCheckSubDirectoriesRecursively : public PathPattern
{
public:
	std::shared_ptr<PathPattern> subPattern;
	virtual void findFiles(const std::string& baseDirectory, std::function<void(const std::string& path)> callback);
	PathPattern_AlsoCheckSubDirectoriesRecursively(std::shared_ptr<PathPattern> _subPattern);
	virtual std::string toString();
};

std::shared_ptr<PathPattern> parsePathPattern(const std::string& pattern);
