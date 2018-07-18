#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
namespace filesys = std::filesystem;
#include <stdexcept>

class JsonFileReader {
public:
	static std::string read(const filesys::path &json_path) {
		if (! filesys::exists(json_path))
			throw std:: runtime_error(json_path.string() + " does not exist");
		std::ifstream json_file(json_path.string());
		std::stringstream json_str;
		std::string line;
		while (std::getline(json_file, line)) {
			// TODO
			// Allow the user to get away with some json comma errors and allow floats to be typed like 0. and .06 instead of 0.0 and 0.06

			//int index = line.find("},");
			//if (std::string::npos != index) {
			//	int i = 0;
			//	while (index + i < line.size() && (line[index + i] == ' ' || line[index + i] == '\t')) {
			//		i++;
			//	}
			//}
			//else if (std::string::npos == line.find(""))


			// TODO find each } (execpt last) and ], and if they're not followed by a comma, then place a comma after them.

			json_str << line << '\n';
		}
		//json_file.close(); // RAII

		return json_str.str();
	}
};