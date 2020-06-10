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

std::array<char, 32> Repository::add(std::string _path)
{
	if (!std::filesystem::exists(_path)) exitWithError("File does not exist: " + _path);
	if (!std::filesystem::is_regular_file(_path)) exitWithError("File is not a regular file: " + _path);
	
	//SHA256 hasher;
	//hasher.init();
	MerkelTree merkelTree;

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
		//std::cout << "Going to read " << amountToRead << " bytes from file...\r\n";
		readExactly(file, &buff[0], amountToRead);
		//hasher.update((unsigned char*)&buff[0], (unsigned int)file.gcount());
		merkelTree.addData(&buff[0], amountToRead);
		bytesRead += amountToRead;
	}
	file.close();

	std::array<char, 32> hash;
	//hasher.final((unsigned char*)&hash[0]);
	merkelTree.finalize();
	//memcpy(hash, merkelTree.hash, 32);
	for (int i = 0; i < 32; i++) hash[i] = merkelTree.hash[i];
	
	std::string hashHex = bytes_to_hex(hash);

	std::string destPath = path;

	destPath += "/" + hashHex.substr(0, 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) exitWithError("Could not create directory: " + destPath); }
	else if (!std::filesystem::is_directory(destPath)) exitWithError("Not a directory: " + destPath);

	destPath += "/" + hashHex.substr(2, 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) exitWithError("Could not create directory: " + destPath); }
	else if (!std::filesystem::is_directory(destPath)) exitWithError("Not a directory: " + destPath);

	destPath += "/" + hashHex.substr(4, 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) exitWithError("Could not create directory: " + destPath); }
	else if (!std::filesystem::is_directory(destPath)) exitWithError("Not a directory: " + destPath);

	destPath += "/" + hashHex;
	if (std::filesystem::exists(destPath))
	{
		if (std::filesystem::file_size(destPath) == std::filesystem::file_size(_path))
		{
			if (DEBUGGING) std::cout << "File " << _path << " already exists at " << destPath << "\r\n";
		}
		else
		{
			exitWithError("File " + _path + " already exists at " + destPath + ", but they have different sizes!");
		}
	}
	else if (!std::filesystem::copy_file(_path, destPath))
	{
		exitWithError("Failed to copy file " + _path + " to " + destPath);
	}

	return hash;
}
