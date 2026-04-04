#pragma once
#include <windows.h>

inline double GetDPIScale() {
    static double scale = 0.0;
    if (scale == 0.0) {
        HDC hdc = GetDC(NULL);
        int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(NULL, hdc);
        scale = dpiX / 96.0;
    }
    return scale;
}