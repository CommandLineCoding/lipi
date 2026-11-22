#include <algorithm>
#include <cmath>

#include "geometry.hpp"
#include "lipi/lipi.h"

namespace {

struct Vec3 {
  double x, y, z;
};

Vec3 cross(const Vec3& a, const Vec3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

double dist(const lipi_point& a, const lipi_point& b) {
  return std::hypot(static_cast<double>(a.x - b.x), static_cast<double>(a.y - b.y));
}

// Aspect ratio (width / height) of the page from the perspective of its quad,
// after Zhang and He, "Whiteboard scanning and image enhancement" (2007).
// Returns 0 when it cannot be recovered.
double aspect_from_perspective(const lipi_quad& q, int image_width, int image_height) {
  const double u0 = image_width / 2.0, v0 = image_height / 2.0;
  auto m = [&](const lipi_point& p) {
    return Vec3{static_cast<double>(p.x) - u0, static_cast<double>(p.y) - v0, 1.0};
  };
  const Vec3 m1 = m(q.p[0]), m2 = m(q.p[1]), m3 = m(q.p[3]), m4 = m(q.p[2]);
  const double k2d = dot(cross(m2, m4), m3);
  const double k3d = dot(cross(m3, m4), m2);
  if (std::fabs(k2d) < 1e-9 || std::fabs(k3d) < 1e-9) return 0.0;
  const double k2 = dot(cross(m1, m4), m3) / k2d;
  const double k3 = dot(cross(m1, m4), m2) / k3d;
  const Vec3 n2{k2 * m2.x - m1.x, k2 * m2.y - m1.y, k2 * m2.z - m1.z};
  const Vec3 n3{k3 * m3.x - m1.x, k3 * m3.y - m1.y, k3 * m3.z - m1.z};
  // The focal length is recovered from the two vanishing points when both are
  // finite and plausible. Otherwise (one-point perspective, noisy corners) a
  // typical phone camera is assumed.
  const double longest = std::max(image_width, image_height);
  const double assumed_f2 = (0.8 * longest) * (0.8 * longest);
  double f2 = assumed_f2;
  if (std::fabs(n2.z) > 1e-6 && std::fabs(n3.z) > 1e-6) {
    const double est = -(n2.x * n3.x + n2.y * n3.y) / (n2.z * n3.z);
    if (std::isfinite(est) && est > 0.16 * longest * longest && est < 4.0 * longest * longest) f2 = est;
  }
  const double w2 = (n2.x * n2.x + n2.y * n2.y) / f2 + n2.z * n2.z;
  const double h2 = (n3.x * n3.x + n3.y * n3.y) / f2 + n3.z * n3.z;
  if (!(w2 > 0.0) || !(h2 > 0.0)) return 0.0;
  const double aspect = std::sqrt(w2 / h2);
  return std::isfinite(aspect) ? aspect : 0.0;
}

}  // namespace

extern "C" {

lipi_status lipi_quad_validate(const lipi_quad* quad) {
  if (quad == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  for (const lipi_point& p : quad->p) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y)) return LIPI_ERR_INVALID_ARGUMENT;
  }
  double sign = 0.0;
  double area2 = 0.0;
  for (int i = 0; i < 4; ++i) {
    const lipi_point& a = quad->p[i];
    const lipi_point& b = quad->p[(i + 1) % 4];
    const lipi_point& c = quad->p[(i + 2) % 4];
    const double cr = static_cast<double>(b.x - a.x) * static_cast<double>(c.y - b.y) -
                      static_cast<double>(b.y - a.y) * static_cast<double>(c.x - b.x);
    if (cr == 0.0) return LIPI_ERR_INVALID_ARGUMENT;
    if (sign == 0.0) sign = cr;
    if ((cr > 0.0) != (sign > 0.0)) return LIPI_ERR_INVALID_ARGUMENT;
    area2 += static_cast<double>(a.x) * static_cast<double>(b.y) -
             static_cast<double>(b.x) * static_cast<double>(a.y);
  }
  if (std::fabs(area2) < 2.0) return LIPI_ERR_INVALID_ARGUMENT;
  // Corner order is top-left, top-right, bottom-right, bottom-left, which is
  // clockwise on screen (y down), i.e. a positive signed area.
  if (area2 <= 0.0) return LIPI_ERR_INVALID_ARGUMENT;
  return LIPI_OK;
}

lipi_status lipi_estimate_page_size(const lipi_quad* quad, int image_width, int image_height,
                                    int max_side, int* out_width, int* out_height) {
  if (out_width == nullptr || out_height == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  if (image_width <= 0 || image_height <= 0 || max_side < 2) return LIPI_ERR_INVALID_ARGUMENT;
  const lipi_status status = lipi_quad_validate(quad);
  if (status != LIPI_OK) return status;

  const double top = dist(quad->p[0], quad->p[1]), bottom = dist(quad->p[3], quad->p[2]);
  const double left = dist(quad->p[0], quad->p[3]), right = dist(quad->p[1], quad->p[2]);
  double aspect = aspect_from_perspective(*quad, image_width, image_height);
  // The perspective estimate is only trusted within a plausible range.
  if (!(aspect > 0.1 && aspect < 10.0)) aspect = ((top + bottom) / 2.0) / ((left + right) / 2.0);

  double w = std::max(top, bottom);
  double h = w / aspect;
  const double longest = std::max(w, h);
  if (longest > max_side) {
    const double scale = max_side / longest;
    w *= scale;
    h *= scale;
  }
  *out_width = std::max(1, static_cast<int>(std::lround(w)));
  *out_height = std::max(1, static_cast<int>(std::lround(h)));
  return LIPI_OK;
}

}  // extern "C"
