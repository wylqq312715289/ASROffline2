#include <thread>
#include <iostream>
#include <map>
#include <memory>
#include <cxxutils/def.h>
#include <cxxutils/utils.h>
#include <frontend/wav.h>
#include "include/ydasr.h"
#include "utils/flags.h"
#include "atomic"

using namespace cxxutils;
using namespace wenet;

DEFINE_string(wavfile, "", "test wavfile");
DEFINE_string(modelpath, "", "test modelpath");
DEFINE_int32(speedup, 1, "测试加速比");
DEFINE_int32(testtimes, 1, "测试加速比");

bool OfflineRun(std::string modelpath, std::string wavfile) {
  long st = cxxutils::Millsec();
  std::string conffile = cxxutils::PathJoin(modelpath, "conf.json");
  if (!CheckFileExistence(conffile)) {
    INFO("load {}/conf.json error", modelpath);
    return false;
  }
  if (!CheckFileExistence(wavfile)) {
    INFO("load {} error", wavfile);
    return false;
  }

  if (!ydasroffline::LoadConf(modelpath)) {
    ERRO("LoadConf");
    return false;
  }
  if (!ydasroffline::InitMem()) {
    ERRO("InitMem");
    return false;
  }
  if (!ydasroffline::InitASREngine("cn")) {
    ERRO("InitASREngine");
    return false;
  }
  INFO("Init ASR all time use={}", cxxutils::Millsec() - st);

  wenet::WavReader wav_reader(wavfile);
  const int sample_rate = 16000;
  // Only support 16K
  CHECK_EQ(wav_reader.sample_rate(), sample_rate);
  const int num_sample = wav_reader.num_sample();
  std::vector<float> pcm_data(wav_reader.data(), wav_reader.data() + num_sample);

  atomic_bool head_or_tail_end{false};
  auto RunPushPCM = [&pcm_data, &num_sample, &head_or_tail_end]() {
    // Send data every 0.5 second
    const float interval = 0.05;
    const int speed_up = FLAGS_speedup; // 加速测试
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
      bool arrive_end  = (end >= num_sample);
      ydasroffline::AcceptWaveform(data.data(), data.size());
      send_sum += data.size();
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval * 1000 / speed_up)));
      DEBG("Send {}/{} samples", send_sum, pcm_data.size());
      if (arrive_end) {
        ydasroffline::ASREnd();
        break;
      }
      if (head_or_tail_end) {
        break;
      }
    }
    INFO("End Send all wav data.");
  };

  auto success_callbackfun = [](std::string asr_result) {
    std::cout << "stream out :" << asr_result << std::endl;
  };
  auto srror_callbackfun = [&head_or_tail_end](std::string asr_result) {
    std::cout << "stream out :" << asr_result << std::endl;
    head_or_tail_end = true;
  };
  ydasroffline::SetSuccessCallback(success_callbackfun);
  ydasroffline::SetErrorCallback(srror_callbackfun);
  ydasroffline::SetHeadSilenceLenForForceDisconnect("cn", 1000);
  ydasroffline::SetTailSilenceLenForForceDisconnect("cn", 10);

  std::vector<std::string> words = {"", "", ""};
  for (int i = 0; i < FLAGS_testtimes; ++i) {
    std::vector<std::string> querys = {words[i % words.size()]};
    /// 需要手动设置命令词时，打开即可，默认读取模型文件内的hotwords.txt
    //ydasroffline::SetCommandWords(querys);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100)));

    ydasroffline::ClearWavformBuffer();
    thread mythread(RunPushPCM);

    if (!ydasroffline::ASRStart()) {
      ERRO("ASRStart");
      return false;
    }
    if (mythread.joinable()) {
      INFO("mythread.joinable");
      mythread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(200)));
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(2000)));
  ydasroffline::ASRReleaseAll();
  return true;
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  OfflineRun(FLAGS_modelpath, FLAGS_wavfile);
  return 0;
}
