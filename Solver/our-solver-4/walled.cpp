#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "nqueens.h"

namespace {

using Cell = std::pair<int, int>;
using Board = std::vector<Cell>;
constexpr int maxSide = 12;
constexpr int maxCells = maxSide * maxSide;

using Mask = std::uint32_t;
using Rows = std::array<int, 12>;
using Solutions = std::vector<Board>;

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
        for (int r = 0; r < n; ++r) out[r] = {r, column(original, inverse, t, r)};
      }
    }
    return results;
  }
};

// Collect compact records in a stack batch, spilling additional batches as
// needed. Release those batches while constructing the final return vector.
template <class Item, std::size_t Capacity>
class Batches {
 public:
  std::size_t size() const { return count_; }

  void add(const Item& item) {
    if (count_ < Capacity) first_[count_] = item;
    else {
      const auto slot = count_ % Capacity;
      if (!slot) extra_.push_back(std::make_unique<Batch>());
      (*extra_.back())[slot] = item;
    }
    ++count_;
  }

  template <class Emit>
  void consume(Emit emit) {
    const auto local = std::min(count_, Capacity);
    std::size_t remaining = count_ - local;
    for (auto& batch : extra_) {
      const auto size = std::min(remaining, Capacity);
      for (std::size_t i = 0; i < size; ++i) emit((*batch)[i]);
      batch.reset();
      remaining -= size;
    }
    for (std::size_t i = 0; i < local; ++i) emit(first_[i]);
  }

 private:
  using Batch = std::array<Item, Capacity>;
  Batch first_;
  std::vector<std::unique_ptr<Batch>> extra_;
  std::size_t count_ = 0;
};

// If one orientation has a single free segment in every row, N is an upper
// bound. A row bitboard can enumerate all placements attaining it directly.
class FullLineSearch {
 public:
  Solutions run(int n, const std::vector<std::string>& grid) {
    n_ = n;
    std::array<std::array<Mask, maxSide>, 2> masks{};
    for (int r = 0; r < n; ++r) for (int c = 0; c < n; ++c) {
      if (grid[r][c] == '.') {
        masks[0][r] |= 1u << c;
        masks[1][c] |= 1u << r;
      }
    }
    for (int a = 0; a < 2; ++a) {
      bool onePerLine = true;
      for (int r = 0; r < n; ++r) {
        const Mask starts = masks[a][r] & ~(masks[a][r] << 1);
        if (!starts || (starts & (starts - 1))) { onePerLine = false; break; }
      }
      if (!onePerLine) continue;
      open_ = masks[a];
      for (int r = 0; r < n; ++r) for (int c = 0; c < n; ++c)
        cells_[r][c] = a == 0 ? Cell{r, c} : Cell{c, r};
      search(0, 0, 0, 0, 0);
      Solutions boards;
      boards.reserve(count_);
      prefixes_.consume([&](std::uint64_t packed) {
        Mask choices = packed & 0xfffu;
        packed >>= 12;
        std::array<Cell, maxSide> path;
        for (int r = n - 2; r >= 0; --r, packed >>= 4)
          path[r] = cells_[r][packed & 15u];
        while (choices) {
          path[n - 1] = cells_[n - 1][__builtin_ctz(choices)];
          boards.emplace_back(path.begin(), path.begin() + n);
          choices &= choices - 1;
        }
      });
      return boards;
    }
    return {};
  }

 private:
  void search(int row, Mask columns, Mask left, Mask right, std::uint64_t path) {
    const Mask open = open_[row];
    columns &= open;
    left &= open;
    right &= open;
    Mask choices = open & ~(columns | left | right);
    if (row == n_ - 1) {
      if (choices) {
        // At most eleven 4-bit columns and twelve last-row candidates.
        prefixes_.add((path << 12) | choices);
        count_ += __builtin_popcount(choices);
      }
      return;
    }
    while (choices) {
      const Mask queen = choices & -choices;
      choices ^= queen;
      search(row + 1, columns | queen, (left | queen) << 1, (right | queen) >> 1,
             (path << 4) | __builtin_ctz(queen));
    }
  }

