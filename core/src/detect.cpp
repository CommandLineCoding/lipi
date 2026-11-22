// Classical page detection: Canny edges, Hough lines, quad search, and
// sub-pixel refinement of the corners on a higher-resolution copy.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <new>
#include <vector>

#include "gray.hpp"
#include "lipi/lipi.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int kDetectSide = 320;   // resolution used to find the page
constexpr int kRefineSide = 1024;  // resolution used to place the corners
constexpr int kThetaBins = 180;

struct P {
  double x = 0, y = 0;
};

P operator+(P a, P b) { return {a.x + b.x, a.y + b.y}; }
P operator-(P a, P b) { return {a.x - b.x, a.y - b.y}; }
P operator*(P a, double s) { return {a.x * s, a.y * s}; }
double cross(P a, P b) { return a.x * b.y - a.y * b.x; }
double norm(P a) { return std::hypot(a.x, a.y); }

// A line in normal form: points p with dot(p, (cos t, sin t)) == rho.
struct Line {
  double theta = 0, rho = 0;
  int votes = 0;
};

bool intersect(const Line& a, const Line& b, P* out) {
  const double c1 = std::cos(a.theta), s1 = std::sin(a.theta);
  const double c2 = std::cos(b.theta), s2 = std::sin(b.theta);
  const double det = c1 * s2 - c2 * s1;
  if (std::fabs(det) < 0.17) return false;  // closer than about 10 degrees to parallel
  out->x = (a.rho * s2 - b.rho * s1) / det;
  out->y = (b.rho * c1 - a.rho * c2) / det;
  return true;
}

// Angle between two undirected lines, in [0, pi/2].
double line_angle_diff(double t1, double t2) {
  double d = std::fabs(t1 - t2);
  d = std::fmod(d, kPi);
  return d > kPi / 2 ? kPi - d : d;
}

lipi::Gray blur(const lipi::Gray& in) {
  static const float k[7] = {0.0702f, 0.1311f, 0.1907f, 0.2161f, 0.1907f, 0.1311f, 0.0702f};
  lipi::Gray tmp = in, out = in;
  for (int y = 0; y < in.h; ++y)
    for (int x = 0; x < in.w; ++x) {
      float s = 0;
      for (int i = -3; i <= 3; ++i) s += k[i + 3] * in.at(x + i, y);
      tmp.v[static_cast<size_t>(y) * static_cast<size_t>(in.w) + static_cast<size_t>(x)] = s;
    }
  for (int y = 0; y < in.h; ++y)
    for (int x = 0; x < in.w; ++x) {
      float s = 0;
      for (int i = -3; i <= 3; ++i) s += k[i + 3] * tmp.at(x, y + i);
      out.v[static_cast<size_t>(y) * static_cast<size_t>(in.w) + static_cast<size_t>(x)] = s;
    }
  return out;
}

struct Edges {
  int w = 0, h = 0;
  std::vector<uint8_t> edge;   // 1 on an edge pixel
  std::vector<float> angle;    // gradient direction in radians (valid on edges)
  bool on(int x, int y) const {
    return x >= 0 && y >= 0 && x < w && y < h && edge[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)];
  }
  float ang(int x, int y) const { return angle[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)]; }
};

