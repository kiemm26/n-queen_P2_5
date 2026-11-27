#include <array>
#include <cstdint>

#include "nqueens.h"

std::vector<std::vector<int>> solveHoles(int n, const std::vector<std::string>& grid) {
  if (n < 4 || n > 12) return {};
  using Mask = std::uint32_t;
  const Mask board = (Mask{1} << n) - 1;
  std::array<Mask, 12> openCells;
  Mask openColumns = 0;
  bool symmetric = true;
  for (int row = 0; row < n; ++row) {
    Mask open = 0;
    Mask reflected = 0;
    for (int col = 0; col < n; ++col) {
      const Mask free = grid[row][col] == '.';
      open |= free << col;
      reflected = (reflected << 1) | free;
    }
    if (open == 0) return {};
    openCells[row] = open;
    openColumns |= open;
    symmetric &= open == reflected;
  }
  if (openColumns != board) return {};

  std::array<int, 12> placement;
  std::vector<std::vector<int>> solutions;
  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right,
                    bool mirror) -> void {
    // Holes only restrict placement. Attacks still shift through them.
    Mask available = openCells[row] & ~(columns | left | right);
    if (row == n - 1) {
      if (available == 0) return;
      placement[row] = __builtin_ctz(available);
      solutions.emplace_back(placement.begin(), placement.begin() + n);
      if (mirror) {
        auto& reflected = solutions.emplace_back(n);
        for (int i = 0; i < n; ++i) reflected[i] = n - 1 - placement[i];
      }
      return;
    }
    while (available != 0) {
      const Mask queen = available & -available;
      available ^= queen;
      placement[row] = __builtin_ctz(queen);
      self(self, row + 1, columns | queen, (left | queen) << 1, (right | queen) >> 1, mirror);
    }
  };

  if (symmetric) {
    for (int col = 0; col <= (n - 1) / 2; ++col) {
      const Mask queen = Mask{1} << col;
      if ((openCells[0] & queen) == 0) continue;
      placement[0] = col;
      search(search, 1, queen, queen << 1, queen >> 1, col < n / 2);
    }
  } else {
    search(search, 0, 0, 0, 0, false);
  }
  return solutions;
}
