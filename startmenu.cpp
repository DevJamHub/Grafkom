// ============================================================================
//  FINAL PROJECT GRAFIKA KOMPUTER - WINDOWS 11 DESKTOP SIMULATION
//  Sigit Novriyanto
//
//  Aturan: HANYA boleh pakai GDI (primitive dasar). Semua widget
//  (start button, menu, folder, window, ikon desktop, context menu, search box)
//  DIGAMBAR SENDIRI + hit-test manual pakai PtInRect().
//  TIDAK ada GUI control bawaan (CreateWindow BUTTON, EDIT, MENU, dll).
//
//  Win32 dipakai HANYA untuk:
//    - bikin 1 window kosong sebagai "kanvas"
//    - terima event mouse/keyboard/timer
//
//  Compile (MinGW / g++):
//    g++ startmenu.cpp -o startmenu.exe -mwindows -lgdi32 -luser32
//  Compile (MSVC / cl):
//    cl startmenu.cpp user32.lib gdi32.lib
//
//  ----------------------------------------------------------------------------
//  FITUR INTERAKTIF (pengembangan dari versi awal):
//    1. Start Menu  -> tetap, tapi sebagian tile sekarang FOLDER (mini-grid 2x2)
//    2. Folder      -> klik folder => kebuka jadi "jendela explorer" isi aplikasi
//    3. Window Manager:
//         - jendela aplikasi bisa DIGESER (drag title bar)
//         - bisa DITUTUP (tombol X per window)
//         - z-order: window yang diklik naik ke depan
//         - bisa buka banyak window sekaligus (cascade)
//    4. Notepad   -> window yang bener-bener bisa DIKETIK (keyboard di-route manual)
//    5. Live Search -> ketik di search box, tile pinned ke-filter real-time
//    6. Desktop Icons -> klik = select, DOUBLE-CLICK = buka window
//    7. Context Menu -> KLIK KANAN desktop => menu (Refresh, Folder baru, dll)
//    8. Caret blink + jam live (pakai WM_TIMER)
//
//  KONTROL:
//    - Klik Start (logo Windows) / ikon Search di taskbar -> buka/tutup Start Menu
//    - Ketik saat Start Menu kebuka -> live search
//    - Klik tile folder -> buka folder window
//    - Drag title bar window -> geser; klik X di window -> tutup window
//    - Double-click ikon desktop -> buka window
//    - Klik kanan area kosong -> context menu
//    - Tombol X pojok kanan atas / ESC -> keluar aplikasi
//
//  [PATCH GCC 4.4.1] tetap dijaga:
//    - CLEARTYPE_QUALITY -> ANTIALIASED_QUALITY
//    - Tidak pakai extended initializer list C++11 (array struct diisi via fungsi)
//    - Tidak pakai uniform initialization pada non-POD
// ============================================================================

#include <windows.h>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

// ------------------------ KONFIGURASI WARNA & UKURAN ------------------------
#define TASKBAR_H   48          // tinggi taskbar
#define CELL        40          // ukuran 1 slot ikon di taskbar
#define GAP         8           // jarak antar ikon taskbar
#define TITLE_H     36          // tinggi title bar window aplikasi
#define MAXWIN      8           // maksimal window aplikasi yang kebuka bareng
#define CTX_W       210         // lebar context menu
#define CTX_ITEM_H  32          // tinggi 1 item context menu
#define CTX_N       5           // jumlah item context menu

static const COLORREF COL_TASKBAR = RGB(32, 32, 32);
static const COLORREF COL_MENU    = RGB(43, 43, 43);
static const COLORREF COL_BORDER  = RGB(60, 60, 60);
static const COLORREF COL_SEARCH  = RGB(55, 55, 55);
static const COLORREF COL_TXT     = RGB(255, 255, 255);
static const COLORREF COL_TXT2    = RGB(180, 180, 180);
static const COLORREF COL_ACCENT  = RGB(0, 120, 215);
static const COLORREF COL_HOVER   = RGB(62, 62, 62);
static const COLORREF COL_WINBG   = RGB(40, 40, 40);

// jenis window
enum { WT_FOLDER, WT_APP, WT_NOTEPAD };
// target fokus keyboard
enum { FOC_NONE, FOC_SEARCH, FOC_WIN };

// ------------------------------ STATE GLOBAL --------------------------------
int  SCREEN_W = 0, SCREEN_H = 0;
bool g_menuOpen = false;
bool g_hovStart = false;
bool g_hovClose = false;
POINT g_mouse   = {0, 0};

RECT g_rcStart  = {0, 0, 0, 0};
RECT g_rcClose  = {0, 0, 0, 0};
RECT g_rcMenu   = {0, 0, 0, 0};
int  g_taskX = 0, g_taskY = 0;

HBITMAP g_bg = NULL;

// layout start menu (dihitung sekali di RecomputeLayout, dipakai draw + hit-test)
RECT g_rcSearch = {0,0,0,0};
int  g_pinTop = 0, g_pinCellW = 0, g_pinCellH = 0;
int  g_recTop = 0, g_recItemW = 0;

// state search
char g_search[64];
int  g_searchLen = 0;

// fokus keyboard
int  g_focus    = FOC_NONE;
int  g_focusWin = -1;

// caret blink
bool g_caret = true;

// drag window
int  g_dragWin = -1;
int  g_dragDX = 0, g_dragDY = 0;

// context menu
bool  g_ctxOpen = false;
POINT g_ctxAt   = {0, 0};
RECT  g_rcCtx   = {0, 0, 0, 0};

// ============================================================================
//  DATA APLIKASI
// ============================================================================
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

// folder = kumpulan index ke g_apps
struct Folder {
    const char* name;
    int items[8];
    int count;
};

// tile di grid Pinned: kalau folder>=0 berarti folder, selain itu app
struct Tile {
    int folder;   // index folder, -1 kalau bukan folder
    int app;      // index app, dipakai kalau folder==-1
};

// window aplikasi yang lagi kebuka
struct Win {
    int      active;
    RECT     rc;
    char     title[64];
    char     glyph[8];
    COLORREF color;
    int      type;       // WT_*
    int      folderIdx;  // dipakai kalau WT_FOLDER
    char     text[256];  // dipakai kalau WT_NOTEPAD
    int      textLen;
};

