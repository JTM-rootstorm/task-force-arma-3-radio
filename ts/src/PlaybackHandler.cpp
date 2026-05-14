#include "PlaybackHandler.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

#include "helpers.hpp"
#include <dspfilters/Filter.h>
#include <dspfilters/Butterworth.h>
#include "clientData.hpp"
#include "task_force_radio.hpp"
#include "Teamspeak.hpp"
#include "Logger.hpp"
#include <filesystem>

namespace {

constexpr unsigned int kFrontStereoMask = 0x1u | 0x2u;
constexpr unsigned int kMonoSpeakerMask = 0x40000000u;
constexpr unsigned int kKnownPlaybackMask = kFrontStereoMask | kMonoSpeakerMask;

#ifdef _WIN32
constexpr std::uint32_t kTeamSpeakMixerSampleRate = 48000;
#else
constexpr std::uint32_t kTeamSpeakMixerSampleRate = 48000;
#endif

std::uint32_t getTeamSpeakMixerSampleRate() {
#ifdef _WIN32
    return kTeamSpeakMixerSampleRate;
#else
    if (const auto* env = std::getenv("TFAR_TS_MIXER_RATE")) {
        char* end = nullptr;
        const auto value = std::strtoul(env, &end, 10);
        if (end != env && *end == '\0' && value >= 8000 && value <= 192000) {
            return static_cast<std::uint32_t>(value);
        }
    }
    return kTeamSpeakMixerSampleRate;
#endif
}

bool isSupportedWavFormat(const clunk::WavFile& wav) {
    return wav.ok() &&
           wav._spec.channels >= 1 &&
           wav._spec.format == clunk::AudioSpec::S16 &&
           wav._spec.sample_rate > 0;
}

std::uint32_t normalizeSourceSampleRate(std::uint32_t sourceSampleRate) {
    return sourceSampleRate == 0 ? getTeamSpeakMixerSampleRate() : sourceSampleRate;
}

short readStereoSourceSample(const short* samples, size_t frame, uint8_t channels, uint8_t channel) {
    const auto sourceChannel = channels == 1 ? 0 : std::min<std::uint8_t>(channel, channels - 1);
    return samples[(frame * channels) + sourceChannel];
}

short interpolateSample(const short* samples, size_t sourceFrames, uint8_t channels, double sourceFrame, uint8_t channel) {
    const auto baseFrame = std::min(static_cast<size_t>(sourceFrame), sourceFrames - 1);
    const auto nextFrame = std::min(baseFrame + 1, sourceFrames - 1);
    const auto fraction = sourceFrame - static_cast<double>(baseFrame);
    const auto first = static_cast<double>(readStereoSourceSample(samples, baseFrame, channels, channel));
    const auto second = static_cast<double>(readStereoSourceSample(samples, nextFrame, channels, channel));
    const auto mixed = first + ((second - first) * fraction);
    return static_cast<short>(std::clamp(mixed,
                                         static_cast<double>(std::numeric_limits<short>::min()),
                                         static_cast<double>(std::numeric_limits<short>::max())));
}

void appendStereoFrames(std::vector<short>& target, const short* samples, size_t sampleCount, uint8_t channels) {
    if (samples == nullptr || sampleCount == 0 || channels == 0) return;

    const auto oldSize = target.size();
    target.resize(oldSize + (sampleCount * 2));
    auto* output = target.data() + oldSize;
    for (size_t frame = 0; frame < sampleCount; ++frame) {
        output[(frame * 2)] = readStereoSourceSample(samples, frame, channels, 0);
        output[(frame * 2) + 1] = readStereoSourceSample(samples, frame, channels, 1);
    }
}

void appendStereoFramesAtRate(std::vector<short>& target, const short* samples, size_t sampleCount, uint8_t channels, std::uint32_t sourceSampleRate) {
    if (samples == nullptr || sampleCount == 0 || channels == 0) return;

    sourceSampleRate = normalizeSourceSampleRate(sourceSampleRate);
    const auto outputSampleRate = getTeamSpeakMixerSampleRate();
    if (sourceSampleRate == outputSampleRate) {
        appendStereoFrames(target, samples, sampleCount, channels);
        return;
    }

    const auto outputFrames = std::max<size_t>(1, ((sampleCount * outputSampleRate) + (sourceSampleRate / 2)) / sourceSampleRate);
    const auto oldSize = target.size();
    target.resize(oldSize + (outputFrames * 2));
    auto* output = target.data() + oldSize;
    const auto sourceStep = static_cast<double>(sourceSampleRate) / static_cast<double>(outputSampleRate);
    for (size_t outputFrame = 0; outputFrame < outputFrames; ++outputFrame) {
        const auto sourceFrame = std::min(static_cast<double>(sampleCount - 1), static_cast<double>(outputFrame) * sourceStep);
        output[(outputFrame * 2)] = interpolateSample(samples, sampleCount, channels, sourceFrame, 0);
        output[(outputFrame * 2) + 1] = interpolateSample(samples, sampleCount, channels, sourceFrame, 1);
    }
}

} // namespace

