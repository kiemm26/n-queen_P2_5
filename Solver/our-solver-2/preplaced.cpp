#include <array>
#include <cstdint>

#include "nqueens.h"

std::vector<std::vector<int>> solvePreplaced(
    int n, const std::vector<std::pair<int, int>>& preplaced) {
  if (n < 4 || n > 12) return {};
  using Mask = std::uint32_t;
  const Mask board = (Mask{1} << n) - 1;
  std::array<Mask, 12> allowed;
  allowed.fill(board);
  std::array<Mask, 12> locked{};
  for (const auto& [row, col] : preplaced) {
    if (row < 0 || row >= n || col < 0 || col >= n) return {};
    const Mask queen = Mask{1} << col;
    if (locked[row] != 0 && locked[row] != queen) return {};
    locked[row] = queen;
  }

  // Account for attacks from locked queens in either direction before searching.
  for (int row = 0; row < n; ++row) {
    const Mask queen = locked[row];
    if (queen == 0) continue;
    for (int other = 0; other < n; ++other) {
      if (other == row) continue;
      const int distance = other < row ? row - other : other - row;
      const Mask attacks = queen | (queen << distance) | (queen >> distance);
      if ((locked[other] & attacks) != 0) return {};
      allowed[other] &= ~attacks;
    }
  }
  for (int row = 0; row < n; ++row) {
    if (locked[row] != 0) allowed[row] = locked[row];
    if (allowed[row] == 0) return {};
  }

  std::array<int, 12> placement;
  std::vector<std::vector<int>> solutions;
  auto search = [&](auto&& self, int row, Mask columns, Mask left, Mask right,
                    bool mirror) -> void {
    Mask available = allowed[row] & ~(columns | left | right);
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

  if (preplaced.empty()) {
    for (int col = 0; col <= (n - 1) / 2; ++col) {
      const Mask queen = Mask{1} << col;
      placement[0] = col;
      search(search, 1, queen, queen << 1, queen >> 1, col < n / 2);
    }
  } else {
    search(search, 0, 0, 0, 0, false);
  }
  return solutions;
}