Edges canny(const lipi::Gray& g) {
  const int w = g.w, h = g.h;
  const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
  std::vector<float> mag(n, 0.0f), ang(n, 0.0f);
  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      const float gx = (g.at(x + 1, y - 1) + 2 * g.at(x + 1, y) + g.at(x + 1, y + 1)) -
                       (g.at(x - 1, y - 1) + 2 * g.at(x - 1, y) + g.at(x - 1, y + 1));
      const float gy = (g.at(x - 1, y + 1) + 2 * g.at(x, y + 1) + g.at(x + 1, y + 1)) -
                       (g.at(x - 1, y - 1) + 2 * g.at(x, y - 1) + g.at(x + 1, y - 1));
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
      mag[i] = std::sqrt(gx * gx + gy * gy) / 4.0f;
      ang[i] = std::atan2(gy, gx);
    }
  // Thresholds from the gradient distribution.
  std::vector<float> sorted(mag);
  const size_t k = static_cast<size_t>(0.90 * static_cast<double>(n));
  std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(k), sorted.end());
  const float high = std::max(12.0f, sorted[k]);
  const float low = 0.4f * high;

  // Non-maximum suppression along the gradient direction.
  std::vector<uint8_t> nms(n, 0);
  for (int y = 1; y < h - 1; ++y)
    for (int x = 1; x < w - 1; ++x) {
      const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
      if (mag[i] < low) continue;
      const float a = ang[i];
      const int dx = static_cast<int>(std::lround(std::cos(a))), dy = static_cast<int>(std::lround(std::sin(a)));
      const float m1 = mag[static_cast<size_t>(y + dy) * static_cast<size_t>(w) + static_cast<size_t>(x + dx)];
      const float m2 = mag[static_cast<size_t>(y - dy) * static_cast<size_t>(w) + static_cast<size_t>(x - dx)];
      if (mag[i] >= m1 && mag[i] >= m2) nms[i] = 1;
    }
  // Hysteresis.
  Edges e;
  e.w = w;
  e.h = h;
  e.edge.assign(n, 0);
  e.angle = ang;
  std::deque<size_t> queue;
  for (size_t i = 0; i < n; ++i)
    if (nms[i] && mag[i] >= high) {
      e.edge[i] = 1;
      queue.push_back(i);
    }
  while (!queue.empty()) {
    const size_t i = queue.front();
    queue.pop_front();
    const int x = static_cast<int>(i % static_cast<size_t>(w)), y = static_cast<int>(i / static_cast<size_t>(w));
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) {
        const int xx = x + dx, yy = y + dy;
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        const size_t j = static_cast<size_t>(yy) * static_cast<size_t>(w) + static_cast<size_t>(xx);
        if (nms[j] && !e.edge[j]) {
          e.edge[j] = 1;
          queue.push_back(j);
        }
      }
  }
  return e;
}