std::string SoundFile::getFullPath() const {
    return TFAR::getInstance().getPluginPath() + fileName + ".wav";
}


PlaybackHandler::PlaybackHandler() {
    TFAR::getInstance().doDiagReport.connect([this](std::stringstream& diag) {
        diag << "PH:\n";
        std::array<std::string_view, 4> typeToString{ "base"sv, "stereo"sv, "raw"sv, "processing"sv };

        for (auto& it : playbacks) {
            diag << TS_INDENT << "PB: " << it.first << ":\n";
            diag << TS_INDENT << TS_INDENT << "Type: " << typeToString[static_cast<uint8_t>(it.second->type())] << "\n";
            diag << TS_INDENT << TS_INDENT << "samplesReady: " << it.second->samplesReady() << "\n";
            diag << TS_INDENT << TS_INDENT << "isDone: " << it.second->isDone() << "\n";
        }

        for (auto& it : wavCache) {
            diag << TS_INDENT << "WC: " << it.first << ":\n";
        }
    });
}

void PlaybackHandler::onEditMixedPlaybackVoiceDataEvent(short * samples, int sampleCount, int channels, const unsigned int *, unsigned int * channelFillMask) {
    ProfileFunction;
    LockGuard_exclusive lock(playbackCriticalSection);
    bool fill = false;
    std::vector<std::string> to_remove;
    const auto filledMask = channelFillMask ? *channelFillMask : 0u;
    const auto requiredMask = channels == 1 ? kMonoSpeakerMask : kFrontStereoMask;
    if ((filledMask & kKnownPlaybackMask) == 0) {
        memset(samples, 0, sampleCount * channels * sizeof(short));
    }
    for (auto& [name,playback] : playbacks) {
        const short* playbackSamples = nullptr;
        const size_t playbackSampleCount = playback->getSamples(playbackSamples);
        if (playbackSampleCount == 0) continue;

        int outputPosition = 0;
        int inputPosition = 0;
        if (channels == 1) {
            while (outputPosition < sampleCount && (static_cast<int>(playbackSampleCount) - inputPosition) >= 2) {
                const auto left = static_cast<int>(playbackSamples[inputPosition]);
                const auto right = static_cast<int>(playbackSamples[inputPosition + 1]);
                const auto mixed = static_cast<short>((left + right) / 2);
                samples[outputPosition] = std::clamp(samples[outputPosition] + mixed, SHRT_MIN, SHRT_MAX);
                outputPosition++;
                inputPosition += 2;
                fill = true;
            }
            playback->cleanSamples(inputPosition);
            if (playback->isDone()) {
                to_remove.push_back(name);
            }
            continue;
        }

        //mix stereo sound into multichannel sound
        while (outputPosition < sampleCount * channels && (static_cast<int>(playbackSampleCount) - inputPosition) > 0) {
            for (int q = 0; q < 2; q++) {
#ifdef _DEBUG
                if (outputPosition > sampleCount * channels) __debugbreak();
                if (inputPosition > playbackSampleCount) __debugbreak();

#endif   
                const auto s = playbackSamples[inputPosition];

                samples[outputPosition] = std::clamp(samples[outputPosition] + s, SHRT_MIN, SHRT_MAX);

                outputPosition++;
                inputPosition++;
                fill = true;
            }
            outputPosition += std::max(channels - 2, 0);
        }
        playback->cleanSamples(inputPosition);
        if (playback->isDone()) {
            to_remove.push_back(name);
        }
    }
    for (const auto& it : to_remove) {
        playbacks.erase(it);
    }

    if (fill && channelFillMask) *channelFillMask |= requiredMask;
}

