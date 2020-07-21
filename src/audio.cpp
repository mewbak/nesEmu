#include "audio.h"

#include <cstdint>
#include <memory>
#include <cmath>

#include <RtAudio.h>

#include "logger.h"

// normal audio quality 44100hz
constexpr uint32_t sampleRate = 44100;

// 8 frames worth of buffer
constexpr int bufferSize = 8 * 735;

// read position of output
static size_t readPos = 0;
// write position of input
static size_t writePos = 0;
// sample buffer
static float buffer[bufferSize];

// used to prevent popping
static float lastSample = 0;

// used to prevent unnecessary resize of inBuffer
static int pushPos;
// samples added by PushSample
static std::vector<float> inBuffer;

static std::unique_ptr<RtAudio> dac;
static bool audioRunning = false;

static int AudioCallback(void* outputBuffer, void* inputBuffer, unsigned int nBufferFrames, double streamTime, RtAudioStreamStatus status, void* userData) {
	const auto outBuffer = static_cast<float*>(outputBuffer);

	if(readPos < writePos) {
		lastSample = buffer[(writePos - 1) % bufferSize];
	}
	
	size_t i = 0;
	// output number of requested samples
	for(; i < nBufferFrames && readPos < writePos; i++) {
		// copy buffer to left and right channel
		outBuffer[i * 2] = outBuffer[i * 2 + 1] = buffer[readPos % bufferSize];
		readPos++;
	}

	// if we didn't have enough samples in the buffer
	if(i < nBufferFrames) {
		// Repeat last sample to prevent popping and fade out over time
		// so single frame drops don't pop and there's no noise over time
		for(; i < nBufferFrames; i++) {
			outBuffer[i * 2] = outBuffer[i * 2 + 1] = lastSample;
			lastSample *= 0.9999;
		}
	}

	return 0;
}

bool Audio::Init() {
	try {
		dac = std::make_unique<RtAudio>();

		if(dac->getDeviceCount() >= 1) {
			RtAudio::StreamParameters parameters;
			parameters.deviceId = dac->getDefaultOutputDevice();
			parameters.nChannels = 2;
			parameters.firstChannel = 0;

			RtAudio::StreamOptions options;
			// options.flags = RTAUDIO_MINIMIZE_LATENCY; // breaks mac TODO: test non fixed sample request

			uint32_t bufferFrames = sampleRate / 60;

			dac->openStream(&parameters, nullptr, RTAUDIO_FLOAT32, sampleRate, &bufferFrames, &AudioCallback, nullptr, &options);
			dac->startStream();

			audioRunning = true;
		} else {
			logger.Log("No audio device found.\n");

			audioRunning = false;
		}
	} catch(RtAudioError & e) {
		logger.Log("Failed to initialize audio driver: %s\n", e.what());

		audioRunning = false;
	}

	return audioRunning;
}

void Audio::Dispose() {
	try {
		dac->stopStream();
	} catch(RtAudioError & e) {
		logger.Log("Failed to stop audio stream: %s\n", e.what());
	}

	if(dac->isStreamOpen()) {
		dac->closeStream();
	}
}

void Audio::Resample() {
	if(!audioRunning || pushPos == 0) {
		pushPos = 0;
		return;
	}
	
	// TODO: prevent aliasing?
	for (size_t i = 0; i < 735; i++) {
		auto pos = i / 734.0 * (pushPos - 1);
		float f = std::fmod(pos, 1);

		buffer[writePos % bufferSize] = (1 - f) * inBuffer[floor(pos)] + f * inBuffer[ceil(pos)];
		writePos++;
	}

	pushPos = 0;
}

void Audio::PushSample(float value) {
	// reduce buffer allocation by overwriting old values
	if(pushPos < inBuffer.size()) {
		inBuffer[pushPos] = value;
	} else {
		inBuffer.push_back(value);
	}
	pushPos++;
}
