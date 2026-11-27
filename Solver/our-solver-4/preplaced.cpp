#include <array>
#include <cstdint>
#include <memory>

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

// Delay allocating the outer vector until the solution count is known. Small
// searches keep their column lists in this frame throughout enumeration.
using CompactRows = std::array<std::uint8_t, 12>;

struct ConstraintSearch {
  static constexpr std::size_t batchSize = 128;
  using Batch = std::array<CompactRows, batchSize>;

  int n;
  CompactRows placement{};
  Batch first;
  std::vector<std::unique_ptr<Batch>> extra;
  std::size_t count = 0;

  void record() {
    if (count < batchSize) {
      first[count] = placement;
    } else {
      const std::size_t slot = count % batchSize;
      if (!slot) extra.push_back(std::make_unique<Batch>());
      (*extra.back())[slot] = placement;
    }
    ++count;
  }

  Solutions materialize() {
    Solutions results;
    results.reserve(count);
    auto append = [&](const Batch& batch, std::size_t size) {
      for (std::size_t i = 0; i < size; ++i)
        results.emplace_back(batch[i].begin(), batch[i].begin() + n);
    };
    // Emit the heap batches first. Their storage can then be reused for
    // the final output rows, whose compact copies are still on the stack.
    const std::size_t onStack = count < batchSize ? count : batchSize;
    std::size_t remaining = count - onStack;
    for (auto& batch : extra) {
      const std::size_t size = remaining < batchSize ? remaining : batchSize;
      append(*batch, size);
      batch.reset();
      remaining -= size;
    }
    append(first, onStack);
    return results;
  }

  Mask legal(int row, Mask free, Mask rising, Mask falling) const {
    return free & ~((rising >> (n - 1 - row)) | (falling >> row));
  }

  // Two free rows and two free columns have just two possible pairings.
  void finishPair(int first, int second, Mask free, Mask rising, Mask falling) {
    if (free & (free >> (second - first))) return;
    const Mask lastChoices = legal(second, free, rising, falling);
    for (Mask choices = legal(first, free, rising, falling); choices; choices &= choices - 1) {
      const Mask queen = choices & -choices;
      const Mask last = free ^ queen;
      if (lastChoices & last) {
        placement[first] = __builtin_ctz(queen);
        placement[second] = __builtin_ctz(last);
        record();
      }
    }
  }

  void search(Mask rows, Mask free, Mask rising, Mask falling) {
    int row = __builtin_ctz(rows);
    const Mask rest = rows & (rows - 1);
    Mask choices = legal(row, free, rising, falling);
    if (!choices) return;
    if (!rest) {
      placement[row] = __builtin_ctz(choices);
      record();
      return;
    }
    if (!(rest & (rest - 1))) {
      finishPair(row, __builtin_ctz(rest), free, rising, falling);
      return;
    }

    // Every free column must belong to a row. A column available in just
    // one row forces that placement even when the row has several choices.
    int smallest = __builtin_popcount(choices);
    Mask seen = choices, repeated = 0;
    for (Mask scan = rest; scan; scan &= scan - 1) {
      const int other = __builtin_ctz(scan);
      const Mask candidates = legal(other, free, rising, falling);
      if (!candidates) return;
      repeated |= seen & candidates;
      seen |= candidates;
      const int options = __builtin_popcount(candidates);
      if (options < smallest) {
        row = other;
        choices = candidates;
        smallest = options;
      }
    }
    if (seen != free) return;
    if (smallest > 1 && repeated != free) {
      const Mask singles = free ^ repeated;
      choices = singles & -singles;
      for (Mask scan = rows; scan; scan &= scan - 1) {
        row = __builtin_ctz(scan);
        if (legal(row, choices, rising, falling)) break;
      }
    }
    const Mask nextRows = rows ^ (Mask{1} << row);
    while (choices) {
      const Mask queen = choices & -choices;
      choices ^= queen;
      placement[row] = __builtin_ctz(queen);
      search(nextRows, free ^ queen, rising | (queen << (n - 1 - row)),
             falling | (queen << row));
    }
  }
};

}  // namespace

std::vector<std::vector<int>> solvePreplaced(int n, const std::vector<std::pair<int, int>>& preplaced) {
  if (n < 4 || n > 12) return {};
  if (preplaced.empty()) {
    OrbitSearch freeBoard{};
    freeBoard.n = n;
    freeBoard.board = (Mask{1} << n) - 1;
    return freeBoard.run();
  }
  const Mask board = (Mask{1} << n) - 1;
  ConstraintSearch s;
  s.n = n;
  Mask rows = board, columns = 0, rising = 0, falling = 0;
  for (const auto& [r, c] : preplaced) {
    if (r < 0 || r >= n || c < 0 || c >= n) return {};
    const Mask row = Mask{1} << r, queen = Mask{1} << c;
    if ((rows & row) == 0 && s.placement[r] == c) continue;
    const Mask up = queen << (n - 1 - r), down = queen << r;
    if (!(rows & row) || (columns & queen) || (rising & up) || (falling & down)) return {};
    rows ^= row;
    columns |= queen;
    rising |= up;
    falling |= down;
    s.placement[r] = c;
  }
  if (rows) s.search(rows, board ^ columns, rising, falling);
  else s.record();
  return s.materialize();
}
