// ============================================================================
//  FINAL PROJECT GRAFIKA KOMPUTER - START MENU WINDOWS 11
//  Sigit Novriyanto
//
//  Aturan: HANYA boleh pakai GDI (primitive dasar). Semua widget
//  (start button, menu, tile aplikasi, search box, tombol X) DIGAMBAR SENDIRI.
//  TIDAK ada GUI control bawaan (CreateWindow BUTTON, EDIT, dll).
//
//  Win32 dipakai HANYA untuk:
//    - bikin 1 window kosong sebagai "kanvas"
//    - terima event mouse/keyboard
//  Selebihnya digambar manual + hit-test manual pakai PtInRect().
//
//  Compile (MinGW / g++):
//    g++ startmenu.cpp -o startmenu.exe -mwindows -lgdi32 -luser32
//  Compile (MSVC / cl):
//    cl startmenu.cpp user32.lib gdi32.lib
//
//  Kontrol:
//    - Klik tombol Start (logo Windows di taskbar) -> buka/tutup Start Menu
//    - Klik tombol X (pojok kanan atas) atau tekan ESC -> keluar
//
//  [PATCH GCC 4.4.1]
//    - CLEARTYPE_QUALITY  -> ANTIALIASED_QUALITY
//    - Initializer list {} -> diubah ke fungsi InitApps() / InitRecs()
//      karena GCC 4.4.1 tidak support extended initializer list (C++11)
// ============================================================================

#include <windows.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ------------------------ KONFIGURASI WARNA & UKURAN ------------------------
#define TASKBAR_H   48          // tinggi taskbar
#define CELL        40          // ukuran 1 slot ikon di taskbar
#define GAP         8           // jarak antar ikon taskbar

static const COLORREF COL_TASKBAR = RGB(32, 32, 32);
static const COLORREF COL_MENU    = RGB(43, 43, 43);
static const COLORREF COL_BORDER  = RGB(60, 60, 60);
static const COLORREF COL_SEARCH  = RGB(55, 55, 55);
static const COLORREF COL_TXT     = RGB(255, 255, 255);
static const COLORREF COL_TXT2    = RGB(180, 180, 180);
static const COLORREF COL_ACCENT  = RGB(0, 120, 215);
static const COLORREF COL_HOVER   = RGB(62, 62, 62);

// ------------------------------ STATE GLOBAL --------------------------------
int  SCREEN_W = 0, SCREEN_H = 0;
bool g_menuOpen   = false;      // status start menu kebuka/ketutup
bool g_hovStart   = false;      // mouse lagi di atas tombol start?
bool g_hovClose   = false;      // mouse lagi di atas tombol X?
POINT g_mouse     = {0, 0};

RECT g_rcStart = {0, 0, 0, 0}; // area tombol start (buat hit-test)
RECT g_rcClose = {0, 0, 0, 0}; // area tombol X
RECT g_rcMenu  = {0, 0, 0, 0}; // area panel start menu
int  g_taskX = 0, g_taskY = 0; // posisi awal grup ikon taskbar

HBITMAP g_bg = NULL;            // cache wallpaper (biar gak hitung gradient terus)

// -------------------------------- DATA APP ----------------------------------
struct App {
    const char* name;
    const char* glyph;
    COLORREF    color;
};

struct Rec {
    const char* name;
    const char* sub;
    const char* glyph;
    COLORREF    color;
};

// [PATCH GCC 4.4.1]
// Dulu pakai extended initializer list (C++11):
//   App g_apps[18] = { {"Edge","e",RGB(...)}, ... };
// GCC 4.4.1 tidak support itu, jadi data diisi lewat fungsi InitApps()
// dan InitRecs() yang dipanggil sekali di WM_CREATE.

App g_apps[18];
Rec g_recs[4];

