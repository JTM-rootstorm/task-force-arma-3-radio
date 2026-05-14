#include "SampleBuffer.hpp"
#include <algorithm>
#include <emmintrin.h>
#include "helpers.hpp"

template<>
void SampleBufferT<short>::applyStereoGain(float gainFrontLeft, float gainFrontRight) {
    auto sampleCount = getSampleCount();
    auto channels = getChannels();
    size_t leftOver = sampleCount * channels;
#ifdef _WIN32
    if (CAN_USE_SSE_ON(begin())) {
        //Can use SSE and memory is correctly aligned
        leftOver = (sampleCount * channels) % 8;
        __m128 xmm3;
        float multiplier[4] = { gainFrontLeft, gainFrontRight, gainFrontLeft, gainFrontRight };
        //This is limiting to 4 channels max. But If we implement surround sound we don't really need a center
        xmm3 = _mm_loadu_ps(multiplier);
        helpers::shortFloatMultEx(begin(), (sampleCount * channels) - leftOver, xmm3);
    }
#endif
    for (size_t i = sampleCount * channels - leftOver; i < sampleCount * channels; i += channels) {
        (*this)[i] = static_cast<short>((*this)[i] * gainFrontLeft);
        (*this)[i + 1] = static_cast<short>((*this)[i + 1] * gainFrontRight);
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
    size_t leftOver = sampleCount * channels;
    if (CAN_USE_SSE_ON(begin())) {
        //Can use SSE and memory is correctly aligned
        leftOver = (sampleCount * channels) % 8;
        float multiplier[4] = { gain, gain, gain, gain };
        //This is limiting to 4 channels max. But If we implement surround sound we don't really need a center
        helpers::shortFloatMultEx(begin(), (sampleCount * channels) - leftOver, _mm_loadu_ps(multiplier));
    }
    for (size_t i = sampleCount * channels - leftOver; i < sampleCount * channels; i += 1)
        (*this)[i] = static_cast<short>((*this)[i] * gain);
}
