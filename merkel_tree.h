#pragma once

#include <memory>
#include <optional>
#include <array>
#include <ostream>
#include <vector>

class Error_MerkelTreeFileCorrupted
{
};

class MerkelNode : public std::enable_shared_from_this<MerkelNode>
{
public:
	MerkelNode(int _level, std::shared_ptr<MerkelNode> _parent);
	MerkelNode(std::istream& _serializedTree);
	void setData(const char* _data, int _amountBytes);
	unsigned char level;
	std::optional<std::array<char, 32>> hash = std::nullopt;
	long dataSize = -1;
	std::shared_ptr<MerkelNode> parent;
	std::shared_ptr<MerkelNode> child0 = nullptr;
	std::shared_ptr<MerkelNode> child1 = nullptr;
	
	void forgetChildrenIfFull();
	
	void setChild0(std::shared_ptr<MerkelNode> _child0);
	void setChild1(std::shared_ptr<MerkelNode> _child1);
	
	const std::array<char, 32>& getHash(bool _cleanupChildrenWhenDone);
	
	bool isFull() const;
	long calcDataSize();
	void serialize(std::ostream& _dest);
	bool equals(MerkelNode& _other);
	void listBlockHashes(std::vector<std::array<char, 32>>& _out) const;
	bool errorCheck(bool _isRightMostNode=true);
};

class MerkelTree
{
private:
	std::shared_ptr<MerkelNode> rootMerkelNode;
	std::shared_ptr<MerkelNode> currentMerkelNode;
	bool seenNon1024segment;
	long totalBytes;
public:
	bool serializable;
	std::optional<std::array<char, 32>> hash;
	MerkelTree(bool _serializable);
	MerkelTree(std::istream& _serializedTree);
	long getTotalBytes() const;
	void finalize();
	void addData(const char* data, int amountBytes);
	void serialize(std::ostream& _dest);
	bool equals(const MerkelTree& _other) const;
	bool errorCheck();
	std::vector<std::array<char, 32>> listBlockHashes() const;
};

std::shared_ptr<MerkelTree> generateMerkelTreeFromFilePath(std::string _path, long maxBytesToRead=-1);