void PlaybackHandler::appendPlayback(std::string name, SoundFile file) {
    if (file.type == SoundFile::SoundFileType::PluginFolderFile) return appendPlayback(name, file, stereoMode::stereo, 1.f);

    LockGuard_exclusive lock(playbackCriticalSection);
    if (playbacks.count(name) == 0) {
        std::shared_ptr<playbackWavRaw> d = std::make_shared<playbackWavRaw>();
        playbacks[name] = d;
        d->appendSamples(file.samples.data(), file.samples.size(), file.channels);
    } else {
        if (playbacks[name]->type() == playbackType::raw) {
            std::static_pointer_cast<playbackWavRaw>(playbacks[name])->appendSamples(file.samples.data(), file.samples.size(), file.channels);
        } else {
            MessageBoxA(0, "void PlaybackHandler::appendPlayback(std::string name, SoundFile file) 93", "tfar", 0);
            __debugbreak();
            //should not have different playback types with same name.
        }
    }
}

void PlaybackHandler::appendPlayback(std::string name, SoundFile file, stereoMode stereo, float gain) {
    LockGuard_exclusive lock(playbackCriticalSection);
    if (playbacks.count(name) == 0) {
        if (file.type == SoundFile::SoundFileType::PluginFolderFile) {
            
            if (auto wave = getWavFileFromPath(file.getFullPath()))
                playbacks[name] = std::make_shared<playbackWavStereo>(wave.get(), stereo, gain);//we can use wave.get() because playbackWavStereo doesn't hold a ref to it
        } else {
            playbacks[name] = std::make_shared<playbackWavStereo>(file.samples.data(), file.samples.size(), file.channels, stereo, gain);
        }
    } else {
        MessageBoxA(0, "void PlaybackHandler::appendPlayback(std::string name, SoundFile file, stereoMode stereo, float gain) 110", "tfar", 0);
        __debugbreak();
        //should not have different playback types with same name.
        //wavStereo types cannot append
    }
}

void PlaybackHandler::appendPlayback(std::string name, SoundFile file, std::vector<std::function<void(SampleBuffer&)>> functors) {
    LockGuard_exclusive lock(playbackCriticalSection);
    if (playbacks.count(name) == 0) {
        if (file.type == SoundFile::SoundFileType::PluginFolderFile) {
            if (auto wave = getWavFileFromPath(file.getFullPath()))
                playbacks[name] = std::make_shared<playbackWavProcessing>(static_cast<short*>(wave->_data.get_ptr()), (wave->_data.get_size() / sizeof(short)) / wave->_spec.channels, wave->_spec.channels, functors, wave->_spec.sample_rate);
        } else {
            playbacks[name] = std::make_shared<playbackWavProcessing>(file.samples.data(), file.samples.size(), file.channels, functors);
        }
    } else {
        MessageBoxA(0, "void PlaybackHandler::appendPlayback(std::string name, SoundFile file, functors) 128", "tfar", 0);
        __debugbreak();
    }
}

void PlaybackHandler::playWavFile(SoundFile file) {
    if (!Teamspeak::isConnected()) return;
    if (file.type != SoundFile::SoundFileType::PluginFolderFile) return playWavFile(Teamspeak::getCurrentServerConnection(), file, 1.0, stereoMode::stereo);
    Teamspeak::playWavFile(file.getFullPath());
}

void PlaybackHandler::playWavFile(TSServerID serverConnectionHandlerID, SoundFile file, float gain, stereoMode stereo) {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _mm_setcsr((_mm_getcsr() & ~0x0040) | (0x0040));//_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    if (!Teamspeak::isConnected(serverConnectionHandlerID)) return;

#ifndef _WIN32
    if (file.type == SoundFile::SoundFileType::PluginFolderFile && std::getenv("TFAR_LINUX_CUSTOM_WAV_MIXER") == nullptr) {
        Teamspeak::playWavFile(file.getFullPath());
        return;
    }
#endif

    appendPlayback(file.fileName + std::to_string(rand()), file, stereo, gain);
}