std::vector<Line> hough_lines(const Edges& e, int max_lines) {
  const double diag = std::hypot(e.w, e.h);
  const int rho_bins = static_cast<int>(2 * std::ceil(diag)) + 1;
  const int rho_off = rho_bins / 2;
  std::vector<int> acc(static_cast<size_t>(kThetaBins) * static_cast<size_t>(rho_bins), 0);
  std::array<double, kThetaBins> cs{}, sn{};
  for (int t = 0; t < kThetaBins; ++t) {
    cs[static_cast<size_t>(t)] = std::cos(t * kPi / kThetaBins);
    sn[static_cast<size_t>(t)] = std::sin(t * kPi / kThetaBins);
  }
  for (int y = 0; y < e.h; ++y)
    for (int x = 0; x < e.w; ++x) {
      if (!e.on(x, y)) continue;
      double th = std::fmod(static_cast<double>(e.ang(x, y)) + 2 * kPi, kPi);
      const int t0 = static_cast<int>(std::lround(th * kThetaBins / kPi));
      for (int dt = -1; dt <= 1; ++dt) {
        const int t = ((t0 + dt) % kThetaBins + kThetaBins) % kThetaBins;
        const double rho = (x + 0.5) * cs[static_cast<size_t>(t)] + (y + 0.5) * sn[static_cast<size_t>(t)];
        const int r = static_cast<int>(std::lround(rho)) + rho_off;
        if (r >= 0 && r < rho_bins) ++acc[static_cast<size_t>(t) * static_cast<size_t>(rho_bins) + static_cast<size_t>(r)];
      }
    }
  // Smooth over neighbouring angles and distances (weights 1-2-1 in each
  // direction, scaled by 1/4) so that a slightly noisy edge still forms a peak.
  std::vector<int> sm(acc.size(), 0);
  for (int t = 0; t < kThetaBins; ++t)
    for (int r = 1; r < rho_bins - 1; ++r) {
      int sum = 0;
      for (int dt = -1; dt <= 1; ++dt) {
        const int tt = ((t + dt) % kThetaBins + kThetaBins) % kThetaBins;
        const int wrap = (t + dt < 0 || t + dt >= kThetaBins) ? -1 : 1;
        const int rc = wrap == 1 ? r : 2 * rho_off - r;
        for (int dr = -1; dr <= 1; ++dr) {
          const int rr = rc + dr;
          if (rr < 0 || rr >= rho_bins) continue;
          sum += (2 - std::abs(dt)) * (2 - std::abs(dr)) *
                 acc[static_cast<size_t>(tt) * static_cast<size_t>(rho_bins) + static_cast<size_t>(rr)];
        }
      }
      sm[static_cast<size_t>(t) * static_cast<size_t>(rho_bins) + static_cast<size_t>(r)] = sum / 4;
    }
  const int min_votes = static_cast<int>(4 * 0.06 * std::min(e.w, e.h));
  std::vector<Line> cands;
  for (int t = 0; t < kThetaBins; ++t)
    for (int r = 1; r < rho_bins - 1; ++r) {
      const int v = sm[static_cast<size_t>(t) * static_cast<size_t>(rho_bins) + static_cast<size_t>(r)];
      if (v < min_votes) continue;
      bool peak = true;
      for (int dt = -4; dt <= 4 && peak; ++dt) {
        const int tt = ((t + dt) % kThetaBins + kThetaBins) % kThetaBins;
        for (int dr = -4; dr <= 4; ++dr) {
          if (dt == 0 && dr == 0) continue;
          // Wrapping past theta = pi flips the sign of rho.
          const int wrap = (t + dt < 0 || t + dt >= kThetaBins) ? -1 : 1;
          const int rr = wrap == 1 ? r + dr : (2 * rho_off - r) + dr;
          if (rr < 0 || rr >= rho_bins) continue;
          const int o = sm[static_cast<size_t>(tt) * static_cast<size_t>(rho_bins) + static_cast<size_t>(rr)];
          if (o > v || (o == v && (dt < 0 || (dt == 0 && dr < 0)))) {
            peak = false;
            break;
          }
        }
      }
      if (peak) cands.push_back({t * kPi / kThetaBins, static_cast<double>(r - rho_off), v});
    }
  std::sort(cands.begin(), cands.end(), [](const Line& a, const Line& b) { return a.votes > b.votes; });
  std::vector<Line> out;
  for (const Line& c : cands) {
    bool dup = false;
    for (const Line& o : out) {
      // Compare as undirected lines, accounting for the theta wrap.
      const double d = line_angle_diff(c.theta, o.theta);
      const bool flipped = std::fabs(c.theta - o.theta) > kPi / 2;
      const double rho_diff = std::fabs(c.rho - (flipped ? -o.rho : o.rho));
      if (d < 8 * kPi / 180 && rho_diff < 8) {
        dup = true;
        break;
      }
    }
    if (!dup) out.push_back(c);
    if (static_cast<int>(out.size()) >= max_lines) break;
  }
  return out;
}

// Orders four points clockwise on screen starting at the top-left corner.
std::array<P, 4> order_corners(std::array<P, 4> pts) {
  P c{};
  for (const P& p : pts) c = c + p * 0.25;
  std::sort(pts.begin(), pts.end(), [&](const P& a, const P& b) {
    return std::atan2(a.y - c.y, a.x - c.x) < std::atan2(b.y - c.y, b.x - c.x);
  });
  size_t start = 0;
  for (size_t i = 1; i < 4; ++i)
    if (pts[i].x + pts[i].y < pts[start].x + pts[start].y) start = i;
  std::array<P, 4> out{};
  for (size_t i = 0; i < 4; ++i) out[i] = pts[(start + i) % 4];
  return out;
}

