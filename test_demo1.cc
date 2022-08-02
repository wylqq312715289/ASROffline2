#include <thread>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "frontend/wav.h"
#include "include/ydasr.h"

using namespace std;
using namespace wenet;
using namespace ydasroffline;

bool OfflineRun(std::string modelpath, std::string wavfile) {

  if (!ydasroffline::LoadConf(modelpath)) {
    cout << "LoadConf error";
    return false;
  }
  if (!ydasroffline::InitMem()) {
    cout << "InitMem error";
    return false;
  }
  if (!ydasroffline::InitASREngine("cn")) {
    cout << "InitASREngine error";
    return false;
  }

  wenet::WavReader wav_reader(wavfile);
  const int sample_rate = 16000;
  // Only support 16K
  const int num_sample = wav_reader.num_sample();
  std::vector<float> pcm_data(wav_reader.data(), wav_reader.data() + num_sample);

  auto RunPushPCM = [&pcm_data, &num_sample]() {
    // Send data every 0.5 second
    const float interval = 0.05;
    const int speed_up = 1; // 加速测试
    const int sample_interval = interval * sample_rate;
    int send_sum = 0;
    for (int start = 0; start < num_sample; start += sample_interval) {
      int end = std::min(start + sample_interval, num_sample);
      // Convert to short
      std::vector<short> data;
      for (int j = start; j < end; j++) {
        data.push_back(static_cast<short>(pcm_data[j]));
      }
      // Send PCM data
      
      bool arrive_end = (end >= num_sample);
      ydasroffline::AcceptWaveform(data.data(), data.size());
      send_sum += data.size();
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval * 1000 / speed_up)));
      // DEBG("Send {}/{} samples", send_sum, pcm_data.size());
      if (arrive_end) {
        ydasroffline::ASREnd();
        break;
      }
    }
    cout << "End Send all wav data.";
    
  };

  auto callbackfun = [](std::string asr_result) {
    std::cout << "stream out :" << asr_result << std::endl;
  };
  ydasroffline::SetSuccessCallback(callbackfun);
  std::vector<std::string> words = {"", "", ""};
  for (int i = 0; i < 1; ++i) {
    std::vector<std::string> querys = {words[i % words.size()]};
    ydasroffline::SetCommandWords(querys);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100)));

    ydasroffline::ClearWavformBuffer();
    thread mythread(RunPushPCM);

    if (!ydasroffline::ASRStart()) {
      std::cout << "ASRStart";
      return false;
    }
    if (mythread.joinable()) {
      std::cout << "mythread.joinable";
      mythread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(200)));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(2000)));
  ydasroffline::ASRReleaseAll();
  return true;
}

int main(int argc, char *argv[]) {
  string modelpath = "/home/asroffline-test/data/models/cn_tsfmer12_3_256_lm_20220624";
  string wavfile = "/home/asroffline-test/test/wavs/cn_60s.wav";

  OfflineRun(modelpath, wavfile);

  return 0;
}
