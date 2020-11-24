#pragma once

#include <string>
#include <map>

class Repository
{
private:
	std::string path;
	std::map<std::string, std::string> config;
	
	std::string config_file;
	
	std::string hashToTreePath(const std::array<char, 32>& _hash);

public:
	Repository(std::string _path);
	std::pair<std::array<char, 32>, bool> add(const std::string& _path);
	bool errorCheck(const std::array<char, 32>& _file);
	std::string hashToFilePath(const std::array<char, 32>& _hash);
};
