#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
class JsonReader {
public:
	static std::string read(std::experimental::filesystem::path json_path) {
		std::ifstream json_file(json_path);
		std::stringstream json_str;
		std::string line;
		while (std::getline(json_file, line)) {

			// Delete any line containing a c style comment
			if (std::string::npos != line.find("//"))
				continue;

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

			json_str << line << '\n';
		}
		//json_file.close(); // RAII

		return json_str.str();
	}
};