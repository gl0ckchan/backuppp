#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

uint64_t hash_file(const std::string& filePath);
std::string find_hash(const std::string& filename, const fs::path& wanted_path);

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cout << "Usage: " << argv[0] << " <in> <out>" << std::endl;

		return 69;
	}

	fs::path src_dir(argv[1]);
    fs::path dst_dir(argv[2]);
    fs::create_directory(dst_dir);

	std::string hashes_path = src_dir.string() + "/hashes.txt";
	std::ofstream hashes(hashes_path, std::ios::app); 

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

		const std::string hash 	   = std::to_string(hash_file(path.string()));
		const std::string old_hash = find_hash(hashes_path, path);

		if (old_hash != hash) {
			std::cout << "changed file: " <<  path << "\n";

			fs::create_directories(target.parent_path()); 
            fs::copy_file(path, target, fs::copy_options::overwrite_existing);
            hashes << path.string() << '\t' << hash << '\n';

            count++;
    	} 
	}

	std::cout << "copied files: " << count << std::endl;

	hashes.close();

	return 0;
}

uint64_t hash_file(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    uint64_t hash = 14695981039346656037ULL; 
    const uint64_t prime = 1099511628211ULL;  

    char buffer[8192];
    while (file.read(buffer, sizeof(buffer))) {
        for (std::streamsize i = 0; i < file.gcount(); ++i) {
            hash ^= (unsigned char)(buffer[i]);
            hash *= prime;
        }
    }

    for (std::streamsize i = 0; i < file.gcount(); ++i) {
        hash ^= (unsigned char)(buffer[i]);
        hash *= prime;
    }

    return hash;
}

std::string find_hash(const std::string& filename, const fs::path& wanted_path) {
    std::ifstream input(filename);

    const std::string wanted = wanted_path.string();
    std::string line;

    while (std::getline(input, line)) {
        const auto tab = line.find('\t');
        if (tab != std::string::npos && line.substr(0, tab) == wanted) {
            return line.substr(tab + 1);
        }
    }

    return "";
}

