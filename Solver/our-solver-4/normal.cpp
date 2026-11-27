#include <array>
#include <cstdint>
#include <utility>

#include "nqueens.h"

namespace {

using Mask = std::uint32_t;
using Diagonals = std::uint64_t;
using Solutions = std::vector<std::vector<int>>;
constexpr int maxSide = 12;
constexpr int maxHalf = maxSide / 2;

// A strip starts at the middle of the board and extends outwards. Its leftmost
// occupied column is translated to zero so shifted column sets share geometry.
template <class Save>
constexpr void visitStrip(int row, Mask free, Mask left, Mask right, Mask columns,
                          Mask plus, Mask minus, Save& save) {
  if (!free) {
    save(columns, (Diagonals{plus} << 32) | minus);
    return;
  }
  Mask choices = free & ~(left | right);
  while (choices) {
    const Mask q = choices & -choices;
    choices ^= q;
    visitStrip(row + 1, free ^ q, (left | q) << 1, (right | q) >> 1,
               columns | (Mask(__builtin_ctz(q)) << (4 * row)),
               plus | (q << row), minus | (q << (maxHalf - 1 - row)), save);
  }
}

template <Mask Shape>
constexpr std::size_t stripCount() {
  std::size_t count = 0;
  auto increment = [&](Mask, Diagonals) { ++count; };
  visitStrip(0, Shape, 0, 0, 0, 0, 0, increment);
  return count;
}

// For each lower strip, index the placements that hit a given diagonal.
// One machine word tests up to 64 possible partners at once.
constexpr int intercepts = maxSide + maxHalf - 1;
struct Block {
  std::uint64_t valid = 0;
  std::array<std::uint64_t, intercepts> plus{}, minus{};

