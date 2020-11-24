#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "util.h"
#include "repository.h"
#include "merkel_tree.h"
#include "sha256.h"

Repository::Repository(std::string _path) :
	path(_path)
{
	config_file = path + "/fmrepo.conf";
	if (std::filesystem::exists(config_file))
	{
		if (!std::filesystem::is_regular_file(config_file))
		{
			exitWithError("Repo config file found is not a regular file: " + config_file);
		}
		
		std::ifstream infile(config_file);
		
		std::string line;
		int lineCount = 0;
		while (std::getline(infile, line))
		{
			lineCount++;
			line = trim(line);
			
			if (line.length() == 0) continue;
			if (line[0] == '#') continue;
			
			size_t equalsSignPos = line.find('=');
			if (equalsSignPos == std::string::npos)
			{
				exitWithError("Syntax error 'no = found' in " + config_file + " on line " + std::to_string(lineCount) + ": " + line);
			}
			
			this->config[trim(line.substr(0, equalsSignPos))] = trim(line.substr(equalsSignPos + 1));
		}
	}
	else
	{
		exitWithError("No repo config file found at " + config_file);
	}
}

std::string Repository::hashToFilePath(const std::array<char, 32>& _hash)
{
	std::string path = this->path;
	
	std::string hashHex = bytes_to_hex(_hash);
	
	path += "/" + hashHex.substr(0, 2);
	if (!std::filesystem::exists(path)) { if (!std::filesystem::create_directory(path)) exitWithError("Could not create directory: " + path); }
	else if (!std::filesystem::is_directory(path)) exitWithError("Not a directory: " + path);
	
	path += "/" + hashHex.substr(2, 2);
	if (!std::filesystem::exists(path)) { if (!std::filesystem::create_directory(path)) exitWithError("Could not create directory: " + path); }
	else if (!std::filesystem::is_directory(path)) exitWithError("Not a directory: " + path);
	
	path += "/" + hashHex.substr(4, 2);
	if (!std::filesystem::exists(path)) { if (!std::filesystem::create_directory(path)) exitWithError("Could not create directory: " + path); }
	else if (!std::filesystem::is_directory(path)) exitWithError("Not a directory: " + path);
	
	path += "/" + hashHex;
	
	return path;
}

std::string Repository::hashToTreePath(const std::array<char, 32>& _hash)
{
	return this->hashToFilePath(_hash) + ".fmtree";
}

std::pair<std::array<char, 32>, bool> Repository::add(const std::string& _path)
{
	if (!std::filesystem::exists(_path)) exitWithError("File does not exist: " + _path);
	if (!std::filesystem::is_regular_file(_path)) exitWithError("File is not a regular file: " + _path);
	
	std::shared_ptr<MerkelTree> merkelTree = generateMerkelTreeFromFilePath(_path);
	std::array<char, 32> hash = *merkelTree->hash;	
	
	std::string destFilePath = this->hashToFilePath(hash);
	
	bool wasNew = false;
	
	if (std::filesystem::exists(destFilePath))
	{
		if (std::filesystem::file_size(destFilePath) == std::filesystem::file_size(_path))
		{
			if (DEBUGGING) std::cout << "File " << _path << " already exists at " << destFilePath << "\r\n";
			wasNew = false;
		}
		else
		{
			exitWithError("File " + _path + " already exists at " + destFilePath + ", but they have different sizes!");
		}
	}
	else
	{
		if (std::filesystem::copy_file(_path, destFilePath))
		{
			wasNew = true;
		}
		else
		{
			exitWithError("Failed to copy file " + _path + " to " + destFilePath);
		}
	}
	
	std::string destTreePath = this->hashToTreePath(hash);
	
	if (!std::filesystem::exists(destTreePath))
	{
		std::ofstream ofs(destTreePath);
		merkelTree->serialize(ofs);
		ofs.close();
	}
	
	if (DEBUGGING)
	{
		std::ifstream ifs(destTreePath);
		MerkelTree a(ifs);
		printf("Deserialized merkel tree reports: %lu bytes\r\n", a.getTotalBytes());
		if (!ifs.eof()) exitWithError("Merkel tree deserialization did not read the entire file.");
		ifs.close();
		if (merkelTree->equals(a)) printf("Merkel trees are equal! :D\r\n");
		else printf("Merkel trees are not equal! :(((\r\n");
	}
	
	return {hash, wasNew};
}

ErrorCheckResult Repository::errorCheck(std::array<char, 32> _file)
{
	std::string filePath = this->hashToFilePath(_file);
	std::string treePath = this->hashToTreePath(_file);
	
	bool treeFileIsTooLong = false;
	
	std::ifstream treeIfs(treePath);
	std::ifstream fileIfs(filePath);
	
	char buff[1024];
	std::array<char, 32> hashFromTree;
	std::array<char, 32> hashFromFile;
	
	while (1)
	{
		fileIfs.read(&buff[0], 1024);
		int amountRead = fileIfs.gcount();
		SHA256 sha256;
		sha256.init();
		sha256.update((const unsigned char*)&buff[0], amountRead);
		sha256.final((unsigned char*)hashFromFile.data());
		
		while (!treeIfs.eof())
		{
			treeIfs.read(buff, 1);
			if (buff[0] == 0) break;
			else treeIfs.read(buff, 8 + 32);
		}
		if (treeIfs.gcount() != 1) return ECR_ERROR;
		treeIfs.read(buff, 8);
		treeIfs.read(hashFromTree.data(), 32);
		
		if (hashFromTree != hashFromFile) return ECR_ERROR;
		
		if (amountRead != 1024) break;
	}
	
	fileIfs.close();
	treeIfs.close();
	
	return ECR_ALL_OK;
}