double polygon_area(const std::array<P, 4>& q) {
  double a = 0;
  for (size_t i = 0; i < 4; ++i) a += cross(q[i], q[(i + 1) % 4]);
  return a / 2.0;
}

bool convex(const std::array<P, 4>& q) {
  double sign = 0;
  for (size_t i = 0; i < 4; ++i) {
    const double c = cross(q[(i + 1) % 4] - q[i], q[(i + 2) % 4] - q[(i + 1) % 4]);
    if (c == 0) return false;
    if (sign == 0) sign = c;
    if ((c > 0) != (sign > 0)) return false;
  }
  return true;
}

// Fraction of the side from a to b that is backed by edge pixels whose
// direction matches the side. Samples outside the image are skipped.
double edge_support(const Edges& e, P a, P b, double* inside_fraction) {
  const double len = norm(b - a);
  const int steps = std::max(2, static_cast<int>(len / 2.0));
  const double ns = std::atan2(b.x - a.x, -(b.y - a.y));  // normal direction of the side
  int total = 0, inside = 0, hits = 0;
  for (int i = 0; i <= steps; ++i) {
    ++total;
    const P p = a + (b - a) * (static_cast<double>(i) / steps);
    const int px = static_cast<int>(std::floor(p.x)), py = static_cast<int>(std::floor(p.y));
    if (px < 0 || py < 0 || px >= e.w || py >= e.h) continue;
    ++inside;
    bool hit = false;
    for (int dy = -1; dy <= 1 && !hit; ++dy)
      for (int dx = -1; dx <= 1 && !hit; ++dx) {
        if (!e.on(px + dx, py + dy)) continue;
        double d = std::fabs(static_cast<double>(e.ang(px + dx, py + dy)) - ns);
        d = std::fmod(d, kPi);
        if (d > kPi / 2) d = kPi - d;
        if (d < 25 * kPi / 180) hit = true;
      }
    if (hit) ++hits;
  }
  *inside_fraction = static_cast<double>(inside) / total;
  return inside == 0 ? 0.0 : static_cast<double>(hits) / inside;
}

struct Candidate {
  std::array<P, 4> q;
  double score = 0;
  double support = 0;
};

bool find_quad(const Edges& e, Candidate* best) {
  const std::vector<Line> lines = hough_lines(e, 24);
  const double img_area = static_cast<double>(e.w) * static_cast<double>(e.h);
  const double perspective = 35 * kPi / 180, adjacent = 40 * kPi / 180;
  bool found = false;
  const size_t n = lines.size();
  for (size_t a = 0; a < n; ++a)
    for (size_t b = a + 1; b < n; ++b)
      for (size_t c = b + 1; c < n; ++c)
        for (size_t d = c + 1; d < n; ++d) {
          const size_t idx[4] = {a, b, c, d};
          static const int pairing[3][4] = {{0, 1, 2, 3}, {0, 2, 1, 3}, {0, 3, 1, 2}};
          for (const auto& pr : pairing) {
            const Line& a1 = lines[idx[pr[0]]];
            const Line& a2 = lines[idx[pr[1]]];
            const Line& b1 = lines[idx[pr[2]]];
            const Line& b2 = lines[idx[pr[3]]];
            if (line_angle_diff(a1.theta, a2.theta) > perspective) continue;
            if (line_angle_diff(b1.theta, b2.theta) > perspective) continue;
            if (line_angle_diff(a1.theta, b1.theta) < adjacent || line_angle_diff(a1.theta, b2.theta) < adjacent ||
                line_angle_diff(a2.theta, b1.theta) < adjacent || line_angle_diff(a2.theta, b2.theta) < adjacent) {
              continue;
            }
            P p00, p01, p11, p10;
            if (!intersect(a1, b1, &p00) || !intersect(a1, b2, &p01) || !intersect(a2, b2, &p11) ||
                !intersect(a2, b1, &p10)) {
              continue;
            }
            std::array<P, 4> q = order_corners({p00, p01, p11, p10});
            bool ok = convex(q) && polygon_area(q) > 0.1 * img_area;
            for (const P& p : q)
              if (p.x < -0.12 * e.w || p.x > 1.12 * e.w || p.y < -0.12 * e.h || p.y > 1.12 * e.h) ok = false;
            if (!ok) continue;
            double support = 0, min_side = 1;
            bool inside_ok = true;
            for (size_t i = 0; i < 4; ++i) {
              double inside = 0;
              const double s = edge_support(e, q[i], q[(i + 1) % 4], &inside);
              if (inside < 0.5) inside_ok = false;
              support += s / 4.0;
              min_side = std::min(min_side, s);
            }
            if (!inside_ok || min_side < 0.3) continue;
            const double score = support * support * std::sqrt(std::min(1.0, polygon_area(q) / img_area));
            if (!found || score > best->score) {
              best->q = q;
              best->score = score;
              best->support = support;
              found = true;
            }
          }
        }
  return found;
}

