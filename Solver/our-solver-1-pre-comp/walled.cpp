#include <cstdint>
#include <cstdlib>

#include "nqueens.h"

namespace {

using Board = std::vector<std::pair<int, int>>;
using Word = std::uint64_t;
using Vertices = std::vector<Word>;
constexpr int wordBits = 64;

void add(Vertices& vertices, int vertex) {
  vertices[vertex / wordBits] |= Word{1} << (vertex % wordBits);
}

void remove(Vertices& vertices, int vertex) {
  vertices[vertex / wordBits] &= ~(Word{1} << (vertex % wordBits));
}

bool attack(const Board::value_type& a, const Board::value_type& b,
            const std::vector<std::string>& grid) {
  const int rowDelta = b.first - a.first;
  const int colDelta = b.second - a.second;
  if (rowDelta != 0 && colDelta != 0 && std::abs(rowDelta) != std::abs(colDelta)) {
    return false;
  }

  const int rowStep = (rowDelta > 0) - (rowDelta < 0);
  const int colStep = (colDelta > 0) - (colDelta < 0);
  for (int row = a.first + rowStep, col = a.second + colStep;
       row != b.first || col != b.second; row += rowStep, col += colStep) {
    if (grid[row][col] == 'W') return false;
  }
  return true;
}

class CliqueSearch {
 public:
  CliqueSearch(int n, const std::vector<std::string>& grid) {
    for (int row = 0; row < n; ++row) {
      for (int col = 0; col < n; ++col) {
        if (grid[row][col] == '.') cells_.emplace_back(row, col);
      }
    }

    wordCount_ = (cells_.size() + wordBits - 1) / wordBits;
    neighbors_.resize(cells_.size(), Vertices(wordCount_));
    for (int a = 0; a < static_cast<int>(cells_.size()); ++a) {
      for (int b = a + 1; b < static_cast<int>(cells_.size()); ++b) {
        if (!attack(cells_[a], cells_[b], grid)) {
          add(neighbors_[a], b);
          add(neighbors_[b], a);
        }
      }
    }

    // Each depth reuses its buffers across sibling branches.
    frames_.reserve(cells_.size() + 1);
    for (std::size_t depth = 0; depth <= cells_.size(); ++depth) {
      frames_.emplace_back(wordCount_);
    }
  }

  std::vector<Board> run() {
    if (cells_.empty()) return {Board{}};
    for (int vertex = 0; vertex < static_cast<int>(cells_.size()); ++vertex) {
      add(frames_[0].candidates, vertex);
    }
    expand(0);
    return std::move(solutions_);
  }

 private:
  struct ColoringEntry {
    int vertex;
    int color;
  };

  struct Frame {
    explicit Frame(std::size_t wordCount)
        : candidates(wordCount), uncolored(wordCount), colorClass(wordCount) {}

    Vertices candidates;
    Vertices uncolored;
    Vertices colorClass;
    std::vector<ColoringEntry> order;
  };

  void colorCandidates(Frame& frame) {
    frame.order.clear();
    frame.uncolored = frame.candidates;
    for (int color = 1; ; ++color) {
      frame.colorClass = frame.uncolored;
      bool usedColor = false;
      for (std::size_t word = 0; word < wordCount_; ++word) {
        while (frame.colorClass[word] != 0) {
          const int vertex = word * wordBits + __builtin_ctzll(frame.colorClass[word]);
          frame.order.push_back({vertex, color});
          usedColor = true;
          remove(frame.uncolored, vertex);
          remove(frame.colorClass, vertex);
          // A color class contains no adjacent vertices.
          for (std::size_t i = word; i < wordCount_; ++i) {
            frame.colorClass[i] &= ~neighbors_[vertex][i];
          }
        }
      }
      if (!usedColor) return;
    }
  }

  void expand(int depth) {
    Frame& frame = frames_[depth];
    colorCandidates(frame);
    for (auto entry = frame.order.rbegin(); entry != frame.order.rend(); ++entry) {
      // A clique takes at most one vertex per color. Keep branches that can tie.
      if (depth + entry->color < bestSize_) return;
      const int vertex = entry->vertex;
      remove(frame.candidates, vertex);
      current_.push_back(cells_[vertex]);

      Vertices& next = frames_[depth + 1].candidates;
      Word nonempty = 0;
      for (std::size_t i = 0; i < wordCount_; ++i) {
        next[i] = frame.candidates[i] & neighbors_[vertex][i];
        nonempty |= next[i];
      }
      if (nonempty != 0) {
        expand(depth + 1);
      } else if (depth + 1 >= bestSize_) {
        if (depth + 1 > bestSize_) {
          bestSize_ = depth + 1;
          solutions_.clear();
        }
        solutions_.push_back(current_);
      }
      current_.pop_back();
    }
  }

  Board cells_;
  std::size_t wordCount_ = 0;
  std::vector<Vertices> neighbors_;
  std::vector<Frame> frames_;
  Board current_;
  int bestSize_ = 0;
  std::vector<Board> solutions_;
};

}  // namespace

std::vector<std::vector<std::pair<int, int>>> solveWalled(
    int n, const std::vector<std::string>& grid) {
  return CliqueSearch(n, grid).run();
}
