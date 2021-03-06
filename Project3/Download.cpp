#ifndef DOWNLOADERIMPLENTATION
#define DOWNLOADERIMPLENTATION

#include <cstring>
#include <algorithm>
#include <utility>
#include <tuple>
#include <functional>
#include <regex>
#include <fstream>
#include <filesystem>// exists(), path
#include <direct.h> //_mkdir()
#include <curl\curl.h>
#include </Json/json.hpp> // json, parse()
#include "Download.h"
#include "Functions.h"

/*******     DOWNLOADER CLASS AND FUNCTIONS *******/
using std::cout; using std::endl;
Downloader::Downloader(const Options& player_options) {
	options = player_options;
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
}

Downloader::~Downloader() {
	curl_easy_cleanup(curl);
	curl_global_cleanup();
}


void Downloader::initCurlSetup(const string & url, const string& buffer) noexcept {

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeJsonData);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);

	curl_slist* header_list = nullptr;
	header_list = curl_slist_append(header_list, "User-Agent: 0"); //4chan will otherwise not allow access.
	header_list = curl_slist_append(header_list, options.imgur_auth.c_str());

	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

}

inline void Downloader::createFolderPath(string& url) const {
	int remove_chars = 0;
	for (auto cbegin = url.crbegin(); cbegin != url.crend(); cbegin++) {
		remove_chars++;
		if (*cbegin == '\\')
			break;
	}
	url.erase(url.end() - remove_chars, url.end());
	options.folder_name.empty() ? url.append("\\Your Pics") : url.append("\\" + options.folder_name);
}


string Downloader::createDirectory(string path) const {
	string end_path(path.begin() + (path.end() - path.begin() - 4), path.end());
	if (end_path == ".exe")
		createFolderPath(path);
	std::experimental::filesystem::path p(path);
	if (!std::experimental::filesystem::exists(p))
		assert(_mkdir(path.c_str()) == 0);
	return path;
}

/*
* We only support pure reddit post,Imgur, 4chan, tumblr, gfycat posts.
* This function takes the url and checks if it is suitable for usage.
* We do not support whole reddit posts, whole chan posts, or whole tumblr posts.
* We do support imgur gallerys, implementation in another function.
*/
bool Downloader::removeNonSupported(const string& url) {
	try {
		string reddit_pattern("i.redd.it/.+\\."), imgur_pattern("(i.)?imgur.com/((.+\\.)|(gallery/.+)|(a/.+)|.+)"), chan_pattern("i.4cdn.org/.+/\\d+\\."),
			tumblr_pattern("\\d+.media.tumblr.com/.+/tumblr_.+\\."), gfycat_pattern("gfycat.com/(.+/)?");
		std::regex reddit_regex(reddit_pattern), imgur_regex(imgur_pattern), chan_regex(chan_pattern), tumblr_regex(tumblr_pattern), gfycat_regex(gfycat_pattern);
		std::smatch sm;
		bool pat = std::regex_search(url, sm, gfycat_regex);
		std::smatch gfycat_matched = sm;
		return !(std::regex_search(url, sm, reddit_regex) || std::regex_search(url, sm, imgur_regex) ||
			std::regex_search(url, sm, chan_regex) || std::regex_search(url, sm, tumblr_regex) || (pat && !gfycat_matched[1].matched));
	}
	catch (std::regex_error err) {
		cout << err.what() << "\n" << err.code();
	}
	throw std::runtime_error("Function did not pass through try catch statement.");
	return false;
}


vector < std::pair<string, string>> Downloader::mapUrls(vector<string>& unmaped_urls) {
	vector<std::pair<string, string>> mapped_urls; //first string is the url, the second is the identification
	string imgur_pattern("(i\\.)?imgur.com/(gallery/)?(a/)?(.+\\.)?"), gfycat_pattern("gfycat.com/(.+/)?"); //matches the wrong, not the right.
	for (const string& unmapped_url : unmaped_urls) {
		std::regex img_reg(imgur_pattern), gf_reg(gfycat_pattern);
		std::smatch img_sm, gf_sm;
		if (std::regex_search(unmapped_url, img_sm, img_reg) && !img_sm[1].matched) {
			if (img_sm[2].matched || img_sm[3].matched)
				mapped_urls.emplace_back(std::make_pair(unmapped_url, "imgurGALLERY"));
			else if (img_sm[4].matched)
				mapped_urls.emplace_back(std::make_pair(unmapped_url, "imgurPOST"));
		}
		else if (std::regex_search(unmapped_url, gf_sm, gf_reg)) {
			mapped_urls.emplace_back(std::make_pair(unmapped_url, "gfycat"));
		}
		else {
			mapped_urls.emplace_back(std::make_pair(unmapped_url, "GOOD"));
		}

	}

	return mapped_urls;
}