void PlaybackHandler::playWavFile(TSServerID serverConnectionHandlerID, SoundFile file, float gain, Position3D position, bool onGround, int radioVolume, bool underwater, float vehicleVolumeLoss, bool vehicleCheck, stereoMode stereoMode) {
    _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
    _mm_setcsr((_mm_getcsr() & ~0x0040) | (0x0040));//_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
    if (!Teamspeak::isConnected(serverConnectionHandlerID)) return;

    const auto clientDataDir = TFAR::getServerDataDirectory()->getClientDataDirectory(serverConnectionHandlerID);
    if (!clientDataDir) return;

    std::vector<std::function<void(SampleBuffer&)>> processors;
    const auto id = file.fileName + std::to_string(rand());
    //apply gain
    if (gain != 1.0f)
        processors.emplace_back([gain](SampleBuffer& samples) {
        samples.applyGain(gain);
    });


    //processors.push_back([gain](short* samples, size_t sampleCount, uint8_t channels) {
    //    std::ofstream f("P:/out.raw", std::ios::binary);
    //    for (size_t i = sampleCount; i < sampleCount * channels; i++)
    //        samples[i] = std::numeric_limits<short>::lowest();
    //
    //
    //    for (size_t i = 0; i < sampleCount * channels; i++)
    //        f.write((char*)&samples[i],2);
    //    f.close();
    //});

    auto myClientData = clientDataDir->myClientData;

    execAtReturn ret([this, &id, &file, &processors]() {
        appendPlayback(id, file, processors);
    });

    if (!myClientData) return;

    const auto speakerDistance = (radioVolume / 10.f) * TFAR::getInstance().m_gameData.speakerDistance;
    const auto distanceFromRadio = position.distanceTo(myClientData->getClientPosition());

    if (vehicleVolumeLoss > 0.01f && !vehicleCheck)
        //In vehicle filter
        processors.emplace_back([id, distanceFromRadio, speakerDistance, vehicleVolumeLoss, myClientDataWeak = std::weak_ptr<clientData>(myClientData)](SampleBuffer& samples) {
        if (auto myClientData = myClientDataWeak.lock()) {
            helpers::processFilterStereo<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<2>, MAX_CHANNELS>>(samples, helpers::volumeAttenuation(distanceFromRadio, true, round(speakerDistance), 1.0f - vehicleVolumeLoss) * pow(1.0f - vehicleVolumeLoss, 1.2f), myClientData->effects.getFilterVehicle(id + "vehicle", vehicleVolumeLoss));
            myClientData->effects.removeFilterVehicle(id + "vehicle");

        }
    });

    if (onGround) {
        //Speaker effect
        processors.emplace_back([id, distanceFromRadio, speakerDistance, myClientDataWeak = std::weak_ptr<clientData>(myClientData)](SampleBuffer& samples) {
            samples.applyGain(helpers::volumeAttenuation(distanceFromRadio, true, round(speakerDistance)));
            if (auto myClientData = myClientDataWeak.lock()) {
                helpers::processFilterStereo<Dsp::SimpleFilter<Dsp::Butterworth::BandPass<1>, MAX_CHANNELS>>(samples, SPEAKER_GAIN, myClientData->effects.getSpeakerFilter(id));
                myClientData->effects.removeSpeakerFilter(id);
            }
        });

        if (underwater) {
            //Underwater Speaker effect
            processors.emplace_back([id, myClientDataWeak = std::weak_ptr<clientData>(myClientData)](SampleBuffer& samples) {
                if (auto myClientData = myClientDataWeak.lock()) {
                    helpers::processFilterStereo<Dsp::SimpleFilter<Dsp::Butterworth::LowPass<4>, MAX_CHANNELS>>(samples, CANT_SPEAK_GAIN * 50, myClientData->effects.getFilterCantSpeak(id));
                    myClientData->effects.removeFilterCantSpeak(id);
                }
            });
        }

        //3D Positioning
        if (!position.isNull())
            if (file.type == SoundFile::SoundFileType::PluginFolderFile) //Clunk can't handle these somehow
                processors.emplace_back([position, myClientDataWeak = std::weak_ptr<clientData>(myClientData), id](SampleBuffer& samples) {
                    auto myClientData = myClientDataWeak.lock();
                    if (!myClientData) return;
                    auto pClunk = myClientData->effects.getClunk(id);
                    auto relativePos = myClientData->getClientPosition().directionTo(position);
                    auto viewDirection = myClientData->getViewDirection().toAngle();
                    helpers::applyILD(samples, relativePos, viewDirection); //interaural level difference
                    myClientData->effects.removeClunk(id);
                });
            else
                processors.emplace_back([position, myClientDataWeak = std::weak_ptr<clientData>(myClientData), id](SampleBuffer& samples) {
                    auto myClientData = myClientDataWeak.lock();
                    if (!myClientData) return;
                    auto pClunk = myClientData->effects.getClunk(id);
                    auto relativePos = myClientData->getClientPosition().directionTo(position);
                    auto viewDirection = myClientData->getViewDirection().toAngle();
                    pClunk->process(samples, relativePos, viewDirection); //interaural time difference
                    helpers::applyILD(samples, relativePos, viewDirection); //interaural level difference
                    myClientData->effects.removeClunk(id);
                });

    } else {
        //muting for stereo mode
        if (stereoMode != stereoMode::stereo)
            processors.emplace_back([stereoMode](SampleBuffer& samples) {
            auto sampleCount = samples.getSampleCount();
            auto channels = samples.getChannels();
            //if (false && channels == 2) { //#TODO this optimization was a fail.
            //    //Performance opt using 32bit operations instead of 16 bit ones
            //    uint32_t* samples32bit = reinterpret_cast<uint32_t*>(samples);
            //    if (stereoMode == stereoMode::leftOnly) {//Only left
            //        for (size_t q = 0; q < sampleCount / 2; q += 1)
            //            samples32bit[q] &= 0xFFFF0000;//mute right channel
            //    } else if (stereoMode == stereoMode::rightOnly)//Only right
            //        for (size_t q = 0; q < sampleCount / 2; q += 1)
            //            samples32bit[q] &= 0x0000FFFF;//mute left channel
            //} else {
                if (stereoMode == stereoMode::leftOnly) {//Only left
                    for (size_t q = 0; q < sampleCount * channels; q += channels)
                        samples[q + 1] = 0;//mute right channel
                } else if (stereoMode == stereoMode::rightOnly)//Only right
                    for (size_t q = 0; q < sampleCount * channels; q += channels)
                        samples[q] = 0;//mute left channel
            //}
        });

    }
}