// ikon di desktop
struct Desk {
    char     name[24];
    char     glyph[8];
    COLORREF color;
    RECT     rc;
    int      sel;
    int      type;   // 0=app, 1=notepad, 2=folder
    int      app;    // index app / folder
};

App    g_apps[18];
Rec    g_recs[4];
Folder g_folders[3];
Tile   g_pinned[18];
Win    g_wins[MAXWIN];
int    g_z[MAXWIN];      // z-order (depan = index terakhir)
int    g_zCount = 0;
Desk   g_desk[12];
int    g_deskCount = 0;

// ------------------------- INISIALISASI DATA --------------------------------
// [PATCH GCC 4.4.1] semua array struct diisi lewat fungsi, bukan brace init C++11
void InitApps() {
    g_apps[0].name  = "Edge";      g_apps[0].glyph  = "e";   g_apps[0].color  = RGB(0,120,215);
    g_apps[1].name  = "Word";      g_apps[1].glyph  = "W";   g_apps[1].color  = RGB(41,84,153);
    g_apps[2].name  = "Excel";     g_apps[2].glyph  = "X";   g_apps[2].color  = RGB(33,115,70);
    g_apps[3].name  = "PowerPoint";g_apps[3].glyph  = "P";   g_apps[3].color  = RGB(197,90,17);
    g_apps[4].name  = "VS Code";   g_apps[4].glyph  = "</>"; g_apps[4].color  = RGB(0,122,204);
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

void InitRecs() {
    g_recs[0].name  = "Final Project.cpp";   g_recs[0].sub  = "Baru saja diedit";  g_recs[0].glyph  = "</>";  g_recs[0].color  = RGB(0,122,204);
    g_recs[1].name  = "Tugas Grafika.docx";  g_recs[1].sub  = "2 jam yang lalu";   g_recs[1].glyph  = "W";    g_recs[1].color  = RGB(41,84,153);
    g_recs[2].name  = "Pinjamin Mockup.png"; g_recs[2].sub  = "Kemarin";           g_recs[2].glyph  = "Ph";   g_recs[2].color  = RGB(0,153,188);
    g_recs[3].name  = "Jadwal Demo.xlsx";    g_recs[3].sub  = "1 hari yang lalu";  g_recs[3].glyph  = "X";    g_recs[3].color  = RGB(33,115,70);
}

void InitFolders() {
    g_folders[0].name = "Office";
    g_folders[0].items[0] = 1;  g_folders[0].items[1] = 2;
    g_folders[0].items[2] = 3;  g_folders[0].items[3] = 16;
    g_folders[0].count = 4;

    g_folders[1].name = "Dev Tools";
    g_folders[1].items[0] = 4;  g_folders[1].items[1] = 15;
    g_folders[1].items[2] = 9;  g_folders[1].items[3] = 10;
    g_folders[1].count = 4;

    g_folders[2].name = "Media";
    g_folders[2].items[0] = 6;  g_folders[2].items[1] = 7;
    g_folders[2].items[2] = 14; g_folders[2].items[3] = 17;
    g_folders[2].count = 4;
}

void InitPinned() {
    // 3 tile pertama = folder, sisanya app
    g_pinned[0].folder = 0; g_pinned[0].app = -1;
    g_pinned[1].folder = 1; g_pinned[1].app = -1;
    g_pinned[2].folder = 2; g_pinned[2].app = -1;
    int order[15] = { 0,5,8,11,12,13,7,14,17,16,10,3,2,1,4 };
    for (int i = 0; i < 15; i++) {
        g_pinned[3 + i].folder = -1;
        g_pinned[3 + i].app = order[i];
    }
}

// ------------------------- UTIL STRING (C98) --------------------------------
void CopyStr(char* dst, const char* src, int cap) {
    int i = 0;
    for (; src[i] && i < cap - 1; i++) dst[i] = src[i];
    dst[i] = 0;
}

char LowerC(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

// substring case-insensitive (buat live search)
bool ContainsCI(const char* hay, const char* nee) {
    if (!nee[0]) return true;
    for (int i = 0; hay[i]; i++) {
        int j = 0;
        while (hay[i + j] && nee[j] && LowerC(hay[i + j]) == LowerC(nee[j])) j++;
        if (!nee[j]) return true;
    }
    return false;
}

const char* TileName(int i) {
    if (g_pinned[i].folder >= 0) return g_folders[g_pinned[i].folder].name;
    return g_apps[g_pinned[i].app].name;
}

// ============================================================================
//  HELPER GAMBAR (semua dari primitive GDI)  -- bawaan versi awal
// ============================================================================
HFONT MakeFont(int px, bool bold) {
    return CreateFontA(-px, 0, 0, 0, bold ? FW_SEMIBOLD : FW_NORMAL,
        FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
        "Segoe UI");
}

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

void DrawMagnifier(HDC dc, int cx, int cy, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN op  = (HPEN)SelectObject(dc, pen);
    HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
    Ellipse(dc, cx - 6, cy - 6, cx + 4, cy + 4);
    MoveToEx(dc, cx + 3, cy + 3, NULL);
    LineTo(dc, cx + 8, cy + 8);
    SelectObject(dc, ob);
    SelectObject(dc, op);
    DeleteObject(pen);
}

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

void DrawX(HDC dc, int cx, int cy, int r, COLORREF col) {
    HPEN pen = CreatePen(PS_SOLID, 2, col);
    HPEN op  = (HPEN)SelectObject(dc, pen);
    MoveToEx(dc, cx - r, cy - r, NULL); LineTo(dc, cx + r + 1, cy + r + 1);
    MoveToEx(dc, cx + r, cy - r, NULL); LineTo(dc, cx - r - 1, cy + r + 1);
    SelectObject(dc, op);
    DeleteObject(pen);
}

void DrawAppTile(HDC dc, int cx, int top, const App& a, int labelW) {
    int s = 44;
    RECT ic = { cx - s/2, top, cx + s/2, top + s };
    RoundFill(dc, ic, 10, a.color, (COLORREF)-1);
    TextAt(dc, ic, a.glyph, 18, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    RECT lr = { cx - labelW/2, top + s + 6, cx + labelW/2, top + s + 26 };
    TextAt(dc, lr, a.name, 12, false, COL_TXT,
           DT_CENTER|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);
}

// Tile folder digambar sebagai mini-grid 2x2 ikon isinya (gaya Windows 11)
void DrawFolderTile(HDC dc, int cx, int top, const Folder& f) {
    int s = 44;
    RECT bg = { cx - s/2, top, cx + s/2, top + s };
    RoundFill(dc, bg, 10, RGB(58, 58, 58), (COLORREF)-1);
    int m = 6, g = 4;
    int mini = (s - 2*m - g) / 2;
    for (int i = 0; i < 4 && i < f.count; i++) {
        int c  = i % 2, r = i / 2;
        int ix = bg.left + m + c * (mini + g);
        int iy = bg.top  + m + r * (mini + g);
        RECT mr = { ix, iy, ix + mini, iy + mini };
        const App& a = g_apps[f.items[i]];
        RoundFill(dc, mr, 4, a.color, (COLORREF)-1);
        TextAt(dc, mr, a.glyph, 9, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    }
    RECT lr = { cx - s, top + s + 6, cx + s, top + s + 26 };
    TextAt(dc, lr, f.name, 12, false, COL_TXT,
           DT_CENTER|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);
}

// ============================================================================
//  WINDOW MANAGER  (jendela aplikasi yang bisa digeser & ditutup)
// ============================================================================
void FocusWindow(int idx) {
    int k = 0;
    for (int i = 0; i < g_zCount; i++) if (g_z[i] != idx) g_z[k++] = g_z[i];
    g_zCount = k;
    g_z[g_zCount++] = idx;
    g_focusWin = idx;
}

int OpenWindow(int type, const char* title, const char* glyph,
               COLORREF col, int folderIdx) {
    int idx = -1;
    for (int i = 0; i < MAXWIN; i++) if (!g_wins[i].active) { idx = i; break; }
    if (idx < 0) return -1;

    Win& w = g_wins[idx];
    w.active = 1;
    w.type = type;
    w.folderIdx = folderIdx;
    w.color = col;
    w.textLen = 0;
    w.text[0] = 0;
    CopyStr(w.title, title, 64);
    CopyStr(w.glyph, glyph, 8);

    int ww = (type == WT_FOLDER) ? 430 : 480;
    int wh = (type == WT_FOLDER) ? 320 : 360;
    int ox = 130 + g_zCount * 28;
    int oy = 80  + g_zCount * 28;
    w.rc.left = ox;  w.rc.top = oy;
    w.rc.right = ox + ww;  w.rc.bottom = oy + wh;

    FocusWindow(idx);
    return idx;
}

void CloseWindow(int idx) {
    g_wins[idx].active = 0;
    int k = 0;
    for (int i = 0; i < g_zCount; i++) if (g_z[i] != idx) g_z[k++] = g_z[i];
    g_zCount = k;
    if (g_focusWin == idx) g_focusWin = (g_zCount > 0) ? g_z[g_zCount - 1] : -1;
}

int WindowAt(POINT p) {
    for (int i = g_zCount - 1; i >= 0; i--) {
        int idx = g_z[i];
        if (PtInRect(&g_wins[idx].rc, p)) return idx;
    }
    return -1;
}

RECT WinCloseRc(const Win& w) {
    RECT r;
    r.right  = w.rc.right - 6;
    r.left   = r.right - 30;
    r.top    = w.rc.top + 4;
    r.bottom = r.top + 28;
    return r;
}

// rect 1 cell isi folder (dipakai draw + hit-test biar sinkron)
RECT FolderCellRc(const Win& w, int i) {
    int cols = 4, pad = 16;
    int top  = w.rc.top + TITLE_H + 46;
    int cw   = (w.rc.right - w.rc.left - 2 * pad) / cols;
    int ch   = 86;
    int c = i % cols, r = i / cols;
    int x = w.rc.left + pad + c * cw;
    int y = top + r * ch;
    RECT rr = { x, y, x + cw, y + ch };
    return rr;
}

// ============================================================================
//  LAYOUT
// ============================================================================
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

void RecomputeLayout() {
    int n = 6;
    int groupW = n * CELL + (n - 1) * GAP;
    g_taskX = SCREEN_W / 2 - groupW / 2;
    g_taskY = SCREEN_H - TASKBAR_H / 2 - CELL / 2;

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

    int pad = 24;
    g_rcSearch.left   = mx + pad;
    g_rcSearch.top    = my + pad;
    g_rcSearch.right  = mx + mw - pad;
    g_rcSearch.bottom = g_rcSearch.top + 40;

    g_pinTop   = g_rcSearch.bottom + 24 + 30;   // setelah search + header "Pinned"
    g_pinCellW = (mw - 2 * pad) / 6;
    g_pinCellH = 92;
    g_recTop   = g_pinTop + 3 * g_pinCellH + 8 + 32;
    g_recItemW = (mw - 2 * pad) / 2;
}

// ============================================================================
//  HIT-TEST HELPER START MENU
// ============================================================================
int BuildMatches(int* out) {
    int n = 0;
    for (int i = 0; i < 18; i++) {
        if (ContainsCI(TileName(i), g_search)) {
            out[n++] = i;
            if (n >= 8) break;
        }
    }
    return n;
}

RECT SearchRowRc(int row) {
    int x = g_rcMenu.left + 24;
    int w = (g_rcMenu.right - g_rcMenu.left) - 48;
    int y = g_pinTop + row * 52;
    RECT r = { x, y, x + w, y + 48 };
    return r;
}

int PinnedAt(POINT p) {
    for (int i = 0; i < 18; i++) {
        int col = i % 6, row = i / 6;
        int cx  = g_rcMenu.left + 24 + col * g_pinCellW + g_pinCellW / 2;
        int top = g_pinTop + row * g_pinCellH;
        RECT cell = { cx - g_pinCellW/2, top - 6, cx + g_pinCellW/2, top + g_pinCellH - 6 };
        if (PtInRect(&cell, p)) return i;
    }
    return -1;
}

RECT RecItemRc(int i) {
    int pad = 24;
    int col = i % 2, row = i / 2;
    int ix = g_rcMenu.left + pad + col * g_recItemW;
    int iy = g_recTop + row * 60;
    RECT r = { ix, iy, ix + g_recItemW, iy + 52 };
    return r;
}

// ============================================================================
//  AKSI: BUKA WINDOW DARI TILE / REC / DESKTOP
// ============================================================================
void OpenAppByIndex(int appIdx) {
    const App& a = g_apps[appIdx];
    int t = (lstrcmpA(a.name, "Notepad") == 0) ? WT_NOTEPAD : WT_APP;
    OpenWindow(t, a.name, a.glyph, a.color, -1);
}

void OpenPinned(int i) {
    if (g_pinned[i].folder >= 0) {
        const Folder& f = g_folders[g_pinned[i].folder];
        OpenWindow(WT_FOLDER, f.name, "[]", RGB(80, 80, 80), g_pinned[i].folder);
    } else {
        OpenAppByIndex(g_pinned[i].app);
    }
    g_menuOpen = false;
    g_focus = FOC_NONE;
}

void OpenRec(int i) {
    if (i == 0) {
        int idx = OpenWindow(WT_NOTEPAD, g_recs[0].name, g_recs[0].glyph,
                             g_recs[0].color, -1);
        if (idx >= 0) {
            CopyStr(g_wins[idx].text,
                    "// Final Project Grafika - Notepad\n// Window ini bisa diketik, digeser, dan ditutup.\n",
                    256);
            g_wins[idx].textLen = lstrlenA(g_wins[idx].text);
        }
    } else {
        OpenWindow(WT_APP, g_recs[i].name, g_recs[i].glyph, g_recs[i].color, -1);
    }
    g_menuOpen = false;
    g_focus = FOC_NONE;
}

void ToggleMenu() {
    g_menuOpen = !g_menuOpen;
    if (g_menuOpen) {
        g_focus = FOC_SEARCH;
        g_searchLen = 0;
        g_search[0] = 0;
    } else {
        g_focus = FOC_NONE;
    }
}

// ---------------------------- DESKTOP ICONS ---------------------------------
void AddDesk(const char* name, const char* glyph, COLORREF c, int type, int app) {
    if (g_deskCount >= 12) return;
    Desk& d = g_desk[g_deskCount];
    CopyStr(d.name, name, 24);
    CopyStr(d.glyph, glyph, 8);
    d.color = c;
    d.type = type;
    d.app = app;
    d.sel = 0;
    int x = 24, y = 24 + g_deskCount * 92;
    d.rc.left = x;  d.rc.top = y;
    d.rc.right = x + 76;  d.rc.bottom = y + 78;
    g_deskCount++;
}

void InitDesk() {
    g_deskCount = 0;
    AddDesk("GARASIN",     "G",   RGB(0,120,215),  0, -1);
    AddDesk("Tugas Akhir", "W",   RGB(41,84,153),  1, -1);   // notepad
    AddDesk("Dev Tools",   "[]",  RGB(80,80,80),   2, 1);    // folder Dev Tools
    AddDesk("This PC",     "PC",  RGB(64,64,64),   0, -1);
    AddDesk("Recycle Bin", "Rb",  RGB(70,90,70),   0, -1);
}

void OpenDesk(int i) {
    Desk& d = g_desk[i];
    if (d.type == 2)       OpenWindow(WT_FOLDER, g_folders[d.app].name, "[]", RGB(80,80,80), d.app);
    else if (d.type == 1)  OpenWindow(WT_NOTEPAD, d.name, d.glyph, d.color, -1);
    else                   OpenWindow(WT_APP, d.name, d.glyph, d.color, -1);
}

// ---------------------------- CONTEXT MENU ----------------------------------
const char* CtxItemName(int i) {
    switch (i) {
        case 0:  return "Tampilan";
        case 1:  return "Urutkan menurut";
        case 2:  return "Refresh";
        case 3:  return "Folder baru";
        default: return "Personalisasi";
    }
}

void ComputeCtxRect() {
    int h = CTX_N * CTX_ITEM_H + 12;
    int x = g_ctxAt.x, y = g_ctxAt.y;
    if (x + CTX_W > SCREEN_W) x = SCREEN_W - CTX_W - 4;
    if (y + h > SCREEN_H - TASKBAR_H) y = SCREEN_H - TASKBAR_H - h - 4;
    g_rcCtx.left = x;  g_rcCtx.top = y;
    g_rcCtx.right = x + CTX_W;  g_rcCtx.bottom = y + h;
}

int CtxItemAt(POINT p) {
    if (!PtInRect(&g_rcCtx, p)) return -1;
    int rel = p.y - (g_rcCtx.top + 6);
    int it = rel / CTX_ITEM_H;
    if (it < 0 || it >= CTX_N) return -1;
    return it;
}

void DoCtxAction(int it) {
    if (it == 3) AddDesk("Folder baru", "[]", RGB(90, 90, 90), 2, 1);
    // item lain (Tampilan/Urutkan/Refresh/Personalisasi) cukup tutup + redraw
}

// ============================================================================
//  GAMBAR TASKBAR
// ============================================================================
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

    for (int i = 0; i < 6; i++) {
        int x  = g_taskX + i * (CELL + GAP);
        int cx = x + CELL / 2;
        int cy = g_taskY + CELL / 2;

        RECT slot = { x, g_taskY, x + CELL, g_taskY + CELL };
        if (PtInRect(&slot, g_mouse) || (i == 0 && g_hovStart)) {
            RoundFill(dc, slot, 6, COL_HOVER, (COLORREF)-1);
        }

        if (i == 0) {
            DrawWinLogo(dc, cx, cy, 20, RGB(74, 162, 232));
        } else if (i == 1) {
            DrawMagnifier(dc, cx, cy, RGB(220, 220, 220));
        } else if (i == 2) {
            HPEN p2 = CreatePen(PS_SOLID, 2, RGB(220, 220, 220));
            HPEN o2 = (HPEN)SelectObject(dc, p2);
            HBRUSH ob = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
            Rectangle(dc, cx - 8, cy - 8, cx + 2, cy + 2);
            Rectangle(dc, cx - 2, cy - 2, cx + 8, cy + 8);
            SelectObject(dc, ob);
            SelectObject(dc, o2);
            DeleteObject(p2);
        } else {
            App* a = (i == 3) ? &g_apps[0] : (i == 4) ? &g_apps[8] : &g_apps[11];
            RECT ic = { cx - 14, cy - 14, cx + 14, cy + 14 };
            RoundFill(dc, ic, 6, a->color, (COLORREF)-1);
            TextAt(dc, ic, a->glyph, 13, true, COL_TXT,
                   DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }
    }

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

// ============================================================================
//  GAMBAR START MENU
// ============================================================================
void DrawStartBottomBar(HDC dc) {
    RECT m = g_rcMenu;
    int mx = m.left, my = m.top;
    int mw = m.right - m.left, mh = m.bottom - m.top;
    int pad = 24;

    int bbH   = 64;
    int bbTop = my + mh - bbH;
    HPEN ln = CreatePen(PS_SOLID, 1, RGB(58, 58, 58));
    HPEN ol = (HPEN)SelectObject(dc, ln);
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

void DrawStartMenu(HDC dc) {
    RECT m  = g_rcMenu;
    int mw  = m.right - m.left;
    int pad = 24;

    RoundFill(dc, m, 12, COL_MENU, COL_BORDER);

    // --- Search box ---
    COLORREF sbBorder = (g_focus == FOC_SEARCH) ? COL_ACCENT : RGB(85, 85, 85);
    RoundFill(dc, g_rcSearch, 20, COL_SEARCH, sbBorder);
    DrawMagnifier(dc, g_rcSearch.left + 22,
                  (g_rcSearch.top + g_rcSearch.bottom) / 2, RGB(170, 170, 170));
    RECT sbt = { g_rcSearch.left + 42, g_rcSearch.top, g_rcSearch.right - 12, g_rcSearch.bottom };
    if (g_searchLen > 0) {
        char buf[66];
        CopyStr(buf, g_search, 64);
        int l = lstrlenA(buf);
        if (g_focus == FOC_SEARCH && g_caret && l < 64) { buf[l] = '|'; buf[l+1] = 0; }
        TextAt(dc, sbt, buf, 13, false, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    } else {
        const char* ph = "Ketik untuk mencari aplikasi & folder...";
        TextAt(dc, sbt, (g_focus == FOC_SEARCH && g_caret) ? "|" : ph, 13, false,
               (g_focus == FOC_SEARCH && g_caret) ? COL_TXT : COL_TXT2,
               DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        if (g_focus == FOC_SEARCH && g_caret) {
            RECT sbt2 = { sbt.left + 8, sbt.top, sbt.right, sbt.bottom };
            TextAt(dc, sbt2, ph, 13, false, COL_TXT2, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        }
    }

    // ====== MODE PENCARIAN ======
    if (g_searchLen > 0) {
        RECT hh = { m.left + pad, g_rcSearch.bottom + 18, m.left + 360, g_rcSearch.bottom + 40 };
        char head[80];
        wsprintfA(head, "Hasil untuk \"%s\"", g_search);
        TextAt(dc, hh, head, 13, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);

        int mt[8];
        int n = BuildMatches(mt);
        if (n == 0) {
            RECT nr = { m.left + pad, g_pinTop, m.right - pad, g_pinTop + 40 };
            TextAt(dc, nr, "Tidak ada hasil ditemukan.", 13, false, COL_TXT2,
                   DT_LEFT|DT_TOP|DT_SINGLELINE);
        }
        for (int r = 0; r < n; r++) {
            RECT rr = SearchRowRc(r);
            if (PtInRect(&rr, g_mouse)) RoundFill(dc, rr, 6, COL_HOVER, (COLORREF)-1);
            int idx = mt[r];
            RECT ic = { rr.left + 8, rr.top + 8, rr.left + 40, rr.top + 40 };
            COLORREF col; const char* gl; const char* nm;
            if (g_pinned[idx].folder >= 0) {
                const Folder& f = g_folders[g_pinned[idx].folder];
                col = RGB(70, 70, 70); gl = "[]"; nm = f.name;
            } else {
                const App& a = g_apps[g_pinned[idx].app];
                col = a.color; gl = a.glyph; nm = a.name;
            }
            RoundFill(dc, ic, 7, col, (COLORREF)-1);
            TextAt(dc, ic, gl, 13, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            RECT tr = { rr.left + 50, rr.top, rr.right - 10, rr.bottom };
            TextAt(dc, tr, nm, 13, false, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        }
        DrawStartBottomBar(dc);
        return;
    }

    // ====== MODE NORMAL: PINNED + RECOMMENDED ======
    RECT h1  = { m.left + pad, g_rcSearch.bottom + 24, m.left + 200, g_rcSearch.bottom + 46 };
    TextAt(dc, h1, "Pinned", 15, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    RECT h1r = { m.right - 130, h1.top, m.right - pad, h1.bottom };
    TextAt(dc, h1r, "All apps  >", 12, false, COL_TXT2, DT_RIGHT|DT_VCENTER|DT_SINGLELINE);

    for (int i = 0; i < 18; i++) {
        int col = i % 6, row = i / 6;
        int cx  = m.left + pad + col * g_pinCellW + g_pinCellW / 2;
        int top = g_pinTop + row * g_pinCellH;
        RECT cell = { cx - g_pinCellW/2, top - 6, cx + g_pinCellW/2, top + g_pinCellH - 6 };
        if (PtInRect(&cell, g_mouse)) RoundFill(dc, cell, 8, COL_HOVER, (COLORREF)-1);
        if (g_pinned[i].folder >= 0)
            DrawFolderTile(dc, cx, top, g_folders[g_pinned[i].folder]);
        else
            DrawAppTile(dc, cx, top, g_apps[g_pinned[i].app], g_pinCellW - 8);
    }

    int recH = g_recTop - 32;
    RECT h2  = { m.left + pad, recH, m.left + 220, recH + 22 };
    TextAt(dc, h2, "Recommended", 15, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    RECT h2r = { m.right - 130, recH, m.right - pad, recH + 22 };
    TextAt(dc, h2r, "More  >", 12, false, COL_TXT2, DT_RIGHT|DT_VCENTER|DT_SINGLELINE);

    for (int i = 0; i < 4; i++) {
        RECT cell = RecItemRc(i);
        if (PtInRect(&cell, g_mouse)) RoundFill(dc, cell, 8, COL_HOVER, (COLORREF)-1);
        RECT ic = { cell.left + 10, cell.top + 8, cell.left + 46, cell.top + 44 };
        RoundFill(dc, ic, 8, g_recs[i].color, (COLORREF)-1);
        TextAt(dc, ic, g_recs[i].glyph, 14, true, COL_TXT,
               DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        RECT nr = { cell.left + 56, cell.top + 8,  cell.right - 8, cell.top + 28 };
        RECT sr = { cell.left + 56, cell.top + 26, cell.right - 8, cell.top + 44 };
        TextAt(dc, nr, g_recs[i].name, 13, false, COL_TXT,
               DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        TextAt(dc, sr, g_recs[i].sub, 11, false, COL_TXT2,
               DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    }

    DrawStartBottomBar(dc);
}

// ============================================================================
//  GAMBAR WINDOW APLIKASI
// ============================================================================
void DrawWindow(HDC dc, int idx) {
    Win& w = g_wins[idx];
    bool focused = (idx == g_focusWin);
    RECT r = w.rc;

    RoundFill(dc, r, 8, COL_WINBG, focused ? COL_ACCENT : RGB(70, 70, 70));

    // garis pemisah title bar
    HPEN ln = CreatePen(PS_SOLID, 1, RGB(60, 60, 60));
    HPEN ol = (HPEN)SelectObject(dc, ln);
    MoveToEx(dc, r.left + 1, r.top + TITLE_H, NULL);
    LineTo(dc, r.right - 1, r.top + TITLE_H);
    SelectObject(dc, ol);
    DeleteObject(ln);

    // glyph chip + judul
    RECT chip = { r.left + 10, r.top + 6, r.left + 34, r.top + 30 };
    RoundFill(dc, chip, 5, w.color, (COLORREF)-1);
    TextAt(dc, chip, w.glyph, 12, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    RECT tt = { r.left + 42, r.top, r.right - 40, r.top + TITLE_H };
    TextAt(dc, tt, w.title, 13, false, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);

    // tombol close window
    RECT cb = WinCloseRc(w);
    bool hovX = PtInRect(&cb, g_mouse) != 0;
    RoundFill(dc, cb, 5, hovX ? RGB(232, 17, 35) : COL_WINBG, (COLORREF)-1);
    DrawX(dc, (cb.left + cb.right)/2, (cb.top + cb.bottom)/2, 5, COL_TXT);

    RECT body = { r.left + 1, r.top + TITLE_H + 1, r.right - 1, r.bottom - 1 };

    if (w.type == WT_FOLDER) {
        const Folder& f = g_folders[w.folderIdx];
        RECT pb = { body.left + 12, body.top + 10, body.right - 12, body.top + 34 };
        RoundFill(dc, pb, 6, RGB(50, 50, 50), RGB(70, 70, 70));
        char pth[90];
        wsprintfA(pth, "   Home  >  %s   (%d item)", f.name, f.count);
        TextAt(dc, pb, pth, 12, false, COL_TXT2, DT_LEFT|DT_VCENTER|DT_SINGLELINE);

        for (int i = 0; i < f.count; i++) {
            RECT cell = FolderCellRc(w, i);
            if (PtInRect(&cell, g_mouse)) RoundFill(dc, cell, 6, COL_HOVER, (COLORREF)-1);
            const App& a = g_apps[f.items[i]];
            int cx = (cell.left + cell.right) / 2;
            RECT ic = { cx - 22, cell.top + 10, cx + 22, cell.top + 54 };
            RoundFill(dc, ic, 10, a.color, (COLORREF)-1);
            TextAt(dc, ic, a.glyph, 16, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            RECT lr = { cell.left + 2, cell.top + 58, cell.right - 2, cell.top + 80 };
            TextAt(dc, lr, a.name, 11, false, COL_TXT,
                   DT_CENTER|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);
        }

    } else if (w.type == WT_NOTEPAD) {
        RECT pad = { body.left + 10, body.top + 10, body.right - 10, body.bottom - 10 };
        RoundFill(dc, pad, 4, RGB(28, 28, 28), RGB(60, 60, 60));
        char buf[260];
        CopyStr(buf, w.text, 258);
        bool foc = (idx == g_focusWin && g_focus == FOC_WIN);
        int l = lstrlenA(buf);
        if (foc && g_caret && l < 258) { buf[l] = '|'; buf[l+1] = 0; }
        RECT tx = { pad.left + 10, pad.top + 8, pad.right - 10, pad.bottom - 8 };
        if (w.textLen == 0 && !(foc && g_caret)) {
            TextAt(dc, tx,
                   "Klik di area ini lalu ketik. Window bisa digeser (drag judul) & ditutup (tombol X).",
                   12, false, COL_TXT2, DT_LEFT|DT_TOP|DT_WORDBREAK);
        } else {
            TextAt(dc, tx, buf, 13, false, RGB(230, 230, 230),
                   DT_LEFT|DT_TOP|DT_WORDBREAK|DT_EDITCONTROL);
        }

    } else { // WT_APP
        RECT band = { body.left + 1, body.top + 1, body.right - 1, body.top + 90 };
        GradientV(dc, band, w.color, RGB(40, 40, 40));
        RECT big = { band.left + 20, band.top + 18, band.left + 74, band.top + 72 };
        RoundFill(dc, big, 12, w.color, RGB(255, 255, 255));
        TextAt(dc, big, w.glyph, 22, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        RECT nm = { band.left + 90, band.top + 20, band.right - 10, band.top + 50 };
        TextAt(dc, nm, w.title, 18, true, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        RECT sub = { band.left + 90, band.top + 50, band.right - 10, band.top + 74 };
        TextAt(dc, sub, "Aplikasi simulasi - digambar 100% pakai GDI", 12, false,
               RGB(230, 230, 230), DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        int yy = band.bottom + 18;
        for (int i = 0; i < 4; i++) {
            RECT bar = { body.left + 20, yy, body.right - 20 - (i * 30), yy + 14 };
            RoundFill(dc, bar, 7, RGB(60, 60, 60), (COLORREF)-1);
            yy += 26;
        }
    }
}

// ============================================================================
//  GAMBAR DESKTOP ICONS + CONTEXT MENU + CLOSE BUTTON
// ============================================================================
void DrawDesktopIcons(HDC dc) {
    for (int i = 0; i < g_deskCount; i++) {
        Desk& d = g_desk[i];
        bool hov = PtInRect(&d.rc, g_mouse) != 0;
        if (d.sel)      RoundFill(dc, d.rc, 6, RGB(0, 90, 158), RGB(0, 120, 215));
        else if (hov)   RoundFill(dc, d.rc, 6, RGB(255, 255, 255), (COLORREF)-1),
                        RoundFill(dc, d.rc, 6, RGB(48, 60, 80), (COLORREF)-1);

        int cx  = (d.rc.left + d.rc.right) / 2;
        int top = d.rc.top + 6;
        int sz  = 44;
        RECT ic = { cx - sz/2, top, cx + sz/2, top + sz };

        if (d.type == 2) {
            // ikon folder: pakai mini-grid juga biar konsisten
            const Folder& f = g_folders[d.app];
            DrawFolderTile(dc, cx, top, f);
        } else {
            RoundFill(dc, ic, 10, d.color, (COLORREF)-1);
            TextAt(dc, ic, d.glyph, 16, true, COL_TXT, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        }

        RECT lr = { d.rc.left - 6, top + sz + 4, d.rc.right + 6, top + sz + 26 };
        TextAt(dc, lr, d.name, 11, false, COL_TXT,
               DT_CENTER|DT_TOP|DT_SINGLELINE|DT_END_ELLIPSIS);
    }
}

void DrawContextMenu(HDC dc) {
    RoundFill(dc, g_rcCtx, 8, RGB(45, 45, 45), RGB(70, 70, 70));
    for (int i = 0; i < CTX_N; i++) {
        RECT ir = { g_rcCtx.left + 6, g_rcCtx.top + 6 + i * CTX_ITEM_H,
                    g_rcCtx.right - 6, g_rcCtx.top + 6 + (i + 1) * CTX_ITEM_H };
        if (PtInRect(&ir, g_mouse)) RoundFill(dc, ir, 5, COL_HOVER, (COLORREF)-1);
        RECT tr = { ir.left + 14, ir.top, ir.right - 10, ir.bottom };
        TextAt(dc, tr, CtxItemName(i), 12, false, COL_TXT, DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    }
}

void DrawCloseButton(HDC dc) {
    RoundFill(dc, g_rcClose, 8,
              g_hovClose ? RGB(232, 17, 35) : RGB(45, 45, 45), (COLORREF)-1);
    int cx = (g_rcClose.left + g_rcClose.right)  / 2;
    int cy = (g_rcClose.top  + g_rcClose.bottom) / 2;
    DrawX(dc, cx, cy, 6, COL_TXT);
}

// ============================================================================
//  RENDER (double buffering -> anti kedip)
// ============================================================================
void Render(HDC dc) {
    HDC mem = CreateCompatibleDC(dc);
    HBITMAP ob = (HBITMAP)SelectObject(mem, g_bg);
    BitBlt(dc, 0, 0, SCREEN_W, SCREEN_H, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteDC(mem);

    DrawDesktopIcons(dc);
    // window dari belakang ke depan
    for (int i = 0; i < g_zCount; i++) DrawWindow(dc, g_z[i]);
    DrawTaskbar(dc);
    if (g_menuOpen) DrawStartMenu(dc);
    if (g_ctxOpen)  DrawContextMenu(dc);
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
        InitApps();
        InitRecs();
        InitFolders();
        InitPinned();
        InitDesk();
        ZeroMemory(g_wins, sizeof(g_wins));
        g_zCount = 0;
        g_search[0] = 0;
        RecomputeLayout();
        BuildBackground(hwnd);
        SetTimer(hwnd, 1, 530, NULL);   // caret blink + jam live
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_TIMER:
        g_caret = !g_caret;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

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
        g_hovStart = PtInRect(&g_rcStart, g_mouse) != 0;
        g_hovClose = PtInRect(&g_rcClose, g_mouse) != 0;

        if (g_dragWin >= 0 && g_wins[g_dragWin].active) {
            Win& w = g_wins[g_dragWin];
            int ww = w.rc.right - w.rc.left;
            int wh = w.rc.bottom - w.rc.top;
            int nl = g_mouse.x - g_dragDX;
            int nt = g_mouse.y - g_dragDY;
            if (nt < 0) nt = 0;
            if (nt > SCREEN_H - TASKBAR_H - TITLE_H) nt = SCREEN_H - TASKBAR_H - TITLE_H;
            if (nl < -(ww - 80)) nl = -(ww - 80);
            if (nl > SCREEN_W - 80) nl = SCREEN_W - 80;
            w.rc.left = nl;  w.rc.top = nt;
            w.rc.right = nl + ww;  w.rc.bottom = nt + wh;
        }
        InvalidateRect(hwnd, NULL, FALSE);   // selalu redraw (hover-effect)
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_dragWin >= 0) { g_dragWin = -1; ReleaseCapture(); }
        return 0;

    case WM_LBUTTONDOWN: {
        POINT p;
        p.x = LOWORD(lp);
        p.y = HIWORD(lp);

        // 1) tombol close aplikasi (paling atas)
        if (PtInRect(&g_rcClose, p)) { DestroyWindow(hwnd); return 0; }

        // 2) context menu (kalau lagi kebuka)
        if (g_ctxOpen) {
            int it = CtxItemAt(p);
            g_ctxOpen = false;
            if (it >= 0) DoCtxAction(it);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }

        // 3) start menu (digambar di atas window)
        if (g_menuOpen) {
            if (PtInRect(&g_rcMenu, p)) {
                if (PtInRect(&g_rcSearch, p)) {
                    g_focus = FOC_SEARCH;
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (g_searchLen > 0) {
                    int mt[8];
                    int n = BuildMatches(mt);
                    for (int r = 0; r < n; r++) {
                        RECT rr = SearchRowRc(r);
                        if (PtInRect(&rr, p)) { OpenPinned(mt[r]); InvalidateRect(hwnd, NULL, FALSE); return 0; }
                    }
                    return 0;
                }
                int pin = PinnedAt(p);
                if (pin >= 0) { OpenPinned(pin); InvalidateRect(hwnd, NULL, FALSE); return 0; }
                for (int i = 0; i < 4; i++) {
                    RECT rr = RecItemRc(i);
                    if (PtInRect(&rr, p)) { OpenRec(i); InvalidateRect(hwnd, NULL, FALSE); return 0; }
                }
                return 0;
            } else {
                // klik di luar menu -> tutup menu, lanjut proses elemen lain
                g_menuOpen = false;
                g_focus = FOC_NONE;
            }
        }

        // 4) taskbar
        {
            RECT tb = { 0, SCREEN_H - TASKBAR_H, SCREEN_W, SCREEN_H };
            if (PtInRect(&tb, p)) {
                for (int i = 0; i < 6; i++) {
                    int x = g_taskX + i * (CELL + GAP);
                    RECT s = { x, g_taskY, x + CELL, g_taskY + CELL };
                    if (PtInRect(&s, p)) {
                        if (i == 0)      ToggleMenu();
                        else if (i == 1) { g_menuOpen = true; g_focus = FOC_SEARCH; g_searchLen = 0; g_search[0] = 0; }
                        else if (i == 3) OpenAppByIndex(0);   // Edge
                        else if (i == 4) OpenAppByIndex(8);   // Settings
                        else if (i == 5) OpenAppByIndex(11);  // Store
                        break;
                    }
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        // 5) window aplikasi (dari depan ke belakang)
        {
            int wi = WindowAt(p);
            if (wi >= 0) {
                FocusWindow(wi);
                Win& w = g_wins[wi];
                RECT cb = WinCloseRc(w);
                if (PtInRect(&cb, p)) { CloseWindow(wi); InvalidateRect(hwnd, NULL, FALSE); return 0; }
                if (p.y < w.rc.top + TITLE_H) {
                    g_dragWin = wi;
                    g_dragDX = p.x - w.rc.left;
                    g_dragDY = p.y - w.rc.top;
                    SetCapture(hwnd);
                    InvalidateRect(hwnd, NULL, FALSE);
                    return 0;
                }
                if (w.type == WT_FOLDER) {
                    const Folder& f = g_folders[w.folderIdx];
                    for (int i = 0; i < f.count; i++) {
                        RECT cr = FolderCellRc(w, i);
                        if (PtInRect(&cr, p)) { OpenAppByIndex(f.items[i]); break; }
                    }
                } else if (w.type == WT_NOTEPAD) {
                    g_focus = FOC_WIN;
                    g_focusWin = wi;
                }
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        // 6) desktop icons -> select
        for (int i = 0; i < g_deskCount; i++) {
            if (PtInRect(&g_desk[i].rc, p)) {
                for (int k = 0; k < g_deskCount; k++) g_desk[k].sel = 0;
                g_desk[i].sel = 1;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }

        // 7) area kosong -> deselect semua
        for (int k = 0; k < g_deskCount; k++) g_desk[k].sel = 0;
        g_focus = FOC_NONE;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        POINT p;
        p.x = LOWORD(lp);
        p.y = HIWORD(lp);
        for (int i = 0; i < g_deskCount; i++) {
            if (PtInRect(&g_desk[i].rc, p)) {
                OpenDesk(i);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }
        return 0;
    }

    case WM_RBUTTONDOWN: {
        POINT p;
        p.x = LOWORD(lp);
        p.y = HIWORD(lp);
        // jangan munculkan ctx kalau klik di menu / window / taskbar
        if (g_menuOpen && PtInRect(&g_rcMenu, p)) return 0;
        if (WindowAt(p) >= 0) return 0;
        RECT tb = { 0, SCREEN_H - TASKBAR_H, SCREEN_W, SCREEN_H };
        if (PtInRect(&tb, p)) return 0;
        g_ctxOpen = true;
        g_ctxAt = p;
        ComputeCtxRect();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_CHAR: {
        char c = (char)wp;
        if (g_focus == FOC_SEARCH && g_menuOpen) {
            if (c == '\b') { if (g_searchLen > 0) g_search[--g_searchLen] = 0; }
            else if (c >= 32 && c < 127 && g_searchLen < 40) {
                g_search[g_searchLen++] = c;
                g_search[g_searchLen] = 0;
            }
            InvalidateRect(hwnd, NULL, FALSE);
        } else if (g_focus == FOC_WIN && g_focusWin >= 0 &&
                   g_wins[g_focusWin].active &&
                   g_wins[g_focusWin].type == WT_NOTEPAD) {
            Win& w = g_wins[g_focusWin];
            if (c == '\b') { if (w.textLen > 0) w.text[--w.textLen] = 0; }
            else if ((c >= 32 && c < 127) || c == '\r' || c == '\t') {
                char ch = (c == '\r') ? '\n' : c;
                if (w.textLen < 250) { w.text[w.textLen++] = ch; w.text[w.textLen] = 0; }
            }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (g_ctxOpen)          g_ctxOpen = false;
            else if (g_zCount > 0)  CloseWindow(g_z[g_zCount - 1]);
            else if (g_menuOpen)    { g_menuOpen = false; g_focus = FOC_NONE; }
            else                    { DestroyWindow(hwnd); return 0; }
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
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
    (void)hPrev;
    (void)lpCmd;

    const char* CLS = "StartMenuFinalProject";

    WNDCLASSA wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.style         = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW;  // CS_DBLCLKS -> dukung double-click
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = CLS;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    RegisterClassA(&wc);

    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);

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