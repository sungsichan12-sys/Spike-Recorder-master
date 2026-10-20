#ifndef BACKYARDBRAINS_GAMESIGNAL_H
#define BACKYARDBRAINS_GAMESIGNAL_H

#include <algorithm>

namespace BackyardBrains {
namespace GameSignal {

inline float triggerLevel(int configuredThreshold, float baseline, float sensitivity)
{
    const float safeSensitivity = std::max(0.25f, std::min(4.0f, sensitivity));
    const float configuredFloor = std::max(8.0f,
        static_cast<float>(configuredThreshold) * 0.25f);
    const float adaptiveFloor = std::max(8.0f, baseline * 1.55f + 8.0f);
    return std::max(configuredFloor, adaptiveFloor) / safeSensitivity;
}

// Convert a captured peak into a stable 0..1 game control. A peak at the
// trigger threshold maps to 0; a peak at three times the threshold maps to 1.
// Games apply this value only when signal_edge is set, so the control does not
// drift back when the input falls silent between spikes.
inline float peakPosition(float signalLevel, float trigger)
{
    if(!(signalLevel > 0.0f) || !(trigger > 0.0f))
        return 0.0f;
    const float ratio = signalLevel / trigger;
    return std::max(0.0f, std::min(1.0f, (ratio - 1.0f) * 0.5f));
}

} // namespace GameSignal
} // namespace BackyardBrains

#endif
