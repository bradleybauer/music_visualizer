#pragma once

#include <string>
namespace filesys {

    class path {
    public:
		path();
        path(const std::string& path);
		path(const char* path) : path(std::string(path)) {};

		std::string string() const;
		path extension() const;
    private:
		std::string rep;
	};

    bool exists(const path&);
    bool is_regular_file(const path&);

	path operator/(const path& lhs, const path& rhs);
	//path operator/(const std::string& lhs, const path& rhs);
}
