#include <windows.h>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "winmm.lib")

const int WIN_WIDTH = GetSystemMetrics(0);
const int WIN_HEIGHT = GetSystemMetrics(1);

#define SECONDS 30
#define SAMPLE_RATE 8000
#define BUFFER_SIZE (SAMPLE_RATE * SECONDS)

struct BouncingTriangle
{
    float x, y;
    float vx, vy;
    float size;
    float angle;
    float rotationSpeed;
    float baseHue;
};

struct PayloadParams
{
    int width;
    int height;
};

std::vector<BouncingTriangle> g_triangles;

struct Point3D { float x, y, z; };
struct Point2D { int x, y; };

RGBQUAD HSLtoRGB(float h, float s, float l)
{
    float c = (1.0f - std::fabs(2.0f * l - 1.0f)) * s;
    float x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = l - c / 2.0f;
    float r = 0, g = 0, b = 0;

    if (h >= 0 && h < 60) { r = c; g = x; b = 0; }
    else if (h >= 60 && h < 120) { r = x; g = c; b = 0; }
    else if (h >= 120 && h < 180) { r = 0; g = c; b = x; }
    else if (h >= 180 && h < 240) { r = 0; g = x; b = c; }
    else if (h >= 240 && h < 300) { r = x; g = 0; b = c; }
    else { r = c; g = 0; b = x; }

    RGBQUAD quad;
    quad.rgbRed = (BYTE)((r + m) * 255.0f);
    quad.rgbGreen = (BYTE)((g + m) * 255.0f);
    quad.rgbBlue = (BYTE)((b + m) * 255.0f);
    quad.rgbReserved = 255; // Opaco
    return quad;
}

float g_zoom = 1.2f;
float g_offsetX = -0.75f;
float g_offsetY = 0.0f;
int g_colorShift = 0;

RGBQUAD GetMandelbrotColor(int iterations, int maxIter, int shift)
{
    if (iterations >= maxIter)
    {
        return { 0, 0, 0, 255 };
    }

    int idx = (iterations + shift) % 256;
    RGBQUAD color;
    color.rgbBlue = (BYTE)(128.0f + 127.0f * std::sin(idx * 0.05f));
    color.rgbGreen = (BYTE)(128.0f + 127.0f * std::sin(idx * 0.08f + 2.0f));
    color.rgbRed = (BYTE)(128.0f + 127.0f * std::sin(idx * 0.12f + 4.0f));
    color.rgbReserved = 255;
    return color;
}

void RenderMandelbrotShader(HWND hwnd)
{
    HDC hdcScreen = GetDC(NULL);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    RGBQUAD* pBits = nullptr;
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = WIN_WIDTH;
    bmi.bmiHeader.biHeight = -WIN_HEIGHT;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

    RECT rcWindow;
    GetWindowRect(hwnd, &rcWindow);

    const int maxIter = 80;
    float aspect = (float)WIN_WIDTH / WIN_HEIGHT;

    for (int y = 0; y < WIN_HEIGHT; ++y)
    {
        for (int x = 0; x < WIN_WIDTH; ++x)
        {
            float x0 = (x - WIN_WIDTH / 2.0f) / (0.4f * g_zoom * WIN_WIDTH) * aspect + g_offsetX;
            float y0 = (y - WIN_HEIGHT / 2.0f) / (0.4f * g_zoom * WIN_HEIGHT) + g_offsetY;

            float zr = 0.0f;
            float zi = 0.0f;
            int n = 0;

            while (zr * zr + zi * zi <= 4.0f && n < maxIter)
            {
                float xtemp = zr * zr - zi * zi + x0;
                zi = 2.0f * zr * zi + y0;
                zr = xtemp;
                n++;
            }

            pBits[y * WIN_WIDTH + x] = GetMandelbrotColor(n, maxIter, g_colorShift);
        }
    }

    POINT ptDst = { rcWindow.left, rcWindow.top };
    SIZE sizeWin = { WIN_WIDTH, WIN_HEIGHT };
    POINT ptSrc = { 0, 0 };

    BLENDFUNCTION blend = {};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;

    UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, hOldBmp);
    DeleteObject(hbmp);
    DeleteDC(hdcMem);
    ReleaseDC(NULL, hdcScreen);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
        SetTimer(hwnd, 1, 30, NULL);
        break;

    case WM_TIMER:
        g_colorShift += 2;
        RenderMandelbrotShader(hwnd);
        break;

    case WM_DESTROY:
        KillTimer(hwnd, 1);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

