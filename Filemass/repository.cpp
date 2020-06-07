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
			std::cout << "\r\nRepo config file found is not a regular file: " << config_file << "\r\n";
			exit(1);
			throw 1;
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
				std::cout << "\r\nSyntax error 'no = found' in " << config_file << " on line " << lineCount << ": " << line << "\r\n";
				exit(1);
				throw 1;
			}

			this->config[trim(line.substr(0, equalsSignPos))] = trim(line.substr(equalsSignPos + 1));
		}
	}
	else
	{
		std::cout << "\r\nNo repo config file found at " << config_file << "\r\n";
		exit(1);
		throw 1;
	}
}

void Repository::add(std::string _path, char* hashOut)
{
	if (!std::filesystem::exists(_path)) { std::cout << "\r\nFile does not exist: " << _path << "\r\n"; exit(1); throw 1; }
	if (!std::filesystem::is_regular_file(_path)) { std::cout << "\r\nFile is not a regular file: " << _path << "\r\n"; exit(1); throw 1; }
		
	//SHA256 hasher;
	//hasher.init();
	MerkelTree merkelTree;

	unsigned long fileSize = std::filesystem::file_size(_path);
	unsigned long bytesRead = 0;

	std::cout << "fileSize=" << fileSize << " _path=" << _path << "\r\n";

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

	char hash[32];
	//hasher.final((unsigned char*)&hash[0]);
	merkelTree.finalize();
	//memcpy(hash, merkelTree.hash, 32);
	for (int i = 0; i < 32; i++) hash[i] = merkelTree.hash[i];

	if (hashOut != nullptr)
	{
		for (int i = 0; i < 32; i++) hashOut[i] = hash[i];
	}

	char hashHex[64];

	bytes_to_hex(hash, 32, hashHex);

	std::string destPath = path;

	destPath += "/" + std::string(&hashHex[0], 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) { std::cout << "\r\nCould not create directory: " << destPath << "\r\n"; exit(1); throw 1; } }
	else if (!std::filesystem::is_directory(destPath)) { std::cout << "\r\nNot a directory: " << destPath << "\r\n"; exit(1); throw 1; }

	destPath += "/" + std::string(&hashHex[2], 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) { std::cout << "\r\nCould not create directory: " << destPath << "\r\n"; exit(1); throw 1; } }
	else if (!std::filesystem::is_directory(destPath)) { std::cout << "\r\nNot a directory: " << destPath << "\r\n"; exit(1); throw 1; }

	destPath += "/" + std::string(&hashHex[4], 2);
	if (!std::filesystem::exists(destPath)) { if (!std::filesystem::create_directory(destPath)) { std::cout << "\r\nCould not create directory: " << destPath << "\r\n"; exit(1); throw 1; } }
	else if (!std::filesystem::is_directory(destPath)) { std::cout << "\r\nNot a directory: " << destPath << "\r\n"; exit(1); throw 1; }

	destPath += "/" + std::string(&hashHex[0], 64);
	if (std::filesystem::exists(destPath))
	{
		if (std::filesystem::file_size(destPath) == std::filesystem::file_size(_path))
		{
			std::cout << "File " << _path << " already exists at " << destPath << "\r\n";
		}
		else
		{
			std::cout << "File " << _path << " already exists at " << destPath << ", but they have different sizes!\r\n";
			exit(1);
			throw 1;
		}
	}
	else if (!std::filesystem::copy_file(_path, destPath)) { std::cout << "\r\nFailed to copy file " << _path << " to " << destPath << "\r\n"; exit(1); throw 1; }
}
