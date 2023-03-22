#include "utils.h"
#include <iomanip>
#include <sstream>

using namespace std;

int open_file(std::string path, int64_t &size) {
	int fd = - 1;
	struct stat st;
	if ((fd = open(path.c_str(), O_RDONLY)) < 0)
		return fd;

	if (fstat(fd, &st) < 0) {
		close(fd);
		return -1;
	}
	size = st.st_size;
	return fd;
}

time_t str2time(const std::string& date_str, const std::string& date_format) {
	istringstream iss(date_str);
	tm date_tm;
	iss >> get_time(&date_tm, date_format.c_str());
	return mktime(&date_tm);
}
