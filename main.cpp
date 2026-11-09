// NvProfileSwitcher - applies NVIDIA Base Profile settings (from two .nip files) via NVAPI DRS.
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <string>
#include <cstdint>
#include <cstring>
#include <windowsx.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

struct Setting { uint32_t id; uint32_t value; const wchar_t* name; };
#include "settings.h"

// ---- minimal NVAPI DRS definitions (loaded dynamically from nvapi64.dll) ----
typedef int NvStatus;
typedef void* NvSession;
typedef void* NvProfile;
#pragma pack(push, 8)
struct NvSettingValueU { uint32_t u32; uint8_t pad[4096]; };      // union: 4100 bytes (binary = len + 4096)
struct NvSetting {
    uint32_t version;
    uint16_t name[2048];
    uint32_t settingId, settingType, settingLocation, isCurrentPredefined, isPredefinedValid;
    uint8_t predefined[4100];
    uint8_t current[4100];
};
#pragma pack(pop)
static_assert(sizeof(NvSetting) == 12320, "NVDRS_SETTING_V1 size mismatch");

typedef void* (*QueryInterface_t)(uint32_t);
typedef NvStatus (*Init_t)();
typedef NvStatus (*CreateSession_t)(NvSession*);
typedef NvStatus (*DestroySession_t)(NvSession);
typedef NvStatus (*LoadSettings_t)(NvSession);
typedef NvStatus (*SaveSettings_t)(NvSession);
typedef NvStatus (*GetBaseProfile_t)(NvSession, NvProfile*);
typedef NvStatus (*SetSetting_t)(NvSession, NvProfile, NvSetting*);
typedef NvStatus (*RestoreSetting_t)(NvSession, NvProfile, uint32_t);
typedef NvStatus (*DeleteSetting_t)(NvSession, NvProfile, uint32_t);

static QueryInterface_t QI;
static HMODULE g_nvapi;

static bool LoadNvapi() {
    g_nvapi = LoadLibraryW(L"nvapi64.dll");
    if (!g_nvapi) return false;
    QI = (QueryInterface_t)GetProcAddress(g_nvapi, "nvapi_QueryInterface");
    return QI != nullptr;
}

enum Mode { MINECRAFT, NORMAL };

static std::wstring Apply(Mode mode) {
    if (!g_nvapi && !LoadNvapi())
        return L"❌ Không tìm thấy nvapi64.dll - máy này không có driver NVIDIA.";
    auto Initialize = (Init_t)QI(0x0150E828);
    auto Create = (CreateSession_t)QI(0x0694D52E);
    auto Destroy = (DestroySession_t)QI(0xDAD9CFF8);
    auto Load = (LoadSettings_t)QI(0x375DBD6B);
    auto Save = (SaveSettings_t)QI(0xFCBC7E14);
    auto GetBase = (GetBaseProfile_t)QI(0xDA8466A0);
    auto Set = (SetSetting_t)QI(0x577DD202);
    auto Restore = (RestoreSetting_t)QI(0x53F0381E);
    auto Del = (DeleteSetting_t)QI(0xE4A26362);
    if (!Initialize || !Create || !Destroy || !Load || !Save || !GetBase || !Set)
        return L"❌ Driver NVIDIA không hỗ trợ các hàm DRS cần thiết.";

    NvStatus st = Initialize();
    if (st) return L"❌ NvAPI_Initialize thất bại (mã " + std::to_wstring(st) + L").";
    NvSession ses = nullptr;
    if ((st = Create(&ses))) return L"❌ Không tạo được phiên DRS (mã " + std::to_wstring(st) + L").";
    NvProfile prof = nullptr;
    std::wstring result;
    int ok = 0, fail = 0;
    if ((st = Load(ses)) || (st = GetBase(ses, &prof))) {
        Destroy(ses);
        return L"❌ Không đọc được Base Profile (mã " + std::to_wstring(st) + L").";
    }

    const Setting* tbl = mode == MINECRAFT ? kMinecraft : kNormal;
    size_t n = mode == MINECRAFT ? sizeof(kMinecraft) / sizeof(Setting) : sizeof(kNormal) / sizeof(Setting);
    std::wstring failList;
    for (size_t i = 0; i < n; i++) {
        NvSetting s; memset(&s, 0, sizeof(s));
        s.version = (uint32_t)sizeof(NvSetting) | (1u << 16);
        s.settingId = tbl[i].id;
        s.settingType = 0; // DWORD
        s.settingLocation = 0; // current profile
        memcpy(s.current, &tbl[i].value, 4);
        NvStatus r = Set(ses, prof, &s);
        if (r == 0) { ok++; }
        else {
            fail++;
            failList += L"\n  - " + std::wstring(tbl[i].name) + L" (id=" + std::to_wstring(tbl[i].id) + L", err=" + std::to_wstring(r) + L")";
        }
    }
    if (mode == NORMAL) {
        // settings that exist only in the Minecraft profile -> go back to driver default
        for (size_t i = 0; i < sizeof(kRestoreForNormal) / sizeof(Setting); i++) {
            NvStatus r = Restore ? Restore(ses, prof, kRestoreForNormal[i].id) : -1;
            if (r != 0 && Del) r = Del(ses, prof, kRestoreForNormal[i].id);
            if (r == 0 || r == -160 /* setting not found = already default */) ok++; else fail++;
        }
    }
    st = Save(ses);
    Destroy(ses);
    if (st) return L"Chưa hoàn thành.\nHãy chạy với quyền Administrator.";
    return L"Đã hoàn thành!\nKhởi động lại BlueStacks để nhận cấu hình mới.";
}

