#include <memory>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <vector>

#include "sha256.h"
#include "util.h"
#include "merkel_tree.h"

MerkelNode::MerkelNode(int _level, std::shared_ptr<MerkelNode> _parent):
	level(_level),
	parent(_parent)
{
}

void MerkelNode::setData(const char* _data, int _amountBytes)
{
	if (this->level != 0) exitWithError("MerkelNode::setData, but node is not level 0");
	if (this->dataSize != -1) exitWithError("MerkelNode::setData, but node already has data");
	if (this->hash.has_value()) exitWithError("MerkelNode::setData, but node already has hash");
	
	SHA256 sha256;
	sha256.init();
	sha256.update((const unsigned char*)&_data[0], _amountBytes);
	this->hash = ZERO_HASH;
	sha256.final((unsigned char*)this->hash->data());
	this->dataSize = _amountBytes;
}

void MerkelNode::forgetChildrenIfFull()
{
	if (this->isFull())
	{
		this->getHash(false);
		this->calcDataSize();
		this->child0 = nullptr;
		this->child1 = nullptr;
		this->parent->forgetChildrenIfFull();
	}
}

void MerkelNode::setChild0(std::shared_ptr<MerkelNode> _child0)
{
	if (level == 0) exitWithError("setChild0 called on level 0");
	this->child0 = _child0;
}

void MerkelNode::setChild1(std::shared_ptr<MerkelNode> _child1)
{
	if (level == 0) exitWithError("setChild1 called on level 0");
	this->child1 = _child1;
}

const std::array<char, 32>& MerkelNode::getHash(bool _cleanupChildrenWhenDone)
{
	if (level == 0)
	{
		if (dataSize == 0) exitWithError("getHash() on level==0 && dataSize==0");
		if (!this->hash.has_value()) exitWithError("getHash() on level==0 without hash");
		return *this->hash;
	}
	else
	{
		if (!this->hash.has_value())
		{
			if (child0 == nullptr && child1 == nullptr)
			{
				exitWithError("child0 == nullptr && child1 == nullptr");
			}
			else if (child0 == nullptr && child1 != nullptr)
			{
				exitWithError("child0 == nullptr && child1 != nullptr");
			}
			else if (child0 != nullptr && child1 == nullptr)
			{
				this->hash = child0->getHash(_cleanupChildrenWhenDone);
				if (_cleanupChildrenWhenDone) this->child0 = nullptr;
			}
			else if (child0 != nullptr && child1 != nullptr)
			{
				this->hash = ZERO_HASH;
				SHA256 sha256;
				sha256.update((unsigned char*)child0->getHash(_cleanupChildrenWhenDone).data(), 32);
				sha256.update((unsigned char*)child1->getHash(_cleanupChildrenWhenDone).data(), 32);
				sha256.final((unsigned char*)this->hash->data());
				if (_cleanupChildrenWhenDone) this->child0 = nullptr;
				if (_cleanupChildrenWhenDone) this->child1 = nullptr;
			}
		}
		return *this->hash;
	}
}

long MerkelNode::calcDataSize()
{
	if (this->dataSize >= 1) return dataSize;
	else return this->dataSize = (child0 == nullptr ? 0 : child0->calcDataSize()) + (child1 == nullptr ? 0 : child1->calcDataSize());
}

bool MerkelNode::isFull() const
{
	if (level == 0) return false; //this->dataSize >= 1;
	else return this->hash.has_value() || (child0 != nullptr && child1 != nullptr && child0->isFull() && child1->isFull());
}

void MerkelNode::serialize(std::ostream& _dest)
{
	this->calcDataSize();
	this->getHash(false);
	if (this->hash == std::nullopt) exitWithError("no hash after getHash()");
	
	_dest.write((char*)&this->level, 1);
	_dest.write((char*)&this->dataSize, 8);
	_dest.write(this->hash->data(), 32);
	if (this->child0 != nullptr) this->child0->serialize(_dest);
	if (this->child1 != nullptr) this->child1->serialize(_dest);
}

