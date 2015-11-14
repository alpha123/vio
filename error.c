#include "error.h"

const char *vio_err_msg(vio_err_t err) {
#define MSG(e,d) case e: return d;

    switch (err) {
    LIST_ERRORS(MSG, MSG, MSG)
    default: return "Unknown error.";
    }

#undef MSG
}
