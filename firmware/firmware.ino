#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <Parameter.h>
#include "./config.h"

ParameterGroup _parameters;
Parameter<float> _roomSizeParameter;
Parameter<float> _mixParameter;
Parameter<float> _bufferSizeParameter;

AudioInputI2S _i2s_in;
AudioEffectFreeverb _reverb;
AudioRecordQueue _recorder;
AudioPlayQueue _player;
AudioOutputI2S _i2s_out;
AudioControlSGTL5000 _sgtl5000_1;
AudioAmplifier _dryAmp;
AudioAmplifier _wetAmp;
AudioMixer4 _mixer;

// Mono connections - only using left channel
AudioConnection in2Dry(_i2s_in, 0, _dryAmp, 0);
AudioConnection in2Reverb(_i2s_in, 0, _reverb, 0);
AudioConnection reverb2Recorder(_reverb, 0, _recorder, 0);
AudioConnection player2Wet(_player, 0, _wetAmp, 0);
AudioConnection dry2Mixer(_dryAmp, 0, _mixer, 0);
AudioConnection wet2Mixer(_wetAmp, 0, _mixer, 1);
AudioConnection mixer2OutL(_mixer, 0, _i2s_out, 0);
AudioConnection mixer2OutR(_mixer, 0, _i2s_out, 1);  // Same mono signal to right

// Dynamic buffer settings
int16_t* audioBuffer = nullptr;
int currentBufferSize = SAMPLES_PER_SECOND / 2;  // Start with 0.5 second
int targetBufferSize = SAMPLES_PER_SECOND / 2;
int bufferIndex = 0;
int playbackIndex = 0;
bool isRecording = true;
bool needToPlay = false;
unsigned long lastUpdateCheck = 0;

void updateBufferSize(int newSize) {
  newSize = constrain(newSize, MIN_BUFFER_SIZE, MAX_BUFFER_SIZE);

  // Only update if the change is significant enough
  if (abs(newSize - currentBufferSize) >= BUFFER_HYSTERESIS) {
    targetBufferSize = newSize;

    // Only perform the update when we're not in the middle of processing
    if (!isRecording && !needToPlay) {
      // Stop current processing
      _recorder.end();

      // Deallocate old buffer
      if (audioBuffer != nullptr) {
        delete[] audioBuffer;
      }

      // Allocate new buffer
      currentBufferSize = targetBufferSize;
      audioBuffer = new int16_t[currentBufferSize];

      // Reset indices and state
      bufferIndex = 0;
      playbackIndex = 0;
      isRecording = true;

      // Restart recording
      _recorder.begin();

      // Debug output in seconds
      Serial.print("Buffer size updated to: ");
      Serial.print((float)currentBufferSize / SAMPLES_PER_SECOND);
      Serial.println(" seconds");
    }
  }
}

void setup() {
  Serial.begin(9600);

  // Setup parameters
  _roomSizeParameter.setup("roomSize", 0.5, 0, 1);
  _mixParameter.setup("mix", 0.5, 0, 1);
  _bufferSizeParameter.setup("bufferSize", 0.5, 0, 1);
  _parameters.add(_roomSizeParameter);
  _parameters.add(_mixParameter);
  _parameters.add(_bufferSizeParameter);

  AudioMemory(120);

  _sgtl5000_1.enable();
  _sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  _sgtl5000_1.volume(0.5);

  _reverb.roomsize(0.8);
  _mixer.gain(0, 0.5);
  _mixer.gain(1, 0.5);

  // Initial buffer allocation
  audioBuffer = new int16_t[currentBufferSize];

  // Start recording
  _recorder.begin();
  lastUpdateCheck = millis();
}

void loop() {
  _dryAmp.gain(0.5);
  _wetAmp.gain(0.5);

  // Check for buffer size updates periodically
  unsigned long currentTime = millis();
  if (currentTime - lastUpdateCheck >= UPDATE_INTERVAL) {
    float bufferTime = map(_bufferSizeParameter * 1000, 0, 1000,
                           MIN_BUFFER_TIME * 1000, MAX_BUFFER_TIME * 1000)
                       / 1000.0f;
    int newBufferSize = bufferTime * SAMPLES_PER_SECOND;
    updateBufferSize(newBufferSize);
    lastUpdateCheck = currentTime;
  }

  // Handle recording
  if (isRecording && audioBuffer != nullptr) {
    if (_recorder.available() >= 1) {  // Only need 1 sample for mono
      memcpy(audioBuffer + bufferIndex, _recorder.readBuffer(), sizeof(int16_t));
      _recorder.freeBuffer();
      bufferIndex++;  // Increment by 1 for mono

      if (bufferIndex >= currentBufferSize) {
        isRecording = false;
        needToPlay = true;
        _recorder.end();
        reverseBuffer();
        playbackIndex = 0;
      }
    }
  }

  // Handle playback
  if (needToPlay && audioBuffer != nullptr) {
    if (_player.available() > 0) {
      if (playbackIndex < currentBufferSize) {
        int16_t* playBuffer = _player.getBuffer();
        memcpy(playBuffer, &audioBuffer[playbackIndex], sizeof(int16_t));
        _player.playBuffer();
        playbackIndex++;  // Increment by 1 for mono
      } else {
        bufferIndex = 0;
        playbackIndex = 0;
        isRecording = true;
        needToPlay = false;
        _recorder.begin();
      }
    }
  }
}

void reverseBuffer() {
  if (audioBuffer == nullptr) return;

  // Simple reversal for mono buffer
  for (int i = 0; i < currentBufferSize / 2; i++) {
    int16_t temp = audioBuffer[i];
    audioBuffer[i] = audioBuffer[currentBufferSize - 1 - i];
    audioBuffer[currentBufferSize - 1 - i] = temp;
  }
}