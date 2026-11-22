# Empat solver N-Queens

## Konteks pre-competition

Pre-competition ini berisi empat variasi **N-Queens** untuk papan berukuran `N × N`, dengan **`N = 4, 5, atau 6`**. Di halaman **Send a program**, pilih satu kasus dan unggah satu file `.cpp` untuk kasus itu. Setiap file mendefinisikan fungsi yang akan dipanggil oleh penilai, menggunakan `#include "nqueens.h"`, dan berukuran di bawah 64 KB. Penilai menyediakan `main()` dan meneruskan papan sebagai argumen fungsi. [Kontrak fungsi C++](../../Tester/Harness/nqueens.h)

Dua ratu saling menyerang jika berbagi **baris, kolom, atau diagonal**, selama jalurnya terbuka. Koordinat dimulai dari `0` dan ditulis sebagai `(baris, kolom)`. Keempat kasus meminta **semua susunan** yang memenuhi targetnya, masing-masing muncul satu kali.

| Kasus | Target | Aturan tambahan | File |
| --- | --- | --- | --- |
| **Normal** | Semua susunan tepat `N` ratu | Papan kosong. | [normal.cpp](normal.cpp) |
| **Pre-placed** | Semua susunan tepat `N` ratu | Setiap ratu yang sudah dipasang wajib dipertahankan. | [preplaced.cpp](preplaced.cpp) |
| **Holes** | Semua susunan tepat `N` ratu | Sel `H` dilarang ditempati. Serangan tetap menembusnya. | [holes.cpp](holes.cpp) |
| **Walled** | Semua susunan dengan jumlah ratu maksimum | Sel `W` dilarang ditempati dan menghentikan serangan yang melintasinya. | [walled.cpp](walled.cpp) |

Untuk **Normal, Pre-placed, dan Holes**, setiap susunan berisi tepat satu ratu per baris. Satu susunan bisa ditulis sebagai `[1, 3, 0, 2]`, yaitu ratu di `(0,1)`, `(1,3)`, `(2,0)`, dan `(3,2)`. Untuk **Walled**, satu baris bisa berisi nol, satu, atau beberapa ratu. Setiap susunan menyimpan pasangan koordinat lengkap.

## 1. Normal

