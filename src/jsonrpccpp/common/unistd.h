#ifndef UNISTD_H
#define UNISTD_H

#ifdef _WIN32
#include <io.h>
#include <BaseTsd.h>

#define read _read
#define write _write

typedef SSIZE_T ssize_t;
#else // !_WIN32
#include <unistd.h>
#endif

#endif // !UNISTD_H
