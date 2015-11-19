#ifndef VIO_ATTRIB_H
#define VIO_ATTRIB_H

#ifdef __GNUC__
#define VIO_PURE __attribute__((pure))
#define VIO_CONST __attribute__((const))
#else
#define VIO_PURE
#define VIO_CONST
#endif

#endif