std::shared_ptr<clunk::WavFile> PlaybackHandler::getWavFileFromPath(const std::string& filePath) {
    if (auto found = wavCache.find(filePath); found != wavCache.end())
        return found->second;

    auto p = std::filesystem::u8path(filePath.c_str()); //Handle utf8 input path

    if (FILE *f =
#ifdef _MSC_VER
        _wfopen(p.c_str(), L"rb")
#else
        std::fopen(p.c_str(), "rb")
#endif
        ) {
        std::shared_ptr<clunk::WavFile> wav = std::make_shared<clunk::WavFile>(f);
        wav->read();
        if (isSupportedWavFormat(*wav)) {
            wavCache[filePath] = wav;
        } else {
            Logger::log(LoggerTypes::teamspeakClientlog, "Cannot read Soundfile: " + filePath, LogLevel::LogLevel_ERROR);
            return {};
        }
        fclose(f);
        return wav;
    }

    Logger::log(LoggerTypes::teamspeakClientlog, "Cannot open Soundfile: " + filePath + " " + strerror(errno), LogLevel::LogLevel_ERROR);
    return {};
}

void playbackWavStereo::construct(clunk::WavFile* wavFile, stereoMode stereo, float gain) {
    if (isSupportedWavFormat(*wavFile)) {
        construct(static_cast<short*>(wavFile->_data.get_ptr()), (wavFile->_data.get_size() / sizeof(short)) / wavFile->_spec.channels, wavFile->_spec.channels, stereo, gain, wavFile->_spec.sample_rate);
    } else if (wavFile->ok()) {
        MessageBoxA(0, "Unknown audio file has invalid format.", "Task Force Arrowhead Radio", MB_OK);
    }
}

