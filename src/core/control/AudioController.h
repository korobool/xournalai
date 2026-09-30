/*
 * Xournal++
 *
 * Audio Recording / Playing controller
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "config-features.h"  // for ENABLE_AUDIO
#ifdef ENABLE_AUDIO

#include <cstddef>     // for size_t
#include <functional>  // for function
#include <memory>   // for make_unique, unique_ptr
#include <vector>   // for vector

#include <portaudiocpp/PortAudioCpp.hxx>  // for AutoSystem

#include "filesystem.h"  // for path

class AudioPlayer;
class AudioRecorder;
class Control;
class DeviceInfo;
class Settings;

class AudioController final {
public:
    // Todo convert Pointers to reference (changes to control.cpp are necessary)
    AudioController(Settings* settings, Control* control);
    ~AudioController();

    bool startRecording();
    bool stopRecording();
    bool isRecording();

    /// xournalai: told when a recording starts (true) or stops (false); null to stop telling
    void setRecordingListener(std::function<void(bool)> listener);
    float takeRecordingLevel();  ///< the loudest sample (0..1) since the last call
    /// xournalai: where in the recording "now" is (ms from the file's first sample), for strokes written meanwhile
    size_t currentRecordingTime() const;

    bool isPlaying();
    bool startPlayback(fs::path const& file, unsigned int timestamp);
    void pausePlayback();
    void continuePlayback();
    void stopPlayback();
    void seekForwards();
    void seekBackwards();

    fs::path const& getAudioFilename() const;
    fs::path getAudioFolder() const;
    size_t getStartTime() const;
    std::vector<DeviceInfo> getOutputDevices() const;
    std::vector<DeviceInfo> getInputDevices() const;

private:
    Settings& settings;
    Control& control;
    std::function<void(bool)> recordingListener;

    /**
     * RAII initializer don't move below the portaudio::System::instance() calls in
     * AudioRecorder and AudioPlayer
     * */
    portaudio::AutoSystem autoSys;
    std::unique_ptr<AudioRecorder> audioRecorder;
    std::unique_ptr<AudioPlayer> audioPlayer;

    fs::path audioFilename;
    size_t timestamp = 0;
};
#else
class AudioController final {};
#endif
