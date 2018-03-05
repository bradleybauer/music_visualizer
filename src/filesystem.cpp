#include "filesystem.h"
using namespace filesys;

static const char sep = '/';

path::path(const std::string& path) : rep(path)
{
	// Store path without trailing separators
	while (rep.size() && rep.back() == sep)
		rep = rep.substr(0, rep.size() - 1);

	if (!rep.size())
		return;

	// Remove duplicate separators
	std::string double_sep = std::string(2, sep);
	size_t index = rep.find(double_sep);
	while (index != std::string::npos) {
		rep = rep.erase(index, 1);
		index = rep.find(double_sep);
	}
}

std::string path::string() const {
	return rep;
}

path filesys::path::extension() const {
	if (rep == "")
		return path();

	size_t index_of_period = rep.rfind('.');
	size_t index_of_sep = rep.rfind(sep);

	if (index_of_period == std::string::npos)
		index_of_period = 0;
	if (index_of_sep == std::string::npos)
		index_of_sep = 0;

	if (index_of_sep >= index_of_period)
		return path();

	std::string extens = rep.substr(index_of_period);
	if (extens == ".")
		return path();

	return path(extens);
}

#include <sys/stat.h>
bool filesys::exists(const path& name) {
	struct stat buffer;   
	return (stat (name.string().c_str(), &buffer) == 0); 
}

bool filesys::is_regular_file(const path&) {
	// Assume path is a file, fail elsewhere
	return true;
}

path filesys::operator/(const path & lhs, const path & rhs) {
	const std::string& lrep = lhs.string();
	const std::string& rrep = rhs.string();

	if (lrep == "")
		return rhs;

	if (rrep == "")
		return lhs;

	if (rrep[0] == sep)
		return path(lrep + rrep);
	else
		return path(lrep + sep + rrep);
}