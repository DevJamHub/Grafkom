# Final Project Grafika Komputer — Simulasi Desktop & Start Menu Windows 11

Program ini membuat **tiruan desktop + Start Menu Windows 11** menggunakan
**hanya primitif grafis GDI** dari Win32 (`windows.h`, `gdi32`, `user32`).
Seluruh elemen antarmuka — taskbar, tombol start, panel menu, search box,
folder, jendela aplikasi, ikon desktop, context menu, tombol X — **digambar
sendiri** dari titik, garis, kotak (rounded), gradien, dan teks. **Tidak ada**
widget GUI bawaan (`BUTTON`, `EDIT`, `MENU`, dll); Win32 dipakai hanya untuk
satu window kanvas + event mouse/keyboard/timer. *Hit-testing* dilakukan manual
dengan `PtInRect()`.

> File sumber: **`startmenu.cpp`** — kompatibel C++98 / GCC 4.4.1.

---

## Fitur Interaktif

1. **Start Menu** — grid aplikasi *pinned* + daftar *Recommended* + bar profil.
2. **Folder** — 3 tile pertama adalah folder (digambar mini-grid 2×2: *Office*,
   *Dev Tools*, *Media*). Diklik → terbuka jadi jendela explorer berisi aplikasi.
3. **Window Manager** — jendela aplikasi bisa **digeser** (drag title bar),
   **ditutup** (tombol X per window), punya **z-order** (yang diklik naik ke
   depan), dan bisa terbuka banyak sekaligus secara *cascade* (maks. 8 window).
4. **Notepad** — jendela yang benar-benar bisa **diketik**; input keyboard
   di-*route* manual ke window yang sedang fokus, lengkap dengan caret berkedip.
5. **Live Search** — saat Start Menu terbuka, ketik langsung untuk memfilter
   tile *pinned* secara real-time (pencarian *case-insensitive*).
6. **Ikon Desktop** — klik = *select*, **double-click** = buka window.
7. **Context Menu** — klik kanan area kosong → menu (Tampilan, Urutkan menurut,
   Refresh, **Folder baru** menambah ikon folder di desktop, Personalisasi).
8. **Jam live + caret blink** — diperbarui via `WM_TIMER`.

---

## Kontrol

| Elemen                     | Lokasi                  | Aksi                                            |
|----------------------------|-------------------------|-------------------------------------------------|
| Tombol Start (logo Windows)| kiri taskbar            | Buka/tutup Start Menu (toggle)                  |
| Ikon Search (kaca pembesar)| taskbar                 | Buka/tutup Start Menu                           |
| Search box                 | dalam Start Menu        | Ketik → filter tile pinned real-time            |
| Tile folder                | grid Start Menu         | Buka jendela explorer folder                    |
| Tile aplikasi              | grid Start Menu         | Buka jendela aplikasi                           |
| Item *Recommended*         | bawah Start Menu        | Buka file (mis. `Final Project.cpp` → notepad)  |
| Title bar window           | atas tiap jendela       | **Drag** untuk menggeser window                 |
| Tombol X window            | pojok kanan title bar   | Tutup window tersebut                           |
| Ikon desktop               | area desktop            | Klik = select · **double-click** = buka         |
| Klik kanan                 | area kosong desktop     | Tampilkan context menu                          |
| Tombol X (merah)           | pojok kanan atas layar  | Keluar program                                  |
| **ESC**                    | —                       | Tutup ctx → tutup window teratas → tutup menu → keluar |

---

## Cara Kompilasi & Menjalankan

Program memakai **GDI murni** (sudah tersedia di setiap toolchain Windows),
jadi **tidak perlu** memasang pustaka pihak ketiga seperti WinBGIm/SDL_bgi.

### A. MinGW / g++ (direkomendasikan)

```bash
g++ startmenu.cpp -o startmenu.exe -mwindows -lgdi32 -luser32
startmenu.exe
```

