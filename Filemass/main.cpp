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

#include "sqlite3.h"
#include "util.h"
#include "sha256.h"
#include "repository.h"
#include "tag.h"
#include "tag_parser.h"

/*
Tag conventions:

photo of 2 people (1 known, 1 unknown wearing an AC/DC t-shirt) standing in front of a castle:
photo,person[name[Jesse Busman]],person[clothing[shirt[AC/DC]]],castle
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

			if (field == "add-files")
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
					std::cout << "\r\n--init-repo takes no arguments\r\n";
					exit(1);
					return 1;
				}
				arg_init_repo = true;
			}
			else if (field == "init-tagbase")
			{
				if (value.length() != 0)
				{
					std::cout << "\r\n--init-tagbase takes no arguments\r\n";
					exit(1);
					return 1;
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
				std::cout << "\r\nUnknown command line argument " << field << "\r\n";
				exit(1);
				return 1;
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
				std::cerr << "\r\nCannot init repository at " << selected_repository_path << " because it's not a directory\r\n";
				exit(1);
				return 1;
			}
			if (!std::filesystem::is_empty(selected_repository_path))
			{
				std::cerr << "\r\nCannot init repository at " << selected_repository_path << " because that directory is not empty\r\n";
				exit(1);
				return 1;
			}

			std::string config_path = selected_repository_path + "/fmrepo.conf";

			if (std::filesystem::exists(config_path))
			{
				std::cerr << "\r\nCannot init repository at " << selected_repository_path << " because config file already exists at " << config_path << "\r\n";
				exit(1);
				return 1;
			}

			std::ofstream outfile(config_path, std::ios::out);
			outfile.write("", 0);
			outfile.flush();
			outfile.close();

			std::cout << "Initialized repository at " << selected_repository_path << "\r\n";
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
					std::cerr << "\r\nSpecified repo path is not a directory: " << selected_repository_path << "\r\n";
					exit(1);
					return 1;
				}
			}
			else
			{
				std::cerr << "\r\nSpecified repo does not exist: " << selected_repository_path << "\r\n";
				exit(1);
				return 1;
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
				std::cerr << "\r\nCannot init tagbase at " << selected_tagbase_path << " because that file already exists\r\n";
				exit(1);
				return 1;
			}

			int sqlite_returncode = sqlite3_open(selected_tagbase_path.c_str(), &tagbase_db);

			if (sqlite_returncode != SQLITE_OK)
			{
				std::cerr << "\r\nCannot init tagbase at " << selected_tagbase_path << " because of sqlite error " << sqlite_returncode << " :\r\n";
				std::cerr << sqlite3_errmsg(tagbase_db) << "\r\n";
				exit(1);
				return 1;
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
			
			

			if (selected_repository == nullptr)
			{
				sqlite3_close(tagbase_db);
				exit(0);
				return 0;
			}
		}




		//////////////////////////////////////////////////
		//// --tagbase

		if (tagbase_db == nullptr && (arg_tag.has_value() || arg_remove_tags.has_value() || arg_add_tags.has_value()))
		{
			if (!std::filesystem::exists(selected_tagbase_path))
			{
				std::cerr << "\r\nCannot open tagbase at " << selected_tagbase_path << " because that file does not exist\r\n";
				exit(1);
				return 1;
			}
			if (!std::filesystem::is_regular_file(selected_tagbase_path))
			{
				std::cerr << "\r\nCannot open tagbase at " << selected_tagbase_path << " because that file is not a regular file\r\n";
				exit(1);
				return 1;
			}

			int sqlite_returncode = sqlite3_open(selected_tagbase_path.c_str(), &tagbase_db);

			if (sqlite_returncode != SQLITE_OK)
			{
				std::cerr << "\r\nCannot open tagbase at " << selected_tagbase_path << " because of sqlite error " << sqlite_returncode << " :\r\n";
				std::cerr << sqlite3_errmsg(tagbase_db) << "\r\n";
				exit(1);
				return 1;
			}
		}






		/////////////////////////////////////////////////////
		//// --add-file

		std::vector<std::array<char, 32>> selected_file_hashes;
	
		if (arg_add_files.has_value())
		{
			if (arg_files.has_value())
			{
				std::cerr << "\r\nCannot use both --add-files and --files\r\n";
				exit(1);
				return 1;
			}


			if (!std::filesystem::exists(*arg_add_files))
			{
				std::cerr << "\r\nThe file path you're trying to add does not exist: " << *arg_add_files << "\r\n";
				exit(1);
				return 1;
			}

			if (std::filesystem::is_directory(*arg_add_files))
			{
			}
			else if (!std::filesystem::is_regular_file(*arg_add_files))
			{
				std::cerr << "\r\nThe file you're trying to add is not a regular file: " << *arg_add_file << "\r\n";
				exit(1);
				return 1;
			}
		
			selected_repository->add(*arg_add_file, selected_file_hash.data());

			char hashHex[65];

			bytes_to_hex(selected_file_hash, hashHex);

			hashHex[64] = 0x00;

			std::cout << hashHex << "\r\n";

			file_selected = true;
		}



		if (arg_file.has_value())
		{
			if (file_selected)
			{
				std::cerr << "\r\nCould not select file using --file; a file was already selected\r\n";
				exit(1);
				return 1;
			}

			int amountBytes = hex_to_bytes(arg_file->c_str(), selected_file_hash);
		
			if (amountBytes != 32)
			{
				std::cerr << "\r\nValue of --file must be 32 hexadecimal bytes\r\n";
				exit(1);
				return 1;
			}

			file_selected = true;
		}






		//////////////////////////////////////////////////////////
		//// --add-tag

		if (arg_add_tag.has_value())
		{
			if (!file_selected)
			{
				std::cerr << "\r\nTo use --add-tag, a file must be selected (using --add-file or --file)\r\n";
				exit(1);
				return 1;
			}

			if (tagbase_db == nullptr)
			{
				std::cerr << "\r\nTo use --add-tag, a tagbase must be selected\r\n";
				exit(1);
				return 1;
			}

			std::vector<std::shared_ptr<Tag>> tags = parseTag(*arg_add_tag);

			q(tagbase_db, "BEGIN TRANSACTION");

			for (auto tag : tags)
			{
				tag->addTo(selected_file_hash, ZERO_HASH, selected_file_hash, tagbase_db, true);
			}

			q(tagbase_db, "COMMIT");
		}





		//////////////////////////////////////////////////////////
		//// --remove-tag

		if (arg_remove_tag.has_value())
		{
			if (!file_selected)
			{
				std::cerr << "\r\nTo use --remove-tag, a file must be selected (using --file)\r\n";
				exit(1);
				return 1;
			}

			if (tagbase_db == nullptr)
			{
				std::cerr << "\r\nTo use --remove-tag, a tagbase must be selected\r\n";
				exit(1);
				return 1;
			}

			std::vector<std::shared_ptr<Tag>> tags = parseTag(*arg_add_tag);

			for (auto tag : tags)
			{
				tag->removeFrom(selected_file_hash, tagbase_db);
			}
		}




		//////////////////////////////////////////////////////////
		//// --tag

		if (arg_tag.has_value())
		{
			if (tagbase_db == nullptr)
			{
				std::cerr << "\r\nTo use --tag, a tagbase must be selected\r\n";
				exit(1);
				return 1;
			}

			std::shared_ptr<TagQuery> tagQuery = parseTagQuery(*arg_tag);
			
			std::map<std::array<char, 32>, bool> fileHashes;
			tagQuery->findIn(ZERO_HASH, fileHashes, tagbase_db);

			std::cout << "Found " << fileHashes.size() << " files:\r\n";

			for (auto const& [fileHash, _] : fileHashes)
			{
				std::shared_ptr<Tag> tags = findTagsOfFile(fileHash, tagbase_db);
				tags->debugPrint();
				std::cout << "\r\n";
			}
		}




		if (tagbase_db != nullptr)
		{
			sqlite3_close(tagbase_db);
		}
		return 0;
	}
	catch (const char* errorMessage)
	{
		std::cerr << "\r\nFatal error: " << errorMessage << "\r\n";
		return 1;
	}
	catch (std::string errorMessage)
	{
		std::cerr << "\r\nFatal error: " << errorMessage << "\r\n";
		return 1;
	}
}
