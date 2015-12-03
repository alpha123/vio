#ifndef VIO_ALLOCA_H
#define VIO_ALLOCA_H

#if defined(__unix__)
#include <sys/param.h>
#ifdef BSD
#include <stdlib.h>
#else
#include <alloca.h>
#endif
#elif defined(_WIN32)
#include <malloc.h>
#define alloca _alloca
#endif

#endif

