#include "audio_player.h"

#include <LittleFS.h>
#include <driver/i2s.h>

namespace {

const i2s_port_t I2S_PORT = I2S_NUM_0;

// Minimal WAV header fields we actually need. Real WAV files can have extra
// chunks before "data" (LIST, fact...) - handled below by scanning for the
// "data" chunk instead of assuming a fixed 44-byte header.
struct WavFormat {
  uint16_t numChannels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint32_t dataSize = 0;
};

bool readWavHeader(File &file, WavFormat &format) {
  char riffId[4];
  file.readBytes(riffId, 4);
  if (memcmp(riffId, "RIFF", 4) != 0) return false;

  file.seek(file.position() + 4); // overall file size, unused
  char waveId[4];
  file.readBytes(waveId, 4);
  if (memcmp(waveId, "WAVE", 4) != 0) return false;

  while (file.available()) {
    char chunkId[4];
    if (file.readBytes(chunkId, 4) != 4) return false;
    uint32_t chunkSize = 0;
    file.readBytes(reinterpret_cast<char *>(&chunkSize), 4);

    if (memcmp(chunkId, "fmt ", 4) == 0) {
      uint16_t audioFormat = 0;
      file.readBytes(reinterpret_cast<char *>(&audioFormat), 2);
      file.readBytes(reinterpret_cast<char *>(&format.numChannels), 2);
      file.readBytes(reinterpret_cast<char *>(&format.sampleRate), 4);
      file.seek(file.position() + 6); // byteRate + blockAlign, unused
      file.readBytes(reinterpret_cast<char *>(&format.bitsPerSample), 2);
      file.seek(file.position() + (chunkSize - 16));
    } else if (memcmp(chunkId, "data", 4) == 0) {
      format.dataSize = chunkSize;
      return format.numChannels > 0 && format.sampleRate > 0 && format.bitsPerSample == 16;
    } else {
      file.seek(file.position() + chunkSize);
    }
  }
  return false;
}

} // namespace

namespace AudioPlayer {

bool begin(int pinBck, int pinWs, int pinData) {
  if (!LittleFS.begin(true)) {
    Serial.println("AudioPlayer: LittleFS mount failed");
    return false;
  }

  i2s_config_t config = {
      .mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX),
      .sample_rate = 44100, // reconfigured per file in playWavFile()
      .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_STAND_I2S,
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
      .dma_buf_count = 8,
      .dma_buf_len = 256,
      .use_apll = false,
      .tx_desc_auto_clear = true,
  };
  if (i2s_driver_install(I2S_PORT, &config, 0, nullptr) != ESP_OK) return false;

  i2s_pin_config_t pins = {
      .bck_io_num = pinBck,
      .ws_io_num = pinWs,
      .data_out_num = pinData,
      .data_in_num = I2S_PIN_NO_CHANGE,
  };
  return i2s_set_pin(I2S_PORT, &pins) == ESP_OK;
}

bool playWavFile(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.printf("AudioPlayer: cannot open %s\n", path);
    return false;
  }

  WavFormat format;
  if (!readWavHeader(file, format)) {
    Serial.println("AudioPlayer: unsupported or malformed WAV file");
    file.close();
    return false;
  }

  i2s_set_clk(I2S_PORT, format.sampleRate, I2S_BITS_PER_SAMPLE_16BIT,
              format.numChannels == 2 ? I2S_CHANNEL_STEREO : I2S_CHANNEL_MONO);

  uint8_t buffer[512];
  size_t remaining = format.dataSize;
  while (remaining > 0 && file.available()) {
    size_t toRead = min(sizeof(buffer), remaining);
    size_t bytesRead = file.readBytes(reinterpret_cast<char *>(buffer), toRead);
    if (bytesRead == 0) break;

    size_t bytesWritten = 0;
    i2s_write(I2S_PORT, buffer, bytesRead, &bytesWritten, portMAX_DELAY);
    remaining -= bytesRead;
  }

  file.close();
  return true;
}

} // namespace AudioPlayer
