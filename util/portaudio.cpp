#include "portaudio.h"

#include <cmath>
#include <algorithm>

namespace Util {

bool PortAudio::latencyBounds(PaDeviceIndex input, PaDeviceIndex output, double *pmin, double *pmax)
{
    if (input < 0 && output < 0)
        return false;

    double min = 0;
    double max = INFINITY;

    const PaDeviceInfo *info;

    if (input != paNoDevice) {
        info = Pa_GetDeviceInfo(input);
        if (!info)
            return false;

        min = std::max(min, info->defaultLowInputLatency);
        max = std::min(max, info->defaultHighInputLatency);
    }

    if (output != paNoDevice) {
        info = Pa_GetDeviceInfo(output);
        if (!info)
            return false;

        min = std::max(min, info->defaultLowOutputLatency);
        max = std::min(max, info->defaultHighOutputLatency);
    }

    if (pmin)
        *pmin = min;
    if (pmax)
        *pmax = max;

    return true;
}

} // namespace Util