  std::uint64_t compatible(Mask up, Mask down) const {
    std::uint64_t pairs = valid;
    for (up &= (1u << intercepts) - 1; up; up &= up - 1)
      pairs &= ~minus[__builtin_ctz(up)];
    for (; down; down &= down - 1)
      pairs &= ~plus[__builtin_ctz(down)];
    return pairs;
  }
};

template <std::size_t Size>
struct Patterns {
  std::array<Mask, Size> columns{};
  std::array<Diagonals, Size> diagonals{};
  std::array<Block, (Size + 63) / 64> blocks{};
};

template <Mask Shape>
constexpr auto makePatterns() {
  Patterns<stripCount<Shape>()> result{};
  std::size_t i = 0;
  auto store = [&](Mask columns, Diagonals diagonals) {
    result.columns[i] = columns;
    result.diagonals[i] = diagonals;
    auto& block = result.blocks[i / 64];
    const std::uint64_t bit = std::uint64_t{1} << (i % 64);
    block.valid |= bit;
    for (Mask hits = diagonals >> 32; hits; hits &= hits - 1)
      block.plus[__builtin_ctz(hits)] |= bit;
    for (Mask hits = static_cast<Mask>(diagonals); hits; hits &= hits - 1)
      block.minus[__builtin_ctz(hits)] |= bit;
    ++i;
  };
  visitStrip(0, Shape, 0, 0, 0, 0, 0, store);
  return result;
}

template <Mask Shape>
constexpr auto patterns = makePatterns<Shape>();

struct Group {
  const Mask* columns;
  const Diagonals* diagonals;
  const Block* blocks;
  std::size_t size;
};

template <std::size_t Index>
constexpr Group groupFor() {
  constexpr Mask shape = (Index << 1) | 1;
  constexpr int height = __builtin_popcount(shape);
  if constexpr (height < 2 || height > maxHalf || shape >= (1u << (2 * height + 1))) {
    return {};
  } else {
    constexpr auto& p = patterns<shape>;
    return {p.columns.data(), p.diagonals.data(), p.blocks.data(), p.columns.size()};
  }
}

template <std::size_t... I>
constexpr auto makeGroups(std::index_sequence<I...>) {
  return std::array<Group, sizeof...(I)>{groupFor<I>()...};
}

constexpr auto groups = makeGroups(std::make_index_sequence<1u << (maxSide - 1)>{});

Mask mirrorColumns(Mask m, int n) {
  m = ((m & 0x5555u) << 1) | ((m >> 1) & 0x5555u);
  m = ((m & 0x3333u) << 2) | ((m >> 2) & 0x3333u);
  m = ((m & 0x0f0fu) << 4) | ((m >> 4) & 0x0f0fu);
  return (((m & 255u) << 8) | (m >> 8)) >> (16 - n);
}

// Search the smallest top-column set among its reflections. Only emit images
// that have distinct top-column sets. This also handles symmetric sets.
template <int N>
Mask imagesFor(Mask top, Mask bottom) {
  const Mask mirror = mirrorColumns(top, N);
  if (mirror < top) return 0;
  Mask images = top == mirror ? 1u : 3u;
  if constexpr (N % 2 == 0) {
    const Mask otherMirror = ((1u << N) - 1) ^ mirror;
    if (bottom < top || otherMirror < top) return 0;
    if (bottom != mirror) {
      images |= 4u;
      if (otherMirror != bottom) images |= 8u;
    }
  }
  return images;
}

template <int N>
void emit(Solutions& out, Mask top, Mask bottom, Mask images) {
  std::array<int, N> rows;
  for (int r = N / 2 - 1; r >= 0; --r, top >>= 4) rows[r] = top & 15;
  for (int r = N / 2; r < N; ++r, bottom >>= 4) rows[r] = bottom & 15;
  out.emplace_back(rows.begin(), rows.end());
  if (images & 4) out.emplace_back(rows.rbegin(), rows.rend());
  if (images & 2) {
    for (int& col : rows) col = N - 1 - col;
    out.emplace_back(rows.begin(), rows.end());
    if (images & 8) out.emplace_back(rows.rbegin(), rows.rend());
  }
}

template <int N, bool Emit>
std::size_t join(Solutions& out) {
  constexpr Mask board = (1u << N) - 1;
  std::size_t count = 0;
  for (Mask columns = 1; columns < board; ++columns) {
    if (__builtin_popcount(columns) != N / 2) continue;
    const Mask other = board ^ columns;
    const Mask images = imagesFor<N>(columns, other);
    if (!images) continue;
    const int a = __builtin_ctz(columns), b = __builtin_ctz(other);
    const auto& top = groups[(columns >> a) >> 1];
    const auto& bottom = groups[(other >> b) >> 1];
    const int offset = a - b;
    std::size_t pairs = 0;
    for (std::size_t i = 0; i < top.size; ++i) {
      const auto d = top.diagonals[i];
      // A queen k rows above the cut attacks a queen j rows below it
      // if their physical columns differ by k+j+1. Align both intercepts
      // with the bottom strip, exchanging the two diagonal directions.
      const Mask plus = static_cast<Mask>(d >> 32) << (maxHalf + offset);
      const Mask minus = static_cast<Mask>(d) >> (maxHalf - offset);
      for (std::size_t start = 0; start < bottom.size; start += 64) {
        auto matches = bottom.blocks[start / 64].compatible(plus, minus);
        if constexpr (Emit) {
          while (matches) {
            const auto j = start + __builtin_ctzll(matches);
            // Each nibble holds a column. Translation stays inside 0..11.
            emit<N>(out, top.columns[i] + 0x111111u * a,
                    bottom.columns[j] + 0x111111u * b, images);
            matches &= matches - 1;
          }
        } else {
          pairs += __builtin_popcountll(matches);
        }
      }
    }
    if constexpr (!Emit) count += pairs * __builtin_popcount(images);
  }
  return count;
}

template <int N>
Solutions solve() {
  Solutions out;
  out.reserve(join<N, false>(out));
  join<N, true>(out);
  return out;
}

}  // namespace

std::vector<std::vector<int>> solveNQueens(int n) {
  using Solver = Solutions (*)();
  static constexpr std::array<Solver, 9> solvers = {
      solve<4>, solve<5>, solve<6>, solve<7>, solve<8>, solve<9>, solve<10>, solve<11>, solve<12>};
  if (n < 4 || n > maxSide) return {};
  return solvers[n - 4]();
}