void playbackWavStereo::construct(std::string wavFilePath, stereoMode stereo, float gain) {
    if (FILE *f = fopen(wavFilePath.c_str(), "rb")) {
        auto wav = new clunk::WavFile(f);
        wav->read();
        if (!isSupportedWavFormat(*wav)) {
            const auto message = "File " + wavFilePath + " has invalid format.";
            MessageBoxA(0, message.c_str(), "Task Force Arrowhead Radio", MB_OK);
        } else {
            construct(wav, stereo, gain);
        }
        fclose(f);
        delete wav;
    } else {
        log_string("Can't Open file " + wavFilePath, LogLevel_ERROR);
    }
}

void playbackWavStereo::construct(const short* samples, size_t sampleCount, uint8_t channels, stereoMode stereo, float gain, std::uint32_t sourceSampleRate) {
    appendStereoFramesAtRate(sampleStore, samples, sampleCount, channels, sourceSampleRate);

    //Behaviour of this code changed as you can see, we already copy samples into sampleStore, so we don't need to copy anything. We just need to set stuff to 0


    if (stereo == stereoMode::stereo) {
        //if (channels != 2) {
        //    const auto target = sampleStore.data();
        //    uint32_t posInTarget = 0;
        //    for (uint32_t q = 0; q < sampleCount*channels; q += channels) {
        //        target[posInTarget++] = samples[q];//copy left channel
        //        target[posInTarget++] = samples[q + 1];//copy right channel
        //    }
        //}
    } else if (stereo == stereoMode::leftOnly) {
        const auto target = sampleStore.data();
        uint32_t posInTarget = 0;
        for (uint32_t q = 0; q < sampleStore.size(); q += 2) {
            //target[posInTarget++] = samples[q];//only copy left channel
            posInTarget++; //leave left channel
            target[posInTarget++] = 0;//set right channel 0
            //posInTarget++;//leave right channel 0
        }
    } else if (stereo == stereoMode::rightOnly) {
        const auto target = sampleStore.data();
        uint32_t posInTarget = 0;
        for (uint32_t q = 0; q < sampleStore.size(); q += 2) {
            //posInTarget++;//leave left channel 0
            //target[posInTarget++] = samples[q + 1];//only copy right channel

            target[posInTarget++] = 0;//set left channel 0
            posInTarget++;//leave right channel 0

        }
    }
    SampleBuffer(sampleStore.data(), sampleStore.size() / 2, 2).applyGain(gain);
}

playbackWavStereo::playbackWavStereo(const short* samples, size_t sampleCount, uint8_t channels, stereoMode stereo, float gain /*= 1.0f*/) : currentPosition(0) {
    construct(samples, sampleCount, channels, stereo, gain);
}

playbackWavStereo::playbackWavStereo(clunk::WavFile* wavFile, stereoMode stereo, float gain /*= 1.0f*/) : currentPosition(0) {
    construct(wavFile, stereo, gain);
}

playbackWavStereo::playbackWavStereo(std::string wavFilePath, stereoMode stereo, float gain) : currentPosition(0) {
    construct(wavFilePath, stereo, gain);
}

playbackWavStereo::~playbackWavStereo() {
    sampleStore.clear();
}

size_t playbackWavStereo::getSamples(const short* &data) {
    const auto end = sampleStore.data() + sampleStore.size();
    data = std::min(sampleStore.data() + currentPosition, end);
    return end - (sampleStore.data() + currentPosition);
}

size_t playbackWavStereo::cleanSamples(size_t sampleCount) {
    const auto increase = std::min(sampleCount, sampleStore.size() - currentPosition);
    currentPosition += increase;
    if (isDone()) {
        sampleStore.clear();
        currentPosition = 0;
    }
    return increase;
}


playbackWavRaw::playbackWavRaw() :currentPosition(0) {
#ifdef DEBUG_PLAYBACK_TIMES
    creation = std::chrono::high_resolution_clock::now();
#endif
}

