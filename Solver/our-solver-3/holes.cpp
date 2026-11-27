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
  std::vector<std::pair<int, int>> holes;
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
    unsigned valid = images;
    for (unsigned scan = images; scan; scan &= scan - 1) {
      const int t = __builtin_ctz(scan);
      for (const auto& [r, c] : holes) {
        if (column(cols, inverse, t, r) == c) {
          valid &= ~(1u << t);
          break;
        }
      }
    }
    if (valid == 0) return;
    std::uint64_t packed = valid;
    for (int r = n - 1; r >= 0; --r) packed = (packed << 4) | cols[r];
    orbits.push_back(packed);
    count += __builtin_popcount(valid);
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

struct OrientedSearch {
  int n;
  Rows cols{};
  std::array<Mask, 12> allowed{};
  Solutions results;
  bool transpose = false;
  bool reverse = false;
  bool symmetric = true;
  bool mirror = false;

  void configure(const std::vector<std::string>& grid) {
    std::array<std::array<Mask, 12>, 2> lines{};
    for (int r = 0; r < n; ++r) {
      for (int c = 0; c < n; ++c) {
        if (grid[r][c] != '.') continue;
        lines[0][r] |= Mask{1} << c;
        lines[1][c] |= Mask{1} << r;
      }
    }

    // Prefer the orientation that exposes the fewest choices in the first three rows.
    int best = 4 * n;
    for (int axis = 0; axis < 2; ++axis) {
      for (int flip = 0; flip < 2; ++flip) {
        int score = 0;
        for (int i = 0; i < 3; ++i)
          score += __builtin_popcount(lines[axis][flip ? n - 1 - i : i]);
        if (score < best) {
          best = score;
          transpose = axis;
          reverse = flip;
        }
      }
    }
    for (int i = 0; i < n; ++i) {
      const Mask line = lines[transpose][reverse ? n - 1 - i : i];
      allowed[i] = line;
      for (int c = 0; symmetric && c < n / 2; ++c)
        symmetric = ((line >> c) & 1u) == ((line >> (n - 1 - c)) & 1u);
    }
  }

  void emit(bool reflect) {
    auto& out = results.emplace_back(n);
    for (int i = 0; i < n; ++i) {
      const int a = reverse ? n - 1 - i : i;
      const int b = reflect ? n - 1 - cols[i] : cols[i];
      if (transpose) out[b] = a;
      else out[a] = b;
    }
  }

  void save() {
    emit(false);
    if (mirror) emit(true);
  }

  template <int Remaining>
  void search(Mask freeColumns, Mask left, Mask right) {
    const int row = n - Remaining;
    Mask choices = freeColumns & ~(left | right) & allowed[row];
    if constexpr (Remaining == 2) {
      // The two unused columns cannot be adjacent in the last two rows.
      if (freeColumns & (freeColumns >> 1)) return;
      while (choices) {
        const Mask queen = choices & -choices;
        choices ^= queen;
        const Mask last = freeColumns ^ queen;
        if ((last & ((left << 1) | (right >> 1))) != 0) continue;
        if ((allowed[row + 1] & last) == 0) continue;
        cols[row] = __builtin_ctz(queen);
        cols[row + 1] = __builtin_ctz(last);
        save();
      }
    } else {
      while (choices) {
        const Mask queen = choices & -choices;
        choices ^= queen;
        cols[row] = __builtin_ctz(queen);
        search<Remaining - 1>(freeColumns ^ queen, (left | queen) << 1, (right | queen) >> 1);
      }
    }
  }

  template <int Size>
  void start() {
    const Mask board = (Mask{1} << Size) - 1;
    Mask choices = board & allowed[0];
    if (symmetric) choices &= (Mask{1} << ((Size + 1) / 2)) - 1;
    while (choices) {
      const Mask queen = choices & -choices;
      choices ^= queen;
      cols[0] = __builtin_ctz(queen);
      mirror = symmetric && cols[0] < Size / 2;
      search<Size - 1>(board ^ queen, queen << 1, queen >> 1);
    }
  }

  void run() {
    switch (n) {
      case 4: start<4>(); break;
      case 5: start<5>(); break;
      case 6: start<6>(); break;
      case 7: start<7>(); break;
      case 8: start<8>(); break;
      case 9: start<9>(); break;
      case 10: start<10>(); break;
      case 11: start<11>(); break;
      case 12: start<12>(); break;
    }
  }
};


struct ConstraintSearch {
  int n;
  std::array<Mask, 12> allowed{};
  Rows placement{};
  Solutions results;

  // The rising diagonal index is column - row + n - 1; the falling one is column + row.
  void search(Mask rows, Mask columns, Mask rising, Mask falling) {
    if (rows == 0) {
      results.emplace_back(placement.begin(), placement.begin() + n);
      return;
    }
    int bestRow = 0;
    int smallest = n + 1;
    Mask choices = 0;
    for (Mask scan = rows; scan; scan &= scan - 1) {
      const int row = __builtin_ctz(scan);
      const Mask candidates = allowed[row] &
          ~(columns | (rising >> (n - 1 - row)) | (falling >> row));
      const int count = __builtin_popcount(candidates);
      if (count == 0) return;
      if (count < smallest) {
        bestRow = row;
        smallest = count;
        choices = candidates;
      }
    }
    rows ^= Mask{1} << bestRow;
    while (choices) {
      const Mask queen = choices & -choices;
      choices ^= queen;
      placement[bestRow] = __builtin_ctz(queen);
      search(rows, columns | queen, rising | (queen << (n - 1 - bestRow)),
             falling | (queen << bestRow));
    }
  }
};


}  // namespace

std::vector<std::vector<int>> solveHoles(int n, const std::vector<std::string>& grid) {
  if (n < 4 || n > 12) return {};
  int holeCount = 0;
  for (const auto& row : grid)
    for (char cell : row) holeCount += cell == 'H';

  const Mask board = (Mask{1} << n) - 1;
  // With few holes, a single unrestricted orbit search can test up to eight images
  // per representative. With dense holes, branch on the row with the fewest choices.
  if (holeCount <= n / 2) {
    OrbitSearch search{};
    search.n = n;
    search.board = board;
    for (int r = 0; r < n; ++r)
      for (int c = 0; c < n; ++c)
        if (grid[r][c] == 'H') search.holes.emplace_back(r, c);
    return search.run();
  }
  if (3 * holeCount >= n * n) {
    ConstraintSearch search{};
    search.n = n;
    for (int r = 0; r < n; ++r)
      for (int c = 0; c < n; ++c)
        if (grid[r][c] == '.') search.allowed[r] |= Mask{1} << c;
    search.search(board, 0, 0, 0);
    return std::move(search.results);
  }
  OrientedSearch search{};
  search.n = n;
  search.configure(grid);
  search.run();
  return std::move(search.results);
}