namespace Bytebeats
{
    DWORD WINAPI Sound1(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                ((t * (t >> 42)) | (t * (t >> 12) | (t * (t >> 7))));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }

    DWORD WINAPI Sound2(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                ((t * (t << 1 | t >> 9) >> (t >> 86)) + (t * (t >> 42)));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }

    DWORD WINAPI Sound3(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                (((t | t >> 9 | t >> 7) * t & (t >> 11 | t >> 9)) + (((t << 4 | t >> 5) | t * t >> 3) + 12));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }

    DWORD WINAPI Sound4(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                (((t >> 4) + (t >> 5 | t)) | t * (t << 8 | t >> 9 | t));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }

    DWORD WINAPI Sound5(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                (((((t * (t >> 8 | t >> 41)) << (17 & t >> 20))) | (t >> 2)) + (t >> t));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }

    DWORD WINAPI Sound6(LPVOID lpvd)
    {
        HWAVEOUT hWaveOut;
        WAVEFORMATEX wfx = { WAVE_FORMAT_PCM, 1, SAMPLE_RATE, SAMPLE_RATE, 1, 8, 0 };

        if (waveOutOpen(&hWaveOut, WAVE_MAPPER, &wfx, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            return 1;
        }

        std::vector<unsigned char> buffer(BUFFER_SIZE);

        for (int t = 0; t < BUFFER_SIZE; ++t)
        {
            buffer[t] =
                (unsigned char)
                (((t * (((t >> 9) ^ ((t >> 9) - 1) ^ 1) % 13)) +
                (t * (-(t >> 8 | t | t >> 9 | t >> 13))) ^ t) + (t / 6));
        }

        WAVEHDR header = { (LPSTR)buffer.data(), (DWORD)buffer.size(), 0, 0, 0, 0, 0, 0 };

        waveOutPrepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutWrite(hWaveOut, &header, sizeof(WAVEHDR));

        while (!(header.dwFlags & WHDR_DONE))
        {
            Sleep(100);
        }

        waveOutUnprepareHeader(hWaveOut, &header, sizeof(WAVEHDR));
        waveOutClose(hWaveOut);

        return 0;
    }
}

namespace GDIPayloads
{
    DWORD WINAPI GDILayer1(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload1";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload1",
            L"fuck you Mazelarper",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        int timeOffset = 0;
        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            timeOffset += 2;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    unsigned char val = (unsigned char)((x + timeOffset) ^ (y + timeOffset));

                    RGBQUAD color;
                    color.rgbBlue = val;
                    color.rgbGreen = (val + timeOffset) % 256;
                    color.rgbRed = val ^ 128;
                    color.rgbReserved = 255;

                    pBits[y * width + x] = color;
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayer2(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload2";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload2",
            L"fuck you Mazelarper 1",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);

        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        float zoom = 1.2f;
        float offsetX = -0.75f;
        float offsetY = 0.0f;
        int colorShift = 0;
        const int maxIter = 70;
        float aspect = (float)width / height;

        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            colorShift += 2;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    float x0 = (x - width / 2.0f) / (0.4f * zoom * width) * aspect + offsetX;
                    float y0 = (y - height / 2.0f) / (0.4f * zoom * height) + offsetY;

                    float zr = 0.0f, zi = 0.0f;
                    int n = 0;

                    while (zr * zr + zi * zi <= 4.0f && n < maxIter)
                    {
                        float xtemp = zr * zr - zi * zi + x0;
                        zi = 2.0f * zr * zi + y0;
                        zr = xtemp;
                        n++;
                    }

                    pBits[y * width + x] = GetMandelbrotColor(n, maxIter, colorShift);
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(30);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayer3(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload3";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload3",
            L"fuck you Mazelarper 2",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        int timeOffset = 0;
        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            timeOffset -= 2;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    unsigned char val = (unsigned char)((x + timeOffset) & (y + timeOffset));

                    RGBQUAD color;
                    color.rgbBlue = val;
                    color.rgbGreen = (val + timeOffset) % 256;
                    color.rgbRed = val ^ 128;
                    color.rgbReserved = 255;

                    pBits[y * width + x] = color;
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayer4(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload3";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload3",
            L"fuck you Mazelarper 2",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        int timeOffset = 0;
        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            timeOffset += 2;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    unsigned char val = (unsigned char)cbrt(((x + timeOffset) * (y + timeOffset)));

                    RGBQUAD color;
                    color.rgbBlue = val;
                    color.rgbGreen = (val + timeOffset) % 256;
                    color.rgbRed = val ^ 128;
                    color.rgbReserved = 255;

                    pBits[y * width + x] = color;
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayer5(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload3";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload3",
            L"fuck you Mazelarper 2",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        int timeOffset = 0;
        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            timeOffset += 5;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    int cx = x - (width / 2);
                    int cy = y - (height / 2);

                    unsigned char val = (unsigned char)((cx * cx) + (cy * cy));

                    RGBQUAD color;
                    color.rgbBlue = val;
                    color.rgbGreen = (val + timeOffset) % 256;
                    color.rgbRed = val ^ 128;
                    color.rgbReserved = 255;

                    pBits[y * width + x] = color;
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayer6(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload3";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload3",
            L"fuck you Mazelarper 2",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        int timeOffset = 0;
        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            timeOffset += 3;

            for (int y = 0; y < height; ++y)
            {
                for (int x = 0; x < width; ++x)
                {
                    unsigned char val = (unsigned char)((x + timeOffset) ^ (y + timeOffset) & (y + timeOffset));

                    float hue = (float)((val + timeOffset * 2) % 360);
                    float saturation = 1.0f;
                    float lightness = 0.5f;

                    pBits[y * width + x] = HSLtoRGB(hue, saturation, lightness);
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 125;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }

    DWORD WINAPI GDILayerCube(LPVOID lpParam)
    {
        PayloadParams* params = (PayloadParams*)lpParam;
        int width = params ? params->width : WIN_WIDTH;
        int height = params ? params->height : WIN_HEIGHT;

        HINSTANCE hInstance = GetModuleHandle(NULL);
        WNDCLASS wc = {};
        wc.lpfnWndProc = DefWindowProc;
        wc.hInstance = hInstance;
        wc.lpszClassName = L"Payload3";
        RegisterClass(&wc);

        HWND hwnd = CreateWindowEx
        (
            WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            L"Payload3",
            L"fuck you Mazelarper 2",
            WS_POPUP,
            0,
            0,
            WIN_WIDTH, WIN_HEIGHT,
            NULL, NULL, hInstance, NULL
        );

        if (!hwnd) return 1;
        ShowWindow(hwnd, SW_SHOW);
        HDC hdcScreen = GetDC(NULL);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        RGBQUAD* pBits = nullptr;
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height;
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        HBITMAP hbmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hbmp);

        Point3D vertices[8] =
        {
            {-1, -1, -1}, { 1, -1, -1}, { 1,  1, -1}, {-1,  1, -1},
            {-1, -1,  1}, { 1, -1,  1}, { 1,  1,  1}, {-1,  1,  1}
        };

        int edges[12][2] =
        {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        };

        float posX = width / 2.0f;
        float posY = height / 2.0f;
        float vx = 4.0f;
        float vy = 3.0f;
        float angleX = 0.0f, angleY = 0.0f;
        float currentHue = 0.0f;

        MSG msg = {};

        while (true)
        {
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                if (msg.message == WM_QUIT) goto cleanup;
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            float cubeRadius = 80.0f;
            posX += vx;
            posY += vy;

            bool collided = false;

            if (posX - cubeRadius < 0) { posX = cubeRadius; vx = -vx; collided = true; }
            else if (posX + cubeRadius > width) { posX = width - cubeRadius; vx = -vx; collided = true; }

            if (posY - cubeRadius < 0) { posY = cubeRadius; vy = -vy; collided = true; }
            else if (posY + cubeRadius > height) { posY = height - cubeRadius; vy = -vy; collided = true; }

            if (collided)
            {
                currentHue = std::fmod(currentHue + 60.0f, 360.0f);
            }

            angleX += 0.03f;
            angleY += 0.04f;

            int totalPixels = width * height;

            for (int i = 0; i < totalPixels; ++i)
            {
                pBits[i] = { 0, 0, 0, 0 };
            }

            Point2D proj[8];
            float scale = 90.0f;

            for (int i = 0; i < 8; ++i)
            {
                Point3D p = vertices[i];

                float x1 = p.x * std::cos(angleY) + p.z * std::sin(angleY);
                float z1 = -p.x * std::sin(angleY) + p.z * std::cos(angleY);
                float y1 = p.y;

                float y2 = y1 * std::cos(angleX) - z1 * std::sin(angleX);
                float z2 = y1 * std::sin(angleX) + z1 * std::cos(angleX);
                float x2 = x1;

                float distance = 3.5f;
                float factor = scale / (z2 + distance);

                proj[i].x = (int)(posX + x2 * factor);
                proj[i].y = (int)(posY + y2 * factor);
            }

            RGBQUAD lineColor = HSLtoRGB(currentHue, 1.0f, 0.6f);

            for (int e = 0; e < 12; ++e)
            {
                int x0 = proj[edges[e][0]].x;
                int y0 = proj[edges[e][0]].y;
                int x1 = proj[edges[e][1]].x;
                int y1 = proj[edges[e][1]].y;

                int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
                int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
                int err = dx + dy, e2;

                while (true)
                {
                    if (x0 >= 0 && x0 < width && y0 >= 0 && y0 < height)
                    {
                        pBits[y0 * width + x0] = lineColor;
                    }

                    if (x0 == x1 && y0 == y1) break;
                    
                    e2 = 2 * err;
                    if (e2 >= dy) { err += dy; x0 += sx; }
                    if (e2 <= dx) { err += dx; y0 += sy; }
                }
            }

            RECT rcWindow;
            GetWindowRect(hwnd, &rcWindow);
            POINT ptDst = { rcWindow.left, rcWindow.top };
            SIZE sizeWin = { width, height };
            POINT ptSrc = { 0, 0 };

            BLENDFUNCTION blend = {};
            blend.BlendOp = AC_SRC_OVER;
            blend.SourceConstantAlpha = 255;
            blend.AlphaFormat = AC_SRC_ALPHA;

            UpdateLayeredWindow(hwnd, hdcScreen, &ptDst, &sizeWin, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

            Sleep(20);
        }

    cleanup:
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hbmp);
        DeleteDC(hdcMem);
        ReleaseDC(NULL, hdcScreen);
        DestroyWindow(hwnd);
        return 0;
    }
}

void InitPayloads()
{
    HANDLE GDIThread1 = CreateThread(0, 0, GDIPayloads::GDILayer1, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound1, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread1, 0);
    HANDLE GDIThread2 = CreateThread(0, 0, GDIPayloads::GDILayer2, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound2, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread2, 0);
    HANDLE GDIThread3 = CreateThread(0, 0, GDIPayloads::GDILayer3, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound3, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread3, 0);
    HANDLE GDIThread4 = CreateThread(0, 0, GDIPayloads::GDILayer4, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound4, 0, 0, 0);
    CreateThread(0, 0, GDIPayloads::GDILayerCube, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread4, 0);
    HANDLE GDIThread5 = CreateThread(0, 0, GDIPayloads::GDILayer5, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound5, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread5, 0);
    HANDLE GDIThread6 = CreateThread(0, 0, GDIPayloads::GDILayer6, 0, 0, 0);
    CreateThread(0, 0, Bytebeats::Sound6, 0, 0, 0);
    Sleep(30000);
    TerminateThread(GDIThread6, 0);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if
        (
            MessageBoxW
            (
                NULL,
                L"This is a malware\n\nYou're really sure that you want to proceed?",
                L"dksopworelw - Warning",
                MB_ICONWARNING | MB_YESNO
            ) != IDYES
            ) return 1;

    Sleep(5000);

    InitPayloads();
    return 0;
}