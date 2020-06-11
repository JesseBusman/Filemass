#pragma once

#include <memory>
#include <optional>
#include <array>

class MerkelNode
{
public:
	MerkelNode(int _level, std::shared_ptr<MerkelNode> _parent);
	void setData(const char* _data, int _amountBytes);
	int level;
	std::optional<std::array<char, 32>> hash = std::nullopt;
	int dataSize = -1;
	std::shared_ptr<MerkelNode> parent;
	std::shared_ptr<MerkelNode> child0 = nullptr;
	std::shared_ptr<MerkelNode> child1 = nullptr;

	void forgetChildrenIfFull();
	
	void setChild0(std::shared_ptr<MerkelNode> _child0);
	void setChild1(std::shared_ptr<MerkelNode> _child1);
	
	const std::array<char, 32>& getHash();
	
	bool isFull() const;
	long calcDataSize();
};

class MerkelTree
{
private:
	std::shared_ptr<MerkelNode> rootMerkelNode;
	std::shared_ptr<MerkelNode> currentMerkelNode;
	bool seenNon1024segment;
	long totalBytes;
public:
	std::optional<std::array<char, 32>> hash;
	MerkelTree();
	long getTotalBytes() const;
	void finalize();
	void addData(const char* data, int amountBytes);
};
