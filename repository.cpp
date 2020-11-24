#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "util.h"
#include "repository.h"
#include "merkel_tree.h"

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
	
	std::array<char, 32> hash;
	
	MerkelTree merkelTree(true);
	{
		{
			unsigned long fileSize = std::filesystem::file_size(_path);
			unsigned long bytesRead = 0;
			
			if (DEBUGGING) std::cout << "fileSize=" << fileSize << " _path=" << _path << "\r\n";
			std::ifstream file(_path, std::ios_base::binary);
			char buff[1024];
			while (bytesRead < fileSize)
			{
				int amountToRead;
				if (bytesRead + 1024 <= fileSize) amountToRead = 1024;
				else amountToRead = fileSize - bytesRead;
				readExactly(file, &buff[0], amountToRead);
				merkelTree.addData(&buff[0], amountToRead);
				bytesRead += amountToRead;
			}
			file.close();
		}
		merkelTree.finalize();
		hash = *merkelTree.hash;
	}
	
	
	
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
		merkelTree.serialize(ofs);
		ofs.close();
	}
	
	if (DEBUGGING)
	{
		std::ifstream ifs(destTreePath);
		MerkelTree a(ifs);
		printf("Deserialized merkel tree reports: %lu bytes\r\n", a.getTotalBytes());
		if (!ifs.eof()) exitWithError("Merkel tree deserialization did not read the entire file.");
		ifs.close();
		if (merkelTree.equals(a)) printf("Merkel trees are equal! :D\r\n");
		else printf("Merkel trees are not equal! :(((\r\n");
	}
	
	return {hash, wasNew};
}
