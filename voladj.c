#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <xmmintrin.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5CDF2C82, 0x841E, 0x4546, 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A);

#define STEP 1
#define IND_W 300       /* indicator size, px at 100% scaling */
#define IND_H 48
#define IND_MARGIN 48   /* gap above the taskbar */
#define IND_HOLD 1000   /* ms visible after the last key press */
#define IND_FADE 200    /* ms fade out */
#define IND_TICK 15     /* ms between fade steps */
#define IND_ALPHA 235   /* opacity 0..255 */

int _fltused = 0;
static DWORD mainThread;

static HWND wnd;
static HDC memdc;
static HFONT iconFont, numFont;
static DWORD *bits;
static BITMAPINFO bmi;
static int dpi = 96, W, H, fadeA;
static UINT_PTR holdT, fadeT;

#define S(x) MulDiv(x, dpi, 96)

static LRESULT CALLBACK hook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION) {
        DWORD vk = ((KBDLLHOOKSTRUCT *)lp)->vkCode;
        if (vk == VK_VOLUME_UP || vk == VK_VOLUME_DOWN) {
            if (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)
                PostThreadMessage(mainThread, WM_APP, vk == VK_VOLUME_UP ? 1 : -1, 0);
            return 1;
        }
    }
    return CallNextHookEx(NULL, code, wp, lp);
}

/* anti-aliased coverage 0..1 of a capsule (horizontal segment grown by rad) at a pixel centre */
static float cap(float px, float py, float l, float r, float cy, float rad)
{
    float a = l + rad, b = r - rad, dx = 0, dy = py - cy, c;

    if (px < a) dx = a - px;
    else if (px > b) dx = px - b;
    c = rad + 0.5f - _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(dx * dx + dy * dy)));
    return c < 0 ? 0 : c > 1 ? 1 : c;
}

static void blend(BYTE alpha)
{
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };

    UpdateLayeredWindow(wnd, NULL, NULL, NULL, NULL, NULL, 0, &bf, ULW_ALPHA);
}

