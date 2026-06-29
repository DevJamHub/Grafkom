# Final Project Grafika Komputer — Tiruan Start Menu (BGI / graphics.h)

Program ini membuat **tiruan Start Menu Windows** menggunakan **hanya primitif
grafis** dari pustaka **BGI** (`graphics.h`). Seluruh elemen antarmuka
(taskbar, tombol start, panel menu, ikon, tombol X) **digambar sendiri** dari
titik, garis, kotak, dan teks — tanpa widget GUI bawaan apa pun.

## Kontrol (sesuai instruksi tugas)
| Elemen          | Lokasi             | Aksi saat diklik              |
|-----------------|--------------------|-------------------------------|
| Tombol Start    | kiri bawah taskbar | Buka/tutup menu (toggle)      |
| Tombol Keluar X | pojok kanan atas   | Menutup program               |
| Ikon aplikasi   | dalam panel menu   | Tidak ada (tampilan saja)     |

---

## Cara Kompilasi & Menjalankan

Program memakai `graphics.h`. Pilih **salah satu** lingkungan berikut.

### A. Dev-C++ + WinBGIm (paling umum, Windows) — DIREKOMENDASIKAN
1. Pasang **Dev-C++** (mis. versi Embarcadero/Bloodshed).
2. Pasang paket WinBGIm: salin `graphics.h` dan `winbgim.h` ke folder
   `include`, serta `libbgi.a` ke folder `lib` dari kompiler Dev-C++.
3. Buka `start_menu.cpp`, lalu masuk menu
   **Project → Project Options → Parameters → Linker** dan tambahkan:
   ```
   -lbgi -lgdi32 -lcomdlg32 -luuid -loleaut32 -lole32
   ```
4. Tekan **F11** (Compile & Run).

### B. Code::Blocks + WinBGIm (Windows)
1. Salin `graphics.h`, `winbgim.h` ke folder `include` MinGW, dan `libbgi.a`
   ke folder `lib`.
2. **Settings → Compiler → Linker settings**, tambahkan pustaka:
   `bgi, gdi32, comdlg32, uuid, oleaut32, ole32` (urutan penting, `bgi` paling atas).
3. Build & Run.

### C. SDL_bgi (lintas platform: Windows/Linux/Mac)
Bila tidak memakai WinBGIm, gunakan SDL_bgi yang menyediakan `graphics.h`
identik di atas SDL2.
```bash
# contoh di Linux setelah SDL2 dev + SDL_bgi terpasang:
g++ start_menu.cpp -o start_menu -lSDL_bgi -lSDL2
./start_menu
```

> **Catatan penting:** fungsi `COLOR(r,g,b)` untuk warna RGB kustom tersedia
> di **WinBGIm** dan **SDL_bgi** (bukan di Turbo C BGI lama). Bila kompiler
> menolak `COLOR`, ganti dengan 16 warna baku BGI (BLACK, BLUE, GREEN, CYAN,
> RED, ..., WHITE).

---

## Ringkasan Isi Kode
- `gambarDesktop()`  — latar gradien (interpolasi warna per baris piksel).
- `gambarTaskbar()`  — bilah tugas bawah.
- `gambarTombolStart()` — tombol start + logo 4 kotak.
- `gambarStartMenu()` — panel menu: avatar, daftar aplikasi (tampilan saja).
- `gambarTombolKeluar()` — tombol X (kotak merah + dua garis silang).
- `didalam()` — *hit-testing* manual: cek apakah klik berada di area tombol.
- `main()` — *event loop*: tangani klik mouse, perbarui state, gambar ulang.

## File
- `start_menu.cpp` — kode sumber lengkap (berkomentar Bahasa Indonesia).
- `pratinjau_tampilan.png` — ilustrasi tampilan akhir (mock-up).
