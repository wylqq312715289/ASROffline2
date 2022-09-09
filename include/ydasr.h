#ifndef YDASR_H_
#define YDASR_H_

#include <iostream>
#include <vector>
#include <functional>

namespace ydasroffline {

typedef std::function<void(std::string)> Callback;

// 通知程序结束识别。发送结束符
bool ASREnd();

// 等待ASR结束。调用ASREnd后，尾部的数据可能还在计算，立马结束识别会有漏识别。
void WaitASREnd(int maxwait_ms=1000);

// 给引擎push数据，必须在载入模型之后进行
bool AcceptWaveform(const short *input, int len);
bool AcceptWaveform(const char *input, int char_len);

// 设置回调函数
void SetSuccessCallback(Callback fun);
void SetErrorCallback(Callback fun);

// 动态设置命令词
bool SetCommandWords(std::vector<std::string> words);

// 重置命令词
bool ResetCommandWords();

// 清空wavform缓冲
bool ClearWavformBuffer();

// 载入模型后开始解码
bool ASRStart();

// 配置智云应用ID和秘钥，并打开有网时走智云在线ASR, 当设置close_onlineasr=true时，
bool SetUseYDOnlineASR(std::string now_lang, std::string zhiyun_appkey,
                       std::string zhiyun_appsecret, bool close_onlineasr=false);

// 设置前端静音检测时长，开始后延续len长度的静音，自动断开链接
bool SetHeadSilenceLenForForceDisconnect(std::string now_lang, int millsec_len);

// 设置末端静音检测时长，语音结束后延续len长度的静音，自动断开链接
bool SetTailSilenceLenForForceDisconnect(std::string now_lang, int millsec_len);

// 开机时初始化ASR
bool InitASR(std::string model_path);

// 载入配置项,配置项包含在模型文件夹内，conf.json, 参数model_path使用系统的绝对路径
bool LoadConf(std::string model_path);

// 根据占用内存最大的模型，初始化申请系统内存
bool InitMem();

// 设置使用哪个单语种，或多语种，同时使用多语种时，自动扩充内存
bool InitASREngine(std::string lang);

// 释放所有资源
bool ASRReleaseAll();

}

#endif // YDASR_H_
