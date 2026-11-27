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

std::vector<Board> solveWithoutWalls(int n) {
  using Mask = std::uint32_t;
  const Mask board = (Mask{1} << n) - 1;
  std::array<Cell, maxSide> placement;
  std::vector<Board> solutions;
  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right,
                    bool mirror) -> void {
    Mask available = board & ~(columns | left | right);
    if (row == n - 1) {
      if (available == 0) return;
      placement[row] = {row, __builtin_ctz(available)};
      solutions.emplace_back(placement.begin(), placement.begin() + n);
      if (mirror) {
        auto& reflected = solutions.emplace_back(n);
        for (int i = 0; i < n; ++i) reflected[i] = {i, n - 1 - placement[i].second};
      }
      return;
    }
    while (available != 0) {
      const Mask queen = available & -available;
      available ^= queen;
      placement[row] = {row, __builtin_ctz(queen)};
      self(self, row + 1, columns | queen, (left | queen) << 1, (right | queen) >> 1, mirror);
    }
  };
  for (int col = 0; col <= (n - 1) / 2; ++col) {
    const Mask queen = Mask{1} << col;
    placement[0] = {0, col};
    search(search, 1, queen, queen << 1, queen >> 1, col < n / 2);
  }
  return solutions;
}

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

    expand(0, candidates);
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

  void expand(int depth, Vertices candidates) {
    if (depth + candidates.count() < bestSize_) return;
    auto& order = orders_[depth];
    for (int i = colorCandidates(candidates, order) - 1; i >= 0; --i) {
      // A clique can use at most one vertex per color, and ties must be kept.
      if (depth + order[i].color < bestSize_) return;
      const int vertex = order[i].vertex;
      candidates.remove(vertex);
      placement_[depth] = cells_[vertex];
      const Vertices next = candidates.intersect(neighbors_[vertex]);
      if (next.any()) {
        expand(depth + 1, next);
      } else if (depth + 1 >= bestSize_) {
        if (depth + 1 > bestSize_) {
          bestSize_ = depth + 1;
          solutions_.clear();
        }
        solutions_.emplace_back(placement_.begin(), placement_.begin() + depth + 1);
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
  if (noWalls) return solveWithoutWalls(n);
  CliqueSearch search;
  return search.run(n, grid);
}
