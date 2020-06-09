#include <string>
#include <vector>
#include <iostream>
#include <map>
#include <optional>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <cctype>
#include <locale>
#include <array>
#include <memory>
#include <Windows.h>

#include "sqlite3.h"
#include "util.h"
#include "sha256.h"
#include "repository.h"
#include "tag.h"
#include "tag_parser.h"
#include "path_pattern.h"
#include "json.h"

/*
Tag conventions:

photo of 2 people (1 known, 1 unknown wearing an AC/DC t-shirt) standing in front of a castle:
photo,person[name[Jesse Busman]],person[clothing[shirt[AC/DC]]],castle
*/

bool arg_debug = false;
bool arg_json = false;
JsonValue_Map jsonOutput;

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

/*
void exitWithErrorIfQueryFailed(int sqliteReturnCode)
{
	if (sqliteReturnCode != SQLITE_DONE && sqliteReturnCode != SQLITE_OK)
	{

	}
}
*/

int main(int argc, char* argv[])
{
	for (int i=0; i<32; i++) ZERO_HASH[i] = 0x00;

	//std::cout << "sha256(hello)=" << sha256("hello") << "\r\n";
	try
	{
		if (argc == 1)
		{
			std::cout
				<< "Repo:\r\n"
				<< "--repo=[dir]         Select repo located at directory [dir]\r\n"
				<< "--init-repo          Initialize the selected repository\r\n"
				<< "Tagbase:\r\n"
				<< "--tagbase=[file]     Select tagbase located in file [file]\r\n"
				<< "--init-tagbase       Initialize the selected tagbase\r\n"
				<< "Files:\r\n"
				<< "--files=[hashlist]   Select the files with hash in [hashlist]\r\n"
				<< "--add-files=[file]   Add files matching [file] to selected repo, and select them\r\n"
				<< "Tags:\r\n"
				<< "--tag=[tagquery]        Find files that match the given [tagquery]\r\n"
				<< "--add-tags=[taglist]    Add [tags] to the selected files\r\n"
				<< "--remove-tags=[taglist] Remove [tags] from the selected files\r\n"
				<< "\r\n"
				<< "Examples of [taglist] syntax:\r\n"
				<< "--add-tag=football,match,sport,team[Los Angeles],team[Chicago]\r\n"
				<< "--untag=team[name=Chicago]\r\n"
				<< "\r\n"
				<< "Examples of [tagquery] syntax:\r\n"
				<< "--tag=\"football & team[Los Angeles | Chicago]\"\r\n";
			exit(0);
			return 0;
		}
		
		std::optional<std::string> arg_repo;
		std::optional<std::string> arg_tagbase;
		std::optional<std::string> arg_files;
		std::optional<std::string> arg_add_files;
		std::optional<std::string> arg_tag;
		std::optional<std::string> arg_add_tags;
		std::optional<std::string> arg_remove_tags;
		
		arg_json = false;
		arg_debug = false;
		bool arg_init_repo = false;
		bool arg_init_tagbase = false;

		for (int i = 1; i < argc; i++)
		{
			const char* arg = argv[i];

			int start = 0;
			if (arg[0] == '-' && arg[1] != '-') start = 1;
			if (arg[0] == '-' && arg[1] == '-') start = 2;

			int equalsSignPosition = -1;

			for (int j = start; ; j++)
			{
				char c = arg[j];
				if (c == 0x00) break;
				if (c == '=')
				{
					equalsSignPosition = j;
				}
			}
		
			std::string field = (equalsSignPosition == -1) ? &arg[start] : std::string(&arg[start], equalsSignPosition - start);
			std::string value = (equalsSignPosition == -1) ? "" : &arg[equalsSignPosition + 1];

			if (field == "json")
			{
				arg_json = true;
			}
			else if (field == "debug")
			{
				arg_debug = true;
			}
			else if (field == "add-files")
			{
				arg_add_files = value;
			}
			else if (field == "repo")
			{
				arg_repo = value;
			}
			else if (field == "tagbase")
			{
				arg_tagbase = value;
			}
			else if (field == "files")
			{
				arg_files = value;
			}
			else if (field == "init-repo")
			{
				if (value.length() != 0)
				{
					exitWithError("--init-repo takes no arguments");
				}
				arg_init_repo = true;
			}
			else if (field == "init-tagbase")
			{
				if (value.length() != 0)
				{
					exitWithError("--init-tagbase takes no arguments");
				}
				arg_init_tagbase = true;
			}
			else if (field == "tags")
			{
				arg_tag = value;
			}
			else if (field == "add-tags")
			{
				arg_add_tags = value;
			}
			else if (field == "remove-tags" || field == "untag")
			{
				arg_remove_tags = value;
			}
			else
			{
				exitWithError("Unknown command line argument " + field);
			}
		}
		




		





		//////////////////////////
		//// --init-repo

		std::string selected_repository_path = arg_repo.has_value() ? *arg_repo : ".";

		if (arg_init_repo)
		{
			if (!std::filesystem::exists(selected_repository_path))
			{
				std::filesystem::create_directories(selected_repository_path);
			}
			if (!std::filesystem::is_directory(selected_repository_path))
			{
				exitWithError("Cannot init repository at " + selected_repository_path + " because it's not a directory");
			}
			if (!std::filesystem::is_empty(selected_repository_path))
			{
				exitWithError("Cannot init repository at " + selected_repository_path + " because that directory is not empty");
			}

			std::string config_path = selected_repository_path + "/fmrepo.conf";

			if (std::filesystem::exists(config_path))
			{
				exitWithError("Cannot init repository at " + selected_repository_path + " because config file already exists at " + config_path);
			}

			std::ofstream outfile(config_path, std::ios::out);
			outfile.write("", 0);
			outfile.flush();
			outfile.close();

			if (arg_json)
			{
				jsonOutput.set("initialized_repository", selected_repository_path);
			}
			else
			{
				std::cout << "Initialized repository at " << selected_repository_path << "\r\n";
			}
		}

		

		//////////////////////////////////////
		//// --repo
	
		std::shared_ptr<Repository> selected_repository = nullptr;

		if (!arg_init_tagbase)
		{
			if (std::filesystem::exists(selected_repository_path))
			{
				if (std::filesystem::is_directory(selected_repository_path))
				{
					selected_repository = std::make_shared<Repository>(selected_repository_path);
				}
				else
				{
					exitWithError("Specified repo path is not a directory: " + selected_repository_path);
				}
			}
			else
			{
				exitWithError("Specified repo does not exist: " + selected_repository_path);
			}
		}



		/////////////////////////////////////////////////
		//// --init-tagbase

		std::string selected_tagbase_path = arg_tagbase.has_value() ? *arg_tagbase : "./default_tagbase.sqlite3";
		sqlite3* tagbase_db = nullptr;

		if (arg_init_tagbase)
		{
			if (std::filesystem::exists(selected_tagbase_path))
			{
				exitWithError("Cannot init tagbase at " + selected_tagbase_path + " because that file already exists");
			}

			int sqlite_returncode = sqlite3_open(selected_tagbase_path.c_str(), &tagbase_db);

			if (sqlite_returncode != SQLITE_OK)
			{
				exitWithError("Cannot init tagbase at " + selected_tagbase_path + " because of sqlite error " + std::to_string(sqlite_returncode) + ": " + sqlite3_errmsg(tagbase_db));
			}
			


			q(
				tagbase_db,
				"CREATE TABLE edges"
				"("
				"	parent_hash_sum BLOB NOT NULL,"
				"	hash_sum BLOB NOT NULL,"
				"	_this_hash BLOB NOT NULL,"
				"	_file_hash BLOB NOT NULL,"
				"	_grandparent_hash_sum BLOB NOT NULL,"
				"	UNIQUE(parent_hash_sum, hash_sum)"
				")"
			);
			q(
				tagbase_db,
				"CREATE INDEX edges__index_on__parent_hash_sum__hash_sum__this_hash ON edges (parent_hash_sum, hash_sum, _this_hash)"
			); // todo figure out whether to make these indices UNIQUE
			q(
				tagbase_db,
				"CREATE INDEX edges__index_on__hash_sum__this_hash ON edges (hash_sum, _this_hash)"
			);
			q(
				tagbase_db,
				"CREATE INDEX edges__index_on__this_hash__parent_hash_sum ON edges (_this_hash, parent_hash_sum)"
			);
			q(
				tagbase_db,
				"CREATE INDEX edges__index_on__this_hash__file_hash ON edges (_this_hash, _file_hash)"
			);
			


			q(
				tagbase_db,
				"CREATE TABLE hashed_data"
				"("
				"	hash BLOB NOT NULL PRIMARY KEY,"
				"	data BLOB NOT NULL"
				")"
			);



			/*
			q(
				tagbase_db,
				"CREATE TABLE path_sums"
				"("
				"	file_hash BLOB NOT NULL,"
				"	partial_hash_sum BLOB NOT NULL,"
				"	start_hash_sum BLOB NOT NULL"
				")"
			);
			q(
				tagbase_db,
				"CREATE INDEX path_sums__index_on__file_hash__partial_hash_sum__start_hash_sum ON path_sums (file_hash, partial_hash_sum, start_hash_sum)"
			);
			q(
				tagbase_db,
				"CREATE INDEX path_sums__index_on__partial_hash_sum__start_hash_sum ON path_sums (partial_hash_sum, start_hash_sum)"
			);
			q(
				tagbase_db,
				"CREATE INDEX path_sums__index_on__start_hash_sum__file_hash ON path_sums (start_hash_sum, file_hash)"
			);
			*/



			q(
				tagbase_db,
				"CREATE TABLE parent_hash_sum_counts"
				"("
				"	parent_hash_sum BLOB NOT NULL,"
				"	count INTEGER NOT NULL"
				")"
			);
			q(
				tagbase_db,
				"CREATE INDEX parent_hash_sum_counts__index_on__parent_hash_sum ON parent_hash_sum_counts (parent_hash_sum)"
			);



			q(
				tagbase_db,
				"CREATE TABLE child_hash_counts"
				"("
				"	child_hash BLOB NOT NULL,"
				"	count INTEGER NOT NULL"
				")"
			);
			q(
				tagbase_db,
				"CREATE INDEX child_hash_counts__index_on__child_hash ON child_hash_counts (child_hash)"
			);



			// In the case of:
			// photo with file_hash AABB with tag person[Jesse Busman]
			// 
			
			
			/* ??
			if (selected_repository == nullptr)
			{
				sqlite3_close(tagbase_db);
				exit(0);
				return 0;
			}
			*/
		}




		//////////////////////////////////////////////////
		//// --tagbase

		if (tagbase_db == nullptr && (arg_tag.has_value() || arg_remove_tags.has_value() || arg_add_tags.has_value()))
		{
			if (!std::filesystem::exists(selected_tagbase_path))
			{
				exitWithError("Cannot open tagbase at " + selected_tagbase_path + " because that file does not exist");
			}
			if (!std::filesystem::is_regular_file(selected_tagbase_path))
			{
				exitWithError("Cannot open tagbase at " + selected_tagbase_path + " because that file is not a regular file");
			}

			int sqlite_returncode = sqlite3_open(selected_tagbase_path.c_str(), &tagbase_db);

			if (sqlite_returncode != SQLITE_OK)
			{
				exitWithError("Cannot open tagbase at " + selected_tagbase_path + " because of sqlite error " + std::to_string(sqlite_returncode) + ": " + sqlite3_errmsg(tagbase_db));
			}
		}






		/////////////////////////////////////////////////////
		//// --add-files

		std::vector<std::array<char, 32>> selected_file_hashes;
	
		if (arg_add_files.has_value())
		{
			std::shared_ptr<PathPattern> pathPattern = parsePathPattern(*arg_add_files);

			if (arg_debug) std::cout << "pathPattern = " << pathPattern->toString() << "\r\n";
			
			pathPattern->findFiles(".", [&selected_repository, &selected_file_hashes](const std::string& path){
				if (!std::filesystem::is_regular_file(path))
				{
					std::cerr << "\r\nThe file you're trying to add is not a regular file: " << path << "\r\n";
					exit(1);
					return 1;
				}
				selected_file_hashes.push_back(selected_repository->add(path));
			});
		}



		if (arg_files.has_value())
		{
			std::array<char, 32> hash;
			for (int i=0; i<arg_files->length(); i++)
			{
				char c = (*arg_files)[i];
				if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
				{
					int amountBytes = hex_to_bytes(&arg_files->c_str()[i], hash);
					if (amountBytes != 32)
					{
						exitWithError("Arguments to --files must be 32 bytes hex values.");
					}
					i += 64;
					selected_file_hashes.push_back(hash);
				}
			}
		}






		//////////////////////////////////////////////////////////
		//// --add-tags

		if (arg_add_tags.has_value())
		{
			if (selected_file_hashes.size() == 0)
			{
				exitWithError("To use --add-tags, at least one file must be selected (using --add-files or --files)");
			}

			if (tagbase_db == nullptr)
			{
				exitWithError("To use --add-tag, a tagbase must be selected");
			}

			std::vector<std::shared_ptr<Tag>> tags = parseTag(*arg_add_tags);

			q(tagbase_db, "BEGIN TRANSACTION");

			for (const auto& file_hash : selected_file_hashes)
			for (const auto& tag : tags)
			{
				tag->addTo(file_hash, ZERO_HASH, file_hash, tagbase_db, true);
			}

			q(tagbase_db, "COMMIT");
		}





		//////////////////////////////////////////////////////////
		//// --remove-tags

		if (arg_remove_tags.has_value())
		{
			if (selected_file_hashes.size() == 0)
			{
				exitWithError("To use --remove-tags, a file must be selected (using --files)");
			}

			if (tagbase_db == nullptr)
			{
				exitWithError("To use --remove-tags, a tagbase must be selected");
			}

			std::vector<std::shared_ptr<Tag>> tags = parseTag(*arg_add_tags);

			for (const auto& file_hash : selected_file_hashes)
			for (auto tag : tags)
			{
				tag->removeFrom(file_hash, tagbase_db);
			}
		}




		//////////////////////////////////////////////////////////
		//// --tag

		if (arg_tag.has_value())
		{
			if (tagbase_db == nullptr)
			{
				exitWithError("To use --tag, a tagbase must be selected");
			}

			std::shared_ptr<TagQuery> tagQuery = parseTagQuery(*arg_tag);
			
			std::map<std::array<char, 32>, bool> fileHashes;
			tagQuery->findIn(ZERO_HASH, fileHashes, tagbase_db);
			
			if (arg_json)
			{
				auto filesArray = std::make_shared<JsonValue_Array>();

				for (auto const& [fileHash, _] : fileHashes)
				{
					auto file = std::make_shared<JsonValue_Map>();
					file->set("hash", bytes_to_hex(fileHash));

					std::shared_ptr<Tag> tags = findTagsOfFile(fileHash, tagbase_db);
					auto tagsArray = std::make_shared<JsonValue_Array>();
					for (auto tag : tags->subtags)
					{
						tagsArray->array.push_back(tag->toJSON());
					}
					file->set("tags", tagsArray);
					filesArray->array.push_back(file);
				}

				jsonOutput.set("files", filesArray);
			}
			else
			{
				std::cout << "Found " << fileHashes.size() << " files:\r\n";

				for (auto const& [fileHash, _] : fileHashes)
				{
					std::shared_ptr<Tag> tags = findTagsOfFile(fileHash, tagbase_db);
					std::cout << tags->toString() << "\r\n";
				}
			}
		}




		if (tagbase_db != nullptr)
		{
			sqlite3_close(tagbase_db);
		}

		if (arg_json)
		{
			std::stringstream str;
			jsonOutput.write(str);
			std::cout << str.rdbuf();
		}

		return 0;
	}
	catch (const char* errorMessage)
	{
		exitWithError("Fatal error: " + std::string(errorMessage));
	}
	catch (std::string errorMessage)
	{
		exitWithError("Fatal error: " + errorMessage);
	}
	return 1;
}
