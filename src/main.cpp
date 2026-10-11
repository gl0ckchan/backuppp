#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include "thread_pool.h"

#include <xxhash.h>

#define NUMBER_OF_THREADS 6

namespace fs = std::filesystem;

std::string hash_file(const std::string& file_path); 

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cout << "Usage: " << argv[0] << " <in> <out>" << std::endl;

		return 69;
	}

	fs::path src_dir(argv[1]);
    fs::path dst_dir(argv[2]);
    fs::create_directory(dst_dir);

	std::string hashes_path = src_dir.string() + "/hashes.txt";

	if (!fs::exists(hashes_path)) {
		std::ofstream create_file(hashes_path);
	    create_file.close();
	}

	std::unordered_map<std::string, std::string> hashes_map;

	// загрузка хэшей в мапу
	{
	    std::ifstream hashes_in(hashes_path);
		if (!hashes_in) {
			std::cerr << "Cannot open hashes.txt for reading\n";

		    return 69;
		}

		std::string line;

		while (std::getline(hashes_in, line)) {
			const size_t pos = line.find(":");
			if (pos == std::string::npos) continue;

			hashes_map[line.substr(0, pos)] = line.substr(pos + 1);
		}

		hashes_in.close();
	}

	thread_pool pool;
	if (!init_thread_pool(pool, NUMBER_OF_THREADS, src_dir, dst_dir, hashes_map)) {
		std::cerr << "failed to init thread pool\n";

		return 69;
	};

	for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator(src_dir)) {
		thread_pool_add_task(pool, dir_entry);
	}

	destroy_thread_pool(pool);

	std::cout << "copied files: " << pool.count << std::endl;

	return 0;
}