MerkelNode::MerkelNode(std::istream& _serializedTree)
{
	_serializedTree.read((char*)&this->level, 1);
	if (_serializedTree.gcount() != 1) throw Error_MerkelTreeFileCorrupted();
	
	_serializedTree.read((char*)&this->dataSize, 8);
	if (_serializedTree.gcount() != 8) throw Error_MerkelTreeFileCorrupted();
	
	this->hash = ZERO_HASH;
	_serializedTree.read(this->hash->data(), 32);
	if (_serializedTree.gcount() != 32) throw Error_MerkelTreeFileCorrupted();
	
	if (!_serializedTree.eof())
	{
		unsigned char nextLevel = (unsigned char)_serializedTree.peek();
		if (nextLevel < level)
		{
			if (nextLevel+1 != level) throw Error_MerkelTreeFileCorrupted();
			this->child0 = std::make_shared<MerkelNode>(_serializedTree);
			//this->child0->parent = this->shared_from_this();
		}
		
		nextLevel = (unsigned char)_serializedTree.peek();
		if (nextLevel < level)
		{
			if (nextLevel+1 != level) throw Error_MerkelTreeFileCorrupted();
			this->child1 = std::make_shared<MerkelNode>(_serializedTree);
			//this->child1->parent = this->shared_from_this();
		}
	}
}


MerkelTree::MerkelTree(std::istream& _serializedTree)
{
	serializable = true;
	rootMerkelNode = std::make_shared<MerkelNode>(_serializedTree);
	this->hash = rootMerkelNode->getHash(false);
	totalBytes = rootMerkelNode->calcDataSize();
}

MerkelTree::MerkelTree(bool _serializable)
{
	currentMerkelNode = rootMerkelNode = std::make_shared<MerkelNode>(0, nullptr);
	seenNon1024segment = false;
	totalBytes = 0;
	serializable = _serializable;
}
long MerkelTree::getTotalBytes() const
{
	if (!this->hash.has_value()) exitWithError("getTotalBytes() called on non-finalized merkel tree");
	return totalBytes;
}
void MerkelTree::finalize()
{
	if (this->hash.has_value()) exitWithError("finalize() called on already finalized merkel tree");
	this->hash = rootMerkelNode->getHash(!serializable);
	if (!serializable) this->rootMerkelNode = nullptr;
}

void MerkelTree::addData(const char* data, int amountBytes)
{
	if (this->hash.has_value()) exitWithError("Cannot addData to finalized merkel tree");
	else if (amountBytes < 0) exitWithError("addData amountBytes<0");
	else if (seenNon1024segment) exitWithError("Merkel tree saw a non-1024-byte segment already.");
	else if (amountBytes == 1024) {}
	else if (amountBytes < 1024) seenNon1024segment = true;
	else exitWithError("Merkel tree must only receive blocks of 1024 bytes, and optionally one last block less than 1024 bytes.");
	
	// Search upwards until we're at the finished root node, or we're at a node with < 2 children
	while (true)
	{
		if (currentMerkelNode == rootMerkelNode)
		{
			rootMerkelNode = std::make_shared<MerkelNode>(currentMerkelNode->level + 1, nullptr);
			rootMerkelNode->setChild0(currentMerkelNode);
			currentMerkelNode->parent = rootMerkelNode;
			break;
		}
		else if (currentMerkelNode->child0 != nullptr && currentMerkelNode->child1 != nullptr)
		{
			currentMerkelNode->getHash(!serializable);
			currentMerkelNode = currentMerkelNode->parent;
			continue;
		}
		else if (currentMerkelNode->child0 == nullptr && currentMerkelNode->child1 == nullptr && currentMerkelNode->hash.has_value())
		{
			currentMerkelNode->getHash(!serializable);
			currentMerkelNode = currentMerkelNode->parent;
			continue;
		}
		else
		{
			break;
		}
	}
	
	// Search downwards until we're at a bottom node
	while (currentMerkelNode->level > 0)
	{
		std::shared_ptr<MerkelNode> newNode = std::make_shared<MerkelNode>(currentMerkelNode->level - 1, currentMerkelNode);
		
		if (currentMerkelNode->child0 == nullptr) currentMerkelNode->setChild0(newNode);
		else if (currentMerkelNode->child1 == nullptr) currentMerkelNode->setChild1(newNode);
		else exitWithError("Fatal bug in MerkelTree: lower level node already has two children");
		
		currentMerkelNode = newNode;
	}
	
	currentMerkelNode->setData(data, amountBytes);
	if (!serializable) currentMerkelNode->parent->forgetChildrenIfFull();
	
	totalBytes += amountBytes;
}

