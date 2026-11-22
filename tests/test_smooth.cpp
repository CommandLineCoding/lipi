#include <cmath>
#include <random>
#include <vector>

#include "doctest.h"
#include "lipi/lipi.h"

namespace {

constexpr int W = 640, H = 480;
constexpr double kDiag = 800.0;  // hypot(640, 480)

lipi_quad base_quad(float dx = 0, float dy = 0) {
  lipi_quad q{};
  q.p[0] = {100 + dx, 80 + dy};
  q.p[1] = {540 + dx, 90 + dy};
  q.p[2] = {530 + dx, 400 + dy};
  q.p[3] = {110 + dx, 390 + dy};
  return q;
}

lipi_quad jitter(const lipi_quad& q, std::mt19937& rng, float sigma) {
  std::normal_distribution<float> n(0.0f, sigma);
  lipi_quad out = q;
  for (auto& p : out.p) { p.x += n(rng); p.y += n(rng); }
  return out;
}

double max_dist(const lipi_quad& a, const lipi_quad& b) {
  double m = 0;
  for (int i = 0; i < 4; ++i) m = std::max(m, std::hypot(double(a.p[i].x - b.p[i].x), double(a.p[i].y - b.p[i].y)));
  return m;
}

// Mean frame-to-frame corner movement as a fraction of the diagonal.
double mean_jitter(const std::vector<lipi_quad>& track) {
  double sum = 0;
  for (size_t i = 1; i < track.size(); ++i) sum += max_dist(track[i], track[i - 1]) / kDiag;
  return sum / double(track.size() - 1);
}

}  // namespace

TEST_CASE("smoothing a still page cuts the jitter and stays on target") {
  lipi_smoother* s = nullptr;
  REQUIRE(lipi_smoother_create(W, H, nullptr, &s) == LIPI_OK);
  std::mt19937 rng(1);
  std::vector<lipi_quad> raw, smooth;
  const lipi_quad truth = base_quad();
  for (int i = 0; i < 150; ++i) {
    const lipi_quad in = jitter(truth, rng, 1.0f);
    lipi_quad out{};
    REQUIRE(lipi_smoother_update(s, &in, i / 30.0, &out) == LIPI_OK);
    if (i >= 30) { raw.push_back(in); smooth.push_back(out); }
  }
  CHECK(mean_jitter(smooth) < mean_jitter(raw) / 3.0);
  CHECK(max_dist(smooth.back(), truth) < 2.0);
  CHECK(lipi_smoother_steady_seconds(s) > 2.0);
  lipi_smoother_free(s);
}

TEST_CASE("a moving page is followed with little lag") {
  lipi_smoother* s = nullptr;
  REQUIRE(lipi_smoother_create(W, H, nullptr, &s) == LIPI_OK);
  std::mt19937 rng(2);
  double worst_lag = 0;
  for (int i = 0; i < 90; ++i) {
    const float dx = 3.0f * float(i);  // 3 px per frame = 90 px/s, about 0.11 diagonals per second
    const lipi_quad truth = base_quad(dx * 0.4f, 0);
    const lipi_quad in = jitter(truth, rng, 1.0f);
    lipi_quad out{};
    REQUIRE(lipi_smoother_update(s, &in, i / 30.0, &out) == LIPI_OK);
    if (i > 30) worst_lag = std::max(worst_lag, max_dist(out, truth));
  }
  CHECK(worst_lag < 12.0);
  CHECK(lipi_smoother_steady_seconds(s) == 0.0);
  lipi_smoother_free(s);
}