// ---- Theme colors ----
namespace Theme {
    const COLORREF BgDark      = RGB(24, 24, 28);
    const COLORREF BgCard      = RGB(34, 35, 40);
    const COLORREF TextPrimary = RGB(230, 232, 240);
    const COLORREF TextSecond  = RGB(160, 165, 180);
    const COLORREF Accent      = RGB(100, 120, 255);
    const COLORREF AccentHover = RGB(125, 145, 255);
    const COLORREF BtnNormal   = RGB(50, 52, 60);
    const COLORREF BtnHover    = RGB(65, 68, 78);
    const COLORREF BtnBorder   = RGB(70, 73, 85);
    const COLORREF BtnBorderHi = RGB(100, 120, 255);
    const COLORREF Green       = RGB(80, 200, 120);
    const COLORREF Separator   = RGB(50, 52, 60);
}

// ---- Owner-drawn button data ----
struct BtnState { bool hovered = false; bool pressed = false; };
static BtnState g_btnMC, g_btnNormal;
static HWND hStatus, hTitle, hBtnMC, hBtnNormal, hHint;
static HBRUSH hBgDark, hBgCard;
static HFONT hFontTitle, hFontBtn, hFontStatus, hFontHint;

enum { ID_MC = 101, ID_NORMAL = 102 };

static HFONT MakeFont(int size, int weight = FW_NORMAL) {
    return CreateFontW(-size, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET,
                       0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
}

static void RoundRect(HDC hdc, RECT rc, int r, HBRUSH fill, COLORREF border) {
    HPEN pen = CreatePen(PS_SOLID, 1, border);
    HPEN oldPen = (HPEN)SelectObject(hdc, pen);
    HBRUSH oldBr = (HBRUSH)SelectObject(hdc, fill);
    ::RoundRect(hdc, rc.left, rc.top, rc.right, rc.bottom, r, r);
    SelectObject(hdc, oldPen);
    SelectObject(hdc, oldBr);
    DeleteObject(pen);
}

static void DrawBtn(DRAWITEMSTRUCT* di, BtnState& st) {
    HDC hdc = di->hDC;
    RECT rc = di->rcItem;

    // 1) Fill entire rect with parent background to clear default white
    FillRect(hdc, &rc, hBgDark);

    // 2) Draw rounded button on top
    COLORREF bgCol = st.pressed ? Theme::Accent : (st.hovered ? Theme::BtnHover : Theme::BtnNormal);
    COLORREF bdCol = st.hovered ? Theme::BtnBorderHi : Theme::BtnBorder;
    if (st.pressed) bdCol = Theme::Accent;

    HBRUSH bg = CreateSolidBrush(bgCol);
    RoundRect(hdc, rc, 14, bg, bdCol);
    DeleteObject(bg);

    // 3) Text
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, st.pressed ? RGB(255,255,255) : Theme::TextPrimary);
    SelectObject(hdc, hFontBtn);

    wchar_t txt[128];
    GetWindowTextW(di->hwndItem, txt, 128);
    DrawTextW(hdc, txt, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// Subclass proc for hover tracking on buttons
static LRESULT CALLBACK BtnSubclassProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp,
                                         UINT_PTR id, DWORD_PTR refData) {
    BtnState* st = (BtnState*)refData;
    switch (msg) {
    case WM_MOUSEMOVE:
        if (!st->hovered) {
            st->hovered = true;
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hw, 0 };
            TrackMouseEvent(&tme);
            InvalidateRect(hw, nullptr, FALSE);
        }
        return 0;
    case WM_MOUSELEAVE:
        st->hovered = false;
        st->pressed = false;
        InvalidateRect(hw, nullptr, FALSE);
        return 0;
    case WM_LBUTTONDOWN:
        st->pressed = true;
        InvalidateRect(hw, nullptr, FALSE);
        break;
    case WM_LBUTTONUP:
        st->pressed = false;
        InvalidateRect(hw, nullptr, FALSE);
        break;
    }
    return DefSubclassProc(hw, msg, wp, lp);
}

