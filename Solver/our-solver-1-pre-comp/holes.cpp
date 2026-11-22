#include <cstdint>

#include "nqueens.h"

std::vector<std::vector<int>> solveHoles(int n, const std::vector<std::string>& grid) {
  using Mask = std::uint64_t;
  if (n < 0 || n > 64) return {};
  std::vector<Mask> openCells(n, 0);
  for (int row = 0; row < n; ++row) {
    for (int col = 0; col < n; ++col) {
      if (grid[row][col] == '.') openCells[row] |= Mask{1} << col;
    }
    if (openCells[row] == 0) return {};
  }

  std::vector<int> queenColumns(n);
  std::vector<std::vector<int>> solutions;
  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right) -> void {
    if (row == n) {
      solutions.push_back(queenColumns);
      return;
    }

    // Holes restrict placement only. Attacks still shift through them.
    Mask available = openCells[row] & ~(columns | left | right);
    while (available != 0) {
      const Mask queen = available & -available;
      available ^= queen;
      queenColumns[row] = __builtin_ctzll(queen);
      self(self, row + 1, columns | queen, (left | queen) << 1, (right | queen) >> 1);
    }
  };
  search(search, 0, 0, 0, 0);
  return solutions;
}
