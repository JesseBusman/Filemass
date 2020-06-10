#include <memory>
#include <iostream>

#include "sha256.h"
#include "util.h"
#include "merkel_tree.h"

MerkelNode::MerkelNode()
{
}

void MerkelNode::calcHash()
{
	if (this->hashWritten) { /*std::cout << "calcHash() called, but hash is already written\r\n";*/ return; }

	if (child0 == nullptr && child1 == nullptr)
	{
		//std::cout << "calcHash() null null\r\n";
		memset(&hash[0], 0x00, 32);
	}
	else if (child0 == nullptr && child1 != nullptr)
	{
		//std::cout << "calcHash() null non-null\r\n";
		throw "child0 == nullptr && child1 != nullptr";
	}
	else if (child0 != nullptr && child1 == nullptr)
	{
		//std::cout << "calcHash() non-null null\r\n";
		child0->calcHash();
		memcpy(&hash[0], &child0->hash[0], 32);
	}
	else if (child0 != nullptr && child1 != nullptr)
	{
		//std::cout << "calcHash() non-null non-null\r\n";
		child0->calcHash(); 
		child1->calcHash();
		SHA256 sha256;
		sha256.update((unsigned char*)& child0->hash[0], 32);
		sha256.update((unsigned char*)& child1->hash[0], 32);
		sha256.final((unsigned char*)& this->hash[0]);
		char hex[65];
		hex[64] = 0x00;
		bytes_to_hex(this->hash, 32, &hex[0]);
		//std::cout << "calcHash() on node with 2 children resulted in: " << hex << "\r\n";;
	}
	else throw "asgsddagsagds";

	//std::cout << "calcHash() done\r\n";

	this->hashWritten = true;
}

void MerkelNode::tryCalcHashForgetChildren()
{
	if (this->hashWritten && this->childrenForgotten) return;
	if (this->childrenForgotten) throw "tryCalcHashForgetChildren() called but childrenForgotten=true";
	if (this->level != 0)
	{
		if (this->child0 == nullptr || this->child1 == nullptr) return;
	}

	if (!this->isFull()) return;
	
	this->calcHash();

	this->child0 = nullptr;
	this->child1 = nullptr;
	this->childrenForgotten = true;
}

long MerkelNode::calcDataSize()
{
	if (level == 0) return dataSize;
	else return child0->calcDataSize() + child1->calcDataSize();
}

bool MerkelNode::isFull()
{
	if (level == 0) return true;
	else return this->childrenForgotten || (child0 != nullptr && child1 != nullptr && child0->isFull() && child1->isFull());
}






MerkelTree::MerkelTree()
{
	rootMerkelNode = std::make_shared<MerkelNode>();
	currentMerkelNode = rootMerkelNode;
	currentMerkelNode->level = 0;
	seenNon1024segment = false;
	finalized = false;
	totalBytes = 0;
}
long MerkelTree::getTotalBytes()
{
	if (!finalized) throw("getTotalBytes() called on non-finalized merkel tree");
	return totalBytes;
}
void MerkelTree::finalize()
{
	// std::cout << "Finalize running calcHash()\r\n";
	if (finalized) throw("finalize() called on already finalized merkel tree");
	rootMerkelNode->calcHash();
	memcpy(&this->hash[0], &rootMerkelNode->hash[0], 32);
	finalized = true;
	// std::cout << "finalize() done\r\n";
}
void MerkelTree::addData(const char* data, int amountBytes)
{
	//std::cout << "addData() called with " << amountBytes << " bytes\r\n";

	if (finalized) throw("Cannot addData to finalized merkel tree");
	else if (amountBytes < 0) throw("addData amountBytes<0");
	else if (seenNon1024segment) throw("Merkel tree saw a non-1024-byte segment already.");
	else if (amountBytes == 1024) {}
	else if (amountBytes < 1024) seenNon1024segment = true;
	else throw "Merkel tree must only receive blocks of 1024 bytes, and optionally one last block less than 1024 bytes.";
	
	// Search upwards until we're at the finished root node, or we're at a node with < 2 children
	while (true)
	{
		//std::cout << "addData() first loop: currentNode: level=" << currentMerkelNode->level << " child0=" << (currentMerkelNode->child0 == nullptr ? "null" : "val") << " child1=" << (currentMerkelNode->child1 == nullptr ? "null" : "val") << "\r\n";

		if (currentMerkelNode == rootMerkelNode)
		{
			//std::cout << "addData() first loop is at the root node!\r\n";
			rootMerkelNode = std::make_shared<MerkelNode>();
			rootMerkelNode->level = currentMerkelNode->level + 1;
			rootMerkelNode->child0 = currentMerkelNode;
			currentMerkelNode->parent = rootMerkelNode;
			break;
		}
		else if (currentMerkelNode->child0 != nullptr && currentMerkelNode->child1 != nullptr)
		{
			//std::cout << "addData() first loop is at a node with 2 children!!\r\n";
			currentMerkelNode->tryCalcHashForgetChildren();
			currentMerkelNode = currentMerkelNode->parent;
			continue;
		}
		else if (currentMerkelNode->child0 == nullptr && currentMerkelNode->child1 == nullptr && currentMerkelNode->hashWritten == true)
		{
			//std::cout << "addData() first loop is at a node with data and 0 children!!\r\n";
			currentMerkelNode->tryCalcHashForgetChildren();
			currentMerkelNode = currentMerkelNode->parent;
			continue;
		}
		else
		{
			//std::cout << "addData() first loop is at a node with <2 children!\r\n";
			break;
		}
	}

	// Search downwards until we're at a bottom node
	while (currentMerkelNode->level > 0)
	{
		//std::cout << "addData() second loop is creating a new node\r\n";

		//std::cout << "addData() second loop: currentNode: level=" << currentMerkelNode->level << " child0=" << (currentMerkelNode->child0 == nullptr ? "null" : "val") << " child1=" << (currentMerkelNode->child1 == nullptr ? "null" : "val") << "\r\n";

		std::shared_ptr<MerkelNode> newNode = std::make_shared<MerkelNode>();
		newNode->level = currentMerkelNode->level - 1;
		newNode->parent = currentMerkelNode;

		if (currentMerkelNode->child0 == nullptr)
		{
			currentMerkelNode->child0 = newNode;
		}
		else if (currentMerkelNode->child1 == nullptr)
		{
			currentMerkelNode->child1 = newNode;
		}
		else throw "agjasjjkdsf";
		currentMerkelNode = newNode;
	}

	//std::cout << "addData() after second loop: currentNode: level=" << currentMerkelNode->level << " child0=" << (currentMerkelNode->child0 == nullptr ? "null" : "val") << " child1=" << (currentMerkelNode->child1 == nullptr ? "null" : "val") << "\r\n";


	if (currentMerkelNode->hashWritten) throw "Fatal bug in merkel tree: attempted to overwrite hash in node";



	totalBytes += amountBytes;


	SHA256 sha256;
	sha256.init();
	sha256.update((unsigned char*)&data[0], amountBytes);
	sha256.final((unsigned char*)&currentMerkelNode->hash[0]);
	currentMerkelNode->hashWritten = true;
	currentMerkelNode->dataSize = amountBytes;
}
