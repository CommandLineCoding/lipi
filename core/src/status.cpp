#include "lipi/lipi.h"

extern "C" const char* lipi_status_string(lipi_status status) {
  switch (status) {
    case LIPI_OK: return "ok";
    case LIPI_ERR_INVALID_ARGUMENT: return "invalid argument";
    case LIPI_ERR_OUT_OF_MEMORY: return "out of memory";
    case LIPI_ERR_UNSUPPORTED: return "unsupported";
    case LIPI_ERR_INTERNAL: return "internal error";
    case LIPI_ERR_NOT_FOUND: return "no page found";
  }
  return "unknown status";
}
