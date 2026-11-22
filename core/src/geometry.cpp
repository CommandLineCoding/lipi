#include "geometry.hpp"

#include <cmath>

namespace lipi {

bool unit_square_to_quad(const lipi_quad& q, Homography* out) {
  const double x0 = static_cast<double>(q.p[0].x), y0 = static_cast<double>(q.p[0].y);
  const double x1 = static_cast<double>(q.p[1].x), y1 = static_cast<double>(q.p[1].y);
  const double x2 = static_cast<double>(q.p[2].x), y2 = static_cast<double>(q.p[2].y);
  const double x3 = static_cast<double>(q.p[3].x), y3 = static_cast<double>(q.p[3].y);
  const double sx = x0 - x1 + x2 - x3;
  const double sy = y0 - y1 + y2 - y3;
  Homography H{};
  if (std::fabs(sx) < 1e-12 && std::fabs(sy) < 1e-12) {
    H = {x1 - x0, x3 - x0, x0, y1 - y0, y3 - y0, y0, 0.0, 0.0};
  } else {
    const double dx1 = x1 - x2, dx2 = x3 - x2, dy1 = y1 - y2, dy2 = y3 - y2;
    const double den = dx1 * dy2 - dx2 * dy1;
    if (std::fabs(den) < 1e-12) return false;
    const double g = (sx * dy2 - sy * dx2) / den;
    const double h = (sy * dx1 - sx * dy1) / den;
    H = {x1 - x0 + g * x1, x3 - x0 + h * x3, x0, y1 - y0 + g * y1, y3 - y0 + h * y3, y0, g, h};
  }
  *out = H;
  return true;
}

}  // namespace lipi
