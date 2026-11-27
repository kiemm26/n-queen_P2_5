#include <algorithm>
#include <array>
#include <cstdint>

#include "nqueens.h"

namespace {

using Cell = std::pair<int, int>;
using Board = std::vector<Cell>;
constexpr int maxSide = 12;
constexpr int maxCells = maxSide * maxSide;

struct Vertices {
  using Word = std::uint64_t;
  std::array<Word, 3> bits{};

  void add(int vertex) { bits[vertex / 64] |= Word{1} << (vertex % 64); }
  void remove(int vertex) { bits[vertex / 64] &= ~(Word{1} << (vertex % 64)); }
  bool any() const { return (bits[0] | bits[1] | bits[2]) != 0; }
  int count() const {
    return __builtin_popcountll(bits[0]) + __builtin_popcountll(bits[1]) +
           __builtin_popcountll(bits[2]);
  }

  Vertices intersect(const Vertices& other) const {
    return {{bits[0] & other.bits[0], bits[1] & other.bits[1], bits[2] & other.bits[2]}};
  }

  void exclude(const Vertices& other) {
    bits[0] &= ~other.bits[0];
    bits[1] &= ~other.bits[1];
    bits[2] &= ~other.bits[2];
  }
};

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

using Segments = std::array<std::uint8_t, 4>;

struct Vertex {
  int index;
  int conflicts;
};

class CliqueSearch {
 public:
  std::vector<Board> run(int n, const std::vector<std::string>& grid) {
    std::array<Segments, maxCells> segments;
    std::array<Segments, maxCells> segmentSizes{};
    std::array<Vertex, maxCells> vertices;
    int size = 0;
    // Each free cell inherits its predecessor's segment in four directions.
    constexpr int previousRow[] = {0, -1, -1, -1};
    constexpr int previousCol[] = {-1, 0, -1, 1};
    for (int row = 0; row < n; ++row) {
      for (int col = 0; col < n; ++col) {
        if (grid[row][col] != '.') continue;
        const int index = row * n + col;
        vertices[size++] = {index, 0};
        for (int direction = 0; direction < 4; ++direction) {
          const int r = row + previousRow[direction];
          const int c = col + previousCol[direction];
          const int segment = r >= 0 && c >= 0 && c < n && grid[r][c] == '.'
              ? segments[r * n + c][direction]
              : index;
          segments[index][direction] = segment;
          ++segmentSizes[segment][direction];
        }
      }
    }
    if (size == 0) return {Board{}};

    for (int v = 0; v < size; ++v) {
      for (int direction = 0; direction < 4; ++direction) {
        const int segment = segments[vertices[v].index][direction];
        vertices[v].conflicts += segmentSizes[segment][direction] - 1;
      }
    }
    // Compatibility degree equals the number of other cells minus conflicts.
    std::sort(vertices.begin(), vertices.begin() + size, [](const Vertex& a, const Vertex& b) {
      if (a.conflicts != b.conflicts) return a.conflicts < b.conflicts;
      return a.index < b.index;
    });

    Vertices candidates;
    for (int a = 0; a < size; ++a) {
      candidates.add(a);
      const int index = vertices[a].index;
      cells_[a] = {index / n, index % n};
      const auto& x = segments[index];
      for (int b = 0; b < a; ++b) {
        const auto& y = segments[vertices[b].index];
        if (x[0] == y[0] || x[1] == y[1] || x[2] == y[2] || x[3] == y[3]) continue;
        neighbors_[a].add(b);
        neighbors_[b].add(a);
      }
    }

    // Prove the maximum first, so only optimal boards allocate result storage.
    expand<false>(0, candidates);
    expand<true>(0, candidates);
    return std::move(solutions_);
  }

 private:
  struct ColoredVertex {
    std::uint8_t vertex;
    std::uint8_t color;
  };

  int colorCandidates(Vertices uncolored, std::array<ColoredVertex, maxCells>& order) const {
    int size = 0;
    for (int color = 1; uncolored.any(); ++color) {
      Vertices independent = uncolored;
      for (int word = 0; word < 3; ++word) {
        while (independent.bits[word] != 0) {
          const int vertex = word * 64 + __builtin_ctzll(independent.bits[word]);
          order[size++] = {static_cast<std::uint8_t>(vertex), static_cast<std::uint8_t>(color)};
          uncolored.remove(vertex);
          independent.bits[word] &= independent.bits[word] - 1;
          independent.exclude(neighbors_[vertex]);
        }
      }
    }
    return size;
  }

  template <bool Enumerate>
  void expand(int depth, Vertices candidates) {
    const int required = bestSize_ + (Enumerate ? 0 : 1);
    if (depth + candidates.count() < required) return;
    auto& order = orders_[depth];
    for (int i = colorCandidates(candidates, order) - 1; i >= 0; --i) {
      // A clique can use at most one vertex per color, and ties must be kept.
      if (depth + order[i].color < bestSize_ + (Enumerate ? 0 : 1)) return;
      const int vertex = order[i].vertex;
      candidates.remove(vertex);
      placement_[depth] = cells_[vertex];
      const Vertices next = candidates.intersect(neighbors_[vertex]);
      if (next.any()) {
        expand<Enumerate>(depth + 1, next);
      } else if constexpr (Enumerate) {
        if (depth + 1 == bestSize_)
          solutions_.emplace_back(placement_.begin(), placement_.begin() + depth + 1);
      } else {
        bestSize_ = std::max(bestSize_, depth + 1);
      }
    }
  }

  std::array<Cell, maxCells> cells_;
  std::array<Vertices, maxCells> neighbors_;
  std::array<std::array<ColoredVertex, maxCells>, maxCells> orders_;
  std::array<Cell, maxCells> placement_;
  int bestSize_ = 0;
  std::vector<Board> solutions_;
};

}  // namespace

std::vector<std::vector<std::pair<int, int>>> solveWalled(
    int n, const std::vector<std::string>& grid) {
  if (n < 4 || n > maxSide) return {};
  const bool noWalls = std::all_of(grid.begin(), grid.end(), [](const std::string& row) {
    return row.find('W') == std::string::npos;
  });
  if (noWalls) {
    OrbitSearch empty{};
    empty.n = n;
    empty.board = (Mask{1} << n) - 1;
    return empty.run();
  }
  CliqueSearch search;
  return search.run(n, grid);
}