size_t playbackWavRaw::getSamples(const short*& data) {
#ifdef DEBUG_PLAYBACK_TIMES
    std::call_once(flag1, [this]() {
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - creation).count();
        log_string("raw use " + std::to_string(duration), LogLevel_WARNING);
    });
#endif
#ifndef isCI 
    const auto debugEnd = sampleStore.data() + sampleStore.size();
    if (sampleStore.data() + currentPosition >= debugEnd) {
        std::stringstream str;
        str << "playbackWavRaw::getSamples tried read beyond end!! " << sampleStore.size() << currentPosition;
        str << "offs " << sampleStore.data() + currentPosition << debugEnd;
        Logger::log(LoggerTypes::teamspeakClientlog, str.str(), LogLevel_WARNING);
    }
#endif

    const auto end = sampleStore.data() + sampleStore.size();
    data = std::min(sampleStore.data() + currentPosition, end);
    return end - (sampleStore.data() + currentPosition);
}

size_t playbackWavRaw::cleanSamples(size_t sampleCount) {
    const auto increase = std::min(sampleCount, sampleStore.size() - currentPosition);
    sampleStore.erase(sampleStore.begin(), sampleStore.begin() + increase);
    // currentPosition += increase;
    if (isDone()) {
        sampleStore.clear();
        currentPosition = 0;
    }
    return increase;
}

void playbackWavRaw::appendSamples(const short* samples, size_t sampleCount, uint8_t channels) {
    appendStereoFrames(sampleStore, samples, sampleCount, channels);
}

playbackWavProcessing::playbackWavProcessing(const short* samples, size_t sampleCount, int channels, std::vector<std::function<void(SampleBuffer&)>> processors, std::uint32_t sourceSampleRate)
    : currentPosition(0), processingDone(false), myThread(nullptr) {
    functors = processors;
    appendStereoFramesAtRate(sampleStore, samples, sampleCount, static_cast<uint8_t>(channels), sourceSampleRate);
    const auto processedSampleCount = sampleStore.size() / 2;
#ifdef DEBUG_PLAYBACK_TIMES
    std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
    myThread = new std::thread([this, processedSampleCount]() {
        ProfileFunctionN("process audio functors");
        SampleBuffer buf(sampleStore.data(), processedSampleCount, 2);
        for (auto& it : functors) {
            it(buf);
        }
        processingDone = true;
    });
#ifdef DEBUG_PLAYBACK_TIMES
    std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
    creation = t2;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    log_string("processing init " + std::to_string(duration), LogLevel_WARNING);
#endif
}

size_t playbackWavProcessing::getSamples(const short*& data) {
    if (!processingDone) {
#ifdef DEBUG_PLAYBACK_TIMES
        //This was used to diagnose the time myThread->join() would block if it wasn't done yet
        std::chrono::high_resolution_clock::time_point t1 = std::chrono::high_resolution_clock::now();
#endif
        //when someone implements waiting again consider next comment
        //Add possibility to wait instead of skipping (add mutex lock so we actually wait for the thread to end) 
        //myThread->join(); //if thread doesnt exist here something is seriously wrong.. Should check for nullptr
        //delete myThread;
#ifdef DEBUG_PLAYBACK_TIMES
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
        log_string("processing skip " + std::to_string(duration), LogLevel_WARNING);
#endif
        return 0;
    }
#ifdef DEBUG_PLAYBACK_TIMES
    std::call_once(flag1, [this]() {
        std::chrono::high_resolution_clock::time_point t2 = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(t2 - creation).count();
        log_string("processing use " + std::to_string(duration), LogLevel_WARNING);
    });
#endif
    const auto end = sampleStore.data() + sampleStore.size();
    data = std::min(sampleStore.data() + currentPosition, end);
    return end - (sampleStore.data() + currentPosition);
}

size_t playbackWavProcessing::cleanSamples(size_t sampleCount) {
    const auto increase = std::min(sampleCount, sampleStore.size() - currentPosition);
    currentPosition += increase;
    if (isDone()) {
        sampleStore.clear();
        currentPosition = 0;
    }
    return increase;
}

bool playbackWavProcessing::samplesReady() {
    return true;//implement threading
}
