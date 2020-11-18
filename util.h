#pragma once

#include <string>
#include <array>

struct sqlite3_stmt;
struct sqlite3;

extern bool DEBUGGING;

void exitWithError(const char* errorMessage);
void exitWithError(const std::string& errorMessage);

sqlite3_stmt* p(sqlite3* db, const char* query);
void q(sqlite3* db, const char* query);

void bytes_to_hex(const char* bytes, int amountBytes, char* hexOut);
int hex_to_bytes(const char* hex, int maxBytes, char* bytesOut);
[[nodiscard]] std::string trim(std::string s);
void non_commutative__non_associative__hash_sum(const unsigned char* hash1, const unsigned char* hash2, unsigned char* outHash);
std::array<char, 32> non_commutative__non_associative__hash_sum(const std::array<char, 32>& hash1, const std::array<char, 32>& hash2);
void xor256bit(unsigned char* sourceAndOut, const unsigned char* source2);
void add256bit(unsigned char* sourceAndOut, const unsigned char* source2);
void readExactly(std::ifstream& source, char* destBuffer, unsigned long long amount);
void readExactly(std::fstream& source, char* destBuffer, unsigned long long amount);
template <unsigned long L>
void bytes_to_hex(const std::array<char, L> bytes, char* hexOut)
{
	bytes_to_hex(bytes.data(), (int)L, hexOut);
}
/*template <int L>
void bytes_to_hex(const std::array<char, L> bytes, char* hexOut)
{
	bytes_to_hex(bytes.data(), L, hexOut);
}*/
template <unsigned long L>
std::string bytes_to_hex(const std::array<char, L>& bytes)
{
	char str[L*2];
	bytes_to_hex(bytes.data(), (int)L, &str[0]);
	return std::string(str, L*2);
}
/*template <int L>
std::string bytes_to_hex(const std::array<char, L>& bytes)
{
	char str[L*2];
	bytes_to_hex(bytes.data(), L, &str[0]);
	return std::string(str, L*2);
}*/
template <unsigned long L>
int hex_to_bytes(const char* str, std::array<char, L>& out)
{
	return hex_to_bytes(str, (int)L, out.data());
}
/*template <int L>
int hex_to_bytes(const char* str, std::array<char, L>& out)
{
	return hex_to_bytes(str, L, out.data());
}*/
extern std::array<char, 32> ZERO_HASH;

std::array<char, 32> sqlite3_column_32chars(sqlite3_stmt* stmt, int columnIndex);
