#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <initguid.h>
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>

DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E);
DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6);
DEFINE_GUID(IID_IAudioEndpointVolume, 0x5CDF2C82, 0x841E, 0x4546, 0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A);

#define STEP 1

int _fltused = 0;
static DWORD mainThread;

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

static void step(int dir)
{
    IMMDeviceEnumerator *en = NULL;
    IMMDevice *dev = NULL;
    IAudioEndpointVolume *vol = NULL;
    float v;
    int pct;

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
    }
    if (vol) IAudioEndpointVolume_Release(vol);
    if (dev) IMMDevice_Release(dev);
    IMMDeviceEnumerator_Release(en);
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
    if (!SetWindowsHookExW(WH_KEYBOARD_LL, hook, GetModuleHandleW(NULL), 0))
        ExitProcess(1);
    while (GetMessageW(&msg, NULL, 0, 0) > 0)
        if (msg.message == WM_APP)
            step((int)msg.wParam);
    ExitProcess(0);
}
