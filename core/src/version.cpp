#include "lipi/lipi.h"

#define LIPI_STR2(x) #x
#define LIPI_STR(x) LIPI_STR2(x)

extern "C" const char* lipi_version(void) {
  return LIPI_STR(LIPI_VERSION_MAJOR) "." LIPI_STR(LIPI_VERSION_MINOR) "." LIPI_STR(LIPI_VERSION_PATCH);
}
