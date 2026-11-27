# N-Queens Solver v4

Folder ini berisi empat solver C++17 untuk papan berukuran 4×4 hingga 12×12. Setiap berkas berdiri sendiri dan bisa dipilih sebagai berkas submission untuk kategori yang sesuai. Tester memanggil fungsi dari `nqueens.h` dengan koordinat berbasis nol.

| Kategori | Berkas dan fungsi | Hasil yang harus dikembalikan |
|---|---|---|
| Normal | [normal.cpp](normal.cpp) · `solveNQueens(n)` | Semua susunan `N` ratu yang saling aman pada papan kosong |
| Pre-placed | [preplaced.cpp](preplaced.cpp) · `solvePreplaced(n, preplaced)` | Semua susunan `N` ratu yang mempertahankan posisi ratu terkunci |
| Holes | [holes.cpp](holes.cpp) · `solveHoles(n, grid)` | Semua susunan `N` ratu di sel bebas. Lubang melarang penempatan dan jalur serangan tetap melewatinya |
| Walled | [walled.cpp](walled.cpp) · `solveWalled(n, grid)` | Semua susunan yang mencapai jumlah ratu maksimum. Dinding melarang penempatan dan memutus serangan |

Normal, Pre-placed, dan Holes memakai satu ratu per baris dan per kolom. Satu solusi adalah vektor panjang `N`. Entri `solution[r]` berisi kolom ratu pada baris `r`. Walled mengembalikan daftar pasangan `(baris, kolom)` untuk setiap susunan. Jika dinding memisahkan dua ratu pada garis yang sama, susunan itu dapat tetap valid. Jumlah ratu optimal untuk Walled bergantung pada letak dinding.

## Ringkasan perbandingan solver