static void DoApply(HWND hw, Mode m) {
    SetWindowTextW(hStatus, L"⏳ Đang áp dụng...");
    UpdateWindow(hw);
    std::wstring r = Apply(m);
    SetWindowTextW(hStatus, r.c_str());
}

LRESULT CALLBACK WndProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE: {
        hFontTitle  = MakeFont(17, FW_SEMIBOLD);
        hFontBtn    = MakeFont(15, FW_MEDIUM);
        hFontStatus = MakeFont(13);
        hFontHint   = MakeFont(12);

        int pad = 24;
        int w = 400;

        // Title
        hTitle = CreateWindowW(L"STATIC", L"NVIDIA Profile Switcher",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            pad, 18, w - pad * 2, 26, hw, nullptr, nullptr, nullptr);
        SendMessageW(hTitle, WM_SETFONT, (WPARAM)hFontTitle, TRUE);

        // Subtitle / description
        hHint = CreateWindowW(L"STATIC", L"Đồ họa NVIDIA cho giả lập BlueStacks",
            WS_CHILD | WS_VISIBLE | SS_CENTER,
            pad, 46, w - pad * 2, 20, hw, nullptr, nullptr, nullptr);
        SendMessageW(hHint, WM_SETFONT, (WPARAM)hFontHint, TRUE);

        // Buttons
        hBtnMC = CreateWindowW(L"BUTTON", L"Minecraft Graphic",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            pad, 82, w - pad * 2, 42, hw, (HMENU)ID_MC, nullptr, nullptr);

        hBtnNormal = CreateWindowW(L"BUTTON", L"Normal Profile",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            pad, 132, w - pad * 2, 42, hw, (HMENU)ID_NORMAL, nullptr, nullptr);

        // Set subclass for hover
        SetWindowSubclass(hBtnMC, BtnSubclassProc, 0, (DWORD_PTR)&g_btnMC);
        SetWindowSubclass(hBtnNormal, BtnSubclassProc, 0, (DWORD_PTR)&g_btnNormal);

        // Separator line (thin static)
        CreateWindowW(L"STATIC", L"",
            WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
            pad, 186, w - pad * 2, 1, hw, nullptr, nullptr, nullptr);

        // Status text
        hStatus = CreateWindowW(L"STATIC", L"Chọn một chế độ để áp dụng.",
            WS_CHILD | WS_VISIBLE,
            pad, 196, w - pad * 2, 44, hw, nullptr, nullptr, nullptr);
        SendMessageW(hStatus, WM_SETFONT, (WPARAM)hFontStatus, TRUE);

        return 0;
    }
    case WM_DRAWITEM: {
        auto* di = (DRAWITEMSTRUCT*)lp;
        if (di->CtlID == ID_MC) DrawBtn(di, g_btnMC);
        else if (di->CtlID == ID_NORMAL) DrawBtn(di, g_btnNormal);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == ID_MC) DoApply(hw, MINECRAFT);
        else if (LOWORD(wp) == ID_NORMAL) DoApply(hw, NORMAL);
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        HWND ctl = (HWND)lp;
        SetBkMode(hdc, TRANSPARENT);
        if (ctl == hTitle)
            SetTextColor(hdc, Theme::TextPrimary);
        else if (ctl == hHint)
            SetTextColor(hdc, Theme::TextSecond);
        else if (ctl == hStatus)
            SetTextColor(hdc, Theme::Green);
        else
            SetTextColor(hdc, Theme::TextSecond);
        return (LRESULT)hBgDark;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wp;
        RECT rc; GetClientRect(hw, &rc);
        FillRect(hdc, &rc, hBgDark);
        return 1;
    }
    case WM_DESTROY:
        DeleteObject(hFontTitle);
        DeleteObject(hFontBtn);
        DeleteObject(hFontStatus);
        DeleteObject(hFontHint);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hw, msg, wp, lp);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    hBgDark = CreateSolidBrush(Theme::BgDark);
    hBgCard = CreateSolidBrush(Theme::BgCard);

    WNDCLASSW wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hbrBackground = hBgDark;
    wc.lpszClassName = L"NvProfSwitcher";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    // Center window on screen
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int ww = 424, wh = 290;
    HWND hw = CreateWindowExW(
        0,
        wc.lpszClassName, L"NvProfileSwitcher",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        (sw - ww) / 2, (sh - wh) / 2, ww, wh,
        nullptr, nullptr, hi, nullptr);

    ShowWindow(hw, show);
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    DeleteObject(hBgDark);
    DeleteObject(hBgCard);
    return 0;
}
