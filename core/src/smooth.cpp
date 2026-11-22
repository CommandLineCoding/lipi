// One-Euro filter (Casiez, Roussel and Vogel, 2012) on the eight corner coordinates.
#include <algorithm>
#include <cmath>
#include <new>

#include "lipi/lipi.h"

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDefaultDt = 1.0 / 30.0;
constexpr double kSteadySpeed = 0.03;  // corner speed, in diagonals per second, below which the quad counts as steady

double alpha_for(double cutoff_hz, double dt) {
  const double tau = 1.0 / (2.0 * kPi * cutoff_hz);
  return 1.0 / (1.0 + tau / dt);
}

}  // namespace

struct lipi_smoother {
  lipi_smoother_options opt{};
  double diag = 1.0;
  bool has = false;
  double last_time = 0.0;
  double last_seen_time = 0.0;
  double steady = 0.0;
  double speed_avg = 0.0;  // moving average of the smoothed corner speed, diagonals per second
  double x[8] = {};      // filtered, in diagonals
  double dx[8] = {};     // filtered speed, diagonals per second
  lipi_quad last{};
};

extern "C" {

lipi_smoother_options lipi_smoother_defaults(void) {
  lipi_smoother_options o{};
  o.min_cutoff_hz = 1.5f;
  o.beta = 25.0f;
  o.reset_distance = 0.12f;
  o.hold_seconds = 0.4f;
  return o;
}

lipi_status lipi_smoother_create(int image_width, int image_height, const lipi_smoother_options* options,
                                 lipi_smoother** out) {
  if (out == nullptr) return LIPI_ERR_INVALID_ARGUMENT;
  *out = nullptr;
  if (image_width <= 0 || image_height <= 0) return LIPI_ERR_INVALID_ARGUMENT;
  lipi_smoother_options o = options != nullptr ? *options : lipi_smoother_defaults();
  if (!(o.min_cutoff_hz > 0.0f) || !(o.beta >= 0.0f) || !(o.reset_distance > 0.0f) || !(o.hold_seconds >= 0.0f)) {
    return LIPI_ERR_INVALID_ARGUMENT;
  }
  try {
    auto* s = new lipi_smoother();
    s->opt = o;
    s->diag = std::hypot(static_cast<double>(image_width), static_cast<double>(image_height));
    *out = s;
    return LIPI_OK;
  } catch (const std::bad_alloc&) {
    return LIPI_ERR_OUT_OF_MEMORY;
  }
}

lipi_status lipi_smoother_update(lipi_smoother* s, const lipi_quad* detected, double t, lipi_quad* out) {
  if (s == nullptr || out == nullptr || !std::isfinite(t)) return LIPI_ERR_INVALID_ARGUMENT;
  if (s->has && t < s->last_time) return LIPI_ERR_INVALID_ARGUMENT;
  if (detected == nullptr) {
    if (!s->has) return LIPI_ERR_NOT_FOUND;
    if (t - s->last_seen_time > static_cast<double>(s->opt.hold_seconds)) {
      lipi_smoother_reset(s);
      return LIPI_ERR_NOT_FOUND;
    }
    s->last_time = t;
    s->steady = 0.0;
    *out = s->last;
    return LIPI_OK;
  }
  if (lipi_quad_validate(detected) != LIPI_OK) return LIPI_ERR_INVALID_ARGUMENT;

  double z[8];
  for (int i = 0; i < 4; ++i) {
    z[2 * i] = static_cast<double>(detected->p[i].x) / s->diag;
    z[2 * i + 1] = static_cast<double>(detected->p[i].y) / s->diag;
  }
  bool jump = false;
  if (s->has) {
    for (int i = 0; i < 4; ++i)
      if (std::hypot(z[2 * i] - s->x[2 * i], z[2 * i + 1] - s->x[2 * i + 1]) > static_cast<double>(s->opt.reset_distance)) jump = true;
  }
  if (!s->has || jump) {
    for (int i = 0; i < 8; ++i) {
      s->x[i] = z[i];
      s->dx[i] = 0.0;
    }
    s->steady = 0.0;
    s->speed_avg = 1.0;  // unknown until a few frames have been seen
  } else {
    double dt = t - s->last_time;
    if (!(dt > 1e-6)) dt = kDefaultDt;
    double moved[8];
    for (int i = 0; i < 8; ++i) {
      const double raw_speed = (z[i] - s->x[i]) / dt;
      s->dx[i] += alpha_for(1.0, dt) * (raw_speed - s->dx[i]);
      const double cutoff = static_cast<double>(s->opt.min_cutoff_hz) + static_cast<double>(s->opt.beta) * std::fabs(s->dx[i]);
      const double before = s->x[i];
      s->x[i] += alpha_for(cutoff, dt) * (z[i] - s->x[i]);
      moved[i] = s->x[i] - before;
    }
    // Steady means the smoothed corners barely move (the raw detection may still jitter).
    double speed = 0.0;
    for (int i = 0; i < 4; ++i) speed = std::max(speed, std::hypot(moved[2 * i], moved[2 * i + 1]) / dt);
    s->speed_avg += alpha_for(3.0, dt) * (speed - s->speed_avg);
    s->steady = s->speed_avg < kSteadySpeed ? s->steady + dt : 0.0;
  }
  s->has = true;
  s->last_time = t;
  s->last_seen_time = t;
  for (int i = 0; i < 4; ++i) {
    s->last.p[i].x = static_cast<float>(s->x[2 * i] * s->diag);
    s->last.p[i].y = static_cast<float>(s->x[2 * i + 1] * s->diag);
  }
  *out = s->last;
  return LIPI_OK;
}

double lipi_smoother_steady_seconds(const lipi_smoother* s) { return s == nullptr ? 0.0 : s->steady; }

void lipi_smoother_reset(lipi_smoother* s) {
  if (s == nullptr) return;
  s->has = false;
  s->steady = 0.0;
  s->speed_avg = 0.0;
  for (int i = 0; i < 8; ++i) s->x[i] = s->dx[i] = 0.0;
}

void lipi_smoother_free(lipi_smoother* s) { delete s; }

}  // extern "C"
