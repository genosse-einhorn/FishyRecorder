#ifndef UTIL_PORTAUDIO_H
#define UTIL_PORTAUDIO_H

#include <portaudio.h>

namespace Util {

namespace PortAudio {
    bool latencyBounds(PaDeviceIndex input, PaDeviceIndex output, double *min, double *max);
}

} // namespace Util

#endif // UTIL_PORTAUDIO_H
