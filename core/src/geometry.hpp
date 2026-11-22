#ifndef LIPI_SRC_GEOMETRY_HPP
#define LIPI_SRC_GEOMETRY_HPP

#include "lipi/lipi.h"

namespace lipi {

// Maps the unit square (0,0),(1,0),(1,1),(0,1) onto a quad (Heckbert, 1989).
struct Homography {
  double a, b, c, d, e, f, g, h;
  void map(double u, double v, double* x, double* y) const {
    const double w = g * u + h * v + 1.0;
    *x = (a * u + b * v + c) / w;
    *y = (d * u + e * v + f) / w;
  }
};

bool unit_square_to_quad(const lipi_quad& quad, Homography* out);

}  // namespace lipi

#endif