// Total least squares line through points; returns false if degenerate.
bool fit_line(const std::vector<P>& pts, P* origin, P* dir) {
  if (pts.size() < 2) return false;
  P c{};
  for (const P& p : pts) c = c + p * (1.0 / static_cast<double>(pts.size()));
  double sxx = 0, sxy = 0, syy = 0;
  for (const P& p : pts) {
    const P d = p - c;
    sxx += d.x * d.x;
    sxy += d.x * d.y;
    syy += d.y * d.y;
  }
  const double t = 0.5 * std::atan2(2 * sxy, sxx - syy);
  *origin = c;
  *dir = {std::cos(t), std::sin(t)};
  return true;
}

bool line_intersect(P o1, P d1, P o2, P d2, P* out) {
  const double det = cross(d1, d2);
  if (std::fabs(det) < 1e-6) return false;
  const double t = cross(o2 - o1, d2) / det;
  *out = o1 + d1 * t;
  return true;
}

// Refines a quad on a gray image; unrefinable sides keep their coarse position.
std::array<P, 4> refine(const lipi::Gray& g, std::array<P, 4> q) {
  const double diag = std::hypot(g.w, g.h);
  P origin[4], dir[4];
  for (size_t i = 0; i < 4; ++i) {
    const P a = q[i], b = q[(i + 1) % 4];
    const double len = norm(b - a);
    origin[i] = a;
    dir[i] = (b - a) * (1.0 / len);
    const P t = dir[i], nrm{-t.y, t.x};
    const double radius = std::min(12.0, std::max(3.0, 0.03 * len));
    std::vector<P> pts;
    for (int k = 0; k < 31; ++k) {
      const double u = 0.1 + 0.8 * k / 30.0;
      const P c = a + (b - a) * u;
      auto profile = [&](double d) {
        float s = 0;
        for (int j = -2; j <= 2; ++j) s += g.sample(static_cast<float>(c.x + nrm.x * d + t.x * j), static_cast<float>(c.y + nrm.y * d + t.y * j));
        return s / 5.0f;
      };
      double best_d = 0, best_g = 0;
      for (double d = -radius; d <= radius; d += 0.5) {
        const double gr = std::fabs(profile(d + 1.0) - profile(d - 1.0));
        if (gr > best_g) {
          best_g = gr;
          best_d = d;
        }
      }
      if (best_g < 6.0) continue;
      pts.push_back(c + nrm * best_d);
    }
    for (int iter = 0; iter < 3 && pts.size() >= 8; ++iter) {
      P o, d;
      if (!fit_line(pts, &o, &d)) break;
      origin[i] = o;
      dir[i] = d;
      std::vector<double> res;
      for (const P& p : pts) res.push_back(std::fabs(cross(d, p - o)));
      std::vector<double> sorted(res);
      std::nth_element(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(sorted.size() / 2), sorted.end());
      const double cut = std::max(1.0, 2.5 * sorted[sorted.size() / 2]);
      std::vector<P> keep;
      for (size_t j = 0; j < pts.size(); ++j)
        if (res[j] <= cut) keep.push_back(pts[j]);
      if (keep.size() == pts.size()) break;
      pts = keep;
    }
    if (pts.size() < 8) {  // not enough support: keep the coarse side
      origin[i] = a;
      dir[i] = t;
    }
  }
  std::array<P, 4> out = q;
  for (size_t i = 0; i < 4; ++i) {
    // corner i is the intersection of side i-1 (ending at i) and side i (starting at i)
    const size_t prev = (i + 3) % 4;
    P p;
    if (line_intersect(origin[prev], dir[prev], origin[i], dir[i], &p) && norm(p - q[i]) < 0.05 * diag) out[i] = p;
  }
  return out;
}

}  // namespace

