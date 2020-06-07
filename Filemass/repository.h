#pragma once

#include <string>
#include <map>

class Repository
{
private:
	std::string path;
	std::map<std::string, std::string> config;

	std::string config_file;

public:
	Repository(std::string _path);
	void add(std::string _path, char* hashOut=nullptr);
};
