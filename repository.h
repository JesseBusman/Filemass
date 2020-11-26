#pragma once

#include <string>
#include <map>

enum ErrorCheckResult
{
	ECR_ALL_OK,
	ECR_FILE_NOT_FOUND,
	ECR_ERROR
};

enum ErrorFixResult
{
	EFR_FIXED,
	EFR_WAS_NOT_BROKEN,
	EFR_FAILED_TO_FIX,
	EFR_FILE_NOT_FOUND
};

class Repository
{
private:
	std::string path;
	std::map<std::string, std::string> config;
	
	std::string config_file;
	
	std::string hashToTreePath(const std::array<char, 32>& _hash);
	std::string hashToParityPath(const std::array<char, 32>& _hash);

public:
	Repository(std::string _path);
	std::pair<std::array<char, 32>, bool> add(const std::string& _path);
	ErrorCheckResult errorCheck(std::array<char, 32> _file);
	ErrorFixResult errorFix(std::array<char, 32> _file);
	std::string hashToFilePath(const std::array<char, 32>& _hash);
};
