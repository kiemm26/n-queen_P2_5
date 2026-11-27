#include <array>
#include <cstdint>
#include <utility>

#include "nqueens.h"

namespace {

using Mask = std::uint32_t;
using Tiles = std::uint64_t;
constexpr int maxN = 12;
constexpr int maxHalf = maxN / 2;

constexpr Mask reverseBits(Mask x) {
  x = ((x >> 1) & 0x55555555u) | ((x & 0x55555555u) << 1);
  x = ((x >> 2) & 0x33333333u) | ((x & 0x33333333u) << 2);
  x = ((x >> 4) & 0x0f0f0f0fu) | ((x & 0x0f0f0f0fu) << 4);
  x = ((x >> 8) & 0x00ff00ffu) | ((x & 0x00ff00ffu) << 8);
  return (x >> 16) | (x << 16);
}

struct Pattern {
  // Bits 0..35 encode six cells per row. Bits 36..57 cache the reflected
  // diagonal mask without increasing the size of a pattern.
  Tiles cellsAndReflection;
  Mask columns;   // Four bits per row, in order of distance from the middle.
  Mask diagonals; // Two masks of diagonals that can cross the middle.
};

// For each block of 64 lower patterns, a table entry is the mask that
// survives four simultaneous diagonal constraints from the upper pattern.
struct Block {
  std::array<std::array<Tiles, 16>, 6> safe{};

  Tiles compatible(Mask diagonals) const {
    Tiles options = safe[0][diagonals & 15] & safe[1][(diagonals >> 4) & 15];
    options &= safe[2][(diagonals >> 8) & 15] & safe[3][(diagonals >> 12) & 15];
    return options & safe[4][(diagonals >> 16) & 15] & safe[5][diagonals >> 20];
  }
};

struct Group {
  const Pattern* data;
  const Block* blocks;
  int size;
  bool reflected;
};

// Geometry shared by all inputs. A group enumerates just one strip of up to
// six rows with a given set of columns. Holes and cross-strip attacks are
// checked at run time, before a full solution can be formed.
template <Mask Columns, class Save>
constexpr void makeStrip(int row, Mask remaining, Mask left, Mask right,
                         Pattern path, Save& save) {
  if (!remaining) {
    path.cellsAndReflection |= Tiles(reverseBits(path.diagonals) >> 10) << 36;
    save(path);
    return;
  }
  Mask legal = remaining & ~(left | right);
  while (legal) {
    const Mask queen = legal & -legal;
    legal ^= queen;
    const int col = __builtin_ctz(queen);
    const int localCol = __builtin_popcount(Columns & (queen - 1));
    Pattern next = path;
    next.columns |= Mask(col) << (4 * row);
    next.cellsAndReflection |= Tiles{1} << (maxHalf * row + localCol);
    // Across the cut, c + row for one queen must differ from c - row - 1
    // for the other, in both directions. Only intercepts 0..10 can meet.
    next.diagonals |= ((queen << row) & 0x7ffu) | ((queen >> (row + 1)) << 11);
    makeStrip<Columns>(row + 1, remaining ^ queen, (left | queen) << 1,
                       (right | queen) >> 1, next, save);
  }
}

template <Mask Columns>
constexpr int stripCount() {
  int count = 0;
  auto increment = [&](Pattern) { ++count; };
  makeStrip<Columns>(0, Columns, 0, 0, {}, increment);
  return count;
}

template <Mask Columns>
constexpr auto makeGroup() {
  std::array<Pattern, stripCount<Columns>()> items{};
  int size = 0;
  auto store = [&](Pattern pattern) { items[size++] = pattern; };
  makeStrip<Columns>(0, Columns, 0, 0, {}, store);
  return items;
}

template <Mask Columns>
constexpr auto patterns = makeGroup<Columns>();

template <Mask Columns>
constexpr auto makeBlocks() {
  constexpr auto& items = patterns<Columns>;
  std::array<Block, (items.size() + 63) / 64> blocks{};
  for (std::size_t block = 0; block < blocks.size(); ++block) {
    std::array<Tiles, 24> attacks{};
    Tiles all = 0;
    for (std::size_t i = 64 * block; i < items.size() && i < 64 * (block + 1); ++i) {
      const Tiles bit = Tiles{1} << (i % 64);
      all |= bit;
      // The two diagonal directions exchange roles across the horizontal cut.
      const Mask d = items[i].diagonals;
      Mask cross = (d >> 11) | ((d & 0x7ffu) << 11);
      while (cross) {
        attacks[__builtin_ctz(cross)] |= bit;
        cross &= cross - 1;
      }
    }
    for (int part = 0; part < 6; ++part) {
      auto& safe = blocks[block].safe[part];
      safe[0] = all;
      for (unsigned subset = 1; subset < 16; ++subset)
        safe[subset] = safe[subset & (subset - 1)] & ~attacks[4 * part + __builtin_ctz(subset)];
    }
  }
  return blocks;
}

template <Mask Columns>
constexpr auto blocks = makeBlocks<Columns>();

template <std::size_t Columns>
constexpr Group groupFor() {
  constexpr int height = __builtin_popcount(Columns);
  // A strip of this height can only occur on boards of width <= 2*height+1.
  if constexpr (height < 2 || height > maxHalf || Columns >= (1u << (2 * height + 1))) {
    return {nullptr, nullptr, 0, false};
  } else {
    constexpr Mask mirror = reverseBits(Columns) >> (32 - maxN);
    constexpr Mask canonical = Columns < mirror ? Columns : mirror;
    return {patterns<canonical>.data(), blocks<canonical>.data(),
            static_cast<int>(patterns<canonical>.size()), canonical != Columns};
  }
}

template <std::size_t... I>
constexpr auto makeGroups(std::index_sequence<I...>) {
  return std::array<Group, sizeof...(I)>{groupFor<I>()...};
}

constexpr auto groups = makeGroups(std::make_index_sequence<1 << maxN>{});

struct HalfBoard {
  std::array<Tiles, maxN> holes{};