- `-mwindows` → aplikasi GUI (tanpa jendela konsol hitam).
- `-lgdi32 -luser32` → tautkan pustaka GDI & User32.

### B. MSVC (Developer Command Prompt)

```bat
cl startmenu.cpp user32.lib gdi32.lib
startmenu.exe
```

### C. Dev-C++ / Code::Blocks

1. Buka `startmenu.cpp` sebagai project baru (Win32 GUI Application).
2. Pastikan linker menautkan `gdi32` dan `user32` (umumnya sudah default pada
   template *Windows Application*; bila belum, tambahkan `-lgdi32 -luser32`).
3. Build & Run.

> **Catatan kompatibilitas:** kode ditulis untuk C++98 / GCC 4.4.1, jadi tidak
> memakai `nullptr`, *range-based for*, maupun *brace-init* non-POD. Untuk
> kualitas font dipakai `ANTIALIASED_QUALITY` (bukan `CLEARTYPE_QUALITY`) agar
> aman di kompiler lama.

---

## Ringkasan Isi Kode

**Data & inisialisasi**
- `InitApps()`, `InitRecs()`, `InitFolders()`, `InitPinned()`, `InitDesk()` —
  mengisi data aplikasi, item *Recommended*, folder, grid *pinned*, dan ikon
  desktop (dipisah jadi fungsi agar kompatibel GCC 4.4.1).

**Primitif & utilitas gambar**
- `GradientV()` / `BuildBackground()` — latar gradien (interpolasi per baris).
- `RoundFill()` — kotak sudut membulat (dasar hampir semua panel/tombol).
- `DrawWinLogo()`, `DrawMagnifier()`, `DrawPower()`, `DrawX()` — ikon vektor.
- `TextAt()`, `MakeFont()` — render teks + font.

**Taskbar & Start Menu**
- `DrawTaskbar()` — bilah tugas: start, search, tray, **jam live**.
- `DrawStartMenu()` + `DrawStartBottomBar()` — panel menu + bar profil
  (avatar "SN" / "Sigit Novriyanto").
- `DrawAppTile()` / `DrawFolderTile()` — tile aplikasi & folder (mini-grid 2×2).

**Window Manager**
- `OpenWindow()`, `CloseWindow()`, `FocusWindow()`, `WindowAt()` — kelola
  jendela: buka/tutup, fokus, z-order (depan = elemen terakhir), maks. `MAXWIN`.
- `DrawWindow()`, `WinCloseRc()`, `FolderCellRc()` — gambar jendela + tombol X +
  sel folder.

**Desktop & Context Menu**
- `DrawDesktopIcons()`, `AddDesk()`, `OpenDesk()` — ikon desktop + aksinya.
- `DrawContextMenu()`, `CtxItemName()`, `CtxItemAt()`, `DoCtxAction()` — menu
  klik kanan.

**Interaksi & event loop**
- `RecomputeLayout()` — hitung ulang posisi semua elemen (mengikuti resolusi
  layar), dipakai bersama oleh penggambar & *hit-test* agar selalu sinkron.
- `BuildMatches()`, `ContainsCI()`, `PinnedAt()`, `SearchRowRc()` — live search
  & hit-test grid.
- `ToggleMenu()` — buka/tutup Start Menu.
- `Render()` — *double buffering* (anti kedip).
- `WndProc()` — tangani `WM_PAINT`, mouse (`WM_LBUTTON*`, `WM_RBUTTONDOWN`,
  `WM_MOUSEMOVE`), keyboard (`WM_CHAR`, `WM_KEYDOWN`), dan `WM_TIMER`.
- `WinMain()` — *entry point*: daftar window class (dengan `CS_DBLCLKS`),
  bikin kanvas *full-screen*, jalankan message loop.

---

## File

- `startmenu.cpp` — kode sumber lengkap (komentar Bahasa Indonesia).