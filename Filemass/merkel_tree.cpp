#include <memory>
#include <iostream>

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
		this->getHash();
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

const std::array<char, 32>& MerkelNode::getHash()
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
				this->hash = child0->getHash();
				this->child0 = nullptr;
			}
			else if (child0 != nullptr && child1 != nullptr)
			{
				this->hash = ZERO_HASH;
				SHA256 sha256;
				sha256.update((unsigned char*)child0->getHash().data(), 32);
				sha256.update((unsigned char*)child1->getHash().data(), 32);
				sha256.final((unsigned char*)this->hash->data());
				this->child0 = nullptr;
				this->child1 = nullptr;
			}
		}
		return *this->hash;
	}
}

long MerkelNode::calcDataSize()
{
	if (this->dataSize >= 1) return dataSize;
	else return this->dataSize = child0->calcDataSize() + child1->calcDataSize();
}

bool MerkelNode::isFull()
{
	if (level == 0) return false; //this->dataSize >= 1;
	else return this->hash.has_value() || (child0 != nullptr && child1 != nullptr && child0->isFull() && child1->isFull());
}

MerkelTree::MerkelTree()
{
	currentMerkelNode = rootMerkelNode = std::make_shared<MerkelNode>(0, nullptr);
	seenNon1024segment = false;
	totalBytes = 0;
}
long MerkelTree::getTotalBytes()
{
	if (!this->hash.has_value()) exitWithError("getTotalBytes() called on non-finalized merkel tree");
	return totalBytes;
}
void MerkelTree::finalize()
{
	if (this->hash.has_value()) exitWithError("finalize() called on already finalized merkel tree");
	this->hash = rootMerkelNode->getHash();
	this->rootMerkelNode = nullptr;
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
			currentMerkelNode->getHash();
			currentMerkelNode = currentMerkelNode->parent;
			continue;
		}
		else if (currentMerkelNode->child0 == nullptr && currentMerkelNode->child1 == nullptr && currentMerkelNode->hash.has_value())
		{
			currentMerkelNode->getHash();
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
	currentMerkelNode->parent->forgetChildrenIfFull();

	totalBytes += amountBytes;
}