vector<string> Downloader::getPureImgUrl(vector<std::pair<string, string>>& mapped) {
	vector<string> newUrls;
	for (auto& mapped_url : mapped) {
		if (mapped_url.second != "GOOD") {
			string json_data;
			if (mapped_url.second == "imgurGALLERY" && options.all_gallery) {
				auto back_slash = std::find(mapped_url.first.rbegin(), mapped_url.first.rend(), '/');
				string query_string(mapped_url.first.rbegin(), back_slash);
				std::reverse(query_string.begin(), query_string.end());
				string request = "https://api.imgur.com/3/album/" + query_string;

				initCurlSetup(request, json_data);
				auto response = curl_easy_perform(curl);
				curl_easy_reset(curl);
				if (response == CURLE_OK) {
					json json_parsed = json::parse(json_data.c_str());
					if (json_parsed["success"]) {
						auto begin = json_parsed["data"]["images"].begin();
						auto end = json_parsed["data"]["images"].end();
						while (begin != end) {
							if (begin->find("mp4") != begin->end())
								newUrls.push_back((*begin)["mp4"]);
							else
								newUrls.push_back((*begin)["link"]);
							begin++;
						}
					}

				}
				else {
					cout << error << endl;
				}
				mapped_url = std::make_pair("DELETE", "");

			}
			else if (mapped_url.second == "imgurPOST") {
				auto back_slash = std::find(mapped_url.first.rbegin(), mapped_url.first.rend(), '/');
				string query_string(mapped_url.first.rbegin(), back_slash);
				std::reverse(query_string.begin(), query_string.end());// Find everything from the end of the query string
				auto dot = std::find(query_string.begin(), query_string.end(), '.');
				if (dot != query_string.end()) {
					query_string = string(query_string.begin(), dot);
				}
				string request = "https://api.imgur.com/3/image/" + query_string;

				initCurlSetup(request, json_data);
				auto response = curl_easy_perform(curl);
				curl_easy_reset(curl);
				if (response == CURLE_OK) {
					json json_parsed = json::parse(json_data.c_str());
					if (json_parsed["success"]) {
						if (json_parsed["data"].find("mp4") != json_parsed["data"].end())
							newUrls.push_back(json_parsed["data"]["mp4"]);
						else
							newUrls.push_back(json_parsed["data"]["link"]);
					}
					else {
						cout << json_parsed["data"]["error"] << endl;
						cout << "url : " << request << endl;
					}
				}
				else {
					cout << error << endl;
				}

				mapped_url = std::make_pair("DELETE", "");
			}
			else if (mapped_url.second == "gfycat") {
				auto back_slash = std::find(mapped_url.first.rbegin(), mapped_url.first.rend(), '/');
				string query(mapped_url.first.rbegin(), back_slash);
				std::reverse(query.begin(), query.end());
				string request("https://api.gfycat.com/v1test/gfycats/" + query);

				initCurlSetup(request, json_data);
				auto response = curl_easy_perform(curl);
				curl_easy_reset(curl);
				if (response == CURLE_OK) {
					json parsed_json = json::parse(json_data.c_str());
					if (parsed_json.find("errorMessage") == parsed_json.end()) {
						auto& gfy_info = parsed_json["gfyItem"];
						if (gfy_info.find("mp4Url") != gfy_info.end()) {
							newUrls.push_back(gfy_info["mp4Url"]);
						}
						else if (gfy_info.find("gifUrl") != gfy_info.end()) {
							newUrls.push_back(gfy_info["gifUrl"]);
						}
					}
					else {
						cout << parsed_json["errorMessage"] << endl;
					}
				}// !if
				else {
					cout << error << endl;
				}

			} // !else if
			mapped_url = std::make_pair("", "DELETE");
		}// !if GOOD

	} // !for loop
	return newUrls;
}// !getPureImgUrl