void MerkelTree::serialize(std::ostream& _dest)
{
	if (!this->serializable) exitWithError("Fatal bug in MerkelTree: serialize called on non-serializable tree");
	
	rootMerkelNode->serialize(_dest);
}

bool MerkelTree::equals(const MerkelTree& _other) const
{
	if (this->totalBytes != _other.totalBytes) return false;
	return this->rootMerkelNode->equals(*_other.rootMerkelNode);
}
bool MerkelNode::equals(MerkelNode& _other)
{
	if (this->level != _other.level) return false;
	this->calcDataSize();
	_other.calcDataSize();
	if (this->dataSize != _other.dataSize) return false;
	if ((this->child0 == nullptr) != (_other.child0 == nullptr)) return false;
	if ((this->child1 == nullptr) != (_other.child1 == nullptr)) return false;
	if (this->child0 != nullptr && !this->child0->equals(*_other.child0)) return false;
	if (this->child1 != nullptr && !this->child1->equals(*_other.child1)) return false;
	return true;
}

bool MerkelNode::errorCheck(bool _isRightMostNode)
{
	if (this->child0 == nullptr && this->child1 != nullptr) return false;
	if ((this->child0 == nullptr && this->child1 == nullptr) ^ (this->level == 0)) return false;
	if (this->child0 != nullptr)
	{
		if (this->child0->level != this->level - 1) return false;
		//if (this->child0->parent.get() != this) return false;
		if (!this->child0->errorCheck(_isRightMostNode && this->child1 == nullptr)) return false;
	}
	if (this->child1 != nullptr)
	{
		if (this->child1->level != this->level - 1) return false;
		//if (this->child1->parent.get() != this) return false;
		if (!this->child1->errorCheck(_isRightMostNode)) return false;
	}
	if (this->level != 0)
	{
		std::array<char, 32> recomputedHash;
		if (child1 != nullptr)
		{
			SHA256 sha256;
			sha256.update((unsigned char*)child0->getHash(false).data(), 32);
			sha256.update((unsigned char*)child1->getHash(false).data(), 32);
			sha256.final((unsigned char*)recomputedHash.data());
		}
		else
		{
			recomputedHash = child0->getHash(false);
		}
		if (this->hash != recomputedHash) return false;
	}
	if (this->level == 0)
	{
		if (this->dataSize != 1024)
		{
			if (!_isRightMostNode) return false;
		}
	}
	return true;
}

bool MerkelTree::errorCheck()
{
	if (this->totalBytes != this->rootMerkelNode->dataSize) return false;
	if (this->hash != this->rootMerkelNode->getHash(false)) return false;
	if (!this->rootMerkelNode->errorCheck()) return false;
	return true;
}

std::shared_ptr<MerkelTree> generateMerkelTreeFromFilePath(std::string _path, long maxBytesToRead)
{
	std::shared_ptr<MerkelTree> merkelTree = std::make_shared<MerkelTree>(true);
	if (maxBytesToRead == -1) maxBytesToRead = std::filesystem::file_size(_path);
	unsigned long bytesRead = 0;
	
	if (DEBUGGING) std::cout << "maxBytesToRead=" << maxBytesToRead << " _path=" << _path << "\r\n";
	std::ifstream file(_path, std::ios_base::binary);
	char buff[1024];
	while (bytesRead < maxBytesToRead)
	{
		int amountToRead;
		if (bytesRead + 1024 <= maxBytesToRead) amountToRead = 1024;
		else amountToRead = maxBytesToRead - bytesRead;
		readExactly(file, &buff[0], amountToRead);
		merkelTree->addData(&buff[0], amountToRead);
		bytesRead += amountToRead;
	}
	file.close();
	if (bytesRead != maxBytesToRead) exitWithError("Failed to read entire file in generateMerkelTreeFromFilePath");
	merkelTree->finalize();
	return merkelTree;
}

void MerkelNode::listBlockHashes(std::vector<std::array<char, 32>>& _out) const
{
	if (this->level == 0)
	{
		_out.push_back(*this->hash);
	}
	else
	{
		if (this->child0 == nullptr) exitWithError("wtf983498324");
		else this->child0->listBlockHashes(_out);
		if (this->child1 != nullptr) this->child1->listBlockHashes(_out);
	}
}

std::vector<std::array<char, 32>> MerkelTree::listBlockHashes() const
{
	std::vector<std::array<char, 32>> ret;
	this->rootMerkelNode->listBlockHashes(ret);
	return ret;
}