  int n_;
  std::array<Mask, maxSide> open_;
  std::array<std::array<Cell, maxSide>, maxSide> cells_;
  Batches<std::uint64_t, 256> prefixes_;
  std::size_t count_ = 0;
};

using Word = std::uint64_t;
using Bits = std::array<Word, 3>;
using Candidates = std::array<Bits, 2>;
constexpr int slots = 3 * 64;

struct Segment {
  std::array<std::uint8_t, maxSide> cells;
  int size = 0;
};

struct Lines {
  std::array<Segment, maxCells> segments;
  int count = 0;
};

struct Axis {
  Bits ends{};
  std::array<std::uint8_t, maxCells> slot;
  std::array<std::uint8_t, slots> cell;
  std::array<Word, slots> segmentMask;
  std::array<Bits, slots> crossMask{};

  void pack(const Lines& lines) {
    int pos = 0;
    for (int i = 0; i < lines.count; ++i) {
      const Segment& seg = lines.segments[i];
      // A segment fits wholly in one word. At most 11 padding bits per
      // boundary still leave room for all 144 cells in three words.
      if (pos % 64 + seg.size > 64) pos = (pos + 63) & ~63;
      const Word mask = ((Word{1} << seg.size) - 1) << (pos % 64);
      for (int j = 0; j < seg.size; ++j, ++pos) {
        slot[seg.cells[j]] = pos;
        cell[pos] = seg.cells[j];
      }
      const int end = pos - 1;
      segmentMask[end] = mask;
      ends[end / 64] |= Word{1} << (end % 64);
    }
  }

  int activeSegments(const Bits& free, Bits& active) const {
    int count = 0;
    for (int i = 0; i < 3; ++i) {
      // Each end bit pays for the subtraction within its own segment.
      // It clears if there is any earlier candidate in that segment.
      active[i] = ends[i] & (free[i] | ~(ends[i] - (free[i] & ~ends[i])));
      count += __builtin_popcountll(active[i]);
    }
    return count;
  }
};

// On heavily split boards, attacking cells in different segments can still
// form a group that contributes at most one queen. Greedy coloring exploits
// these groups to bound a maximum clique of mutually compatible cells.
class CliqueSearch {
 public:
  template <class Save>
  int run(const Axis& axis, const Bits& free, const std::array<Candidates, maxCells>& conflicts,
          Save& save) {
    struct Vertex { int slot, degree; };
    std::array<Vertex, maxCells> order;
    int size = 0;
    for (int w = 0; w < 3; ++w) for (Word cells = free[w]; cells; cells &= cells - 1) {
      const int slot = 64 * w + __builtin_ctzll(cells);
      const Bits& attacked = conflicts[axis.cell[slot]][0];
      const int degree = __builtin_popcountll(attacked[0]) + __builtin_popcountll(attacked[1]) +
                         __builtin_popcountll(attacked[2]);
      order[size++] = {slot, degree};
    }
    std::sort(order.begin(), order.begin() + size, [](Vertex a, Vertex b) {
      return a.degree != b.degree ? a.degree < b.degree : a.slot < b.slot;
    });
    std::array<int, slots> rank;
    Bits candidates{};
    for (int i = 0; i < size; ++i) {
      slot_[i] = order[i].slot;
      rank[slot_[i]] = i;
      candidates[i / 64] |= Word{1} << (i % 64);
    }
    for (int i = 0; i < size; ++i) {
      const Bits& attacked = conflicts[axis.cell[slot_[i]]][0];
      for (int w = 0; w < 3; ++w) for (Word bits = attacked[w]; bits; bits &= bits - 1) {
        const int other = rank[64 * w + __builtin_ctzll(bits)];
        blocked_[i][other / 64] |= Word{1} << (other % 64);
      }
    }
    expand<false>(0, candidates, save);
    expand<true>(0, candidates, save);
    return best_;
  }

 private:
  struct Colored { std::uint8_t vertex, color; };

