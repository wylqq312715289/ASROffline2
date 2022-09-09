#include <thread>
#include <iostream>
#include <map>
#include <memory>
#include <cxxutils/def.h>
#include <cxxutils/utils.h>
#include <frontend/wav.h>
#include "include/ydasr.h"
#include "utils/flags.h"
#include <unistd.h>
#include <dirent.h>

using namespace cxxutils;
using namespace wenet;

DEFINE_string(wavfile, "", "test wavfile");
DEFINE_string(modelpath, "", "test modelpath");
DEFINE_int32(speedup, 1, "测试加速比");
DEFINE_int32(testtimes, 1, "测试加速比");
DEFINE_double(runhours, -1, "持续运行时间");
DEFINE_int32(online, 0, "测试在线ASR");

bool Main(std::string modelpath, std::string wavfile_or_path) {
  INFO("Now Test {} ", wavfile_or_path);
  long st = cxxutils::Millsec();
  std::string conffile = cxxutils::PathJoin(modelpath, "conf.json");
  if (!CheckFileExistence(conffile)) {
    INFO("load {}/conf.json error", modelpath);
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

  auto RunPushPCM = [](std::string wavfile) {
    INFO("Now Test RunPushPCM {} ", wavfile);
    wenet::WavReader wav_reader(wavfile);
    const int sample_rate = 16000;
    // Only support 16K
    CHECK_EQ(wav_reader.sample_rate(), sample_rate);
    const int num_sample = wav_reader.num_sample();
    std::vector<float> pcm_data(wav_reader.data(), wav_reader.data() + num_sample);

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
      bool arrive_end = (end >= num_sample);
      ydasroffline::AcceptWaveform(data.data(), data.size());
      send_sum += data.size();
      std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(interval * 1000 / speed_up)));
      DEBG("Send {}/{} samples", send_sum, pcm_data.size());
      if (arrive_end) {
        ydasroffline::ASREnd();
        ydasroffline::WaitASREnd(5000); // 5s是最长等待时间，这个时间可以按产品的要求设置
        break;
      }
    }
    INFO("End Send all wav data.");
  };

  auto callbackfun = [](std::string asr_result) {
    std::cout << "stream out :" << asr_result << std::endl;
  };

  /// 设置正常接收数据的回调函数
  ydasroffline::SetSuccessCallback(callbackfun);
  /// 设置异常时的处理函数
  ydasroffline::SetErrorCallback(callbackfun);
  if (FLAGS_online) {
    ydasroffline::SetUseYDOnlineASR("**","***","***");
  }

  std::vector<std::string> words = {"", "", ""};
  long start_time = cxxutils::Millsec();
  vector<std::string> fnames(FLAGS_testtimes, wavfile_or_path);
  if (cxxutils::IsDir(wavfile_or_path)) {
    fnames = cxxutils::ListDir(wavfile_or_path);
  }
  for (int i = 0; i < fnames.size(); ++i) {
    std::vector<std::string> querys = {words[i % words.size()]};
    /// 需要手动设置命令词时，打开即可，默认读取模型文件内的hotwords.txt
    //ydasroffline::SetCommandWords(querys);
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(100)));

    ydasroffline::ClearWavformBuffer();
    if (!ydasroffline::ASRStart()) {
      ERRO("ASRStart");
      return false;
    }
    thread mythread(RunPushPCM, fnames[i]);

    if (mythread.joinable()) {
      INFO("mythread.joinable");
      mythread.join();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(200)));
    if (FLAGS_runhours != -1 && 1.0 * (cxxutils::Millsec() - start_time) / (3600 * 1000) > FLAGS_runhours) {
      break;
    }
    if (FLAGS_runhours > 0 && i == fnames.size() - 1) { i = -1; }
    LOGINFO("run " + std::to_string(i) + " times. now memused\n" + cxxutils::GetMemUsed());
  }
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(2000)));
  ydasroffline::ASRReleaseAll();
  return true;
}


int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, false);
  google::InitGoogleLogging(argv[0]);
  Main(FLAGS_modelpath, FLAGS_wavfile);
  return 0;
}