TEST_CASE("a different page starts a new track at once") {
  lipi_smoother* s = nullptr;
  REQUIRE(lipi_smoother_create(W, H, nullptr, &s) == LIPI_OK);
  lipi_quad out{};
  const lipi_quad a = base_quad();
  for (int i = 0; i < 30; ++i) REQUIRE(lipi_smoother_update(s, &a, i / 30.0, &out) == LIPI_OK);
  lipi_quad b{};
  b.p[0] = {20, 20};
  b.p[1] = {300, 30};
  b.p[2] = {290, 200};
  b.p[3] = {25, 190};
  REQUIRE(lipi_smoother_update(s, &b, 1.0, &out) == LIPI_OK);
  CHECK(max_dist(out, b) < 0.01);
  CHECK(lipi_smoother_steady_seconds(s) == 0.0);
  lipi_smoother_free(s);
}

TEST_CASE("short dropouts are bridged and long ones are reported") {
  lipi_smoother* s = nullptr;
  REQUIRE(lipi_smoother_create(W, H, nullptr, &s) == LIPI_OK);
  lipi_quad out{};
  CHECK(lipi_smoother_update(s, nullptr, 0.0, &out) == LIPI_ERR_NOT_FOUND);  // nothing seen yet
  const lipi_quad a = base_quad();
  REQUIRE(lipi_smoother_update(s, &a, 0.1, &out) == LIPI_OK);
  REQUIRE(lipi_smoother_update(s, nullptr, 0.2, &out) == LIPI_OK);  // held
  CHECK(max_dist(out, a) < 0.01);
  CHECK(lipi_smoother_update(s, nullptr, 0.8, &out) == LIPI_ERR_NOT_FOUND);  // lost for too long
  CHECK(lipi_smoother_update(s, nullptr, 0.9, &out) == LIPI_ERR_NOT_FOUND);
  REQUIRE(lipi_smoother_update(s, &a, 1.0, &out) == LIPI_OK);  // and it can start again
  lipi_smoother_free(s);
}

TEST_CASE("reset forgets the track") {
  lipi_smoother* s = nullptr;
  REQUIRE(lipi_smoother_create(W, H, nullptr, &s) == LIPI_OK);
  lipi_quad out{};
  const lipi_quad a = base_quad();
  REQUIRE(lipi_smoother_update(s, &a, 0.0, &out) == LIPI_OK);
  lipi_smoother_reset(s);
  CHECK(lipi_smoother_update(s, nullptr, 0.1, &out) == LIPI_ERR_NOT_FOUND);
  lipi_smoother_free(s);
}

TEST_CASE("bad arguments are rejected") {
  lipi_smoother* s = reinterpret_cast<lipi_smoother*>(0x1);
  CHECK(lipi_smoother_create(0, 480, nullptr, &s) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(s == nullptr);
  CHECK(lipi_smoother_create(640, 480, nullptr, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  lipi_smoother_options o = lipi_smoother_defaults();
  o.min_cutoff_hz = 0;
  CHECK(lipi_smoother_create(640, 480, &o, &s) == LIPI_ERR_INVALID_ARGUMENT);
  REQUIRE(lipi_smoother_create(640, 480, nullptr, &s) == LIPI_OK);
  lipi_quad out{};
  const lipi_quad a = base_quad();
  CHECK(lipi_smoother_update(nullptr, &a, 0.0, &out) == LIPI_ERR_INVALID_ARGUMENT);
  CHECK(lipi_smoother_update(s, &a, 0.0, nullptr) == LIPI_ERR_INVALID_ARGUMENT);
  const lipi_quad bow = {{{0, 0}, {10, 10}, {10, 0}, {0, 10}}};
  CHECK(lipi_smoother_update(s, &bow, 0.0, &out) == LIPI_ERR_INVALID_ARGUMENT);
  REQUIRE(lipi_smoother_update(s, &a, 1.0, &out) == LIPI_OK);
  CHECK(lipi_smoother_update(s, &a, 0.5, &out) == LIPI_ERR_INVALID_ARGUMENT);  // time went backwards
  lipi_smoother_free(s);
  lipi_smoother_free(nullptr);
  CHECK(lipi_smoother_steady_seconds(nullptr) == 0.0);
}