  template <bool Emit, class Save>
  void expand(int depth, Bits pool, Save& save) {
    std::array<Colored, maxCells> order;
    Bits remaining = pool;
    int size = 0;
    for (int color = 1; remaining[0] | remaining[1] | remaining[2]; ++color) {
      Bits group = remaining;
      for (int w = 0; w < 3; ++w) while (group[w]) {
        const Word bit = group[w] & -group[w];
        const int v = 64 * w + __builtin_ctzll(bit);
        order[size++] = {static_cast<std::uint8_t>(v), static_cast<std::uint8_t>(color)};
        remaining[w] ^= bit;
        group[w] ^= bit;
        for (int j = w; j < 3; ++j) group[j] &= blocked_[v][j];
      }
    }
    while (size) {
      const auto [v, color] = order[--size];
      if (depth + color < best_ + (Emit ? 0 : 1)) break;
      pool[v / 64] &= ~(Word{1} << (v % 64));
      Bits next;
      for (int j = 0; j < 3; ++j) next[j] = pool[j] & ~blocked_[v][j];
      const int slot = slot_[v];
      const Word queen = Word{1} << (slot % 64);
      if constexpr (Emit) placed_[slot / 64] ^= queen;
      if (next[0] | next[1] | next[2]) {
        expand<Emit>(depth + 1, next, save);
      } else if constexpr (Emit) {
        if (depth + 1 == best_) save(placed_);
      } else {
        best_ = std::max(best_, depth + 1);
      }
      if constexpr (Emit) placed_[slot / 64] ^= queen;
    }
  }

  int best_ = 0;
  std::array<int, maxCells> slot_;
  std::array<Bits, maxCells> blocked_{};
  Bits placed_{};
};

class SegmentSearch {
 public:
  Solutions run(int n, const std::vector<std::string>& grid) {
    std::array<Lines, 4> lines;
    constexpr int dr[] = {0, 1, 1, 1}, dc[] = {1, 0, 1, -1};
    auto open = [&](int r, int c) {
      return r >= 0 && c >= 0 && r < n && c < n && grid[r][c] == '.';
    };
    for (int d = 0; d < 4; ++d) {
      for (int r = 0; r < n; ++r) for (int c = 0; c < n; ++c) {
        if (!open(r, c) || open(r - dr[d], c - dc[d])) continue;
        auto& seg = lines[d].segments[lines[d].count++];
        for (int i = r, j = c; open(i, j); i += dr[d], j += dc[d])
          seg.cells[seg.size++] = i * n + j;
      }
    }
    std::array<int, 4> order{0, 1, 2, 3};
    std::sort(order.begin(), order.end(), [&](int a, int b) {
      return lines[a].count != lines[b].count ? lines[a].count < lines[b].count : a < b;
    });
    if (lines[order[0]].count == 0) return {Board{}};
    for (int a = 0; a < 2; ++a) axes_[a].pack(lines[order[a]]);

    Candidates initial{};
    for (int d = 0; d < 4; ++d) {
      for (int s = 0; s < lines[d].count; ++s) {
        const Segment& seg = lines[d].segments[s];
        Candidates members{};
        for (int a = 0; a < 2; ++a) {
          for (int j = 0; j < seg.size; ++j) {
            const int slot = axes_[a].slot[seg.cells[j]];
            members[a][slot / 64] |= Word{1} << (slot % 64);
          }
        }
        for (int a = 0; a < 2; ++a) {
          if (d == order[a]) {
            const int end = axes_[a].slot[seg.cells[seg.size - 1]];
            axes_[a].crossMask[end] = members[1 - a];
          }
          for (int w = 0; w < 3; ++w) initial[a][w] |= members[a][w];
          for (int j = 0; j < seg.size; ++j) {
            auto& attacked = conflicts_[seg.cells[j]][a];
            for (int w = 0; w < 3; ++w) attacked[w] |= members[a][w];
          }
        }
      }
    }
    if (lines[order[0]].count >= 2 * n) {
      CliqueSearch dense;
      auto save = [&](const Bits& placement) { solutions_.add(placement); };
      const int best = dense.run(axes_[0], initial[0], conflicts_, save);
      return materialize(n, best);
    }
    // One queen per segment bounds the optimum. The first attainable
    // target yields every maximum board without retaining smaller ones.
    for (int target = lines[order[0]].count; target > 0; --target) {
      search(initial, target);
      if (solutions_.size()) return materialize(n, target);
    }
    return {Board{}};
  }

