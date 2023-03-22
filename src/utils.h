#ifndef _HTTP_UTILS_H_
#define _HTTP_UTILS_H_

#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32 //------- WIN 32
#include <io.h>
//#ifndef stat
//#define stat _stat
//#endif
//#ifndef fstat
//#define fstat _fstat
//#endif
//#ifndef open
//#define open _open
//#endif
//#ifndef close
//#define close _close
//#endif
//#ifndef O_RDONLY
//#define O_RDONLY _O_RDONLY
//#endif
#else //-------------- LINUX
#include <unistd.h>
#endif /* _WIN32 */
#include <fcntl.h>

int open_file(std::string path, int64_t& size);

time_t str2time(const std::string& date_str, const std::string& date_format);

#endif // !_HTTP_UTILS_H_
