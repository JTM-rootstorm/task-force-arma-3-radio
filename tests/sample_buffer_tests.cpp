#include "../ts/src/SampleBuffer.hpp"

#include <iostream>

namespace {
int failures = 0;
#define CHECK(expression) do { if (!(expression)) { std::cerr << "FAIL line " << __LINE__ << ": " #expression "\n"; ++failures; } } while (false)
}

int main() {
    short monoData[] = { 100, -200 };
    SampleBuffer mono(monoData, 2, 1);
    auto monoCopy = mono.copy();
    CHECK(monoCopy.getChannels() == 1);
    CHECK(monoCopy.getSampleCount() == 2);

    short stereoData[] = { 1, 2, 3, 4 };
    SampleBuffer stereo(stereoData, 2, 2);
    SampleBuffer ownedTarget(1, 1);
    CHECK(stereo.copyTo(ownedTarget));
    CHECK(ownedTarget.getChannels() == 2);
    CHECK(ownedTarget.getSampleCount() == 2);
    CHECK(ownedTarget[3] == 4);

    short borrowedData[] = { 9, 9 };
    SampleBuffer borrowed(borrowedData, 2, 1);
    CHECK(!stereo.copyTo(borrowed));
    CHECK(borrowedData[0] == 9);

    mono.applyStereoGain(2.0f, 2.0f);
    CHECK(monoData[0] == 100);
    short loud[] = { 20000, -20000 };
    SampleBuffer clamped(loud, 1, 2);
    clamped.applyStereoGain(2.0f, 2.0f);
    CHECK(loud[0] == SHRT_MAX);
    CHECK(loud[1] == SHRT_MIN);
    return failures == 0 ? 0 : 1;
}