// Inisialisasi data array g_apps
void InitApps() {
    // slot 0-17 diisi satu per satu (tidak pakai brace initializer list C++11)
    g_apps[0].name  = "Edge";      g_apps[0].glyph  = "e";   g_apps[0].color  = RGB(0,120,215);
    g_apps[1].name  = "Word";      g_apps[1].glyph  = "W";   g_apps[1].color  = RGB(41,84,153);
    g_apps[2].name  = "Excel";     g_apps[2].glyph  = "X";   g_apps[2].color  = RGB(33,115,70);
    g_apps[3].name  = "PowerPoint";g_apps[3].glyph  = "P";   g_apps[3].color  = RGB(197,90,17);
    g_apps[4].name  = "VS Code";   g_apps[4].glyph  = "</>";  g_apps[4].color  = RGB(0,122,204);
    g_apps[5].name  = "Chrome";    g_apps[5].glyph  = "C";   g_apps[5].color  = RGB(66,133,244);
    g_apps[6].name  = "Spotify";   g_apps[6].glyph  = "S";   g_apps[6].color  = RGB(30,185,84);
    g_apps[7].name  = "Photos";    g_apps[7].glyph  = "Ph";  g_apps[7].color  = RGB(0,153,188);
    g_apps[8].name  = "Settings";  g_apps[8].glyph  = "St";  g_apps[8].color  = RGB(90,90,90);
    g_apps[9].name  = "Calculator";g_apps[9].glyph  = "=";   g_apps[9].color  = RGB(70,70,70);
    g_apps[10].name = "Mail";      g_apps[10].glyph = "M";   g_apps[10].color = RGB(0,120,215);
    g_apps[11].name = "Store";     g_apps[11].glyph = "a";   g_apps[11].color = RGB(0,99,177);
    g_apps[12].name = "Calendar";  g_apps[12].glyph = "31";  g_apps[12].color = RGB(0,120,215);
    g_apps[13].name = "Clock";     g_apps[13].glyph = "O";   g_apps[13].color = RGB(72,86,196);
    g_apps[14].name = "Paint";     g_apps[14].glyph = "Pt";  g_apps[14].color = RGB(214,75,75);
    g_apps[15].name = "Terminal";  g_apps[15].glyph = ">_";  g_apps[15].color = RGB(36,36,36);
    g_apps[16].name = "Notepad";   g_apps[16].glyph = "N";   g_apps[16].color = RGB(72,128,184);
    g_apps[17].name = "Camera";    g_apps[17].glyph = "Ca";  g_apps[17].color = RGB(64,64,64);
}

// Inisialisasi data array g_recs
void InitRecs() {
    g_recs[0].name  = "Final Project.cpp";   g_recs[0].sub  = "Baru saja diedit";  g_recs[0].glyph  = "</>";  g_recs[0].color  = RGB(0,122,204);
    g_recs[1].name  = "Tugas Grafika.docx";  g_recs[1].sub  = "2 jam yang lalu";   g_recs[1].glyph  = "W";    g_recs[1].color  = RGB(41,84,153);
    g_recs[2].name  = "Pinjamin Mockup.png"; g_recs[2].sub  = "Kemarin";           g_recs[2].glyph  = "Ph";   g_recs[2].color  = RGB(0,153,188);
    g_recs[3].name  = "Jadwal Demo.xlsx";    g_recs[3].sub  = "1 hari yang lalu";  g_recs[3].glyph  = "X";    g_recs[3].color  = RGB(33,115,70);
}

// ============================================================================
//  HELPER GAMBAR (semua dari primitive GDI)
// ============================================================================

// Bikin font Segoe UI (font default Windows 11)
// [PATCH GCC 4.4.1] CLEARTYPE_QUALITY -> ANTIALIASED_QUALITY
// CLEARTYPE_QUALITY (5) tidak terdefinisi di header MinGW lama.
// ANTIALIASED_QUALITY (4) sudah ada sejak Windows 95 / MinGW 3.x.
HFONT MakeFont(int px, bool bold) {
    return CreateFontA(-px, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI");
}

// Tulis teks di dalam rect
void TextAt(HDC dc, RECT rc, const char* s, int px, bool bold,
            COLORREF col, UINT flags) {
    HFONT f  = MakeFont(px, bold);
    HFONT of = (HFONT)SelectObject(dc, f);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, col);
    DrawTextA(dc, s, -1, &rc, flags | DT_NOPREFIX);
    SelectObject(dc, of);
    DeleteObject(f);
}

