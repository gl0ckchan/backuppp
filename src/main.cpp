#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>

#include <xxhash.h>

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

	std::ofstream hashes_out(hashes_path, std::ios::app);
	if (!hashes_out) {
		std::cerr << "Cannot open hashes.txt for writing\n";

	    return 69;
	}

	size_t count = 0;
	for (const fs::directory_entry &dir_entry : fs::recursive_directory_iterator(src_dir)) {
		const fs::path &path = dir_entry.path();
		
		fs::path relative = fs::relative(path, src_dir);
        fs::path target   = dst_dir / relative;

		if (dir_entry.is_directory()) { 
			fs::create_directory(target);
			continue;
		}

		if (!dir_entry.is_regular_file() || path.filename() == "hashes.txt") continue;

		const std::string hash 	   = hash_file(path.string());
		const std::string old_hash = hashes_map[path.string()];

		if (old_hash != hash) {
			std::cout << "changed file: " <<  path << "\n";

			fs::create_directories(target.parent_path()); 
            fs::copy_file(path, target, fs::copy_options::overwrite_existing);
            hashes_out << path.string() << ':' << hash << '\n';

            count++;
    	} 
	}

	std::cout << "copied files: " << count << std::endl;

	hashes_out.close();

	return 0;
}

std::string hash_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open file: " + file_path);

    XXH3_state_t* state = XXH3_createState();
    XXH3_64bits_reset(state);

    // Крупный буфер + чтение через readsome / rdbuf
    constexpr size_t BUF = 1 << 20; // 1 MiB
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

