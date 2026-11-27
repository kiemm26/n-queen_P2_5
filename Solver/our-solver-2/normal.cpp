#include <array>
#include <cstdint>

#include "nqueens.h"

std::vector<std::vector<int>> solveNQueens(int n) {
  if (n < 4 || n > 12) return {};
  using Mask = std::uint32_t;
  const Mask board = (Mask{1} << n) - 1;
  std::array<int, 12> placement;
  std::vector<std::vector<int>> solutions;

  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right,
                    bool mirror) -> void {
    Mask available = board & ~(columns | left | right);
    if (row == n - 1) {
      // Only one column is left unused, so at most one bit can be set.
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

  // Each noncentral branch has a distinct partner on the other half.
  for (int col = 0; col <= (n - 1) / 2; ++col) {
    const Mask queen = Mask{1} << col;
    placement[0] = col;
    search(search, 1, queen, queen << 1, queen >> 1, col < n / 2);
  }
  return solutions;
}