static void show(int pct, int muted)
{
    RECT wa, ri, rn;
    POINT pos, src = { 0, 0 };
    SIZE sz = { W, H };
    BLENDFUNCTION bf = { AC_SRC_OVER, 0, IND_ALPHA, AC_SRC_ALPHA };
    DWORD light = 0, n = sizeof(light);
    float bg, fg, tx0 = (float)S(56), tx1 = (float)S(236), tr = (float)S(3), fw, rad = H / 2.0f;
    wchar_t num[4];
    int x, y, len;

    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, NULL, &light, &n);
    bg = light ? 243.0f : 32.0f;
    fg = light ? 26.0f : 255.0f;
    fw = (tx1 - tx0) * pct / 100.0f;
    if (pct > 0 && fw < tr * 2) fw = tr * 2;

    for (y = 0; y < H; y++)
        for (x = 0; x < W; x++) {
            float px = x + 0.5f, py = y + 0.5f;
            float a = cap(px, py, 0, (float)W, rad, rad);
            float t = cap(px, py, tx0, tx1, rad, tr) * 0.25f;
            float f = pct > 0 ? cap(px, py, tx0, tx0 + fw, rad, tr) : 0;
            float c = bg + (fg - bg) * t;
            int v;

            c += (fg - c) * f;
            v = (int)(c * a + 0.5f);
            bits[y * W + x] = ((DWORD)(int)(a * 255.0f + 0.5f) << 24) | (v << 16) | (v << 8) | v;
        }

    ri.left = S(16); ri.right = S(48); ri.top = S(10); ri.bottom = H - S(10);
    rn.left = S(242); rn.right = W - S(16); rn.top = ri.top; rn.bottom = ri.bottom;
    SetTextColor(memdc, light ? RGB(26, 26, 26) : RGB(255, 255, 255));
    SelectObject(memdc, iconFont);
    DrawTextW(memdc, muted ? L"\xE74F" : L"\xE767", 1, &ri, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    len = 0;
    if (pct >= 100) num[len++] = L'1';
    if (pct >= 10) num[len++] = L'0' + pct / 10 % 10;
    num[len++] = L'0' + pct % 10;
    SelectObject(memdc, numFont);
    DrawTextW(memdc, num, len, &rn, DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    GdiFlush();
    for (y = ri.top; y < ri.bottom; y++) {
        for (x = ri.left; x < ri.right; x++) bits[y * W + x] |= 0xFF000000;
        for (x = rn.left; x < rn.right; x++) bits[y * W + x] |= 0xFF000000;
    }

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    pos.x = wa.left + (wa.right - wa.left - W) / 2;
    pos.y = wa.bottom - H - S(IND_MARGIN);
    UpdateLayeredWindow(wnd, NULL, &pos, &sz, memdc, &src, 0, &bf, ULW_ALPHA);
    ShowWindow(wnd, SW_SHOWNOACTIVATE);
    if (fadeT) KillTimer(NULL, fadeT);
    if (holdT) KillTimer(NULL, holdT);
    fadeT = 0;
    holdT = SetTimer(NULL, 0, IND_HOLD, NULL);
}

static void tick(UINT_PTR id)
{
    if (id == holdT && holdT) {
        KillTimer(NULL, holdT);
        holdT = 0;
        fadeA = IND_ALPHA;
        fadeT = SetTimer(NULL, 0, IND_TICK, NULL);
    } else if (id == fadeT && fadeT) {
        fadeA -= IND_ALPHA * IND_TICK / IND_FADE + 1;
        if (fadeA <= 0) {
            KillTimer(NULL, fadeT);
            fadeT = 0;
            ShowWindow(wnd, SW_HIDE);
        } else
            blend((BYTE)fadeA);
    }
}

static void step(int dir)
{
    IMMDeviceEnumerator *en = NULL;
    IMMDevice *dev = NULL;
    IAudioEndpointVolume *vol = NULL;
    float v;
    int pct = 0, ok = 0;
    BOOL muted = FALSE;

    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&en)))
        return;
    if (SUCCEEDED(IMMDeviceEnumerator_GetDefaultAudioEndpoint(en, eRender, eMultimedia, &dev)) &&
        SUCCEEDED(IMMDevice_Activate(dev, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void **)&vol)) &&
        SUCCEEDED(IAudioEndpointVolume_GetMasterVolumeLevelScalar(vol, &v))) {
        pct = (int)(v * 100.0f + 0.5f) + dir * STEP;
        if (pct < 0) pct = 0;
        if (pct > 100) pct = 100;
        if (SUCCEEDED(IAudioEndpointVolume_SetMasterVolumeLevelScalar(vol, pct / 100.0f, NULL)) && dir > 0)
            IAudioEndpointVolume_SetMute(vol, FALSE, NULL);
        IAudioEndpointVolume_GetMute(vol, &muted);
        ok = 1;
    }
    if (vol) IAudioEndpointVolume_Release(vol);
    if (dev) IMMDevice_Release(dev);
    IMMDeviceEnumerator_Release(en);
    if (ok) show(pct, muted);
}

static void init(void)
{
    WNDCLASSW wc;
    HDC dc = GetDC(NULL);
    void *p;

    SetProcessDPIAware();
    dpi = GetDeviceCaps(dc, LOGPIXELSX);
    W = S(IND_W);
    H = S(IND_H);
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = W;
    bmi.bmiHeader.biHeight = -H;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    memdc = CreateCompatibleDC(dc);
    SelectObject(memdc, CreateDIBSection(dc, &bmi, DIB_RGB_COLORS, &p, NULL, 0));
    bits = p;
    ReleaseDC(NULL, dc);
    SetBkMode(memdc, TRANSPARENT);
    iconFont = CreateFontW(-S(16), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe MDL2 Assets");
    numFont = CreateFontW(-S(18), 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, ANTIALIASED_QUALITY, 0, L"Segoe UI");
    wc.style = 0;
    wc.lpfnWndProc = DefWindowProcW;
    wc.cbClsExtra = wc.cbWndExtra = 0;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hIcon = NULL;
    wc.hCursor = NULL;
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"voladj_indicator";
    RegisterClassW(&wc);
    wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                          wc.lpszClassName, NULL, WS_POPUP, 0, 0, W, H, NULL, NULL, wc.hInstance, NULL);
}

void entry(void)
{
    MSG msg;

    CreateMutexW(NULL, FALSE, L"voladj_single_instance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
        ExitProcess(0);
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
        ExitProcess(1);
    mainThread = GetCurrentThreadId();
    init();
    if (!wnd)
        ExitProcess(1);
    if (!SetWindowsHookExW(WH_KEYBOARD_LL, hook, GetModuleHandleW(NULL), 0))
        ExitProcess(1);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (msg.message == WM_APP)
            step((int)msg.wParam);
        else if (msg.message == WM_TIMER && !msg.hwnd)
            tick(msg.wParam);
        else
            DispatchMessageW(&msg);
    }
    ExitProcess(0);
}