extern "C" {

lipi_status lipi_detect_page_classical(const lipi_image_view* src, lipi_quad* out_quad, float* out_confidence) {
  if (src == nullptr || src->data == nullptr || out_quad == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width < 16 || src->height < 16 || src->stride < src->width * channels) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  try {
    float scale = 1.0f;
    const lipi::Gray small = blur(lipi::to_gray(*src, kDetectSide, &scale));
    const Edges edges = canny(small);
    Candidate best;
    if (!find_quad(edges, &best)) return LIPI_ERR_NOT_FOUND;

    float fine_scale = 1.0f;
    const lipi::Gray fine = blur(lipi::to_gray(*src, kRefineSide, &fine_scale));
    const double ratio = static_cast<double>(scale) / static_cast<double>(fine_scale);
    std::array<P, 4> q{};
    for (size_t i = 0; i < 4; ++i) {
      q[i] = {std::min(std::max(best.q[i].x, 0.0), static_cast<double>(small.w)) * ratio,
              std::min(std::max(best.q[i].y, 0.0), static_cast<double>(small.h)) * ratio};
    }
    q = refine(fine, q);
    for (size_t i = 0; i < 4; ++i) {
      out_quad->p[i].x = static_cast<float>(std::min(std::max(q[i].x * fine_scale, 0.0), static_cast<double>(src->width)));
      out_quad->p[i].y = static_cast<float>(std::min(std::max(q[i].y * fine_scale, 0.0), static_cast<double>(src->height)));
    }
    if (out_confidence != nullptr) *out_confidence = static_cast<float>(std::min(1.0, best.support));
    return lipi_quad_validate(out_quad) == LIPI_OK ? LIPI_OK : LIPI_ERR_NOT_FOUND;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

lipi_status lipi_refine_quad(const lipi_image_view* src, const lipi_quad* in_quad, lipi_quad* out_quad) {
  if (src == nullptr || src->data == nullptr || in_quad == nullptr || out_quad == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  const int channels = lipi_pixel_format_channels(src->format);
  if (channels == 0 || src->width < 16 || src->height < 16 || src->stride < src->width * channels) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  const lipi_status status = lipi_quad_validate(in_quad);
  if (status != LIPI_OK) return status;
  try {
    float scale = 1.0f;
    const lipi::Gray fine = blur(lipi::to_gray(*src, kRefineSide, &scale));
    std::array<P, 4> q{};
    for (size_t i = 0; i < 4; ++i) q[i] = {static_cast<double>(in_quad->p[i].x) / scale, static_cast<double>(in_quad->p[i].y) / scale};
    q = refine(fine, q);
    for (size_t i = 0; i < 4; ++i) {
      out_quad->p[i].x = static_cast<float>(std::min(std::max(q[i].x * scale, 0.0), static_cast<double>(src->width)));
      out_quad->p[i].y = static_cast<float>(std::min(std::max(q[i].y * scale, 0.0), static_cast<double>(src->height)));
    }
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  } catch (...) {
    return LIPI_ERR_INTERNAL;
  }
}

}  // extern "C"
