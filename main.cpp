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
#include <magic.h>

#include "sqlite3.h"
#include "util.h"
#include "sha256.h"
#include "repository.h"
#include "tag.h"
#include "tag_query.h"
#include "tag_parser.h"
#include "tag_query_parser.h"
#include "path_pattern.h"
#include "json.h"
#include "uuid.h"

bool DEBUGGING = false;
bool arg_json = false;
JsonValue_Map jsonOutput;

int main(int argc, char* argv[])
{
	for (int i=0; i<32; i++) ZERO_HASH[i] = 0x00;
	
	try
	{
		if (argc == 1)
		{
			std::cout
				<< "Repo:\r\n"
				<< "--repo=[dir]         Select repo located at directory [dir]\r\n"
				<< "--init-repo          Initialize the selected repository\r\n"
				<< "\r\nTagbase:\r\n"
				<< "--tagbase=[file]     Select tagbase located in file [file]\r\n"
				<< "--init-tagbase       Initialize the selected tagbase\r\n"
				<< "\r\nFiles:\r\n"
				<< "--files=[hashlist]   Select the files with hash in [hashlist]\r\n"
				<< "--add-files=[file]   Add files matching [file] to selected repo, and select them\r\n"
				<< "--errcheck           Run error checks on the selected files\r\n"
				<< "\r\nTags:\r\n"
				<< "--tag=[tagquery]        Find files that match the given [tagquery]\r\n"
				<< "--add-tags=[taglist]    Add [tags] to the selected files\r\n"
				<< "--add-fs-tags           Add #original_path tags to the selected files\r\n"
				<< "--remove-tags=[taglist] Remove [tags] from the selected files\r\n"
				<< "\r\nOutput format:\r\n"
				<< "--json               Format output as JSON\r\n"
				<< "\r\nExamples of [taglist] syntax:\r\n"
				<< "--add-tag=football,match,sport,team[Los Angeles],team[Chicago]\r\n"
				<< "--untag=team[name=Chicago]\r\n"
				<< "\r\nExamples of [tagquery] syntax:\r\n"
				<< "--tag=\"football & team[Los Angeles | Chicago]\"\r\n";
			exit(0);
			return 0;
		}
		
		std::optional<std::string> arg_repo;
		std::optional<std::string> arg_tagbase;
		std::optional<std::string> arg_files;
		std::optional<std::string> arg_add_files;
		std::optional<std::string> arg_tags;
		std::optional<std::string> arg_add_tags;
		std::optional<std::string> arg_remove_tags;
		
		arg_json = false;
		DEBUGGING = false;
		bool arg_init_repo = false;
		bool arg_init_tagbase = false;
		bool arg_add_fs_tags = false;
		bool arg_errcheck = false;
		bool arg_errfix = false;
		
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
			else if (field == "errcheck")
			{
				arg_errcheck = true;
			}
			else if (field == "errfix")
			{
				arg_errfix = true;
			}
			else if (field == "debug")
			{
				DEBUGGING = true;
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
				arg_tags = value;
			}
			else if (field == "add-tags")
			{
				arg_add_tags = value;
			}
			else if (field == "add-fs-tags")
			{
				arg_add_fs_tags = true;
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
			std::string initialConfigFile = "uuid="+generate_uuid_v4()+"\r\n";
			outfile.write(initialConfigFile.c_str(), initialConfigFile.length());
			outfile.flush();
			outfile.close();
			
			if (arg_json)
			{
				jsonOutput.set("initialized_repository", selected_repository_path);
			}
			else
			{
				std::cout << "[--init-repo] Initialized repository at " << selected_repository_path << "\r\n";
			}
		}
		
		
		
		//////////////////////////////////////
		//// --repo
	
		std::shared_ptr<Repository> selected_repository = nullptr;
		
		if (std::filesystem::exists(selected_repository_path))
		{
			if (std::filesystem::is_directory(selected_repository_path))
			{
				selected_repository = std::make_shared<Repository>(selected_repository_path);
			}
			else if (arg_repo.has_value())
			{
				exitWithError("Specified repo path is not a directory: " + selected_repository_path);
			}
		}
		else if (arg_repo.has_value())
		{
			exitWithError("Specified repo does not exist: " + selected_repository_path);
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
		}
		
		
		
		
		//////////////////////////////////////////////////
		//// --tagbase
		
		if (tagbase_db == nullptr && arg_tagbase.has_value()) // && (arg_tags.has_value() || arg_remove_tags.has_value() || arg_add_tags.has_value()))
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
		std::vector<std::string> selected_file_paths;
	
		if (arg_add_files.has_value())
		{
			std::shared_ptr<PathPattern> pathPattern = parsePathPattern(*arg_add_files);
			
			if (DEBUGGING)
			{
				if (arg_json) jsonOutput.set("_debug_pathPattern", pathPattern->toString());
				else std::cout << "[--add-files] pathPattern = " << pathPattern->toString() << "\r\n";
			}
			
			long long amountOFilesAdded = 0;
			long long amountOfNewFilesAdded = 0;
			
			pathPattern->findFiles(".", [&selected_repository, &selected_file_hashes, &selected_file_paths, &amountOFilesAdded, &amountOfNewFilesAdded](const std::string& path){
				if (!std::filesystem::is_regular_file(path))
				{
					exitWithError("The file you're trying to add is not a regular file: " + path);
				}
				auto [hash, wasNewlyAdded] = selected_repository->add(path);
				selected_file_hashes.push_back(hash);
				selected_file_paths.push_back(path);
				amountOFilesAdded++;
				if (wasNewlyAdded) amountOfNewFilesAdded++;
			});
			
			if (arg_json)
			{
				jsonOutput.set("filesAdded", amountOFilesAdded);
				jsonOutput.set("newFilesAdded", amountOfNewFilesAdded);
			}
			else
			{
				std::cout << "Added " << amountOFilesAdded << " files to the repository, " << amountOfNewFilesAdded << " of which were new.\r\n"; 
			}
		}
		
		
		
		
		
		/////////////////////////////////////////////////////
		//// --files
		
		if (arg_files.has_value())
		{
			std::array<char, 32> hash;
			for (unsigned int i=0; i<arg_files->length(); i++)
			{
				char c = (*arg_files)[i];
				if (c == ',' || c == ' ' || c == ';') { }
				else if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))
				{
					int amountBytes = hex_to_bytes(&arg_files->c_str()[i], hash);
					if (amountBytes != 32)
					{
						exitWithError("Arguments to --files must be 32 bytes hex values.");
					}
					i += 64;
					selected_file_hashes.push_back(hash);
					selected_file_paths.push_back("");
				}
				else exitWithError("Unexpected character in --files argument");
			}
			
			auto filesArray = std::make_shared<JsonValue_Array>();
			
			for (auto const& fileHash : selected_file_hashes)
			{
				auto file = std::make_shared<JsonValue_Map>();
				file->set("hash", bytes_to_hex(fileHash));
				
				if (tagbase_db != nullptr)
				{
					std::shared_ptr<Tag> tags = findTagsOfFile(fileHash, tagbase_db);
					auto tagsArray = std::make_shared<JsonValue_Array>();
					for (auto tag : tags->subtags)
					{
						tagsArray->array.push_back(tag->toJSON());
					}
					file->set("tags", tagsArray);
				}
				
				filesArray->array.push_back(file);
			}
			
			jsonOutput.set("files", filesArray);
		}
		
		
		
		
		
		/////////////////////////////////////////////////////
		//// --errcheck
		
		if (arg_errcheck)
		{
			if (selected_repository == nullptr)
			{
				exitWithError("A repository must be selected to use --errcheck");
			}
			
			std::shared_ptr<JsonValue_Array> filesNoError = std::make_shared<JsonValue_Array>();
			std::shared_ptr<JsonValue_Array> filesNotFound = std::make_shared<JsonValue_Array>();
			std::shared_ptr<JsonValue_Array> filesError = std::make_shared<JsonValue_Array>();
			
			unsigned long amountNoError = 0;
			unsigned long amountNotFound = 0;
			unsigned long amountError = 0;
			
			for (auto hash : selected_file_hashes)
			{
				std::string hashStr = bytes_to_hex(hash);
				
				ErrorCheckResult ecr = selected_repository->errorCheck(hash);
				
				if (arg_json)
				{
					if (ecr == ECR_ALL_OK) filesNoError->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr)));
					else if (ecr == ECR_FILE_NOT_FOUND) filesNotFound->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr)));
					else if (ecr == ECR_ERROR) filesError->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr)));
					else exitWithError("Unknown ECR code");
				}
				else
				{
					if (ecr == ECR_ERROR) { printf("[--errcheck] File %s has an error!\r\n", hashStr.c_str()); amountError++; }
					else if (ecr == ECR_FILE_NOT_FOUND) { printf("[--errcheck] File %s was not found!\r\n", hashStr.c_str()); amountNotFound++; }
					else if (ecr == ECR_ALL_OK) { amountNoError++; }
					else exitWithError("Unknown ECR code");
				}
			}
			
			if (arg_json)
			{
				jsonOutput.set("files_no_error", filesNoError);
				jsonOutput.set("files_not_found", filesNotFound);
				jsonOutput.set("files_error", filesError);
			}
			else
			{
				printf("[--errcheck] %lu files checked for errors: %lu ok, %lu with error, %lu not found\r\n", amountNoError + amountError + amountNotFound, amountNoError, amountError, amountNotFound);
			}
		}
		
		
		
		
		
		/////////////////////////////////////////////////////
		//// --errfix
		
		if (arg_errfix)
		{
			if (selected_repository == nullptr)
			{
				exitWithError("A repository must be selected to use --errcheck");
			}
			
			std::shared_ptr<JsonValue_Array> filesFailedToFix = std::make_shared<JsonValue_Array>();
			std::shared_ptr<JsonValue_Array> filesFixed = std::make_shared<JsonValue_Array>();
			std::shared_ptr<JsonValue_Array> filesWereNotBroken = std::make_shared<JsonValue_Array>();
			std::shared_ptr<JsonValue_Array> filesNotFound = std::make_shared<JsonValue_Array>();
			
			unsigned long amountFailedToFix = 0;
			unsigned long amountFixed = 0;
			unsigned long amountNotBroken = 0;
			unsigned long amountNotFound = 0;
			
			for (auto hash : selected_file_hashes)
			{
				std::string hashStr = bytes_to_hex(hash);
				
				ErrorFixResult efr = selected_repository->errorFix(hash);
				
				if (arg_json)
				{
					if (efr == EFR_FIXED) { filesFixed->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr))); amountFixed++; }
					else if (efr == EFR_FAILED_TO_FIX) { filesFailedToFix->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr))); amountFailedToFix++; }
					else if (efr == EFR_FILE_NOT_FOUND) { filesNotFound->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr))); amountNotFound++; }
					else if (efr == EFR_WAS_NOT_BROKEN) { filesWereNotBroken->array.push_back(std::shared_ptr<JsonValue>(new JsonValue_String(hashStr))); amountNotBroken++; }
					else exitWithError("Unknown ECR code");
				}
				else
				{
					if (efr == EFR_FIXED) { printf("[--errfix] :) File %s has been fixed!\r\n", hashStr.c_str()); amountFixed++; }
					else if (efr == EFR_FAILED_TO_FIX) { printf("[--errfix] :( File %s could not be fixed!\r\n", hashStr.c_str()); amountFailedToFix++; }
					else if (efr == EFR_WAS_NOT_BROKEN) { printf("[--errfix] :) File %s was not broken!\r\n", hashStr.c_str()); amountNotBroken++; }
					else if (efr == EFR_FILE_NOT_FOUND) { printf("[--errfix] :| File %s was not found!\r\n", hashStr.c_str()); amountNotFound++; }
					else exitWithError("Unknown ECR code");
				}
			}
			
			if (arg_json)
			{
				jsonOutput.set("files_fixed", filesFixed);
				jsonOutput.set("files_failed_to_fix", filesFailedToFix);
				jsonOutput.set("files_not_found", filesNotFound);
				jsonOutput.set("files_were_not_broken", filesWereNotBroken);
			}
			else
			{
				printf("[--errfix] Tried to fix %lu files:\n", amountFixed+amountFailedToFix+amountNotFound+amountNotBroken);
				printf("           - %lu fixed\n", amountFixed);
				printf("           - %lu failed to fix\n", amountFailedToFix);
				printf("           - %lu not found\n", amountNotFound);
				printf("           - %lu were not broken\n", amountNotBroken);
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
				exitWithError("To use --add-tags, a tagbase must be selected");
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
		//// --add-fs-tags
		
		if (arg_add_fs_tags)
		{
			if (selected_file_hashes.size() == 0)
			{
				exitWithError("To use --add-fs-tags, at least one file must be selected (using --add-files)");
			}
			
			if (tagbase_db == nullptr)
			{
				exitWithError("To use --add-fs-tags, a tagbase must be selected");
			}
			
			if (selected_file_hashes.size() != selected_file_paths.size())
			{
				// wtf
				printf("WTF");
				exit(1);
				// wtf
				return 1;
			}
			
			const char *magic_full;
			magic_t magic_cookie;
			
			magic_cookie = magic_open(MAGIC_MIME);
			
			if (magic_cookie == NULL)
			{
				printf("Unable to initialize magic library\n");
				exit(1);
				return 1;
			}
			
			if (magic_load(magic_cookie, NULL) != 0)
			{
				printf("Cannot load magic database: %s\n", magic_error(magic_cookie));
				magic_close(magic_cookie);
				exit(1);
				return 1;
			}
			
			q(tagbase_db, "BEGIN TRANSACTION");
			
			for (unsigned int i=0; i<selected_file_paths.size(); i++)
			{
				// #original_path
				
				const auto& file_path = selected_file_paths[i];
				if (file_path.length() == 0) continue;
				const auto& file_hash = selected_file_hashes[i];
				
				std::string abs_file_path = std::filesystem::canonical(file_path);
				std::vector<std::string> tag_chain{"#original_path"};
				unsigned int prevStart = 0;
				for (unsigned int j=0; j<=abs_file_path.size(); j++)
				{
					char c = abs_file_path.c_str()[j];
					if (c == '/' || c == 0x00)
					{
						if (prevStart != j)
						{
							tag_chain.push_back(abs_file_path.substr(prevStart, j-prevStart));
						}
						prevStart = j + 1;
					}
				}
				
				Tag(tag_chain).addTo(file_hash, ZERO_HASH, file_hash, tagbase_db, true);
				
				
				
				
				
				// #mime_content_type
				
				magic_full = magic_file(magic_cookie, file_path.c_str());
				
				if (magic_full != nullptr)
				{
					Tag({"#mime_content_type", std::string(magic_full)}).addTo(file_hash, ZERO_HASH, file_hash, tagbase_db, true);
				}
			}
			
			magic_close(magic_cookie);
			
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
		//// --tags
		
		if (arg_tags.has_value())
		{
			if (tagbase_db == nullptr)
			{
				exitWithError("To use --tag, a tagbase must be selected");
			}
			
			std::shared_ptr<TagQuery> tagQuery = parseTagQuery(*arg_tags);
			
			if (DEBUGGING) std::cout << "[--tags] Tag query: " << tagQuery->toString() << "\r\n";
			
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
			jsonOutput.write(std::cout);
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
