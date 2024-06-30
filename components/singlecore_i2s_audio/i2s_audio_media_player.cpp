#include "i2s_audio_media_player.h"

#define LED1_Pin            12        // external LED1 pin
#define LED2_Pin            13        // external LED2 pin

namespace esphome {
namespace i2s_audio {

static const char *const TAG = "audio";
bool replace(std::string& str, const std::string& from, const std::string& to) {
    size_t start_pos = str.find(from);
    if(start_pos == std::string::npos)
        return false;
    str.replace(start_pos, from.length(), to);
    return true;
}
void I2SAudioMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_media_url().has_value()) {
    optional<std::string> current_url_ = call.get_media_url();
    std::string str = current_url_.value();
    char* cStr = new char[str.size() + 1];
    strcpy(cStr, str.c_str());
    playaudio(cStr);
  }
  if (call.get_volume().has_value()) {
    this->volume = call.get_volume().value();
    this->set_volume_(volume);
    this->unmute_();
  }
  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        this->stop_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->mute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->unmute_();
        break;
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP: {
        float new_volume = this->volume + 0.1f;
        if (new_volume > 1.0f)
          new_volume = 1.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN: {
        float new_volume = this->volume - 0.1f;
        if (new_volume < 0.0f)
          new_volume = 0.0f;
        this->set_volume_(new_volume);
        this->unmute_();
        break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_PAUSE: {
          isPaused = true;
          broadcastStatus("paused");
          break;
      }
      case media_player::MEDIA_PLAYER_COMMAND_PLAY: {
          isPaused = false;
          broadcastStatus("playing");
          break;
      }
    }
  }
  this->publish_state();
}

void I2SAudioMediaPlayer::mute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(true);
  } else {
    this->set_volume_(0.0f, false);
  }
  this->muted_ = true;
}

void I2SAudioMediaPlayer::unmute_() {
  if (this->mute_pin_ != nullptr) {
    this->mute_pin_->digital_write(false);
  } else {
    this->set_volume_(this->volume, false);
  }
  this->muted_ = false;
}

void I2SAudioMediaPlayer::set_volume_(float volume, bool publish) {
  out->SetGain(volume);
  volume_level = volume;
  if (publish)
    this->volume = volume;
}

void I2SAudioMediaPlayer::stop_() {
  this->stopPlaying();
}

void I2SAudioMediaPlayer::broadcastStatus(const char* msg) {
    std::string msg_cpp(msg);
    if(msg_cpp == "paused") state = esphome::media_player::MEDIA_PLAYER_STATE_PAUSED;
    if(msg_cpp == "playing") state = esphome::media_player::MEDIA_PLAYER_STATE_PLAYING;
    if(msg_cpp == "idle") state = esphome::media_player::MEDIA_PLAYER_STATE_IDLE;
}

void I2SAudioMediaPlayer::updateLEDBrightness(int brightness_percentage) {
  return;
}

void I2SAudioMediaPlayer::stopPlaying() {
    if (mp3) {
        mp3->stop();
        delete mp3;
        mp3 = NULL;
    }
    if (wav) {
        wav->stop();
        delete wav;
        wav = NULL;
    }
    if (flac) {
        flac->stop();
        delete flac;
        flac = NULL;
    }
    if (buff) {
        buff->close();
        delete buff;
        buff = NULL;
    }
    if (file_http) {
        file_http->close();
        delete file_http;
        file_http = NULL;
    }
    if (file_progmem) {
        file_progmem->close();
        delete file_progmem;
        file_progmem = NULL;
    }

    broadcastStatus("idle");
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
    this->publish_state();
    updateLEDBrightness(0);
    ESP_LOGCONFIG(TAG, "Stoped and Idle");
}


void I2SAudioMediaPlayer::playaudio(const char* source)  {
    stopPlaying();
    ESP_LOGCONFIG(TAG, "Start Playing...");
    this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
    this->publish_state();
    file_http = new AudioFileSourceHTTPStream();
    std::string source_cpp(source);
    std::string flac_str(".flac");
    std::string mp3_str(".mp3");
    replace(source_cpp,mp3_str,flac_str);
    if ( file_http->open(source_cpp.c_str())) {
        broadcastStatus("playing");
        updateLEDBrightness(10);
        ESP_LOGCONFIG(TAG, "url:");
        ESP_LOGCONFIG(TAG, source_cpp.c_str());
        // dim while playing
        buff = new AudioFileSourceBuffer(file_http, preallocateBuffer, preallocateBufferSize);
        mp3 = new AudioGeneratorMP3();
        mp3->begin(buff, out);
    }else {
          ESP_LOGCONFIG(TAG, "file_http failed");
          stopPlaying();
    }
}

void I2SAudioMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Audio...");

  pinMode(LED1_Pin, OUTPUT); 
  pinMode(LED2_Pin, OUTPUT);
  updateLEDBrightness(0);

  out = new AudioOutputI2S();
  if(mclk_pin_ == 250) {
        out->SetPinout(bclk_pin_, lrclk_pin_, dout_pin_);
    }else {
        out->SetPinout(bclk_pin_, lrclk_pin_, dout_pin_, mclk_pin_);
    }
  out->SetGain(volume_level);
  this->volume = volume_level;
  ESP_LOGCONFIG(TAG, "Setuped");
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->publish_state();
}

void I2SAudioMediaPlayer::loop() {
      
  if (mp3   && !isPaused && !mp3->loop())          stopPlaying();
  if (wav   && !isPaused && !wav->loop())          stopPlaying();
  if(flac && !isPaused && !flac->loop()) stopPlaying();
}

media_player::MediaPlayerTraits I2SAudioMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  return traits;
}

void I2SAudioMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "dump_config:OK");

  if (this->is_failed()) {
    ESP_LOGCONFIG(TAG, "Audio failed to initialize!");
    return;
  }

}

}  // namespace i2s_audio
} // namespace esphome

