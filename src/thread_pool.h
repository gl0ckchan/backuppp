#pragma once

#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <condition_variable>

#include <xxhash.h>

namespace fs = std::filesystem;

std::string hash_file(const std::string &file_path);

typedef struct {
	std::vector<std::thread> _threads;
	std::queue<fs::directory_entry> _tasks;

	std::unordered_map<std::string, std::string> hashes_map;
	std::ofstream hashes_out;
	fs::path _src_dir;
	fs::path _dst_dir;

	std::mutex _mu;
	std::condition_variable _condition;

	bool _stop = false;

	size_t count = 0;

	void worker();
} thread_pool;

bool init_thread_pool(thread_pool &pool, int number_of_threads, fs::path &src_dir, fs::path &dst_dir, std::unordered_map<std::string, std::string> &map) {
	pool._src_dir = src_dir;
    pool._dst_dir = dst_dir;

    pool.hashes_out.open(src_dir.string() + "/hashes.txt", std::ios::app);
    if (!pool.hashes_out) {
		std::cerr << "Cannot open hashes.txt for writing\n";

	    return false;
	}

    pool.hashes_map = map;

	for (size_t i = 0; i < number_of_threads; ++i) {
		pool._threads.push_back(std::thread(&thread_pool::worker, &pool));
	}

	return true;
}

void destroy_thread_pool(thread_pool &pool) {
	{
		std::lock_guard lock(pool._mu);

		pool._stop = true;
		pool._condition.notify_all();
	}


	for (auto &t : pool._threads) t.join();
}

void thread_pool_add_task(thread_pool &pool, const fs::directory_entry &dir_entry) {
	{
		std::lock_guard lock(pool._mu);
		pool._tasks.push(dir_entry);
	}

	pool._condition.notify_one();
}

void thread_pool::worker() {
    while (true) {
		fs::directory_entry dir_entry;
		{
			std::unique_lock lock(_mu);

			_condition.wait(lock, [this] {
				return _stop || !_tasks.empty();
			});

			if (_stop && _tasks.empty()) return;

			dir_entry = _tasks.front();
			_tasks.pop();
		}

		// PROCESS FILE
		const fs::path &path = dir_entry.path();

		fs::path relative = fs::relative(path, _src_dir);
	    fs::path target = _dst_dir / relative;

	    if (path == target) continue; 

        if (dir_entry.is_directory()) { 
			fs::create_directory(target);

			continue;
		}

		if (!dir_entry.is_regular_file() || path.filename() == "hashes.txt") continue;

		const std::string key = path.string();

		std::string hash = hash_file(key);
		std::string old_hash;
		{
			std::lock_guard lock(_mu);
			old_hash = hashes_map[key];
		}

		if (old_hash != hash) {
			fs::create_directories(target.parent_path()); 
		    fs::copy_file(key, target, fs::copy_options::overwrite_existing);

			std::lock_guard lock(_mu);

			hashes_map[key] = hash;
			hashes_out << key << ':' << hash << '\n';
			std::cout << "changed file: " <<  key << "\n";

			++count;
		}
	}
}

std::string hash_file(const std::string &file_path) {
    std::ifstream file(file_path, std::ios::binary);

    if (!file) { 
    	std:: cerr << "Cannot open file: " << file_path << "\n";
    	return "";
    }

    XXH3_state_t *state = XXH3_createState();
    XXH3_64bits_reset(state);

    constexpr size_t BUF = 1 << 20; // 1 MB
    std::vector<char> buffer(BUF);

    while (file) {
        file.read(buffer.data(), BUF);
        std::streamsize got = file.gcount();
        if (got > 0) XXH3_64bits_update(state, buffer.data(), got);
    }

    uint64_t h = XXH3_64bits_digest(state);
    XXH3_freeState(state);

    return std::to_string(h);
}

