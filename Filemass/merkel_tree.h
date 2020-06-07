#pragma once

#include <memory>

class MerkelNode
{
public:
	int level = -1;
	char hash[32];
	bool hashWritten = false;
	bool childrenForgotten = false;
	int dataSize = 0;
	std::shared_ptr<MerkelNode> parent = nullptr;
	std::shared_ptr<MerkelNode> child0 = nullptr;
	std::shared_ptr<MerkelNode> child1 = nullptr;

	MerkelNode();

	void calcHash();
	void tryCalcHashForgetChildren();

	bool isFull();
	long calcDataSize();
};

class MerkelTree
{
private:
	std::shared_ptr<MerkelNode> rootMerkelNode;
	std::shared_ptr<MerkelNode> currentMerkelNode;
	bool seenNon1024segment;
	bool finalized;
	long totalBytes;
public:
	char hash[32];
	MerkelTree();
	long getTotalBytes();
	void finalize();
	void addData(const char* data, int amountBytes);
};
