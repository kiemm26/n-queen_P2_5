#include <algorithm>
#include <array>
#include <bitset>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include "nqueens.h"

namespace {
using Mask = std::uint32_t;
using Word = std::uint64_t;
using Results = std::vector<std::vector<int>>;
int columnIndex(Mask q) {
  return static_cast<int>(std::bitset<32>(q - 1).count());
}
Mask reverseMask(Mask m, int n) {
  m = ((m & 0x5555u) << 1) | ((m >> 1) & 0x5555u);
  m = ((m & 0x3333u) << 2) | ((m >> 2) & 0x3333u);
  m = ((m & 0x0f0fu) << 4) | ((m >> 4) & 0x0f0fu);
  return (((m & 255u) << 8) | (m >> 8)) >> (16 - n);
}

// At most 6! permutations can use a fixed set of six columns.
template <int Height> struct HalfLayouts {
  static_assert(Height >= 4 && Height <= 6);
  static constexpr unsigned capacity =
      Height == 4 ? 24 : (Height == 5 ? 120 : 720);
  Mask diagonals[capacity];
  Mask paths[capacity];
  unsigned size;
};

// Release completed blocks while expanding boards, so the result and the full
// staging allocation do not reach their peaks simultaneously.
template <int N> struct PendingBoards {
  struct Block {
    std::unique_ptr<Block> previous;
    std::array<Word, (N < 8 ? 4 : (N < 10 ? 16 : 128))> paths;
    unsigned used = 0;
  };
  std::unique_ptr<Block> last;
  std::size_t count = 0, total = 0;
  Word firstPath = 0;
  std::array<std::array<unsigned char, N>, N> orientations;
  explicit PendingBoards(const std::vector<std::string> &grid) {
    for (int r = 0; r < N; ++r)
      for (int c = 0; c < N; ++c) {
        orientations[r][c] = (grid[r][c] == '.') |
                             ((grid[r][N - 1 - c] == '.') << 1) |
                             ((grid[N - 1 - r][c] == '.') << 2) |
                             ((grid[N - 1 - r][N - 1 - c] == '.') << 3);
      }
  }
  void add(Word path) {
    // Only emit reflections that avoid every hole on this particular board.
    Mask images = path >> (4 * N);
    Word cols = path;
    for (int r = 0; r < N && images; ++r, cols >>= 4)
      images &= orientations[r][cols & 15];
    if (!images)
      return;
    total += std::bitset<4>(images).count();
    path = (path & ((Word{1} << (4 * N)) - 1)) | (Word(images) << (4 * N));
    if (count == 0) {
      firstPath = path;
      ++count;
      return;
    }
    if (!last || last->used == last->paths.size()) {
      std::unique_ptr<Block> next(new Block);
      next->previous = std::move(last);
      last = std::move(next);
    }
    last->paths[last->used++] = path;
    ++count;
  }
  // Bits 0, 1, 2, 3 select identity, column reflection, row reflection,
  // and both reflections respectively.
  Results expand() {
    Results results;
    results.reserve(total);
    auto emit = [&](Word hit) {
      Word path = hit;
      auto images = hit >> (4 * N);
      std::array<int, N> rows;
      for (auto &c : rows) {
        c = path & 15;
        path >>= 4;
      }
      if (images & 1)
        results.emplace_back(rows.begin(), rows.end());
      if (images & 4)
        results.emplace_back(rows.rbegin(), rows.rend());
      if (images & 10) {
        for (auto &c : rows)
          c = N - 1 - c;
        if (images & 2)
          results.emplace_back(rows.begin(), rows.end());
        if (images & 8)
          results.emplace_back(rows.rbegin(), rows.rend());
      }
    };
    while (last) {
      for (unsigned i = 1; i < last->used; ++i)
        emit(last->paths[i]);
      const Word finalPath = last->paths[0];
      auto previous = std::move(last->previous);
      last = std::move(previous);
      emit(finalPath);
    }
    if (count)
      emit(firstPath);
    return results;
  }
};
template <int N> struct StripSearch {
  // Even and odd bits below bit 36 encode the two diagonal directions.
  // Columns occupy four-bit fields above bit 35. The path fields of adjacent
  // strips are disjoint, so a single AND tests their diagonal conflicts.
  struct Layout {
    Word first;
    std::array<Word, 1 + ((N - N / 2) / 2 - (N / 2) / 2)> shifted;
    template <int Height> Word second() const {
      return shifted[Height / 2 - N / 4];
    }
  };
  struct StripGroup {
    Layout items[6];
    unsigned size;
  };
  static constexpr Mask board = (1u << N) - 1;
  // Index references only populated groups, with at most C(N,2)+C(N,3) entries.
  static_assert((N == 12 ? 0 : N * (N - 1) / 2) + N * (N - 1) * (N - 2) / 6 <=
                255);
  std::array<std::uint8_t, 1u << N> index;
  std::array<StripGroup,
             1 + (N == 12 ? 0 : N * (N - 1) / 2) + N *(N - 1) * (N - 2) / 6>
      groups;
  unsigned groupCount = 0;
  HalfLayouts<N / 2> top;
  HalfLayouts<N - N / 2> bottom;
  static constexpr unsigned digits = (2 * (N - 1) + 3) / 4;
  std::array<Word, 32> planes;
  Word blocked[digits][16];

  void prepareShortStrips() {
    std::array<Word, N> ray;
    for (int c = 0; c < N; ++c)
      ray[c] = (Word{1} << (2 * c)) | (Word{2} << (2 * (N - 1 - c)));
    auto store = [](StripGroup &group, Word attacks, Mask columns) {
      auto &item = group.items[group.size++];
      const Word path = Word(columns) << 36;
      item.first = attacks | path;
      for (unsigned i = 0; i < item.shifted.size(); ++i) {
        const int offset = N / 4 + i;
        item.shifted[i] = (attacks << (2 * offset)) | (path << (4 * offset));
      }
    };
    for (int a = 0; a < N; ++a)
      for (int b = a + 1; b < N; ++b) {
        const Mask pair = (1u << a) | (1u << b);
        if constexpr (N != 12) {
          index[pair] = ++groupCount;
          auto &two = groups[groupCount];
          two.size = 0;
          if (b - a > 1) {
            store(two, ray[a] | (ray[b] << 2), a | (b << 4));
            store(two, ray[b] | (ray[a] << 2), b | (a << 4));
          }
        }
        for (int c = b + 1; c < N; ++c) {
          index[pair | (1u << c)] = ++groupCount;
          auto &three = groups[groupCount];
          three.size = 0;
          auto middle = [&](int left, int mid, int right) {
            if (right - left == 2 || std::abs(left - mid) == 1 ||
                std::abs(right - mid) == 1)
              return;
            const Word center = ray[mid] << 2;
            store(three, ray[left] | center | (ray[right] << 4),
                  left | (mid << 4) | (right << 8));
            store(three, ray[right] | center | (ray[left] << 4),
                  right | (mid << 4) | (left << 8));
          };
          middle(a, b, c);
          middle(b, a, c);
          middle(a, c, b);
        }
      }
  }

  template <int Height>
  void buildHalf(Mask set, HalfLayouts<Height> &result, int shift) {
    result.size = 0;
    // For equal-sized strips keep one order of the column partition.
    // Reversing a joined layout below recovers the opposite order.
    Mask subset = set;
    if constexpr (Height % 2 == 0)
      subset ^= set & -set;
    for (Mask first = subset; first; first &= first - 1) {
      for (Mask second = first & (first - 1); second; second &= second - 1) {
        Mask a = (first & -first) | (second & -second);
        if constexpr (Height == 6)
          a ^= subset;
        const auto &upperGroup = groups[index[a]];
        const auto &lowerGroup = groups[index[set ^ a]];
        for (unsigned j = 0; j < lowerGroup.size; j += 2) {
          for (unsigned i = 0; i < upperGroup.size; i += 2) {
            auto combine = [&](unsigned upper, unsigned lower) {
              const Word u = upperGroup.items[upper].first;
              const Word l = lowerGroup.items[lower].template second<Height>();
              const unsigned k = result.size;
              const bool valid = (u & l) == 0;
              constexpr Mask crossing = (1u << (2 * (N - 1))) - 1;
              const Word joined = u | l;
              result.diagonals[k] = Mask(joined >> (2 * shift)) & crossing;
              result.paths[k] = joined >> 36;
              if constexpr (Height % 2 == 0) {
                Word rev =
                    lowerGroup.items[lower ^ 1].first |
                    upperGroup.items[upper ^ 1].template second<Height>();
                result.diagonals[k + 1] = Mask(rev >> (2 * shift)) & crossing;
                result.paths[k + 1] = rev >> 36;
                result.size += 2 * unsigned(valid);
              } else
                result.size += unsigned(valid);
            };
            combine(i, j);
            combine(i + 1, j);
            combine(i, j + 1);
            combine(i + 1, j + 1);
          }
        }
      }
    }
  }

  // Next mask in numeric order having the same number of set bits.
  static Mask nextSet(Mask set) {
    const Mask bit = set & -set;
    const Mask carry = set + bit;
    return carry | (((carry ^ set) >> 2) >> columnIndex(bit));
  }

  void indexUpperDiagonals(unsigned start, unsigned length) {
    planes.fill(0);
    for (unsigned lane = 0; lane < std::min(length, 32u); ++lane)
      planes[lane] = top.diagonals[start + lane];
    for (unsigned lane = 32; lane < length; ++lane)
      planes[lane - 32] |= Word(top.diagonals[start + lane]) << 32;
    // Transpose two 32-by-32 bit matrices in parallel. Plane d then
    // contains one bit per upper layout that occupies diagonal d.
    auto transpose = [&](auto width, Word mask) {
      for (unsigned base = 0; base < 32; base += 2 * width) {
        for (unsigned k = base; k < base + width; ++k) {
          Word swap = ((planes[k] >> width) ^ planes[k + width]) & mask;
          planes[k] ^= swap << width;
          planes[k + width] ^= swap;
        }
      }
    };
    transpose(1, 0x5555555555555555ull);
    transpose(2, 0x3333333333333333ull);
    transpose(4, 0x0f0f0f0f0f0f0f0full);
    transpose(8, 0x00ff00ff00ff00ffull);
    transpose(16, 0x0000ffff0000ffffull);
    // A lower layout selects one entry per four diagonal bits. Taking
    // their union excludes every upper layout with a crossing attack.
    for (unsigned digit = 0; digit < digits; ++digit) {
      auto &table = blocked[digit];
      table[0] = 0;
      for (unsigned bit = 0; bit < 4; ++bit) {
        for (unsigned m = 0; m < (1u << bit); ++m)
          table[(1u << bit) + m] = table[m] | planes[4 * digit + bit];
      }
    }
  }

  void run(PendingBoards<N> &pending) {
    prepareShortStrips();
    for (Mask a = (1u << (N / 2)) - 1; a < board; a = nextSet(a)) {
      Mask b = board ^ a, m = reverseMask(a, N);
      if (m < a)
        continue;
      Mask images = a == m ? 1 : 3;
      if constexpr (N % 2 == 0) {
        if (b < a || (board ^ m) < a)
          continue;
        if (b != m)
          images |= (b == (board ^ m) ? 4 : 12);
      }
      // On odd boards the middle queen belongs to the lower strip. Include
      // its column in each key so reflected partitions remain distinct.
      std::array<Mask, N> centerImages{};
      if constexpr (N % 2) {
        bool any = false;
        for (Mask cells = b; cells; cells &= cells - 1) {
          Mask q = cells & -cells;
          Mask lower = b ^ q, revLower = reverseMask(lower, N);
          int c = columnIndex(q), rev = N - 1 - c;
          const Mask key = (a << 4) | Mask(c);
          unsigned imageMask = 0;
          if (key <= ((m << 4) | Mask(rev)) && a < lower &&
              key <= ((revLower << 4) | Mask(rev))) {
            imageMask = 15;
            if (c == rev) {
              imageMask = a == m ? 1 : 3;
              if (lower != m)
                imageMask |= (lower == revLower ? 4 : 12);
            }
          }
          centerImages[c] = imageMask;
          any |= imageMask != 0;
        }
        if (!any)
          continue;
      }
      buildHalf<N / 2>(a, top, N / 2);
      buildHalf<N - N / 2>(b, bottom, 0);
      // One orientation per distinct reflected partition; expand only after
      // the search workspace has been released.

      for (unsigned start = 0; start < top.size; start += 64) {
        const unsigned length = std::min(64u, top.size - start);
        const Word valid = ~Word{0} >> (64 - length);
        if (length <= 8 || bottom.size <= 4) {
          for (unsigned j = 0; j < bottom.size; ++j) {
            Mask imageMask = images;
            if constexpr (N % 2)
              imageMask = centerImages[bottom.paths[j] & 15];
            if (!imageMask)
              continue;
            const Word prefix = (Word(bottom.paths[j]) << (4 * (N / 2))) |
                                (Word(imageMask) << (4 * N));
            for (unsigned i = start; i < start + length; ++i) {
              if (top.diagonals[i] & bottom.diagonals[j])
                continue;
              pending.add(prefix | top.paths[i]);
            }
          }
          continue;
        }
        indexUpperDiagonals(start, length);
        for (unsigned j = 0; j < bottom.size; ++j) {
          Mask imageMask = images;
          if constexpr (N % 2)
            imageMask = centerImages[bottom.paths[j] & 15];
          if (!imageMask)
            continue;
          Mask rays = bottom.diagonals[j];
          Word conflicts = 0;
          for (unsigned digit = 0; digit < digits; ++digit, rays >>= 4)
            conflicts |= blocked[digit][rays & 15];
          Word matches = valid & ~conflicts;
          while (matches) {
            Word bit = matches & -matches;
            unsigned lane = std::bitset<64>(bit - 1).count();
            matches ^= bit;
            pending.add(top.paths[start + lane] |
                        (Word(bottom.paths[j]) << (4 * (N / 2))) |
                        (Word(imageMask) << (4 * N)));
          }
        }
      }
    }
  }
};

template <int N> Results searchStrips(const std::vector<std::string> &grid) {
  PendingBoards<N> pending(grid);
  {
    std::unique_ptr<StripSearch<N>> search(new StripSearch<N>);
    search->run(pending);
  }
  return pending.expand();
}
struct ConstraintSearch {
  int n;
  std::array<Mask, 12> allowed{};
  std::array<int, 12> placement{};
  Results results;

