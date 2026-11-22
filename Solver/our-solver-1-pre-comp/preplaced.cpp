#include <cstdint>

#include "nqueens.h"

std::vector<std::vector<int>> solvePreplaced(
    int n, const std::vector<std::pair<int, int>>& preplaced) {
  using Mask = std::uint64_t;
  if (n < 0 || n > 64) return {};
  const Mask boardMask = n == 64 ? ~Mask{0} : (Mask{1} << n) - 1;
  std::vector<Mask> allowed(n, boardMask);
  for (const auto& [row, col] : preplaced) {
    if (row < 0 || row >= n || col < 0 || col >= n) return {};
    const Mask queen = Mask{1} << col;
    if ((allowed[row] & queen) == 0) return {};
    allowed[row] = queen;
  }

  std::vector<int> queenColumns(n);
  std::vector<std::vector<int>> solutions;
  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right) -> void {
    if (row == n) {
      solutions.push_back(queenColumns);
      return;
    }

    Mask available = allowed[row] & ~(columns | left | right);
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