 private:
  struct Choice {
    int axis = 0, end = 0, size = maxSide + 1;
    Word options = 0;
  };

  void select(int a, const Bits& free, const Bits& active, Choice& best) const {
    for (int w = 0; w < 3; ++w) {
      for (Word ends = active[w]; ends; ends &= ends - 1) {
        const int end = 64 * w + __builtin_ctzll(ends);
        const Word options = free[w] & axes_[a].segmentMask[end];
        const int size = __builtin_popcountll(options);
        if (size < best.size) {
          best = {a, end, size, options};
          if (size == 1) return;
        }
      }
    }
  }

  void search(const Candidates& free, int need) {
    if (need == 1) {
      for (int w = 0; w < 3; ++w) for (Word cells = free[0][w]; cells; cells &= cells - 1) {
        const Word queen = cells & -cells;
        placed_[w] ^= queen;
        solutions_.add(placed_);
        placed_[w] ^= queen;
      }
      return;
    }
    std::array<Bits, 2> active;
    const int h = axes_[0].activeSegments(free[0], active[0]);
    if (h < need) return;
    const int v = axes_[1].activeSegments(free[1], active[1]);
    if (v < need) return;

    Choice best;
    const int primary = h <= v ? 0 : 1;
    select(primary, free[primary], active[primary], best);
    if (h == v && best.size > 1) select(1 - primary, free[1 - primary], active[1 - primary], best);
    const int axis = best.axis, word = best.end / 64;
    for (Word choices = best.options; choices; choices &= choices - 1) {
      const int cell = axes_[axis].cell[word * 64 + __builtin_ctzll(choices)];
      const int slot = axes_[0].slot[cell];
      const Word queen = Word{1} << (slot % 64);
      Candidates next;
      for (int a = 0; a < 2; ++a) for (int w = 0; w < 3; ++w)
        next[a][w] = free[a][w] & ~conflicts_[cell][a][w];
      placed_[slot / 64] ^= queen;
      search(next, need - 1);
      placed_[slot / 64] ^= queen;
    }
    if ((axis == 0 ? h : v) > need) {
      Candidates skipped = free;
      skipped[axis][word] &= ~best.options;
      for (int w = 0; w < 3; ++w) skipped[1 - axis][w] &= ~axes_[axis].crossMask[best.end][w];
      search(skipped, need);
    }
  }

  Solutions materialize(int n, int target) {
    Solutions result;
    result.reserve(solutions_.size());
    solutions_.consume([&](const Bits& placement) {
      auto& board = result.emplace_back();
      board.reserve(target);
      for (int w = 0; w < 3; ++w) {
        for (Word bits = placement[w]; bits; bits &= bits - 1) {
          const int cell = axes_[0].cell[64 * w + __builtin_ctzll(bits)];
          board.emplace_back(cell / n, cell % n);
        }
      }
    });
    return result;
  }

  std::array<Axis, 2> axes_;
  std::array<Candidates, maxCells> conflicts_{};
  Bits placed_{};
  Batches<Bits, 64> solutions_;
};

}  // namespace

std::vector<std::vector<std::pair<int, int>>> solveWalled(
    int n, const std::vector<std::string>& grid) {
  if (n < 4 || n > maxSide || static_cast<int>(grid.size()) != n) return {};
  bool hasWalls = false;
  for (const auto& row : grid) {
    if (static_cast<int>(row.size()) != n) return {};
    if (row.find('W') != std::string::npos) hasWalls = true;
  }
  if (!hasWalls) {
    OrbitSearch empty{};
    empty.n = n;
    empty.board = (Mask{1} << n) - 1;
    return empty.run();
  }
  FullLineSearch lines;
  auto full = lines.run(n, grid);
  if (!full.empty()) return full;
  SegmentSearch search;
  return search.run(n, grid);
}