  // The rising diagonal index is column - row + n - 1; the falling one is
  // column + row.
  void search(Mask rows, Mask columns, Mask rising, Mask falling) {
    if (rows == 0) {
      results.emplace_back(placement.begin(), placement.begin() + n);
      return;
    }
    int bestRow = 0;
    int smallest = n + 1;
    Mask choices = 0;
    for (Mask scan = rows; scan; scan &= scan - 1) {
      const int row = columnIndex(scan & -scan);
      const Mask candidates =
          allowed[row] &
          ~(columns | (rising >> (n - 1 - row)) | (falling >> row));
      const int count = std::bitset<32>(candidates).count();
      if (count == 0)
        return;
      if (count < smallest) {
        bestRow = row;
        smallest = count;
        choices = candidates;
      }
    }
    rows ^= Mask{1} << bestRow;
    while (choices) {
      const Mask queen = choices & -choices;
      choices ^= queen;
      placement[bestRow] = columnIndex(queen);
      search(rows, columns | queen, rising | (queen << (n - 1 - bestRow)),
             falling | (queen << bestRow));
    }
  }
};
} // namespace

std::vector<std::vector<int>> solveHoles(int n,
                                         const std::vector<std::string> &grid) {
  if (n < 4 || n > 12 || grid.size() != static_cast<unsigned>(n))
    return {};
  ConstraintSearch dense{};
  dense.n = n;
  int holes = 0;
  for (int r = 0; r < n; ++r) {
    if (grid[r].size() != static_cast<unsigned>(n))
      return {};
    for (int c = 0; c < n; ++c) {
      if (grid[r][c] == '.')
        dense.allowed[r] |= 1u << c;
      else
        ++holes;
    }
    if (!dense.allowed[r])
      return {};
  }
  if (n <= 8 || holes * 4 >= n * n) {
    dense.search((1u << n) - 1, 0, 0, 0);
    return std::move(dense.results);
  }
  switch (n) {
  case 9:
    return searchStrips<9>(grid);
  case 10:
    return searchStrips<10>(grid);
  case 11:
    return searchStrips<11>(grid);
  default:
    return searchStrips<12>(grid);
  }
}