| Kategori | Strategi utama | Kapan strategi paling membantu | Biaya yang perlu diperhatikan |
|---|---|---|---|
| [Normal](#normal) | Bagi papan menjadi dua strip, gabungkan pola dengan bitset, dan bentuk kembali hasil lewat refleksi | Papan besar yang memiliki banyak pasangan strip untuk diperiksa. Satu blok bitset dapat menyaring hingga 64 pola bawah sekaligus | Tabel menambah waktu kompilasi dan ukuran modul. Setiap solusi tetap perlu dibentuk sebagai vektor kolom |
| [Pre-placed](#pre-placed) | Pasang ratu terkunci, pilih baris dengan kandidat paling sedikit, dan manfaatkan kolom yang hanya bisa diisi dari satu baris | Posisi terkunci membuat beberapa baris atau kolom cepat kehilangan pilihan. Satu atau dua baris terakhir ditangani langsung | Memindai kandidat semua baris menambah biaya di setiap simpul. Buffer ringkas 128 solusi pertama membatasi alokasi sementara |
| [Holes](#holes) | Saring pola strip dengan mask lubang. Jika lubang menempati minimal sepertiga papan, gunakan backtracking dengan prioritas baris berkandidat paling sedikit | Tabel strip membantu ketika papan masih punya banyak sel terbuka. Backtracking dapat berhenti lebih awal ketika banyak baris sudah sempit | Pada jalur strip, pola yang menyentuh lubang tetap perlu disaring. Letak lubang ikut menentukan berapa banyak cabang yang lolos |
| [Walled](#walled) | Coba capai `N` lewat propagasi serangan jika tiap garis pada satu orientasi punya satu ruas. Untuk kasus umum, gunakan batas ruas atau *maximum clique* | Batas ruas membantu saat jumlah ruas aktif ketat. Pewarnaan graf memberi batas tambahan saat ruas terlalu banyak | Solver harus membuktikan jumlah ratu maksimum dan mengembalikan seluruh susunan yang mencapainya. Banyaknya susunan optimal dapat mendominasi waktu dan heap |

### Poin utama program

- **Normal dan Holes berbagi gagasan pemisahan papan.** Set kolom kedua strip saling melengkapi. Normal memanfaatkan refleksi papan kosong, sedangkan Holes memeriksa posisi lubang pada tiap pola sebelum kedua strip digabungkan.
- **Pre-placed dan Holes padat memprioritaskan baris yang paling dibatasi.** Ratu terkunci menjadi posisi yang sudah terisi, sedangkan lubang mengurangi daftar kandidat. Pre-placed juga memangkas cabang jika sebuah kolom kosong sudah mustahil diisi.
- **Walled mengubah batas penempatan menjadi satu ratu per ruas.** Dinding memutus serangan, sehingga satu baris atau kolom bisa memuat beberapa ratu. Jalur *maximum clique* dipakai jika arah dengan ruas paling sedikit masih memiliki setidaknya `2 * N` ruas.
- **Keempat solver memakai operasi bit untuk menyaring kandidat.** Saat jumlah hasil diketahui sebelum materialisasi, `reserve` mengurangi pertumbuhan vektor luar. Jumlah solusi tetap menentukan berapa banyak koordinat yang perlu disimpan dan dikembalikan.

## Cara membaca operasi bit

| Operasi | Arti dalam pencarian |
|---|---|
| `1u << c` | Satu bit untuk kolom `c` |
| `available & -available` | Mengambil bit terendah yang menyala dari mask unsigned |
| `available &= available - 1` | Menghapus bit terendah yang menyala |
| `__builtin_ctz(bit)` | Mengubah satu bit aktif menjadi indeksnya. Kode memanggilnya saat mask berisi setidaknya satu bit |
| `__builtin_popcount(mask)` | Menghitung bit aktif, misalnya banyak kolom kandidat |
| `free & ~attacks` | Mempertahankan kandidat yang belum terpakai dan bebas dari serangan |

Bit yang sama dapat menyatakan hal yang berbeda antarstruktur. Mask kolom menunjuk posisi fisik. Bit pada blok kompatibilitas menunjuk susunan parsial. Bit pada representasi Walled menunjuk sel bebas yang posisinya sudah dipetakan ulang. Arti mask ditentukan oleh struktur yang menyimpannya.

## Normal

### Alur utama

1. `solveNQueens` memeriksa ukuran dan memilih spesialisasi `solve<N>()`. Ukuran yang diketahui saat kompilasi memudahkan compiler menyederhanakan batas loop dan operasi mask.
2. `join` membagi papan di antara dua baris tengah. Bagian atas memuat `N / 2` ratu dan bagian bawah memuat sisanya. Setiap set kolom atas memiliki pasangan tunggal berupa komplemennya. Dengan demikian, kedua bagian pasti memakai kolom yang saling lepas.
3. `imagesFor` memilih satu set kolom atas dari kelompok set yang saling berkaitan oleh refleksi. Program menguji pasangan strip untuk set yang dipilih.
4. Untuk setiap susunan atas, program menyelaraskan mask diagonal dan mencari susunan bawah yang kompatibel. Satu blok menguji sampai 64 susunan bawah sekaligus.
5. Putaran pertama menghitung jumlah akhir dengan `popcount` dan memperhitungkan refleksi yang akan dibentuk. Program memakai jumlah itu untuk `reserve` pada vektor luar.
6. Putaran kedua mengubah pasangan strip yang lolos menjadi vektor posisi. `emit` menambahkan refleksi yang diperlukan.

### Dari geometri strip ke tabel kompatibilitas

`visitStrip` bekerja dari baris yang terdekat dengan tengah papan menuju tepi. Pada setiap tingkat, ia memilih satu kolom bebas yang lolos dua mask diagonal. Letak ratu dan serangan digeser ketika turun ke tingkat berikutnya. Setelah semua kolom terpakai, pola disimpan dalam dua bentuk. Posisi setiap ratu memakai empat bit, cukup untuk kolom 0–11. Dua mask lainnya menyimpan indeks diagonal.

`join` menormalisasi set kolom dengan menggeser kolom terkecil ke posisi nol. `groupFor` menyediakan tabel untuk bentuk yang sudah dinormalisasi. Contohnya, kolom `{2, 4, 5}` dan `{5, 7, 8}` sama-sama memakai pola relatif `{0, 2, 3}`. Translasi menjaga jarak antarkolom dan pertentangan diagonal di dalam strip. Saat menggabungkan pola, program memperhitungkan kembali perpindahan masing-masing strip.

`makePatterns` membuat daftar pola dan blok pada waktu kompilasi. Untuk setiap arah dan indeks diagonal, blok menyimpan bitset susunan bawah yang memakai diagonal tersebut. `Block::compatible` mulai dari `valid` dan menghapus semua bit yang terkena serangan pola atas. Mask `valid` juga membatasi blok terakhir agar hanya memuat indeks yang benar-benar ada.

Misalkan ratu atas berjarak `k` baris dari baris terdekat di atas potongan, dan ratu bawah berjarak `j` dari baris terdekat di bawahnya. Jarak vertikal kedua ratu adalah `k + j + 1`. Keduanya bertentangan jika selisih kolom absolut sama dengan jarak itu. Kode menyelaraskan indeks diagonal dengan offset kolom kedua pola dan menukar arah saat melintasi potongan. Hanya pasangan yang memenuhi kedua pemeriksaan arah yang dapat menjadi solusi.

### Refleksi, kelengkapan, dan biaya

Untuk semua ukuran, `imagesFor` membandingkan set kolom atas dengan cerminnya terhadap sumbu vertikal. Untuk ukuran genap, pembalikan urutan baris juga dapat dipakai karena kedua bagian sama tinggi. Set kolom atas setelah pembalikan menjadi set kolom bawah yang semula. Kode mengambil mask set terkecil dalam kelompok tersebut. Saat dua transformasi menghasilkan set kolom atas yang sama, pencarian pada set yang terpilih sudah mencakup seluruh pola valid di set itu. `imagesFor` memilih satu transformasi per set tujuan sehingga tidak menggandakan hasil.

Setiap solusi lengkap dapat dipecah menjadi tepat satu pasangan strip untuk set kolom atasnya. Tabel mencakup semua pola parsial yang aman secara internal, dan penggabungan mengecek serangan yang tersisa. Argumen ini juga menjelaskan mengapa menyimpan pola parsial cukup untuk menghasilkan seluruh solusi ukuran `N` saat fungsi dipanggil.

Dibandingkan pencarian baris demi baris pada seluruh papan, teknik ini memindahkan enumerasi strip ke kompilasi dan mengurangi jumlah pemeriksaan saat jalan. Tabel menambah ukuran modul dan biaya kompilasi. Penggabungan dua putaran mengulang sebagian operasi bit, tetapi menghindari pencadangan berlebih dan realokasi vektor luar. Setiap solusi yang dikembalikan tetap memerlukan vektor posisi sendiri.

| `N` | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Jumlah solusi | 2 | 10 | 4 | 40 | 92 | 352 | 724 | 2.680 | 14.200 |

## Pre-placed

### Menerapkan ratu terkunci

`solvePreplaced` menyimpan baris yang belum terisi dalam `rows`, kolom terpakai dalam `columns`, serta dua arah diagonal terpakai dalam `rising` dan `falling`. Untuk ratu di `(r, c)`, indeks diagonal masing-masing adalah `c - r + n - 1` dan `c + r`. Tambahan `n - 1` menjaga indeks pertama tetap nonnegatif.

Setiap koordinat diperiksa. Jika ratu terkunci bertabrakan atau berada di luar papan, fungsi mengembalikan daftar kosong. Pengulangan koordinat yang identik diabaikan. Ratu yang sah langsung ditulis ke `placement`. Pencarian hanya mengubah baris yang masih kosong. Jika seluruh baris sudah terisi secara valid, papan itu menjadi satu-satunya hasil.

### Memilih cabang dan menyelesaikan ujung pencarian

`ConstraintSearch::legal` menghitung kandidat untuk satu baris dengan menggeser kedua mask diagonal ke koordinat kolom baris itu. Operand `free` menyisakan kolom yang belum terpakai. Pencarian memakai aturan berikut.

- **Baris dengan kandidat tersedikit.** Untuk tiga baris kosong atau lebih, solver mencari baris dengan `popcount` kandidat terkecil. Baris tanpa kandidat langsung menggugurkan cabang. Strategi *minimum remaining values* ini biasanya menunda pilihan yang lebar sampai batasan bertambah.
- **Kolom yang wajib diisi.** `seen` adalah gabungan kandidat dari semua baris kosong. `repeated` menandai kolom yang muncul di setidaknya dua baris. Jika `seen != free`, ada kolom kosong yang tidak dapat diisi oleh baris mana pun. Jika baris terpilih masih memiliki beberapa kandidat dan sebuah kolom hanya tersedia pada satu baris, solver memilih penempatan wajib itu. Contohnya, bila hanya baris 3 yang bisa memakai kolom 7, solusi lengkap harus menempatkan ratu di `(3, 7)`.
- **Satu baris tersisa.** Solver memeriksa satu-satunya kolom yang tersisa dan langsung mencatat hasil jika valid.
- **Dua baris tersisa.** `finishPair` menguji paling banyak dua pasangan. Jika jarak kedua kolom sama dengan jarak kedua baris, kedua penugasan pasti bertentangan secara diagonal. Jika jaraknya berbeda, pemeriksaan tinggal memastikan masing-masing sel aman terhadap ratu yang sudah ditempatkan.

Setiap pemilihan kandidat mengurangi satu baris dan satu kolom. Solver memperbarui dua diagonal untuk cabang itu. Informasi serangan diteruskan sebagai mask bernilai salinan. Saat rekursi kembali, cabang selanjutnya memakai mask induk yang masih utuh. Entri lama pada baris kosong dalam `placement` akan ditimpa sebelum papan lengkap dicatat.

### Penyimpanan hasil dan jalur tanpa ratu terkunci

`record` menyimpan satu byte per kolom dalam `CompactRows`. Sebanyak 128 susunan pertama muat di buffer stack. Susunan selebihnya disimpan pada blok heap berukuran sama. `materialize` mencadangkan vektor luar sesuai jumlah solusi, membentuk vektor `int` per solusi, lalu melepaskan blok ringkas yang selesai dibaca agar ruangnya dapat dipakai lagi. Teknik ini menukar satu tahap penyalinan dengan pertumbuhan vektor yang lebih terkendali.

Jika daftar posisi kosong, fungsi memakai `OrbitSearch` yang terdapat di berkas ini. Jalur ini melakukan pencarian bitboard pada papan kosong dan memilih wakil melalui delapan simetri persegi. Pembatasan posisi ratu di tepi mengurangi cabang awal. Pada susunan lengkap, program membandingkan transformasi secara leksikografis dan menyimpan satu wakil. Refleksi atau rotasi yang identik hanya dihitung sekali. Saat keluaran dibentuk, transformasi yang berbeda ditambahkan kembali. Jalur ini berfungsi mandiri saat berkas Pre-placed dikompilasi sendiri.

Pemindaian seluruh baris untuk mencari kandidat minimum dan kolom wajib menambah biaya per simpul. Keuntungannya muncul ketika pemangkasan mengurangi banyak simpul. Untuk papan dengan sedikit solusi, buffer stack sering cukup untuk seluruh hasil sementara. Heap keluaran tetap bergantung pada banyaknya solusi.

## Holes

### Membentuk mask dan memilih jalur

`solveHoles` membangun `allowed[r]` dari sel `.` dan menghitung jumlah lubang pada input kategori ini. Jika ada baris dengan mask kosong, solusi lengkap mustahil. Hal yang sama berlaku jika gabungan seluruh baris tidak mencakup semua kolom. Pemeriksaan ini murah dan menghindari pencarian untuk dua kondisi gagal yang umum.

Jika `3 * holes >= n * n`, solver memakai `ConstraintSearch`. Untuk kepadatan di bawah ambang tersebut, solver memakai `StripSearch`. Ambang adalah keputusan performa. Kedua jalur menerapkan aturan legalitas yang sama.

### Jalur strip untuk lubang yang relatif sedikit

1. `makeStrip` membuat pola parsial tanpa konflik di dalam strip pada saat kompilasi. Grup diindeks berdasarkan set kolom. Untuk dua set yang merupakan cermin pada lebar referensi 12 kolom, `groupFor` berbagi tabel pola dan menyimpan tanda `reflected`.
2. `Pattern` menyimpan urutan kolom dalam nibble, posisi ratu relatif terhadap kolom kelompok, serta mask diagonal lintas potongan. Dalam `cellsAndReflection`, 36 bit rendah menandai sel yang dipakai di ruang lokal hingga enam baris × enam kolom. Bit di atasnya menyimpan bentuk diagonal cermin.
3. Saat jalan, `StripSearch` mencatat lubang pada dua bagian papan. `HalfBoard::forbidden` mengekstrak kolom yang relevan untuk satu grup dan memetakan lubang ke ruang lokal pola. Urutan indeks dibalik jika tabel grup memakai representasi cermin.
4. Untuk setiap set kolom atas dan komplemennya, `join` menyaring pola yang menyentuh lubang. Program memeriksa pola atas dengan satu irisan bit dan membangun mask `permitted` untuk satu blok pola bawah. Jika tidak ada lubang yang relevan pada blok, mask awal blok dapat langsung dipakai.
5. `Block::compatible` memeriksa dua arah diagonal yang melintasi potongan. Tabel `safe` membagi kondisi diagonal ke enam kelompok empat bit. Satu lookup menggabungkan hingga empat batasan. Irisan hasil lookup dengan `permitted` menghasilkan pasangan yang memenuhi aturan lubang dan diagonal. Jika hanya satu bagian memakai representasi cermin, kode menukar dan membalik arah diagonal sebelum membandingkan kedua bagian.
6. Putaran pertama menjumlahkan `popcount` dari pasangan yang lolos. Putaran kedua membentuk vektor untuk pasangan yang sama setelah vektor luar dicadangkan tepat sesuai jumlah hasil. Saat mendekode tabel cermin, kode mengembalikan kolom fisik dengan operasi `11 - c` per nibble.

Pemisahan ini menjamin bahwa pola gabungan sudah aman di dalam tiap strip, memakai semua kolom tanpa pengulangan, dan lolos konflik lintas potongan. Lubang hanya ikut dalam penyaringan sel yang boleh ditempati. Karena itu, dua ratu tetap bentrok bila berada pada diagonal yang sama meski ada lubang di antaranya.

### Jalur pencarian untuk papan padat

`ConstraintSearch` memilih baris kosong dengan jumlah kandidat legal paling sedikit. Kandidat merupakan irisan `allowed[row]` dengan kolom bebas dan sel tanpa serangan diagonal. Setiap pemilihan memperbarui mask kolom dan diagonal. Jika baris kosong kehilangan semua kandidat, cabang berhenti. Jika semua baris sudah terisi, solver menambahkan solusi yang ditemukan. Vektor hasil tumbuh menurut kebutuhan pada jalur ini.

Saat banyak lubang, tiap baris cenderung memiliki sedikit kandidat dan pemangkasan terjadi dangkal. Ini bisa lebih murah daripada memindai blok pola yang sebagian besar gugur saat difilter. Jalur strip lebih menarik ketika ruang pencarian awal masih besar. Tabel strip disimpan dalam modul, sementara hasil tetap dialokasikan dari heap. Menambah lubang dapat mengurangi jumlah solusi, tetapi pola letaknya juga menentukan berapa lama solver harus membuktikan sebuah cabang buntu.

## Walled

### Mengapa aturan pencariannya berbeda

Dinding membelah baris, kolom dan diagonal menjadi ruas bebas. Dalam satu ruas hanya boleh ada satu ratu. Dua sel pada baris yang sama bisa dipakai bersamaan jika keduanya terpisah dinding, selama tidak ada serangan lewat arah lain. Solver harus menentukan ukuran optimum sebelum mengetahui susunan mana yang layak dikembalikan.

`solveWalled` memeriksa ukuran papan dan panjang tiap baris. Untuk papan tanpa dinding, `OrbitSearch` memakai delapan simetri papan kosong seperti pada Pre-placed dan mengubah setiap hasil menjadi pasangan koordinat. Untuk papan berdinding, fungsi terlebih dulu mencoba `FullLineSearch`. Jika jalur itu tidak membawa hasil, `SegmentSearch` menyelesaikan kasus umum.

### Jalur cepat dengan satu ruas di tiap garis

`FullLineSearch` membentuk mask sel terbuka untuk orientasi biasa dan orientasi transpos. Ekspresi `open & ~(open << 1)` menandai awal setiap ruas. Bila semua baris pada satu orientasi memiliki tepat satu ruas, `N` adalah batas atas karena hanya satu ratu boleh dipilih dari setiap baris. Untuk mencapai batas itu, pencarian menempatkan tepat satu ratu setiap kali maju ke baris berikutnya.

Pada setiap baris, serangan vertikal dan diagonal yang masuk diiriskan dengan mask sel terbuka. Serangan yang mengenai dinding terhapus sebelum diteruskan ke baris berikutnya. Kandidat ratu adalah sel terbuka yang selamat dari ketiga serangan. Setelah memilih ratu, solver menambahkan bit serangannya dan menggeser mask diagonal untuk baris berikutnya.

Di baris terakhir, pola disimpan sebagai satu bilangan 64-bit. Empat bit per baris menyimpan kolom pada awalan, dan 12 bit rendah menyimpan seluruh kandidat baris terakhir. Satu awalan dapat menghasilkan beberapa susunan. Program menghitung seluruh susunan dari mask ini, mencadangkan vektor luar, lalu mendekode koordinat sambil mengembalikannya ke orientasi semula. Jika menemukan setidaknya satu susunan berisi `N` ratu, batas atas tercapai dan seluruh susunan yang dicatat optimal.

### Membangun ruas dan konflik pada kasus umum

`SegmentSearch::run` memindai empat arah, yaitu baris, kolom, dan kedua diagonal. Pemindaian memulai ruas dari sel bebas yang pendahulunya berada di luar papan atau tertutup. Dua ratu bentrok jika selnya berada dalam ruas yang sama pada salah satu arah.

Program mengurutkan arah menurut jumlah ruasnya. Dua arah dengan jumlah ruas terkecil menjadi dua indeks untuk himpunan kandidat yang sama. Tabel `conflicts_` memetakan setiap sel ke semua sel yang bertentangan dengannya dalam kedua indeks, termasuk sel itu sendiri. Ketika memilih sebuah ratu, solver menghapus konflik dari kedua indeks dengan operasi bit.

`Axis::pack` menaruh sel dalam satu ruas pada bit yang berurutan di satu word 64-bit. Bila ruas akan melintasi batas word, ruas dimulai dari word berikutnya. Tiga word cukup untuk 144 sel dan padding. `ends` menandai bit terakhir setiap ruas. `activeSegments` memakai pengurangan pada word untuk mendeteksi ruas yang punya kandidat sebelum bit terakhir, lalu menggabungkannya dengan ruas yang kandidatnya berada tepat di bit terakhir. `popcount` memberi jumlah ruas aktif tanpa memeriksa satu ruas pada satu waktu.

### Mencari optimum dengan ruas atau *maximum clique*

**Pencarian ruas.** Target awal adalah jumlah ruas pada arah yang paling sedikit ruasnya. Jika tidak ada hasil pada target tersebut, program menurunkannya satu per satu. Pada sebuah simpul, `need` adalah jumlah ratu tambahan yang masih harus ditempatkan. Cabang gugur jika salah satu orientasi punya kurang dari `need` ruas aktif. Solver memilih ruas dengan sedikit kandidat pada orientasi yang lebih ketat. Bila kedua orientasi sama ketat, solver juga dapat memeriksa orientasi kedua.

Setiap cabang menempatkan satu ratu pada satu kandidat dari ruas terpilih. Cabang alternatif melewati ruas bila masih tersedia cukup ruas untuk memenuhi target. Kedua jenis cabang membagi semua kemungkinan berdasarkan pilihan pada ruas tersebut. Saat `need == 1`, semua sel kandidat yang tersisa bisa langsung menghasilkan solusi. Karena target diperiksa dari atas ke bawah, target pertama dengan hasil adalah optimum, dan solver mencatat semua susunan yang mencapainya.

**Pencarian graf.** Jika arah dengan ruas paling sedikit pun memiliki setidaknya `2 * n` ruas, pencarian beralih ke `CliqueSearch`. Dalam graf ini, simpul adalah sel bebas dan sisi menghubungkan dua sel yang kompatibel. Sekelompok ratu yang semuanya saling aman adalah *clique*. Solver mengurutkan simpul menurut jumlah konflik, lalu membangun kelompok warna secara rakus. Sel dalam satu kelompok warna saling menyerang, sehingga satu clique dapat mengambil paling banyak satu sel dari tiap warna. Jumlah warna memberi batas atas lanjutan. Putaran pertama hanya mencari ukuran maksimum, sedangkan putaran kedua menyimpan semua clique dengan ukuran tersebut. Perbandingan batas pada putaran kedua mempertahankan cabang yang masih bisa menyamai optimum.

Jika semua sel berdinding, hasilnya satu susunan kosong dengan nol ratu. Pada jalur kasus umum, susunan optimal dicatat sebagai bitset posisi, kemudian dikonversi menjadi pasangan `(baris, kolom)`. `Batches` menyimpan blok pertama pada stack dan memakai blok heap bila kapasitasnya terlampaui. Blok heap dilepas sambil membentuk hasil akhir. Jalur ruas memanfaatkan dinding yang menghasilkan batas ketat, sedangkan pewarnaan graf membantu saat jumlah ruas saja memberi batas yang terlalu longgar.

## Empat contoh untuk menelusuri kode

| Masukan | Hasil yang diharapkan | Bagian kode yang dapat diikuti |
|---|---|---|
| Normal, `N = 4` | Dua susunan, yaitu `[1, 3, 0, 2]` dan `[2, 0, 3, 1]` | Set atas untuk susunan pertama adalah `{1, 3}` dan set bawahnya `{0, 2}`. Kedua susunan terkait refleksi kolom |
| Pre-placed, `N = 4`, ratu terkunci di `(0, 1)` | Hanya `[1, 3, 0, 2]` | Bit baris 0 dikeluarkan dari `rows`, kolom 1 dikeluarkan dari `free`, dan kedua diagonal tercatat sebelum pencarian |
| Holes, `N = 4`, satu lubang di `(0, 1)` | Hanya `[2, 0, 3, 1]` | Pola atas yang semula memuat ratu di lubang gugur saat diperiksa dengan `upperHoles` |
| Walled, `N = 4`, sel bebas hanya `(0, 0)` dan `(0, 2)`, sisanya dinding | Satu susunan optimum berisi kedua ratu | Baris 0 memiliki dua ruas. Sel `(0, 1)` memutus serangan, sehingga kedua sel bebas dapat dipilih sekaligus |

## Dampak pada waktu dan memori

- **Waktu pencarian** dipengaruhi oleh jumlah kandidat yang diperiksa, seberapa cepat cabang buntu terdeteksi, jumlah pasangan strip yang diperiksa, dan biaya membentuk setiap solusi. Ukuran papan dan jumlah lubang atau dinding saja belum cukup untuk memperkirakan waktu.
- **Heap keluaran** dipengaruhi oleh jumlah solusi dan panjang tiap solusi. `reserve` dapat menghemat kapasitas berlebih pada vektor luar, sementara setiap konfigurasi tetap memiliki penyimpanan untuk koordinatnya sendiri. Pada Normal, sembilan ukuran menghasilkan 18.104 susunan. Pada ABI WASM proyek, payload kolom ditambah metadata vektor dalam berjumlah sekitar 1,06 MB desimal sebelum overhead allocator. Pada Walled, 42.000 susunan optimum dengan 12 ratu memerlukan sekitar 4,03 MB desimal untuk pasangan koordinat saja.
- **Heap yang dilaporkan tester** berasal dari selisih batas heap Emscripten sebelum dan sesudah pemanggilan solver. Jika allocator masih memiliki ruang, beberapa alokasi dapat terjadi tanpa menambah angka itu. Buffer stack dan tabel statis memakai memori modul tetapi berada di luar pengukuran pertambahan heap ini.
- **Kompilasi dan ukuran modul** merupakan biaya tabel strip `constexpr` pada Normal dan Holes. Tabel tersebut menyimpan pola parsial dan indeks konflik. Pengecekan diagonal antarbagiannya dan pembentukan papan lengkap berlangsung saat solver dipanggil.
- **Perbandingan hasil** perlu memakai papan yang sama, toolchain yang sama dan instance WASM baru untuk setiap run. Tester mengukur fungsi solver dengan satu warm-up dan tujuh run terukur. Waktu yang ditampilkan adalah yang tercepat. Parsing input, pengemasan hasil, dan validasi berlangsung di luar waktu fungsi. Lihat [dokumentasi pengukuran tester](../../README.md#time-memory-and-the-cutoff).

Jika `S` adalah jumlah solusi, materialisasi Normal, Pre-placed, dan Holes perlu menulis `S × N` posisi. Untuk Walled, dengan `K` ratu pada setiap susunan optimal, materialisasi perlu menulis `S × K` pasangan. Ini merupakan biaya minimum dari format keluaran. Penggabungan strip memeriksa blok pola bawah untuk setiap pola atas pada set kolom yang dipilih. Waktunya juga dipengaruhi jumlah bit diagonal mask atas pada Normal dan banyak pola yang perlu disaring oleh lubang pada Holes. Pada Pre-placed, biaya satu simpul pencarian paling banyak sebanding dengan jumlah baris kosong yang dipindai. Kedalaman rekursinya maksimal sebanyak baris kosong awal. Pada Walled, ketatnya batas ruas atau pewarnaan menentukan berapa banyak simpul yang benar-benar dibuka. Pencarian backtracking pada kasus ini masih dapat tumbuh eksponensial.

### Hasil pengukuran lokal Normal

Tiga pengulangan benchmark WebAssembly untuk sembilan ukuran papan memberi total waktu v3 sekitar 2,65–2,71 ms dan v4 sekitar 1,40–1,43 ms. Setiap total menjumlahkan waktu tercepat dari tujuh run per ukuran. Total pertambahan heap turun dari 1.200.128 byte menjadi 1.183.744 byte. Penghematan waktu sekitar 47% terutama berasal dari ukuran terbesar. Heap turun sekitar 16 KiB dan masih di atas 1 MB. Seluruh 18.104 susunan cocok dengan enumerasi referensi independen. Hasil ini berasal dari tester lokal, sementara waktu dan heap pada leaderboard diukur oleh runner kompetisi.

## Menjalankan dan memeriksa hasil

Dari root proyek, jalankan `npm run build:solvers` untuk membangun modul dan `npm run dev` untuk membuka tester lokal. Pilih kategori yang cocok dan solver `our-solver-4/normal`, `our-solver-4/preplaced`, `our-solver-4/holes`, atau `our-solver-4/walled`.

Untuk Normal, cocokkan jumlah solusi dengan tabel dan periksa himpunan susunan. Untuk Pre-placed dan Holes, gunakan papan dengan nol batasan untuk menguji kesetaraan dengan Normal. Gunakan juga papan yang mustahil dan papan dengan pilihan yang dipaksa. Untuk Walled, papan tanpa dinding harus menghasilkan optimum `N`, papan berdinding penuh memiliki satu susunan kosong, dan papan kecil dengan dinding di antara dua sel pada garis yang sama menguji pemutusan serangan. Untuk memastikan semua susunan optimal ditemukan, bandingkan dengan enumerasi independen atau solver referensi. Jika tester lokal hanya memeriksa validitas dan duplikasi, kelengkapan harus diperiksa dengan membandingkan himpunan hasil. Lihat [format jawaban dan cara kerja tester](../../README.md).