// Gradient vertikal (digambar baris per baris -> murni GDI)
void GradientV(HDC dc, RECT rc, COLORREF c1, COLORREF c2) {
    int h = rc.bottom - rc.top;
    if (h <= 1) return;
    for (int y = 0; y < h; y++) {
        double t = (double)y / (h - 1);
        int r = GetRValue(c1) + (int)((GetRValue(c2) - GetRValue(c1)) * t);
        int g = GetGValue(c1) + (int)((GetGValue(c2) - GetGValue(c1)) * t);
        int b = GetBValue(c1) + (int)((GetBValue(c2) - GetBValue(c1)) * t);
        HPEN pen = CreatePen(PS_SOLID, 1, RGB(r, g, b));
        HPEN op  = (HPEN)SelectObject(dc, pen);
        MoveToEx(dc, rc.left, rc.top + y, NULL);
        LineTo(dc, rc.right, rc.top + y);
        SelectObject(dc, op);
        DeleteObject(pen);
    }
}

// Kotak sudut bulat berwarna (border = (COLORREF)-1 berarti tanpa border)
void RoundFill(HDC dc, RECT rc, int r, COLORREF fill, COLORREF border) {
    HBRUSH b  = CreateSolidBrush(fill);
    HPEN   p  = CreatePen(PS_SOLID, 1, border == (COLORREF)-1 ? fill : border);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN   op = (HPEN)SelectObject(dc, p);
    RoundRect(dc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
    DeleteObject(p);
}

// Logo Windows (4 kotak)
void DrawWinLogo(HDC dc, int cx, int cy, int size, COLORREF col) {
    int gap  = size / 12;
    int cell = (size - gap) / 2;
    HBRUSH b  = CreateSolidBrush(col);
    HBRUSH ob = (HBRUSH)SelectObject(dc, b);
    HPEN   op = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    int x0 = cx - size / 2;
    int y0 = cy - size / 2;
    Rectangle(dc, x0,              y0,              x0 + cell,          y0 + cell);
    Rectangle(dc, x0 + cell + gap, y0,              x0 + 2*cell + gap,  y0 + cell);
    Rectangle(dc, x0,              y0 + cell + gap, x0 + cell,          y0 + 2*cell + gap);
    Rectangle(dc, x0 + cell + gap, y0 + cell + gap, x0 + 2*cell + gap,  y0 + 2*cell + gap);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(b);
}

// Ikon kaca pembesar (search)
void DrawMagnifier(HDC dc, int cx, int cy, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN op  = (HPEN)SelectObject(dc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - 6, cy - 6, cx + 4, cy + 4);   // lensa
    MoveToEx(dc, cx + 3, cy + 3, NULL);             // gagang
    LineTo(dc, cx + 8, cy + 8);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
}

// Tombol power (lingkaran + garis atas)
void DrawPower(HDC dc, int cx, int cy, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN op  = (HPEN)SelectObject(dc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Arc(dc, cx - 8, cy - 8, cx + 8, cy + 8, cx + 6, cy - 4, cx - 6, cy - 4);
    MoveToEx(dc, cx, cy - 9, NULL);
    LineTo(dc, cx, cy - 1);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
}

// 1 tile aplikasi (ikon kotak + glyph + label)
void DrawAppTile(HDC dc, int cx, int top, const App& a, int labelW) {
    int s = 44;
    RECT ic = { cx - s/2, top, cx + s/2, top + s };
    RoundFill(dc, ic, 10, a.color, (COLORREF)-1);
    TextAt(dc, ic, a.glyph, 18, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    RECT lr = { cx - labelW/2, top + s + 6, cx + labelW/2, top + s + 26 };
    TextAt(dc, lr, a.name, 12, false, COL_TXT,
           DT_CENTER|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);
}

// ============================================================================
//  KOMPONEN UTAMA
// ============================================================================

// Bangun cache wallpaper sekali (gradient mahal kalau dihitung tiap frame)
void BuildBackground(HWND hwnd) {
    HDC dc = GetDC(hwnd);
    if (g_bg) DeleteObject(g_bg);
    g_bg = CreateCompatibleBitmap(dc, SCREEN_W, SCREEN_H);
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP old = (HBITMAP)SelectObject(mem, g_bg);
    RECT full = { 0, 0, SCREEN_W, SCREEN_H };
    GradientV(mem, full, RGB(18, 32, 72), RGB(58, 112, 170));
    SelectObject(mem, old);
    DeleteDC(mem);
    ReleaseDC(hwnd, dc);
}

// Hitung ulang posisi semua elemen (dipanggil saat window dibuat)
void RecomputeLayout() {
    int n = 6;
    int groupW = n * CELL + (n - 1) * GAP;
    g_taskX = SCREEN_W / 2 - groupW / 2;
    g_taskY = SCREEN_H - TASKBAR_H / 2 - CELL / 2;

    // [PATCH GCC 4.4.1] Tidak pakai uniform initialization {x,y,w,h}
    // Harus assign field satu per satu agar kompatibel C++98
    g_rcStart.left   = g_taskX;
    g_rcStart.top    = g_taskY;
    g_rcStart.right  = g_taskX + CELL;
    g_rcStart.bottom = g_taskY + CELL;

    g_rcClose.left   = SCREEN_W - 52;
    g_rcClose.top    = 12;
    g_rcClose.right  = SCREEN_W - 12;
    g_rcClose.bottom = 44;

    int mw = 640, mh = 720;
    int mx = SCREEN_W / 2 - mw / 2;
    int my = SCREEN_H - TASKBAR_H - 12 - mh;
    g_rcMenu.left   = mx;
    g_rcMenu.top    = my;
    g_rcMenu.right  = mx + mw;
    g_rcMenu.bottom = my + mh;
}

// Gambar taskbar + ikon-ikonnya
void DrawTaskbar(HDC dc) {
    RECT tb = { 0, SCREEN_H - TASKBAR_H, SCREEN_W, SCREEN_H };
    HBRUSH b = CreateSolidBrush(COL_TASKBAR);
    FillRect(dc, &tb, b);
    DeleteObject(b);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(55, 55, 55));
    HPEN op  = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, 0, tb.top, NULL);
    LineTo(dc, SCREEN_W, tb.top);
    SelectObject(dc, op);
    DeleteObject(pen);

    // 6 slot ikon: [Start][Search][TaskView][Edge][Files][Store]
    for (int i = 0; i < 6; i++) {
        int x  = g_taskX + i * (CELL + GAP);
        int cx = x + CELL / 2;
        int cy = g_taskY + CELL / 2;

        if (i == 0) { // START
            if (g_hovStart) {
                RECT h = { x, g_taskY, x + CELL, g_taskY + CELL };
                RoundFill(dc, h, 6, COL_HOVER, (COLORREF)-1);
            }
            DrawWinLogo(dc, cx, cy, 20, RGB(74, 162, 232));
        } else if (i == 1) { // SEARCH
            DrawMagnifier(dc, cx, cy, RGB(220, 220, 220));
        } else if (i == 2) { // TASK VIEW (dua kotak)
            HPEN p2 = CreatePen(PS_SOLID, 2, RGB(220, 220, 220));
            HPEN o2 = (HPEN)SelectObject(dc, p2);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, cx - 8, cy - 8, cx + 2, cy + 2);
            Rectangle(dc, cx - 2, cy - 2, cx + 8, cy + 8);
            SelectObject(dc, ob);
            SelectObject(dc, o2);
            DeleteObject(p2);
        } else { // mini tile aplikasi
            App* a = (i == 3) ? &g_apps[0] : (i == 4) ? &g_apps[8] : &g_apps[11];
            RECT ic = { cx - 14, cy - 14, cx + 14, cy + 14 };
            RoundFill(dc, ic, 6, a->color, (COLORREF)-1);
            TextAt(dc, ic, a->glyph, 13, true, COL_TXT,
                   DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }

    // Jam + tanggal (system tray kanan)
    SYSTEMTIME st;
    GetLocalTime(&st);
    char tbuf[16], dbuf[16];
    wsprintfA(tbuf, "%02d:%02d", st.wHour, st.wMinute);
    wsprintfA(dbuf, "%02d/%02d/%04d", st.wDay, st.wMonth, st.wYear);
    RECT rt = { SCREEN_W - 130, tb.top + 6,  SCREEN_W - 16, tb.top + 24 };
    RECT rd = { SCREEN_W - 130, tb.top + 24, SCREEN_W - 16, tb.top + 42 };
    TextAt(dc, rt, tbuf, 13, false, COL_TXT,  DT_RIGHT|DT_SINGLELINE);
    TextAt(dc, rd, dbuf, 12, false, COL_TXT2, DT_RIGHT|DT_SINGLELINE);
}

// Gambar panel Start Menu
void DrawStartMenu(HDC dc) {
    RECT m  = g_rcMenu;
    int mx  = m.left;
    int my  = m.top;
    int mw  = m.right  - m.left;
    int mh  = m.bottom - m.top;

    RoundFill(dc, m, 12, COL_MENU, COL_BORDER);

    int pad = 24;
    int y   = my + pad;

    // --- Search box ---
    RECT sb  = { mx + pad, y, mx + mw - pad, y + 40 };
    RoundFill(dc, sb, 20, COL_SEARCH, RGB(85, 85, 85));
    DrawMagnifier(dc, sb.left + 22, (sb.top + sb.bottom) / 2, RGB(170, 170, 170));
    RECT sbt = { sb.left + 42, sb.top, sb.right - 12, sb.bottom };
    TextAt(dc, sbt, "Search for apps, settings, and documents", 13, false,
           COL_TXT2, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    y += 40 + 24;

    // --- Header "Pinned" ---
    RECT h1  = { mx + pad, y, mx + 200, y + 22 };
    TextAt(dc, h1, "Pinned", 15, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    RECT h1r = { mx + mw - 130, y, mx + mw - pad, y + 22 };
    TextAt(dc, h1r, "All apps  >", 12, false, COL_TXT2,
           DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    y += 30;

    // --- Grid 6 x 3 ---
    int cols  = 6, rows = 3;
    int cellW = (mw - 2 * pad) / cols;
    int cellH = 92;
    for (int i = 0; i < cols * rows; i++) {
        int col = i % cols;
        int row = i / cols;
        int cx  = mx + pad + col * cellW + cellW / 2;
        int top = y + row * cellH;
        DrawAppTile(dc, cx, top, g_apps[i], cellW - 8);
    }
    y += rows * cellH + 8;

    // --- Header "Recommended" ---
    RECT h2  = { mx + pad, y, mx + 220, y + 22 };
    TextAt(dc, h2, "Recommended", 15, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    RECT h2r = { mx + mw - 130, y, mx + mw - pad, y + 22 };
    TextAt(dc, h2r, "More  >", 12, false, COL_TXT2, DT_RIGHT|DT_VCENTER|DT_SINGLELINE);
    y += 32;

    // --- Recommended items (2 x 2) ---
    int itemW = (mw - 2 * pad) / 2;
    for (int i = 0; i < 4; i++) {
        int col = i % 2;
        int row = i / 2;
        int ix  = mx + pad + col * itemW;
        int iy  = y + row * 60;
        RECT ic = { ix, iy + 8, ix + 36, iy + 44 };
        RoundFill(dc, ic, 8, g_recs[i].color, (COLORREF)-1);
        TextAt(dc, ic, g_recs[i].glyph, 14, true, COL_TXT,
               DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        RECT nr = { ix + 46, iy + 8,  ix + itemW - 8, iy + 28 };
        RECT sr = { ix + 46, iy + 26, ix + itemW - 8, iy + 44 };
        TextAt(dc, nr, g_recs[i].name, 13, false, COL_TXT,
               DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        TextAt(dc, sr, g_recs[i].sub, 11, false, COL_TXT2,
               DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    }

    // --- Bottom bar: avatar + nama + power ---
    int bbH   = 64;
    int bbTop = my + mh - bbH;
    HPEN ln  = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
    HPEN ol  = (HPEN)SelectObject(dc, ln);
    MoveToEx(dc, mx + pad, bbTop, NULL);
    LineTo(dc, mx + mw - pad, bbTop);
    SelectObject(dc, ol);
    DeleteObject(ln);

    int av  = 36;
    int avY = bbTop + (bbH - av) / 2;
    HBRUSH ab  = CreateSolidBrush(COL_ACCENT);
    HBRUSH oab = (HBRUSH)SelectObject(dc, ab);
    HPEN   onp = (HPEN)SelectObject(dc, GetStockObject(NULL_PEN));
    Ellipse(dc, mx + pad, avY, mx + pad + av, avY + av);
    SelectObject(dc, onp);
    SelectObject(dc, oab);
    DeleteObject(ab);
    RECT avr = { mx + pad, avY, mx + pad + av, avY + av };
    TextAt(dc, avr, "SN", 14, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);

    RECT nm = { mx + pad + av + 12, bbTop, mx + mw - 80, my + mh };
    TextAt(dc, nm, "Sigit Novriyanto", 14, false, COL_TXT,
           DT_LEFT|DT_VCENTER|DT_SINGLELINE);

    DrawPower(dc, mx + mw - pad - 14, bbTop + bbH / 2, RGB(220, 220, 220));
}

// Tombol X buat keluar
void DrawCloseButton(HDC dc) {
    RoundFill(dc, g_rcClose, 8,
              g_hovClose ? RGB(232, 17, 35) : RGB(45, 45, 45), (COLORREF)-1);
    int cx = (g_rcClose.left + g_rcClose.right)  / 2;
    int cy = (g_rcClose.top  + g_rcClose.bottom) / 2;
    HPEN pen = CreatePen(PS_SOLID, 2, COL_TXT);
    HPEN op  = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, cx - 6, cy - 6, NULL); LineTo(dc, cx + 7, cy + 7);
    MoveToEx(dc, cx + 6, cy - 6, NULL); LineTo(dc, cx - 7, cy + 7);
    SelectObject(dc, op);
    DeleteObject(pen);
}

// Render semua ke back-buffer (double buffering -> anti kedip)
void Render(HDC dc) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP ob = (HBITMAP)SelectObject(mem, g_bg);
    BitBlt(dc, 0, 0, SCREEN_W, SCREEN_H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteDC(mem);

    DrawTaskbar(dc);
    if (g_menuOpen) DrawStartMenu(dc);
    DrawCloseButton(dc);
}

// ============================================================================
//  WINDOW PROCEDURE
// ============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        SCREEN_W = GetSystemMetrics(SM_CXSCREEN);
        SCREEN_H = GetSystemMetrics(SM_CYSCREEN);
        // [PATCH GCC 4.4.1] Inisialisasi array dilakukan di sini (bukan di deklarasi global)
        InitApps();
        InitRecs();
        RecomputeLayout();
        BuildBackground(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; // cegah kedip, kita handle sendiri di WM_PAINT

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc  = BeginPaint(hwnd, &ps);
        HDC back = CreateCompatibleDC(hdc);
        HBITMAP bb = CreateCompatibleBitmap(hdc, SCREEN_W, SCREEN_H);
        HBITMAP ob = (HBITMAP)SelectObject(back, bb);
        Render(back);
        BitBlt(hdc, 0, 0, SCREEN_W, SCREEN_H, back, 0, 0, SRCCOPY);
        SelectObject(back, ob);
        DeleteObject(bb);
        DeleteDC(back);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        g_mouse.x = LOWORD(lp);
        g_mouse.y = HIWORD(lp);
        bool hs = PtInRect(&g_rcStart, g_mouse) != 0;
        bool hc = PtInRect(&g_rcClose, g_mouse) != 0;
        if (hs != g_hovStart || hc != g_hovClose) {
            g_hovStart = hs;
            g_hovClose = hc;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        POINT p;
        p.x = LOWORD(lp);
        p.y = HIWORD(lp);
        // [PATCH GCC 4.4.1] Tidak pakai POINT p = { LOWORD(lp), HIWORD(lp) }
        // Uniform initialization pada non-POD struct bisa bermasalah di GCC 4.4.1
        if (PtInRect(&g_rcClose, p)) { DestroyWindow(hwnd); return 0; }
        if (PtInRect(&g_rcStart, p)) {
            g_menuOpen = !g_menuOpen;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        // klik di luar menu -> tutup menu
        if (g_menuOpen && !PtInRect(&g_rcMenu, p)) {
            g_menuOpen = false;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_bg) DeleteObject(g_bg);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
//  ENTRY POINT
// ============================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    // Suppress unused parameter warnings (valid di C++98)
    (void)hPrev;
    (void)lpCmd;

    const char* CLS = "StartMenuFinalProject";

    // [PATCH GCC 4.4.1] WNDCLASSA wc = {0} diganti ZeroMemory
    // Meskipun = {0} umumnya valid di C++98, beberapa versi GCC 4.4.x
    // memberikan warning "missing initializer" untuk aggregate init struct.
    // ZeroMemory lebih eksplisit dan aman untuk semua versi.
    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLS;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

    // WS_POPUP = window tanpa border/title bar -> fullscreen bersih
    HWND hwnd = CreateWindowExA(0, CLS, "Start Menu",
        WS_POPUP | WS_VISIBLE, 0, 0, sw, sh,
        NULL, NULL, hInst, NULL);

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}