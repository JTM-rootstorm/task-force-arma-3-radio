#include "SampleBuffer.hpp"
#include <algorithm>

template<>
void SampleBufferT<short>::applyStereoGain(float gainFrontLeft, float gainFrontRight) {
    auto sampleCount = getSampleCount();
    auto channels = getChannels();
	if (channels < 2 || sampleCount == 0) return;
    for (size_t i = 0; i < sampleCount * channels; i += channels) {
        (*this)[i] = static_cast<short>(std::clamp(static_cast<float>((*this)[i]) * gainFrontLeft, static_cast<float>(SHRT_MIN), static_cast<float>(SHRT_MAX)));
        (*this)[i + 1] = static_cast<short>(std::clamp(static_cast<float>((*this)[i + 1]) * gainFrontRight, static_cast<float>(SHRT_MIN), static_cast<float>(SHRT_MAX)));
    }
}

template<>
void SampleBufferT<short>::applyStereoGainRamp(float startGainFrontLeft, float startGainFrontRight, float endGainFrontLeft, float endGainFrontRight) {
    auto sampleCount = getSampleCount();
    auto channels = getChannels();
    if (channels < 2 || sampleCount == 0) return;

    if (sampleCount == 1) {
        return applyStereoGain(endGainFrontLeft, endGainFrontRight);
    }

    const auto leftStep = (endGainFrontLeft - startGainFrontLeft) / static_cast<float>(sampleCount - 1);
    const auto rightStep = (endGainFrontRight - startGainFrontRight) / static_cast<float>(sampleCount - 1);

    auto leftGain = startGainFrontLeft;
    auto rightGain = startGainFrontRight;
    for (size_t i = 0; i < sampleCount * channels; i += channels) {
        (*this)[i] = static_cast<short>(std::clamp(static_cast<float>((*this)[i]) * leftGain, static_cast<float>(SHRT_MIN), static_cast<float>(SHRT_MAX)));
        (*this)[i + 1] = static_cast<short>(std::clamp(static_cast<float>((*this)[i + 1]) * rightGain, static_cast<float>(SHRT_MIN), static_cast<float>(SHRT_MAX)));
        leftGain += leftStep;
        rightGain += rightStep;
    }
}

template<>
void SampleBufferT<short>::applyMonoGain(float gain) {
    auto sampleCount = getSampleCount();
    auto channels = getChannels();
    for (size_t i = 0; i < sampleCount * channels; i += 1)
		(*this)[i] = static_cast<short>(std::clamp(static_cast<float>((*this)[i]) * gain, static_cast<float>(SHRT_MIN), static_cast<float>(SHRT_MAX)));
}
