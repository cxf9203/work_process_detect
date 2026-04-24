#ifndef CAMERA_H
#define CAMERA_H
#include <QThread>
#pragma warning(disable : 4819) // 解决SDK中包含中文问题；忽略C4819错误
#include <iostream>
#include <string>
#include <QDebug>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSettings>
#include <cmath>
#include <queue>
#include "yolov8.h"
#include <vector>
#include <modbus.h>
#include <QCoreApplication>
#include <HCNetSDK.h>
#include <plaympeg4.h>
#include <PlayM4.h>

// 相机线程类
class Camera : public QObject
{
    Q_OBJECT
public:
    Camera(QObject *parent = nullptr);
    void initCamera();
    ~Camera();
    static std::queue<cv::Mat> gImage;
    void closeDevice(); // 关闭设备
    void stop_camera();
    bool imageProcess(cv::Mat image);
    QImage cvMat2QImage(const cv::Mat &mat);
    QImage image;
    cv::Mat getOneFrame();
    // 设定图像处理参数
    int setRoi();
    // 设定PLC参数
    // void setM(int value); // 设定M元件
    // void ResetM(int value); // 读取M元件
    void setD(int address, int value);       // 设置整型D元件
    void set32D(int address, int32_t value); // 设置整型D元件
    void aiTest();                           // ai model test
    void igonoreAction(int index);           // 忽略某个动作

signals:
    // 给主线程发消息
    void sendImgToAutoMain(cv::Mat img, double left_tuoshuizhou, double right_tuoshuizhou, double left_dashuifeng, double right_dashuifeng, double theta_t, double theta_d, bool result);
    void sendQImgToAutoMain(QImage img);
    void finished(); // 新增：用于在循环结束时通知主线程
    void updateActionState(std::vector<bool> actionGroup);
    void sendQStringtoMain(QString message);
    void sendResult(QString left_tuoshuizhou, QString right_tuoshuizhou, QString left_dashuifeng, QString right_dashuifeng, QString theta_t, QString theta_d, QString result);
    void sendNumber(QString str_chilun_num, QString str_luosi_num);
    void finishedthread();
    void updateButtonState(bool p1, bool p2, bool p3);
    void send_connectstate(bool state);
    void resetSystem();
    void triggerAlarm();

private:
    QString filePath = "D:\\jiance\\process.onnx";
    std::string onnxModelPath = filePath.toStdString();
    YoloV8Config config;
    // static void __stdcall ImageCallBackEx(unsigned char *pData, MV_FRAME_OUT_INFO_EX *pFrameInfo, void *pUser);
    bool Camera_thread_flag;
    cv::Mat BGR_image;
    int id;
    bool cameraOpened = false;
    bool isGrabbingFlag = false;
    // 连接PLC modbustcp
    modbus_t *ctx;
    int rc;
    uint16_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    uint32_t query32D[MODBUS_TCP_MAX_ADU_LENGTH];
    uint8_t queryM[MODBUS_TCP_MAX_ADU_LENGTH];
    cv::VideoCapture cap;
    std::string m_videoPath = "D:\\jiance\\video.mp4"; // 视频文件路径
    // 登录
    NET_DVR_USER_LOGIN_INFO lpLoginInfo = {0};
    NET_DVR_DEVICEINFO_V40 lpDeviceInfo = {0}; // NET_DVR_DEVICEINFO_V40
    NET_DVR_PREVIEWINFO struPlayInfo;          // NET_DVR_PREVIEWINFO_V30
    WORD wPort = 8000;
    LONG lUserID;
    bool output = false;
    // 系统状态
    enum ProcessState { WAIT_P1, WAIT_P2, WAIT_P3, COMPLETE, ALARM };
    ProcessState currentState = WAIT_P1;
    std::vector<std::string> classes = {"process1", "process2", "process3"};
    std::vector<bool> actionGroup = {false, false, false, false, false}; // "luosi_left_bottom", "luosi_left_top", "luosi_right_bottom", "luosi_right_top", "place_chilun"; //动作序列
    // 用于存储每个类别的计数
    int CHILUN_NUM = 1; // 标准齿轮数
    int LUOSI_NUM = 4; // 标准螺丝数
    bool res_flag = false; // 结果标志位
    bool chilun_flag = false; // 齿轮标志位
    int luosi_flag = 0; // 螺丝标志位
    int cur_keti = 0; // 当前keti计数
    int last_keti = 0; // 上次keti计数
    // 滑动窗口相关变量，用于平滑壳体检测结果
    std::deque<bool> keti_history; // 存储最近 KETI_WINDOW_SIZE 个壳体检测结果
    const int KETI_WINDOW_SIZE = 1; // 滑动窗口大小
    const int KETI_THRESHOLD = 1; // 判定壳体存在的阈值
    // 图像参数
    int roi_x = 0;
    int roi_y = 0;
    int roi_w = 500;
    int roi_h = 500;

protected:

public slots:
    void run();
    void ExecuteMianToThread();
};

#endif // CAMERA_H