void Downloader::changeImgToMp4(string& imgur_url) {
	auto dot = std::find(imgur_url.rbegin(), imgur_url.rend(), '.');
	if (dot != imgur_url.rend() && (dot - imgur_url.rbegin()) < 5) {
		string extension(dot.base(), imgur_url.end());
		if (extension == "gif" || extension == "gifv") {
			imgur_url.erase(dot.base(), imgur_url.end());
			imgur_url.append("mp4");
		}
	}
}

void Downloader::getFileNames(const vector<string>& single_url, vector<std::pair<string, string>>& output) const noexcept {
	for (auto begin = urls.begin(); begin != urls.end(); begin++) {
		auto back_slash = std::find(begin->rbegin(), begin->rend(), '/');
		string name(begin->rbegin(), back_slash);
		std::reverse(name.begin(), name.end());
		output.push_back(std::make_pair(std::move(*begin), name));
	}
	for (auto& pair : output) { //remove tumblr if its there.
		if (string(pair.second.begin(), pair.second.begin() + 6) == "tumblr") {
			auto iter = std::find(pair.second.begin(), pair.second.end(), '_') + 1;
			pair.second = string(iter, pair.second.end());
		}
	}
}

void Downloader::download() {
	vector<std::pair<string, string>> extensions;//first string is url, second is name and extension.
	extensions.reserve(urls.size());
	vector<string> delete_urls;
	getFileNames(urls, extensions);

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &downloadFile);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error);
	for (auto& download_urls : extensions) {
		string& img_name("\\" + download_urls.second);
		std::experimental::filesystem::path image_path(options.current_path + img_name);
		bool img_exists = std::experimental::filesystem::exists(image_path);

		if (!(options.duplicate_file == File::SKIP && img_exists)) {
			string full_path;
			if (options.duplicate_file == File::CREATENEW && img_exists) { 
				// adds a number to indicate the nth copy; sdp.jpg(2).
				int new_duplicate = 2;
				auto dot = std::find(download_urls.second.cbegin(), download_urls.second.cend(), '.');
				string ext(dot, download_urls.second.cend());
				string name(download_urls.second.cbegin(), dot);
				full_path = options.current_path + "\\" + name + " (" + std::to_string(new_duplicate) + ")" + ext;
				std::experimental::filesystem::path duplicate_image_path(full_path);
				bool duplicate_img_exists = std::experimental::filesystem::exists(duplicate_image_path);
				while (!duplicate_img_exists) { // makes sure the largest number is used that is not already in the folder.
					new_duplicate++;
					full_path = options.current_path + "\\" + name + " (" + std::to_string(new_duplicate) + ")" + ext;
					duplicate_image_path = std::experimental::filesystem::path(full_path);
					duplicate_img_exists = std::experimental::filesystem::exists(duplicate_image_path);
				}
			}
			else {
				full_path = options.current_path + img_name;
			}
			//Uses the paths to download the images.
			std::ofstream ofs(full_path, std::ios::binary); 
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ofs);
			curl_easy_setopt(curl, CURLOPT_URL, download_urls.first.c_str());
			auto response = curl_easy_perform(curl);

			if (!(response == CURLE_OK || ofs)) {
				delete_urls.push_back(full_path);
				cout << error << endl;
				cout << "Problem downloading file : " << download_urls.second << "\n" <<
					"url : " << download_urls.first << endl;
			}
			else {
				if (getFileSize(full_path) == 0) { // checks to see if the downloading failed; 0 bytes in file.
					delete_urls.push_back(full_path);
				}
			}
		} // !if
		else {
			cout << download_urls.second << " already exists" << endl;
		}
	} // !for
	curl_easy_reset(curl);
	for (auto& d : delete_urls) { //delete dead img/gifs with 0 bytes
		if (remove(d.c_str()) != 0)
			perror("Error occured while deleting file");

	}
}


#endif DOWNLOADERIMPLENTATION