  Tiles forbidden(Mask columns, bool reflected) const {
    Tiles blocked = 0;
    int i = reflected ? __builtin_popcount(columns) - 1 : 0;
    while (columns) {
      const int col = __builtin_ctz(columns);
      columns &= columns - 1;
      blocked |= holes[col] << i;
      i += reflected ? -1 : 1;
    }
    return blocked;
  }
};

using Solutions = std::vector<std::vector<int>>;

class StripSearch {
 public:
  StripSearch(int n, const std::vector<std::string>& grid) : n_(n), cut_(n / 2) {
    for (int r = 0; r < n; ++r) {
      const bool top = r < cut_;
      const int localRow = top ? cut_ - 1 - r : r - cut_;
      auto& half = top ? top_ : bottom_;
      for (int c = 0; c < n; ++c) {
        if (grid[r][c] == 'H') half.holes[c] |= Tiles{1} << (maxHalf * localRow);
      }
    }
  }

  Solutions run() {
    Solutions results;
    // Counting uses population counts on the same bitsets used for emission.
    // The required return vector gets exactly one outer allocation.
    results.reserve(join<false>(results));
    join<true>(results);
    return results;
  }

 private:
  template <bool Emit>
  std::size_t join(Solutions& results) const {
    const Mask board = (1u << n_) - 1;
    std::size_t count = 0;
    for (Mask columns = 1; columns < board; ++columns) {
      if (__builtin_popcount(columns) != cut_) continue;
      const auto& upper = groups[columns];
      const auto& lower = groups[board ^ columns];
      if (!upper.size || !lower.size) continue;
      const Tiles upperHoles = top_.forbidden(columns, upper.reflected);
      const Tiles lowerHoles = bottom_.forbidden(board ^ columns, lower.reflected);
      for (int start = 0; start < lower.size; start += 64) {
        const auto& block = lower.blocks[start / 64];
        Tiles permitted = block.safe[0][0];
        if (lowerHoles) {
          permitted = 0;
          const int length = lower.size - start < 64 ? lower.size - start : 64;
          for (int i = 0; i < length; ++i)
            permitted |= Tiles((lower.data[start + i].cellsAndReflection & lowerHoles) == 0) << i;
        }
        if (!permitted) continue;
        for (int i = 0; i < upper.size; ++i) {
          const auto& first = upper.data[i];
          if (first.cellsAndReflection & upperHoles) continue;
          // Reflecting a strip swaps the two diagonal masks and reverses
          // their eleven bits. Compare in the lower strip's coordinate system.
          const Mask diagonals = upper.reflected == lower.reflected
              ? first.diagonals : static_cast<Mask>(first.cellsAndReflection >> 36);
          Tiles pairs = permitted & block.compatible(diagonals);
          if constexpr (Emit) {
            if (!pairs) continue;
            std::array<int, maxN> cols;
            Mask top = upper.reflected ? 0xbbbbbbu - first.columns : first.columns;
            for (int r = cut_ - 1; r >= 0; --r, top >>= 4) cols[r] = top & 15;
            while (pairs) {
              const int j = start + __builtin_ctzll(pairs);
              // Each nibble stores a column 0..11, so subtraction reflects
              // the packed columns independently without a borrow.
              const Mask path = lower.data[j].columns;
              Mask bottom = lower.reflected ? 0xbbbbbbu - path : path;
              for (int r = cut_; r < n_; ++r, bottom >>= 4) cols[r] = bottom & 15;
              results.emplace_back(cols.begin(), cols.begin() + n_);
              pairs &= pairs - 1;
            }
          } else {
            count += __builtin_popcountll(pairs);
          }
        }
      }
    }
    return count;
  }

  int n_, cut_;
  HalfBoard top_, bottom_;
};

struct ConstraintSearch {
  int n;
  std::array<Mask, maxN> allowed{};
  std::array<int, maxN> placement{};
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
  if (n < 4 || n > maxN) return {};
  ConstraintSearch dense{};
  dense.n = n;
  int holes = 0;
  Mask usableColumns = 0;
  for (int r = 0; r < n; ++r) {
    for (int c = 0; c < n; ++c) {
      if (grid[r][c] == '.') dense.allowed[r] |= 1u << c;
      else ++holes;
    }
    if (!dense.allowed[r]) return {};
    usableColumns |= dense.allowed[r];
  }
  const Mask board = (1u << n) - 1;
  if (usableColumns != board) return {};
  if (3 * holes >= n * n) {
    dense.search(board, 0, 0, 0);
    return std::move(dense.results);
  }
  return StripSearch(n, grid).run();
}
