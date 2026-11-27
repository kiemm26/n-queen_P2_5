#include <array>
#include <cstdint>

#include "nqueens.h"

namespace {

using Mask = std::uint32_t;
using Rows = std::array<int, 12>;
using Solutions = std::vector<std::vector<int>>;

struct OrbitSearch {
  int n;
  Mask board;
  Rows cols{};
  std::array<Mask, 12> allowed{};
  std::vector<std::uint64_t> orbits;
  std::size_t count = 0;

  // The eight images are a transpose followed by independent row/column reflections.
  int column(const Rows& original, const Rows& inverse, int image, int row) const {
    const auto& positions = image & 4 ? inverse : original;
    const int c = positions[image & 2 ? n - 1 - row : row];
    return image & 1 ? n - 1 - c : c;
  }

  void record() {
    Rows inverse;
    for (int r = 0; r < n; ++r) inverse[cols[r]] = r;
    unsigned images = 1;
    bool hasSymmetry = false;
    for (int t = 1; t < 8; ++t) {
      int r = 0;
      while (r < n && column(cols, inverse, t, r) == cols[r]) ++r;
      if (r == n) hasSymmetry = true;
      else if (column(cols, inverse, t, r) < cols[r]) return;
    }

    for (int t = 1; t < 8; ++t) {
      bool duplicate = false;
      if (hasSymmetry) {
        // A symmetric solution can have fewer than eight distinct images.
        for (unsigned seen = images; seen; seen &= seen - 1) {
          const int other = __builtin_ctz(seen);
          int r = 0;
          while (r < n && column(cols, inverse, t, r) == column(cols, inverse, other, r)) ++r;
          if (r == n) { duplicate = true; break; }
        }
      }
      if (!duplicate) images |= 1u << t;
    }
    std::uint64_t packed = images;
    for (int r = n - 1; r >= 0; --r) packed = (packed << 4) | cols[r];
    orbits.push_back(packed);
    count += __builtin_popcount(images);
  }

  void search(int row, Mask free, Mask left, Mask right) {
    Mask choices = allowed[row] & free & ~(left | right);
    if (row == n - 2) {
      if (free & (free >> 1)) return;
      while (choices) {
        const Mask q = choices & -choices;
        choices ^= q;
        const Mask last = free ^ q;
        if (!(allowed[row + 1] & last) || (last & ((left << 1) | (right >> 1)))) continue;
        cols[row] = __builtin_ctz(q);
        cols[row + 1] = __builtin_ctz(last);
        record();
      }
      return;
    }
    while (choices) {
      const Mask q = choices & -choices;
      choices ^= q;
      cols[row] = __builtin_ctz(q);
      search(row + 1, free ^ q, (left | q) << 1, (right | q) >> 1);
    }
  }

  Solutions run() {
    for (int first = 0; first <= (n - 1) / 2; ++first) {
      allowed.fill(board);
      cols[0] = first;
      const Mask q = Mask{1} << first;
      // The top queen in the canonical image has the smallest distance to a
      // corner among the four edge queens. Restrict the opposite and side edges.
      const Mask nearCorner = q - 1;
      allowed[n - 1] &= ~(nearCorner | (nearCorner << (n - first)));
      const Mask sideEdges = 1u | (1u << (n - 1));
      for (int r = 1; r < first; ++r) allowed[r] &= ~sideEdges;
      for (int r = n - first; r < n; ++r) allowed[r] &= ~sideEdges;
      if (first == 0) {
        // Transposition interchanges the second queen's column and the row of column 1.
        for (int c = 2; c < n; ++c) {
          if (c > 2) allowed[c - 1] &= ~2u;
          const Mask next = Mask{1} << c;
          cols[1] = c;
          search(2, board ^ q ^ next, ((q << 1) | next) << 1, (next >> 1));
        }
      } else {
        search(1, board ^ q, q << 1, q >> 1);
      }
    }
    Solutions results;
    results.reserve(count);
    for (auto packed : orbits) {
      Rows original, inverse;
      for (int r = 0; r < n; ++r, packed >>= 4) {
        original[r] = packed & 15;
        inverse[original[r]] = r;
      }
      for (unsigned scan = packed; scan; scan &= scan - 1) {
        const int t = __builtin_ctz(scan);
        auto& out = results.emplace_back(n);
        for (int r = 0; r < n; ++r) out[r] = column(original, inverse, t, r);
      }
    }
    return results;
  }
};

}  // namespace

std::vector<std::vector<int>> solveNQueens(int n) {
  if (n < 4 || n > 12) return {};
  OrbitSearch search{};
  search.n = n;
  search.board = (Mask{1} << n) - 1;
  return search.run();
}
