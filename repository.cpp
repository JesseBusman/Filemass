#include <string>
#include <map>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "util.h"
#include "repository.h"
#include "merkel_tree.h"
#include "sha256.h"

#define DEBUGGING false

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

std::string Repository::hashToParityPath(const std::array<char, 32>& _hash)
{
	return this->hashToFilePath(_hash) + ".fmparity";
}

std::pair<std::array<char, 32>, bool> Repository::add(const std::string& _path)
{
	if (!std::filesystem::exists(_path)) exitWithError("File does not exist: " + _path);
	if (!std::filesystem::is_regular_file(_path)) exitWithError("File is not a regular file: " + _path);
	
	std::shared_ptr<MerkelTree> merkelTree = generateMerkelTreeFromFilePath(_path);
	std::array<char, 32> hash = *merkelTree->hash;	
	
	std::string destFilePath = this->hashToFilePath(hash);
	
	long sourceFileSize = std::filesystem::file_size(_path);
	
	bool wasNew = false;
	
	if (std::filesystem::exists(destFilePath))
	{
		if (std::filesystem::file_size(destFilePath) == sourceFileSize)
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
	
	// Generate parity file
	std::string destParityPath = this->hashToParityPath(hash);
	
	if (!std::filesystem::exists(destParityPath))
	{
		std::ifstream ifs(destFilePath);
		
		const unsigned long amountOfBlocksInSourceFile = (sourceFileSize+1023) / 1024;
		unsigned long amountOfBlocksInSourceFile_log2 = 0;
		{
			unsigned long temp = amountOfBlocksInSourceFile;
			while (temp > 0)
			{
				temp >>= 1;
				amountOfBlocksInSourceFile_log2++;
			}
		}
		
		const int minDivisor = 2;
		const int maxDivisor = 11; //(amountOfBlocksInSourceFile >> (amountOfBlocksInSourceFile_log2 / 2)) / ; // TODO make amount of parity configurable
		
		char** divisor_to_mod_to_parityBlock[maxDivisor+1];
		
		for (int d=minDivisor; d<=maxDivisor; d++)
		{
			divisor_to_mod_to_parityBlock[d] = new char*[d];
			for (int m=0; m<d; m++)
			{
				divisor_to_mod_to_parityBlock[d][m] = new char[1024];
				for (int i=0; i<1024; i++) divisor_to_mod_to_parityBlock[d][m][i] = 0x00;
			}
		}
		
		char buff[1024];
		long totalRead = 0;
		long blockIndex = 0;
		while (!ifs.eof())
		{
			ifs.read(buff, 1024);
			int amountRead = ifs.gcount();
			
			for (int d=minDivisor; d<=maxDivisor; d++)
			{
				for (int i=0; i<amountRead; i++)
				{
					divisor_to_mod_to_parityBlock[d][blockIndex%d][i] ^= buff[i];
				}
			}
			
			if (amountRead != 1024 && !ifs.eof()) exitWithError("Failed to generate parity blocks (#2)");
			
			totalRead += amountRead;
			blockIndex++;
		}
		
		if (totalRead != sourceFileSize) exitWithError("Failed to generate parity blocks");
		if (blockIndex != amountOfBlocksInSourceFile) exitWithError("Failed to generate parity blocks (#3)");
		
		std::ofstream parityOfs(destParityPath);
		parityOfs.write((char*)&minDivisor, 4);
		parityOfs.write((char*)&maxDivisor, 4);
		for (int d=minDivisor; d<=maxDivisor; d++)
		{
			for (int m=0; m<d; m++)
			{
				parityOfs.write(divisor_to_mod_to_parityBlock[d][m], 1024);
			}
		}
		parityOfs.close();
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
	
	std::ifstream treeIfs(treePath);
	std::ifstream fileIfs(filePath);
	
	char buff[1024];
	std::array<char, 32> hashFromTree;
	std::array<char, 32> hashFromFile;
	
	long lengthAccordingToTreeFile = -1;
	long totalRead = 0;
	
	while (1)
	{
		fileIfs.read(&buff[0], 1024);
		int amountRead = fileIfs.gcount();
		if (amountRead < 0) exitWithError("amountRead<0");
		SHA256 sha256;
		sha256.init();
		sha256.update((const unsigned char*)&buff[0], amountRead);
		sha256.final((unsigned char*)hashFromFile.data());
		
		while (!treeIfs.eof())
		{
			treeIfs.read(buff, 1);
			if (buff[0] == 0) break;
			else
			{
				if (lengthAccordingToTreeFile == -1)
				{
					treeIfs.read((char*)&lengthAccordingToTreeFile, 8);
					treeIfs.read(buff, 32);
				}
				else
				{
					treeIfs.read(buff, 8 + 32);
				}
			}
		}
		if (treeIfs.gcount() != 1) { if (DEBUGGING) { printf("Repository::errorCheck(): treeIfs.gcount() != 1\n"); } return ECR_ERROR; }
		if (lengthAccordingToTreeFile == -1) treeIfs.read((char*)&lengthAccordingToTreeFile, 8);
		else treeIfs.read(buff, 8);
		if (treeIfs.gcount() != 8) { if (DEBUGGING) { printf("Repository::errorCheck(): treeIfs.gcount() != 8\n"); } return ECR_ERROR; }
		treeIfs.read(hashFromTree.data(), 32);
		if (treeIfs.gcount() != 32) { if (DEBUGGING) { printf("Repository::errorCheck(): treeIfs.gcount() != 32\n"); } return ECR_ERROR; }
		
		if (hashFromTree != hashFromFile) { if (DEBUGGING) { printf("Repository::errorCheck(): hashFromTree != hashFromFile\n"); } return ECR_ERROR; }
		
		totalRead += amountRead;
		
		if (amountRead != 1024) break;
	}
	
	if (lengthAccordingToTreeFile != totalRead) { if (DEBUGGING) { printf("Repository::errorCheck(): lengthAccordingToTreeFile=%lu != totalRead=%lu\n", lengthAccordingToTreeFile, totalRead); } return ECR_ERROR; }
	
	fileIfs.close();
	treeIfs.close();
	
	return ECR_ALL_OK;
}

bool tryFixBlockUsingParityAndHash(const std::string& filePath, std::ifstream& file, std::istream& parityFile, long blockIndexToFix, char* _buff, int _buffSize, const std::array<char, 32>& _hash)
{
	if (DEBUGGING) printf("tryFixBlockUsingParityAndHash blockIndexToFix=%i _buffSize=%i\r\n", (int)blockIndexToFix, _buffSize);
	
	
	file.seekg(0, file.beg);
	parityFile.seekg(0, parityFile.beg);
	
	int minDivisor;
	int maxDivisor;
	
	parityFile.read((char*)&minDivisor, 4);
	parityFile.read((char*)&maxDivisor, 4);
	
	if (DEBUGGING) printf("minDivisor=%i maxDivisor=%i\r\n", minDivisor, maxDivisor);
	
	int divisor_to_mod[maxDivisor+1];
	long divisor_to_index[maxDivisor+1];
	
	for (int d=minDivisor; d<=maxDivisor; d++)
	{
		divisor_to_mod[d] = blockIndexToFix % d;
		if (d == minDivisor) divisor_to_index[d] = 8;
		else divisor_to_index[d] = divisor_to_index[d-1] + (d-1) * 1024;
	}
	
	
	for (int d=maxDivisor; d>=minDivisor; d--)
	{
		int m = divisor_to_mod[d];
		if (DEBUGGING) printf("Trying to fix with parity divisor %i modulo %i\r\n", d, m);
		
		char theParity[1024];
		
		parityFile.seekg(divisor_to_index[d] + m*1024, file.beg);
		if (parityFile.tellg() != divisor_to_index[d] + m*1024) exitWithError("Failed to seek in parity file");
		parityFile.read(theParity, 1024);
		if (parityFile.gcount() != 1024) exitWithError("Failed to read from parity file");
		
		char fileParity[1024];
		for (int i=0; i<1024; i++) fileParity[i] = 0x00;
		
		file.seekg(m * 1024, file.beg);
		if (file.tellg() != m*1024) { printf("tellg=%i eof=%i\r\n", file.tellg(), file.eof()); exitWithError("seekg failed"); }
		
		long currentBlockIndex = m;
		while (!file.eof())
		{
			char buff[1024];
			
			file.read(buff, 1024);
			int amountRead = file.gcount();
			
			if (amountRead != 1024 && !file.eof())
			{
				if (DEBUGGING) printf("amountRead=%i file.eof()=%i file.tellg()=%i\r\n", amountRead, file.eof(), file.tellg());
				exitWithError("adfskjsdjkf");
			}
			
			if (currentBlockIndex == blockIndexToFix)
			{
				if (DEBUGGING) printf("skipping block index to fix\r\n");
			}
			else
			{
				for (int i=0; i<amountRead; i++) fileParity[i] ^= buff[i];
			}
			
			if (file.eof()) break;
			
			file.seekg((d-1) * 1024, file.cur);
			currentBlockIndex += d;
		}
		
		// Reset eof and fail flags
		// file.clear();
		file.close();
		file = std::ifstream(filePath);
		
		
		for (int i=0; i<1024; i++) fileParity[i] ^= theParity[i];
		
		std::array<char, 32> newHash;
		SHA256 sha256;
		sha256.init();
		sha256.update((const unsigned char*)&fileParity[0], _buffSize);
		sha256.final((unsigned char*)newHash.data());
		
		if (newHash == _hash)
		{
			if (DEBUGGING) printf("Fixed with parity! :D\r\n");
			
			for (int i=0; i<_buffSize; i++) _buff[i] = fileParity[i];
			return true;
		}
	}
	
	if (DEBUGGING) printf("tryFixBlockUsingParityAndHash failed :(\r\n");
	
	return false;
}

bool tryFixBlockUsingHash(char* _buff, int _buffSize, const std::array<char, 32>& _hash)
{
	static char prevShiftedChar;
	
	std::array<char, 32> newHash;
	
	// Try to fix 2 adjacent swapped bytes
	for (int i=0; i<_buffSize-1; i++)
	{
		std::swap(_buff[i], _buff[i+1]);
		
		SHA256 sha256;
		sha256.init();
		sha256.update((const unsigned char*)&_buff[0], _buffSize);
		sha256.final((unsigned char*)newHash.data());
		if (newHash == _hash) return true;
		
		std::swap(_buff[i], _buff[i+1]);
	}
	
	// Try to fix 1 modified byte
	for (int i=0; i<_buffSize; i++)
	{
		char orig = _buff[i];
		for (int j=0; j<256; j++)
		{
			if (j == orig) continue;
			_buff[i] = (char)j;
			SHA256 sha256;
			sha256.init();
			sha256.update((const unsigned char*)&_buff[0], _buffSize);
			sha256.final((unsigned char*)newHash.data());
			if (newHash == _hash) return true;
		}
		_buff[i] = orig;
	}
	
	// Try to fix 1 inserted byte
	char buff1[1];
	for (int i=0; i<_buffSize; i++)
	{
		for (int j=-1; j<256; j++)
		{
			if (j == -1) buff1[0] = prevShiftedChar;
			else buff1[0] = (char)j;
			SHA256 sha256;
			sha256.init();
			sha256.update((const unsigned char*)&_buff[0], i);
			sha256.update((const unsigned char*)&buff1[0], 1);
			sha256.update((const unsigned char*)&_buff[i], _buffSize - i - 1);
			sha256.final((unsigned char*)newHash.data());
			if (newHash == _hash)
			{
				prevShiftedChar = _buff[_buffSize-1];
				for (int k=_buffSize-1; k>i; k--)
				{
					_buff[k] = _buff[k-1];
				}
				_buff[i] = buff1[0];
				return true;
			}
		}
	}
	
	// Try to fix 2 modified bytes
	/*
	for (int i=0; i<_buffSize; i++)
	{
		char orig_i = _buff[i];
		for (int k=i+1; k<_buffSize; k++)
		{
			char orig_k = _buff[k];
			for (int j=0; j<256; j++)
			{
				if (j == orig_i) continue;
				_buff[i] = (char)j;
				for (int m=0; m<256; m++)
				{
					if (m == orig_k) continue;
					_buff[k] = m;
					SHA256 sha256;
					sha256.init();
					sha256.update((const unsigned char*)&_buff[0], _buffSize);
					sha256.final((unsigned char*)newHash.data());
					if (newHash == _hash) return true;
				}
			}
			_buff[i] = orig_i;
			_buff[k] = orig_k;
		}
	}
	*/
	if (DEBUGGING) printf("tryFixBlockUsingHash failed :(\r\n");
	
	return false;
}

ErrorFixResult Repository::errorFix(std::array<char, 32> _file)
{
	int fixAttempts = 0;
checkFixed:
	ErrorCheckResult ecr = errorCheck(_file);
	if (ecr == ECR_ERROR) ;
	else if (ecr == ECR_ALL_OK) return (fixAttempts == 0) ? EFR_WAS_NOT_BROKEN : EFR_FIXED;
	else if (ecr == ECR_FILE_NOT_FOUND) return EFR_FILE_NOT_FOUND;
	else exitWithError("wtf");
	
	fixAttempts++;
	if (fixAttempts > 3)
	{
		if (DEBUGGING) printf("File is not fixed after 3 runs of errorFix(..). giving up\r\n");
		return EFR_FAILED_TO_FIX;
	}
	
	std::string filePath = this->hashToFilePath(_file);
	std::string treePath = this->hashToTreePath(_file);
	std::string parityPath = this->hashToParityPath(_file);
	
	std::shared_ptr<MerkelTree> newTree = generateMerkelTreeFromFilePath(filePath);
	
	std::ifstream treeIfs(treePath, std::ios::binary);
	std::ifstream fileIfs(filePath, std::ios::binary);
	std::ifstream parityIfs(parityPath, std::ios::binary);
	
	MerkelTree storedTree(true);
	try
	{
		storedTree = MerkelTree(treeIfs);
	}
	catch (Error_MerkelTreeFileCorrupted)
	{
		treeIfs.close();
		
		// The stored tree file is corrupted.
		
		// If the new tree has the correct hash...
		if (*newTree->hash == _file)
		{
			// ...just write the new tree to the file.
			std::ofstream treeOfs(treePath);
			newTree->serialize(treeOfs);
			treeOfs.close();
			goto checkFixed;
		}
		
		// TODO
		if (DEBUGGING) printf("TODO: merkel file corrupted\r\n");
		goto checkFixed;
	}
	
	// If the new tree's hash is equal to the file hash, but the stored tree is corrupted or not equal to the new tree...
	if (_file == *newTree->hash && (!newTree->equals(storedTree) || !storedTree.errorCheck()))
	{
		if (!newTree->errorCheck()) exitWithError("Failed to generate merkel tree");
		
		treeIfs.close();
		// ...just write the new tree to the file.
		std::ofstream treeOfs(treePath);
		newTree->serialize(treeOfs);
		treeOfs.close();
		goto checkFixed;
	}
	
	// If the .fmtree file is larger than it should be, and everything else is fine...
	if (!treeIfs.eof() && _file == *newTree->hash && newTree->equals(storedTree))
	{
		// ... just truncate the tree file
		long pos = treeIfs.tellg();
		if (pos < 0) exitWithError("tellg() < 0, this should never happen"); // wtf
		treeIfs.close();
		std::filesystem::resize_file(treePath, pos);
		goto checkFixed;
	}
	
	// If the new tree's hash is not equal to the file hash, but the stored tree is valid and the stored tree hash is equal to the file hash...
	if (storedTree.errorCheck() && *storedTree.hash == _file)
	{
		// ... then the file is corrupted.
		
		// Check file length
		
		fileIfs.seekg(0, fileIfs.end);
		long currentFileLength = fileIfs.tellg();
		fileIfs.seekg(0, fileIfs.beg);
		
		if (currentFileLength > storedTree.getTotalBytes())
		{
			// The file is too long!
			
			if (DEBUGGING) printf("File is too long!\r\n");
			
			newTree = generateMerkelTreeFromFilePath(filePath, storedTree.getTotalBytes());
			
			if (!newTree->errorCheck())
			{
				// wtf
				exitWithError("Fatal: Failed to generate merkel tree properly!\r\n");
			}
			
			// If the first storedTree.calcDataSize() bytes of the file match the stored tree perfectly...
			if (newTree->equals(storedTree))
			{
				// ...just truncate the file
				fileIfs.close();
				std::filesystem::resize_file(filePath, storedTree.getTotalBytes());
				goto checkFixed;
			}
			else
			{
				std::vector<std::array<char, 32>> storedTreeBlockHashes = storedTree.listBlockHashes();
				std::vector<std::array<char, 32>> newTreeBlockHashes = newTree->listBlockHashes();
				
				for (long blockIndex=0; blockIndex<storedTreeBlockHashes.size() && blockIndex<newTreeBlockHashes.size(); blockIndex++)
				{
					if (storedTreeBlockHashes[blockIndex] != newTreeBlockHashes[blockIndex])
					{
						char buff[1024];
						fileIfs.seekg(blockIndex * 1024, fileIfs.beg);
						fileIfs.read(&buff[0], 1024);
						int amountRead = fileIfs.gcount();
						
						if (tryFixBlockUsingHash(&buff[0], amountRead, storedTreeBlockHashes[blockIndex]) == true)
						{
							// YAY :)
							// Write the correct block to the file
							
							if (DEBUGGING) printf("A mischief was fixed :D\r\n");
							
							long pos = fileIfs.tellg();
							if (pos <= 0) exitWithError("askdjfjkasdfjkdsf");
							
							fileIfs.close();
							
							{
								std::fstream fileIOfs(filePath, std::ios::in | std::ios::out | std::ios::binary);
								fileIOfs.seekg(pos-amountRead, std::ios_base::beg);
								fileIOfs.write(&buff[0], amountRead);
								if (fileIOfs.tellg() != pos) exitWithError("askaskdjdsjkaf");
								fileIOfs.close();
							}
							
							fileIfs = std::ifstream(filePath, std::ios::in | std::ios::binary);
							fileIfs.seekg(pos);
							if (fileIfs.tellg() != pos) exitWithError("askaskdjdsjkaf");
						}
					}
				}
				goto checkFixed;
			}
		}
		else if (currentFileLength < storedTree.getTotalBytes())
		{
			// The file is too short!
			
			if (DEBUGGING) printf("File is too short!\r\n");
			
			// TODO
			goto checkFixed;
		}
		else
		{
			// The file has the correct length!
			
			if (DEBUGGING) printf("File has the correct length!\r\n");
			
			// Search for the corruption, block by block.
			
			std::vector<std::array<char, 32>> blockhashes = storedTree.listBlockHashes();
			
			char buff[1024];
			std::array<char, 32> hashFromFile;
			
			for (long blockIndex=0; blockIndex<blockhashes.size(); blockIndex++)
			{
				fileIfs.seekg(blockIndex * 1024, fileIfs.beg);
				fileIfs.read(&buff[0], 1024);
				int amountRead = fileIfs.gcount();
				if (amountRead <= 0) exitWithError("fileIfs.gcount() <= 0");
				SHA256 sha256;
				sha256.init();
				sha256.update((const unsigned char*)&buff[0], amountRead);
				sha256.final((unsigned char*)hashFromFile.data());
				
				if (amountRead != 1024)
				{
					if (blockIndex != blockhashes.size()-1)
					{
						printf("The file cannot be fully read. errorFix() is confused and cannot continue.\r\n");
						return EFR_FAILED_TO_FIX;
					}
				}
				
				if (blockhashes[blockIndex] != hashFromFile)
				{
					// We found the mischief!
					
					if (DEBUGGING)
					{
						std::string fs = bytes_to_hex(hashFromFile);
						std::string ts = bytes_to_hex(blockhashes[blockIndex]);
						
						printf("Mischief found in blockIndex=%i hashFromFile=%s hash from tree=%s amountRead=%i!\r\n", blockIndex, fs.c_str(), ts.c_str(), amountRead);
					}
					
					if (tryFixBlockUsingHash(&buff[0], amountRead, blockhashes[blockIndex]) == true)
					{
						// YAY :)
						// Write the correct block to the file
						
						if (DEBUGGING) printf("Mischief fixed using hash :D\r\n");
						
						//long pos = fileIfs.tellg();
						//if (pos <= 0) exitWithError("askdjfjkasdfjkdsf");
						long pos = blockIndex * 1024 + amountRead;
						
						fileIfs.close();
						
						{
							std::fstream fileIOfs(filePath, std::ios::in | std::ios::out | std::ios::binary);
							fileIOfs.seekg(pos-amountRead, std::ios_base::beg);
							fileIOfs.write(&buff[0], amountRead);
							if (fileIOfs.tellg() != pos) { printf("pos=%i fileIOfs.tellg()=%i\r\n", (int)pos, (int)fileIOfs.tellg()); exitWithError("askaskdjdsjkaf"); }
							fileIOfs.close();
						}
						
						fileIfs = std::ifstream(filePath, std::ios::in | std::ios::binary);
						fileIfs.seekg(pos);
						if (fileIfs.tellg() != pos) exitWithError("askaskdjdsjkaf fileIfs.tellg() != pos");
						
						/*
						fileIfs.seekg(-amountRead, std::ios_base::cur);
						fileIfs.write(&buff[0], amountRead);
						*/
					}
					else if (tryFixBlockUsingParityAndHash(filePath, fileIfs, parityIfs, blockIndex, &buff[0], amountRead, blockhashes[blockIndex]))
					{
						if (DEBUGGING) printf("Mischief fixed using parity :D\r\n");
						
						
						//long pos = fileIfs.tellg();
						//if (pos <= 0) exitWithError("askdjfjkasdfjkdsf");
						long pos = blockIndex * 1024 + amountRead;
						
						fileIfs.close();
						
						{
							std::fstream fileIOfs(filePath, std::ios::in | std::ios::out | std::ios::binary);
							fileIOfs.seekg(pos-amountRead, std::ios_base::beg);
							fileIOfs.write(&buff[0], amountRead);
							if (fileIOfs.tellg() != pos) { printf("pos=%i fileIOfs.tellg()=%i\r\n", (int)pos, (int)fileIOfs.tellg()); exitWithError("askaskdjdsjkaf"); }
							fileIOfs.close();
						}
						
						fileIfs = std::ifstream(filePath, std::ios::in | std::ios::binary);
						fileIfs.seekg(pos);
						if (fileIfs.tellg() != pos) exitWithError("askaskdjdsjkaf fileIfs.tellg() != pos");
						
						/*fileIfs.seekg(-amountRead, std::ios_base::cur);
						fileIfs.write(&buff[0], amountRead);*/
					}
					else
					{
						return EFR_FAILED_TO_FIX;
					}
				}
			}
			
			fileIfs.close();
			treeIfs.close();
			goto checkFixed;
		}
		
		fileIfs.close();
		treeIfs.close();
	}
	else if (storedTree.errorCheck() && *storedTree.hash != _file)
	{
		// Catastrophic failure, this should never happen,
		// unless the user manually overwrite this file's tree with another file's tree.
		return EFR_FAILED_TO_FIX;
	}
	
	treeIfs.close();
	goto checkFixed;
}
