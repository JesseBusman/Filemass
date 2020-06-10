#include <string>
#include <algorithm>
#include <fstream>
#include <array>
#include <map>
#include <iostream>
#include <sstream>

#include "util.h"
#include "sqlite3.h"
#include "json.h"

char HEX_CHARS[] = "0123456789ABCDEF";

extern bool arg_json;
extern JsonValue_Map jsonOutput;

void exitWithError(const char* errorMessage)
{
	if (arg_json)
	{
		jsonOutput.map["error"] = std::make_shared<JsonValue_String>(errorMessage);
		std::stringstream str;
		jsonOutput.write(str);
		std::cout << str.rdbuf();
	}
	else
	{
		std::cerr << errorMessage << "\r\n";
	}
	exit(1);
}

void exitWithError(const std::string& errorMessage)
{
	exitWithError(errorMessage.c_str());
}

sqlite3_stmt* p(sqlite3* db, const char* query)
{
	static std::map<const char*, sqlite3_stmt*> query_to_stmt;
	{
		auto it = query_to_stmt.find(query);
		if (it != query_to_stmt.end())
		{
			int ret = sqlite3_reset(it->second);
			if (ret != SQLITE_OK)
			{
				std::cout << "\r\nsqlite3_reset was not OK: " << std::to_string(ret) << "\r\n" << query << "\r\n";
				sqlite3_finalize(it->second);
			}
			else
			{
				return it->second;
			}
		}
	}
	sqlite3_stmt* stmt;
	int prepareResult = sqlite3_prepare_v2(db, query, strlen(query), &stmt, nullptr);
	if (prepareResult != SQLITE_OK)
	{
		std::cout << "\r\nsqlite3_prepare_v2 returned error " << prepareResult << " on query " << query << "\r\n";
		std::cout << sqlite3_errmsg(db) << "\r\n";
		exit(1);
		throw 1;
	}
	query_to_stmt[query] = stmt;
	return stmt;
}

void q(sqlite3* db, const char* query)
{
	int stepResult = sqlite3_step(p(db, query));
	if (stepResult != SQLITE_DONE)
	{
		std::cerr << "\r\nsqlite3_step did not return SQLITE_DONE on query " << query << "\r\n";
		std::cerr << sqlite3_errmsg(db) << "\r\n";
		exit(1);
		throw 1;
	}
}

void bytes_to_hex(const char* bytes, int amountBytes, char* hexOut)
{
	for (int i = 0; i < amountBytes; i++)
	{
		hexOut[i * 2 + 0] = HEX_CHARS[(bytes[i] >> 4) & 0xF];
		hexOut[i * 2 + 1] = HEX_CHARS[(bytes[i] >> 0) & 0xF];
	}
}

inline int hex_char_to_bits(char c)
{
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= '0' && c <= '9') return c - '0';
	return -1;
}

int hex_to_bytes(const char* hex, int maxBytes, char* bytesOut)
{
	int i = 0;
	for (; i<maxBytes; i++)
	{
		int bits1 = hex_char_to_bits(hex[0]);
		if (bits1 == -1) return i;

		int bits2 = hex_char_to_bits(hex[1]);
		if (bits2 == -1) throw "HEX string must have even length";

		bytesOut[i] = (char)((bits1 << 4) | bits2);

		hex += 2;
	}
	return i;
}

[[nodiscard]] std::string trim(std::string s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
	return s;
}

void xor256bit(unsigned char* sourceAndOut, const unsigned char* source2)
{
	for (int i=0; i<32; i++) sourceAndOut[i] ^= source2[i];
}

void add256bit(unsigned char* sourceAndOut, const unsigned char* source2)
{
	unsigned char carry = 0;
	for (int i=31; i>=0; i--)
	{
		unsigned char b1 = sourceAndOut[i];
		unsigned char b2 = source2[i];

		unsigned char sum = b1 + b2 + carry;

		if (sum < b1 || sum < b2)
		{
			carry = 1;
		}

		sourceAndOut[i] = sum;
	}
}

void non_commutative__non_associative__hash_sum(const unsigned char* hash1, const unsigned char* hash2, unsigned char* outHash)
{
	const unsigned char CONSTANT[] = {
		0x60, 0xeb, 0xdf, 0x09, 0xe0, 0x93, 0x2a, 0x53, 0xb0, 0xa2, 0x1c, 0x4f, 0x84, 0xa0, 0x93, 0x66,
		0xad, 0x4c, 0x78, 0x1a, 0xef, 0xd3, 0x07, 0x19, 0xa1, 0x07, 0x28, 0xe5, 0x30, 0x8c, 0x48, 0x80,
	};

	for (int i=0; i<32; i++) outHash[i] = hash1[i] ^ CONSTANT[i];

	xor256bit(outHash, CONSTANT);

	add256bit(outHash, hash2);
}
/*
void reverse__non_commutative__non_associative__hash_sum(const unsigned char* hash1, const unsigned char* hash2, unsigned char* outHash)
{
	const unsigned char CONSTANT[] = {
		0x60, 0xeb, 0xdf, 0x09, 0xe0, 0x93, 0x2a, 0x53, 0xb0, 0xa2, 0x1c, 0x4f, 0x84, 0xa0, 0x93, 0x66,
		0xad, 0x4c, 0x78, 0x1a, 0xef, 0xd3, 0x07, 0x19, 0xa1, 0x07, 0x28, 0xe5, 0x30, 0x8c, 0x48, 0x80,
	};

	for (int i=0; i<32; i++) outHash[i] = hash1[i] ^ CONSTANT[i];

	xor256bit(outHash, CONSTANT);

	add256bit(outHash, hash2);
}
*/

std::array<char, 32> non_commutative__non_associative__hash_sum(const std::array<char, 32>& hash1, const std::array<char, 32>& hash2)
{
	std::array<char, 32> ret;
	non_commutative__non_associative__hash_sum((const unsigned char*)hash1.data(), (const unsigned char*)hash2.data(), (unsigned char*)ret.data());
	return ret;
}

void readExactly(std::ifstream& source, char* destBuffer, unsigned long long amount)
{
	while (amount > 0)
	{
		source.read(destBuffer, amount);
		std::streamsize amountRead = source.gcount();

		if (amountRead > amount) exitWithError("wtf in readExactly: amountRead > amount");

		destBuffer += amountRead;
		amount -= amountRead;

		if (amount > 0 && source.eof()) exitWithError("readExactly() reached end of file! " + std::to_string(amount) + " bytes remaining");
	}
}

void readExactly(std::fstream& source, char* destBuffer, unsigned long long amount)
{
	while (amount > 0)
	{
		source.read(destBuffer, amount);
		std::streamsize amountRead = source.gcount();

		if (amountRead > amount) exitWithError("wtf in readExactly: amountRead > amount");

		destBuffer += amountRead;
		amount -= amountRead;
	}
}

std::array<char, 32> ZERO_HASH;

std::array<char, 32> sqlite3_column_32chars(sqlite3_stmt* stmt, int columnIndex)
{
	int size = sqlite3_column_bytes(stmt, columnIndex);
	if (size != 32) exitWithError("sqlite3_column_32chars on column with size of " +std::to_string(size));
	std::array<char, 32> ret;
	memcpy(ret.data(), sqlite3_column_blob(stmt, columnIndex), 32);
	return ret;
}