Fungsi `solveNQueens(int n)` menerima ukuran papan dan mengembalikan `std::vector<std::vector<int>>`. Untuk `n = 4`, susunan lengkapnya adalah `[[1, 3, 0, 2], [2, 0, 3, 1]]`. Berikut gambar susunan pertama. [normal.cpp](normal.cpp#L5)

```text
     0 1 2 3
  0  . Q . .
  1  . . . Q
  2  Q . . .
  3  . . Q .
```

### Backtracking per baris

Di baris sekarang, program mencoba setiap kolom yang aman. Setelah menaruh ratu, program lanjut ke baris berikutnya. Jika baris berikutnya kehabisan pilihan, pemanggilan itu selesai dan program kembali untuk mencoba pilihan lain. Jika sudah mencapai `row == n`, program menyalin susunan lengkap ke `solutions`.

Misalnya, jika ratu pertama dipasang di `(0,1)`, maka di baris `1` kolom `1` diserang secara vertikal, sedangkan kolom `0` dan `2` diserang secara diagonal. Kolom `3` menjadi satu-satunya pilihan. Setelah memilih `(1,3)`, pilihan aman berikutnya adalah `(2,0)`, lalu `(3,2)`. Cabang ini menghasilkan `[1, 3, 0, 2]`.

### Pengecekan dengan bitmask

Program menyimpan kumpulan kolom sebagai angka 64-bit. **Bit ke-`c` mewakili kolom `c`.** Untuk papan 4 kolom, `1111` berarti semua kolom ada di papan. Bit paling kanan mewakili kolom `0`.

| Mask | Informasi yang disimpan |
| --- | --- |
| `boardMask` | Kolom yang berada di dalam papan |
| `columns` | Kolom yang sudah ditempati ratu di baris sebelumnya |
| `left`, `right` | Kolom di baris sekarang yang terkena dua arah diagonal |

`columns | left | right` menggabungkan seluruh kolom terlarang. Operator `~` membalik bitnya, lalu `& boardMask` menyisakan kolom aman di dalam papan. [Perhitungan kolom yang tersedia](normal.cpp#L19)

```cpp
Mask available = boardMask & ~(columns | left | right);
```

Di dalam loop, `available & -available` mengambil satu bit aktif paling kanan. `__builtin_ctzll(queen)` menghitung jumlah bit nol di sebelah kanannya untuk mendapatkan indeks kolom. `available ^= queen` menghapus pilihan itu dari daftar yang masih harus dicoba pada baris tersebut.

Untuk baris berikutnya, serangan vertikal tetap di kolom yang sama. Serangan diagonal berpindah satu kolom setiap turun satu baris. Itulah fungsi dari `<< 1` dan `>> 1`. Ketiga mask diteruskan sebagai nilai ke pemanggilan rekursif, sehingga saat kembali, mask milik cabang sebelumnya masih tersedia.

Program menyimpan cabang ketika sudah menempatkan `N` ratu. Setiap susunan memiliki urutan pilihan kolom yang unik dari baris `0` sampai `N-1`, sehingga tidak ada duplikat. Jika dua susunan merupakan hasil pencerminan satu sama lain, keduanya tetap dihitung karena koordinat ratunya berbeda.

## 2. Pre-placed

Fungsi `solvePreplaced(n, preplaced)` menerima ukuran papan dan daftar koordinat ratu terkunci. Ratu terkunci ikut dicantumkan dalam susunan akhir. Untuk `n = 4` dan `preplaced = {{0,1}}`, satu-satunya hasil adalah `[[1, 3, 0, 2]]`. Jika ratu dikunci di `(0,0)`, hasilnya `[]`, karena kedua solusi untuk papan 4×4 memakai kolom `1` atau `2` pada baris `0`. [preplaced.cpp](preplaced.cpp#L5)

Algoritmanya memakai backtracking dan mask serangan seperti Normal. Tambahannya adalah `allowed[row]`, yaitu mask kolom yang diizinkan oleh aturan ratu terkunci untuk tiap baris.

- Pada awalnya, semua baris memiliki mask `1111` untuk `N = 4`.
- Jika ratu di `(0,1)` dikunci, `allowed[0]` menjadi `0010`. Hanya kolom `1` yang boleh dipilih di baris `0`.
- Saat mencari langkah, program mengambil irisan antara kolom yang diizinkan dan kolom yang aman dari ratu sebelumnya. [Pembatasan kolom dalam pencarian](preplaced.cpp#L26)

```cpp
Mask available = allowed[row] & ~(columns | left | right);
```

Jika ratu terkunci berada di baris `3`, program tetap memproses baris `0` sampai `2` lebih dulu. Saat mencapai baris `3`, hanya kolom ratu terkunci yang diizinkan. Jika pilihan dari baris sebelumnya menyerang posisi itu, `available` menjadi nol dan cabang berhenti. Ratu terkunci yang saling menyerang juga menghasilkan daftar kosong. Jika satu baris diberi dua kolom terkunci berbeda, program langsung mengembalikan daftar kosong saat membangun `allowed`.

Dengan daftar `preplaced` kosong, semua `allowed[row]` sama dengan `boardMask`. Pencariannya pun menghasilkan susunan yang sama dengan Normal.

## 3. Holes

Fungsi `solveHoles(n, grid)` menerima papan sebagai `N` string. Karakter `.` adalah sel bebas dan `H` adalah lubang. Targetnya masih tepat `N` ratu. Contoh berikut menaruh lubang di `(0,1)`, sehingga solusi `[1, 3, 0, 2]` gugur dan hasilnya tinggal `[[2, 0, 3, 1]]`. [holes.cpp](holes.cpp#L5)

```text
.H..
....
....
....
```

Sebelum mulai mencari, program membaca `grid` dan membangun `openCells[row]`. Bit kolom hanya dinyalakan jika karakter pada sel itu adalah `.`. Pada contoh di atas, `openCells[0]` adalah `1101` karena bit kolom `1` mati. Jika ada satu baris penuh `H`, program langsung mengembalikan `[]` karena target tepat satu ratu per baris mustahil dipenuhi.

```cpp
Mask available = openCells[row] & ~(columns | left | right);
```

`openCells` dipakai saat memilih tempat berdiri. Saat meneruskan serangan ke baris berikutnya, program tetap menggeser mask diagonal seperti Normal. Jadi ratu di `(0,0)` tetap menyerang `(2,2)` melalui lubang di `(1,1)`. Hal yang sama berlaku untuk serangan dalam satu baris atau kolom. [Pencarian dan pergeseran diagonal](holes.cpp#L24)

## 4. Walled

Fungsi `solveWalled(n, grid)` menerima sel bebas `.` dan dinding `W`. Karena dinding bisa memutus jalur serangan, targetnya adalah **cari jumlah ratu terbesar yang bisa dipasang, lalu kembalikan semua susunan dengan jumlah itu**. Jumlah maksimum bisa lebih kecil, sama, atau lebih besar dari `N`. Misalnya, pada papan 5×5 dengan sel bebas hanya di perpotongan baris `0, 2, 4` dan kolom `0, 2, 4`, kesembilan sel bebas bisa diisi. Setiap pasangan yang segaris dipisahkan dinding. [walled.cpp](walled.cpp#L153)

### Langkah 1 — Ubah papan menjadi graf

Program membuat satu **simpul** untuk setiap sel `.`. Dua simpul dihubungkan dengan **sisi** jika kedua sel tersebut bisa diisi ratu bersamaan. Fungsi `attack` memeriksa apakah kedua ratu akan saling menyerang. [Pemeriksaan jalur dan pembangunan graf](walled.cpp#L21)

1. Jika kedua sel tidak berbagi baris, kolom, atau diagonal, keduanya aman.
2. Jika segaris, program berjalan satu sel sekali dari posisi pertama menuju posisi kedua.
3. Jika ditemukan `W` tepat di antara keduanya, serangan terputus. Jika jalurnya terbuka sampai posisi kedua, kedua ratu saling menyerang.

Sekumpulan simpul yang **setiap pasangannya terhubung** disebut *clique*. Satu clique mewakili satu susunan ratu yang aman. Maka mencari jumlah ratu terbesar sama dengan mencari clique terbesar atau *maximum clique*.

Contoh papan 4×4 berikut melabeli tiga sel bebas sebagai `A`, `B`, dan `C` agar mudah diikuti. Input aslinya memakai `.` untuk ketiganya.

```text
A B W C
W W W W
W W W W
W W W W
```

`A` dan `B` bersebelahan, sehingga saling menyerang. `C` aman dipasangkan dengan `A` maupun `B` karena ada dinding di kolom `2`. Grafnya memiliki sisi `A–C` dan `B–C`. Ada **dua** susunan maksimum, masing-masing berisi **dua ratu**.

```text
[[(0,0), (0,3)], [(0,1), (0,3)]]
```

### Langkah 2 — Tambah ratu yang kompatibel

Pencarian menyimpan ratu yang sudah dipilih di `current_`. Saat menambahkan satu simpul, kandidat berikutnya diambil dari **irisan kandidat yang tersisa dan tetangga simpul yang baru dipilih**. Kandidat yang sudah tersisa kompatibel dengan seluruh ratu sebelumnya, dan irisan ini memastikan mereka juga kompatibel dengan ratu yang baru. Program menyimpan himpunan tersebut sebagai bitset, sehingga irisan dilakukan dengan operasi `&`. [Ekspansi clique](walled.cpp#L113)

Setiap simpul dihapus dari daftar kandidat pada tingkat itu saat cabangnya diproses. Pada contoh, saat pencarian memilih `C` lebih dulu, ia bisa menemukan `{C,A}` dan `{C,B}`. Setelah cabang `C` selesai, cabang berikutnya di tingkat yang sama tidak dapat memilih `C` lagi. Ini mencegah susunan yang sama muncul ulang melalui urutan pilihan berbeda.

### Langkah 3 — Pangkas cabang dengan pewarnaan graf

Program memakai **branch-and-bound**, yaitu mencari sambil menghitung batas atas jumlah ratu yang masih mungkin dicapai di suatu cabang. Batas itu diperoleh dengan memberi warna pada simpul kandidat. Simpul yang memiliki sisi satu sama lain wajib diberi warna berbeda. Karena semua anggota sebuah clique saling terhubung, sebuah clique hanya bisa mengambil **maksimal satu simpul dari setiap warna**. [Pewarnaan kandidat](walled.cpp#L90)

Pada contoh di atas, `A` dan `B` boleh sewarna karena tidak ada sisi di antara keduanya. `C` mendapat warna kedua. Dua warna memberi batas atas dua ratu. Secara umum, pewarnaan ini memberikan batas yang aman meskipun jumlah ratu yang benar-benar bisa ditambahkan terkadang lebih kecil.

Program mencoba kandidat mulai dari warna tertinggi. Setelah kandidat dari warna yang lebih tinggi diproses, kandidat tersisa mempunyai batas atas sebesar warna pada langkah sekarang. [Syarat pemangkasan](walled.cpp#L118)

```cpp
if (depth + entry->color < bestSize_) return;
```

`depth` adalah jumlah ratu yang sudah dipilih. Jika rekor saat ini `5` dan cabang berisi `3` ratu dengan maksimal `1` tambahan, cabang itu bisa dihentikan karena hanya bisa mencapai `4`. Jika masih mungkin menambah `2`, cabangnya tetap diperiksa karena bisa menghasilkan susunan lain dengan `5` ratu. **Tanda `<` penting** karena kompetisi meminta semua susunan yang menyamai maksimum.

Saat sudah tidak ada kandidat lanjutan, program membandingkan jumlah ratu dengan rekor. Jika lebih besar, hasil lama dibuang dan susunan baru disimpan. Jika sama, susunan baru ditambahkan. Untuk papan yang seluruhnya `W`, hasilnya **`[[]]`** karena ada tepat satu susunan optimal berisi nol ratu. Pada Pre-placed atau Holes yang mustahil diselesaikan, hasilnya **`[]`** karena tidak ada susunan dengan tepat `N` ratu. [Penyimpanan susunan optimal](walled.cpp#L125) · [Papan tanpa sel bebas](walled.cpp#L65)

## Biaya pencarian dan hasil verifikasi

Pada tiga solver pertama, tiap simpul pencarian menghitung kolom aman dengan beberapa operasi bit sekaligus. Program memangkas susunan parsial yang bentrok sebelum mengisi seluruh papan. Batas atas sederhana untuk banyak simpul pencarian adalah `O(N!)`, karena kolom yang sudah dipakai tidak bisa dipakai ulang. Menyimpan `S` solusi membutuhkan `O(S × N)` ruang. Pada Walled, graf dibangun dengan memeriksa setiap pasangan sel bebas. Pencarian clique bisa eksponensial pada kasus terburuk, dan banyaknya susunan optimal juga memengaruhi waktu karena semuanya harus dikembalikan.

Keempat solver dikompilasi ke WebAssembly dan diuji lewat runner tester lokal. Himpunan hasilnya cocok dengan solver contoh pada **110 papan uji**. Normal menghasilkan **2, 10, dan 4 solusi** untuk `N = 4, 5, dan 6`, dan jumlahnya juga cocok dengan README hingga `N = 12`. Perbandingan Walled memeriksa **jumlah ratu maksimum** dan **seluruh susunan** yang mencapai jumlah tersebut. Hasil tes tersembunyi baru diketahui setelah penilaian kompetisi. [Tabel jumlah solusi di README utama](../../README.md#result-statuses)
