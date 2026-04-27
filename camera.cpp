#include "camera.h"
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QTimer>
#include <QThread>
#include <string>
#include <QSettings>
#include <cmath> // For std::atan and std::abs
#include <QDateTime>
std::queue<cv::Mat> Camera::gImage;
cv::Mat g_BGRImage;
LONG g_nPort = -1; // 初始化为-1表示未获取端口
LONG nUser = 1;
// 全局变量存储ROI坐标
cv::Rect roi_rect;
bool roi_selected = false;
namespace fs = std::filesystem;
QMutex queueMutex;

// 鼠标回调函数
void onMouse(int event, int x, int y, int flags, void *param)
{
    static cv::Point pt1, pt2;
    cv::Mat *img = static_cast<cv::Mat *>(param);

    if (event == cv::EVENT_LBUTTONDOWN)
    {
        pt1 = cv::Point(x, y);
    }
    else if (event == cv::EVENT_LBUTTONUP)
    {
        pt2 = cv::Point(x, y);
        roi_rect = cv::Rect(pt1, pt2);
        roi_selected = true;

        // 在原图上绘制矩形
        cv::Mat display_img = img->clone();
        cv::rectangle(display_img, roi_rect, cv::Scalar(0, 255, 0), 2);
        cv::imshow("Select ROI", display_img);
    }
    else if (event == cv::EVENT_MOUSEMOVE && (flags & cv::EVENT_FLAG_LBUTTON))
    {
        cv::Mat display_img = img->clone();
        cv::rectangle(display_img, pt1, cv::Point(x, y), cv::Scalar(0, 255, 0), 2);
        cv::imshow("Select ROI", display_img);
    }
}

// 数据解码回调函数，
// 功能：将YV_12格式的视频数据流转码为可供opencv处理的BGR类型的图片数据，并实时显示。
void CALLBACK DecCBFun(long nPort, char *pBuf, long nSize, FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
{
    // std::cout << nUser << std::endl;
    // if (nUser == 1)
    // {
    //     std::cout << "camera" << std::endl;
    // }
    if (pFrameInfo->nType == T_YV12)
    {
        // std::cout << "the frame infomation is T_YV12" << std::endl;
        if (g_BGRImage.empty())
        {
            g_BGRImage.create(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
        }
        cv::Mat YUVImage(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char *)pBuf);

        cv::cvtColor(YUVImage, g_BGRImage, cv::COLOR_YUV2BGR_YV12);
        // cv::imshow("RGBImage1", g_BGRImage);
        // cv::waitKey(15);
        QMutexLocker locker(&queueMutex);
        if (Camera::gImage.size() > 1)
        {
            Camera::gImage.pop();
        }
        Camera::gImage.push(g_BGRImage.clone()); // 使用clone确保深拷贝，避免内存问题
    }
}

// 实时视频码流数据获取 回调函数
void CALLBACK g_RealDataCallBack_V30(LONG lPlayHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, void *pUser)
{
    if (dwDataType == NET_DVR_STREAMDATA) // 码流数据
    {
        if (dwBufSize > 0 && g_nPort != -1)
        {
            if (!PlayM4_InputData(g_nPort, pBuffer, dwBufSize))
            {
                std::cout << "fail input data" << std::endl;
            }
            else
            {
                // std::cout << "success input data" << std::endl;
            }
        }
    }
}

Camera::Camera(QObject *parent)
{
    initCamera();
}

Camera::~Camera()
{
    // 销毁事件回调指针
    QMutexLocker locker(&queueMutex);
    while (!Camera::gImage.empty())
    {
        // 释放 cv::Mat 对象占用的内存
        Camera::gImage.front().release();
        // 从队列中移除该元素
        Camera::gImage.pop();
    }

    // 清理全局Mat
    if (!g_BGRImage.empty())
    {
        g_BGRImage.release();
    }
}

void Camera::initCamera()
{ // 初始化相机参数
}

void Camera::run()
{
    // 链接PLC
    ctx = modbus_new_tcp("192.168.1.99", 2001); // 西门子smart 200
    if (ctx == NULL)
    {
        qDebug() << "cannot create modbus";
        emit sendQStringtoMain("cannot create modbus");
        // return;
    }

    // 连接到Modbus服务器
    if (modbus_connect(ctx) == -1)
    {
        qDebug() << "connect to server fail";
        modbus_free(ctx);
        ctx = NULL; // 将ctx置为NULL，避免后续使用已释放的内存

        emit send_connectstate(false);
        emit sendQStringtoMain("connect to server fail");
        // return;
    }
    else
    {
        emit send_connectstate(true);
        emit sendQStringtoMain("connect to plc success");
    }

    emit sendQStringtoMain("loading ai model...");
    // fp32精度模型
    // config.precision = Precision::FP32;
    YoloV8 yoloV8(onnxModelPath, config); // 加载深度学习模型

    emit sendQStringtoMain("load ai model success");

    // ========== 相机模式选择 ==========
    #define USE_LOCAL_VIDEO 0 // 1: 使用本地视频, 0: 使用真实相机

    #if USE_LOCAL_VIDEO
        // 本地视频模式
        emit sendQStringtoMain("Using local video mode...");
        if (!cap.open(m_videoPath))
        {
            emit sendQStringtoMain("Failed to open local video!");
            return;
        }
        emit sendQStringtoMain("Local video opened successfully");
    #else
        // 真实相机模式
        emit sendQStringtoMain("Using real camera mode...");

        // 初始化
        NET_DVR_Init();
        // 设置连接时间与重连时间
        NET_DVR_SetConnectTime(2000, 1);
        NET_DVR_SetReconnect(10000, true);

        // 登录
        lpLoginInfo = {0};
        lpDeviceInfo = {0};

        lpLoginInfo.bUseAsynLogin = 0; // 同步登录方式
        char *sDeviceAddress, *sUserName, *sPassword;
        wPort = 8000;
        // 修改后
        // char ip[] = "192.168.31.105"; // 栈上创建可修改副本
        char ip[] = "192.168.1.64"; // 厂里camera
        sDeviceAddress = ip;
        char admin[] = "admin";
        sUserName = admin;
        char pwd[] = "CXF643200";
        sPassword = pwd;
        strcpy_s(lpLoginInfo.sDeviceAddress, sDeviceAddress);
        strcpy_s(lpLoginInfo.sUserName, sUserName);
        strcpy_s(lpLoginInfo.sPassword, sPassword);
        lpLoginInfo.wPort = wPort;

        lUserID = NET_DVR_Login_V40(&lpLoginInfo, &lpDeviceInfo);
        if (lUserID < 0)
        {
            std::cout << "注册失败！\n";
            emit sendQStringtoMain("register fail with camera!");
            system("pause");
        }
        else
        {
            std::cout << "注册成功！" << std::endl;
            Sleep(1000); // 显示注册相关信息
        }

        if (PlayM4_GetPort(&g_nPort)) // 获取播放库通道号
        {
            if (PlayM4_SetStreamOpenMode(g_nPort, STREAME_REALTIME)) // 设置流模式
            {
                if (PlayM4_OpenStream(g_nPort, NULL, 0, 1024 * 1024)) // 打开流
                {
                    if (PlayM4_SetDecCallBackExMend(g_nPort, DecCBFun, NULL, 0, nUser)) // NULL 替换为nUser了
                    {
                        if (PlayM4_Play(g_nPort, NULL))
                        {
                            std::cout << "success to set play mode" << std::endl;
                        }
                        else
                        {
                            std::cout << "fail to set play mode" << std::endl;
                        }
                    }
                    else
                    {
                        std::cout << "fail to set dec callback " << std::endl;
                    }
                }
                else
                {
                    std::cout << "fail to open stream" << std::endl;
                }
            }
            else
            {
                std::cout << "fail to set stream open mode" << std::endl;
            }
        }
        else
        {
            std::cout << "fail to get port" << std::endl;
        }
        Sleep(1000); // 显示播放端口打开情况

        // 启动预览并设置回调数据流
        struPlayInfo = {0};
        struPlayInfo.hPlayWnd = NULL;  // 窗口为空，设备SDK不解码只取流
        struPlayInfo.lChannel = 1;     // Channel number 设备通道
        struPlayInfo.dwStreamType = 0; // 码流类型，0-主码流，1-子码流，2-码流3，3-码流4, 4-码流5,5-码流6,7-码流7,8-码流8,9-码流9,10-码流10
        struPlayInfo.dwLinkMode = 0;   // 0：TCP方式,1：UDP方式,2：多播方式,3 - RTP方式，4-RTP/RTSP,5-RSTP/HTTP
        struPlayInfo.bBlocked = 0;     // 0-非阻塞取流, 1-阻塞取流, 如果阻塞SDK内部connect失败将会有5s的超时才能够返回,不适合于轮询取流操作.

        qDebug() << "Camera" << id << "opened successfully";
        emit sendQStringtoMain("Camera " + QString::number(id) + " opened successfully");
        setD(0, 0);
        setD(1, 0);
        setD(2, 0);
        if (NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL)) // 开始取流
        {
            // cv::namedWindow("RGBImage2");
        }
    #endif

    try
    {
        this->Camera_thread_flag = false;
        // 等待结束
        int fps;

        while (true)
        {
            QThread::msleep(30);
            if (Camera_thread_flag)
            {
                break;
            }

            #if USE_LOCAL_VIDEO
                // 从本地视频读取帧
                cap >> BGR_image;
                if (BGR_image.empty())
                {
                    // 视频结束，循环播放
                    qDebug() << "Video ended, restarting...";
                    emit sendQStringtoMain("Video ended, restarting...");

                    // 重新打开视频文件
                    cap.release();
                    if (!cap.open(m_videoPath))
                    {
                        qDebug() << "Failed to reopen video!";
                        emit sendQStringtoMain("Failed to reopen video!");
                        break;
                    }
                    cap >> BGR_image;
                    if (BGR_image.empty())
                    {
                        qDebug() << "Still cannot read frame!";
                        continue;
                    }
                    qDebug() << "Video restarted successfully";
                }
            #else
                // 从真实相机获取帧
                if (Camera::gImage.empty())
                {
                    continue;
                }

                QMutexLocker locker(&queueMutex);
                BGR_image = Camera::gImage.front();
                Camera::gImage.pop();
            #endif

            // 图像处理
            output = false;
            // 处理检测到的工序
            try
            {
                // Run inference 推理
                // qDebug() << "run inference";
                const auto objects = yoloV8.detectObjects(BGR_image);
                // 设置检测区域ROI
                cv::Rect detectionROI(roi_x, roi_y, roi_w, roi_h);
                yoloV8.setDetectionROI(detectionROI);
                yoloV8.enableROIDetection(m_enableROIDetection);
                // Draw the bounding boxes on the image
                yoloV8.drawObjectLabels(BGR_image, objects); // 绘制框
                std::vector<int> classCount = yoloV8.getclassnumer();
                int chilun_num = classCount[0];
                int keti_num = classCount[1];
                int luosi_num = classCount[2];
                QString str_chilun_num = QString::number(chilun_num);
                QString str_luosi_num = QString::number(luosi_num);
                emit sendNumber(str_chilun_num, str_luosi_num);
                // 检查动作是否有做到了（瞬时动作可以消失）
                std::vector<bool> tempAction = yoloV8.getActionFlag();
                for (size_t i = 0; i < tempAction.size(); i++)
                {
                    if (tempAction[i])
                    {
                        actionGroup[i] = true;
                    }
                }
                emit updateActionState(actionGroup);
                // "chilun",   "keti" ,"luosi"
                // std::cout << "class0" << chilun_num << "class1" << keti_num << "class2" << luosi_num << std::endl;

                // 将当前帧的壳体检测结果添加到滑动窗口
                bool current_keti_detected = keti_num > 0;
                keti_history.push_back(current_keti_detected);

                // 保持滑动窗口大小为 KETI_WINDOW_SIZE
                if (keti_history.size() > KETI_WINDOW_SIZE)
                {
                    keti_history.pop_front();
                }

                // 计算滑动窗口中检测到壳体的帧数
                int keti_count = std::count(keti_history.begin(), keti_history.end(), true);

                // 根据阈值确定最终的壳体状态
                cur_keti = keti_count >= KETI_THRESHOLD ? 1 : 0;
                if (cur_keti > 0)
                {
                    if (chilun_num == CHILUN_NUM)
                    {
                        // 满了 plc res_flag置1
                        // cv2.putText(image, "OK", (10, 120), cv::FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                        chilun_flag = true;
                    }

                    if (luosi_num == LUOSI_NUM)
                    {
                        // 满了 plc res_flag置1
                        // cv2.putText(image, "OK", (10, 130), cv::FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                        luosi_flag = true;
                    }
                }

                if (chilun_flag && !luosi_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi not yet", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(true, false, false); // 齿轮/螺丝/总体
                }

                if (luosi_flag && !chilun_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun not yet", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(false, true, false); // 齿轮/螺丝/总体
                }

                if (chilun_flag && luosi_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "ALL OK", cv::Point(10, 290), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(true, true, true); // 齿轮/螺丝/总体
                    // PLC 接收
                    // setD(2,1);//绿灯
                }

                if (cur_keti == 0 && last_keti == 1)
                {
                    // 并检查是否漏装对齐
                    if (!chilun_flag)
                    {
                        // 绘制消息框
                        cv::putText(BGR_image, "chilun miss", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // PLC 报警
                        setD(0, 1); // PLC置位
                    }

                    if (!luosi_flag)
                    {
                        // 绘制消息框
                        cv::putText(BGR_image, "luosi miss", cv::Point(10, 230), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // PLC 报警
                        setD(0, 1); // PLC置位
                    }

                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                }

                if (cur_keti == 0 && last_keti == 0)
                {
                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                    // reset actionGroup and buttonState
                    actionGroup = {false, false, false, false, false};
                    emit updateButtonState(false, false, false); // 齿轮/螺丝/总体
                    // 复位PLC输出(让PLC自己复位)
                    // setD(0, 0); // 复位报警
                    // setD(2, 0); // 复位绿灯
                }

                last_keti = cur_keti;
            }
            catch (...)
            {
                // 处理所有异常的逻辑
                std::cerr << "An unknown exception occurred during image processing." << std::endl;
                emit sendQStringtoMain("An unknown exception occurred during image processing.");
            }
            // cv::imshow("Camera", BGR_image);
            // cv::waitKey(1);
            QImage a = cvMat2QImage(BGR_image);
            QImage IMG = a.scaled(640, 640, Qt::KeepAspectRatio);
            emit sendQImgToAutoMain(IMG);
        }

        #if USE_LOCAL_VIDEO
            // 本地视频模式清理
            cap.release();
        #else
            // 真实相机模式清理
            // 结束停止采集
            // 先停止预览
            NET_DVR_StopRealPlay(lUserID);

            // 停止播放库
            if (g_nPort != -1)
            {
                PlayM4_Stop(g_nPort);
                PlayM4_CloseStream(g_nPort);
                PlayM4_FreePort(g_nPort);
                g_nPort = -1;
            }

            // 发送停采命令
            NET_DVR_Logout(lUserID);
            NET_DVR_Cleanup();
        #endif

        emit finishedthread();
        // 注销采集回调
        // 注销远端设备事件
        // 释放资源
    }
    catch (std::exception &e)
    {
        qDebug() << "error info: " << e.what();
    }
    // 反初始化库
    // 销毁事件回调指针
}

void Camera::ExecuteMianToThread()
{
    stop_camera();
    qDebug() << "shoudao " << "\n";
    // DestroyedHandle();
    emit finished();
}

void Camera::stop_camera()
{
    this->Camera_thread_flag = true;
}

void Camera::closeDevice()
{ // 关闭设备
    // ch:关闭设备 | Close device

    qDebug("Closed");

    emit finished();
}

bool Camera::imageProcess(cv::Mat image)
{
    return true;
}

QImage Camera::cvMat2QImage(const cv::Mat &mat)
{
    // 检查Mat是否为空
    if (mat.empty())
    {
        qWarning() << "Input Mat is empty";
        return QImage();
    }

    // 8-bits unsigned, NO. OF CHANNELS = 1
    if (mat.type() == CV_8UC1)
    {
        QImage qimage(mat.cols, mat.rows, QImage::Format_Indexed8);
        // Set the color table (used to translate colour indexes to qRgb values)
        qimage.setColorCount(256);
        for (int i = 0; i < 256; i++)
        {
            qimage.setColor(i, qRgb(i, i, i));
        }
        // Copy input Mat
        uchar *pSrc = mat.data;
        for (int row = 0; row < mat.rows; row++)
        {
            uchar *pDest = qimage.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return qimage.copy(); // 使用copy确保深拷贝
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    else if (mat.type() == CV_8UC3)
    {
        // Copy input Mat
        const uchar *pSrc = (const uchar *)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped().copy(); // 添加copy确保深拷贝
    }
    else if (mat.type() == CV_8UC4)
    {
        // Copy input Mat
        const uchar *pSrc = (const uchar *)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {
        qWarning() << "Unsupported Mat type:" << mat.type();
        return QImage();
    }
}

void Camera::set32D(int address, int32_t value)
{ // 设置32位D
    if (ctx == NULL)
    {
        emit sendQStringtoMain("Modbus context is NULL, skip write");
        return;
    }
    // 确保value在int32_t的范围内
    if (value < INT32_MIN || value > INT32_MAX)
    {
        std::cerr << "Value out of range for int32_t" << std::endl;
    }

    // 将32位整数分割为两个16位部分
    uint16_t high = static_cast<uint16_t>((value >> 16) & 0xFFFF); // 取高16位
    uint16_t low = static_cast<uint16_t>(value & 0xFFFF);          // 取低16位
    rc = modbus_write_register(ctx, address, low);
    rc = modbus_write_register(ctx, address + 1, high);
}

void Camera::setD(int address, int value)
{ // 设置16位D
    if (ctx == NULL)
    {
        emit sendQStringtoMain("Modbus context is NULL, skip write");
        return;
    }
    rc = modbus_write_register(ctx, address, value);
    if (rc == -1)
    {
        emit sendQStringtoMain("Failed to write register: " + QString::number(address));
        // 尝试重新连接
    }
    emit sendQStringtoMain("setD address: " + QString::number(address) + ", value is: " + QString::number(value));
}

int Camera::setRoi()
{
    // 获取一张BGR_image，显示出来让用户进行手动框选，然后保存框选的坐标保存到roi_x,roi_y,roi_w,roi_h中
    // 然后根据框选的坐标进行图像处理
    // test 实际使用时注释
    cv::Mat BGR_image = cv::imread("C:\\Users\\chenxinfeng\\Desktop\\异物图片 裁剪后\\22.bmp");
    if (BGR_image.empty())
    {
        qDebug() << "cannot load image";
        return -1;
    }

    // 显示图像并设置鼠标回调
    cv::namedWindow("Select ROI", cv::WINDOW_NORMAL);
    cv::setMouseCallback("Select ROI", onMouse, &BGR_image);
    cv::imshow("Select ROI", BGR_image);

    qDebug() << "select roi by mouse...";
    cv::waitKey(0);

    if (roi_selected)
    {
        // 保存ROI坐标
        int roi_x = roi_rect.x;
        int roi_y = roi_rect.y;
        int roi_w = roi_rect.width;
        int roi_h = roi_rect.height;

        qDebug() << "select ROI : x=" << roi_x << ", y=" << roi_y << ", width=" << roi_w << ", height=" << roi_h;

        // 提取ROI区域
        cv::Mat roi_image = BGR_image(roi_rect);

        // 显示ROI区域
        cv::imshow("Selected ROI", roi_image);
        cv::waitKey(0);
    }
    else
    {
        qDebug() << "not select roi yet";
    }
    qDebug() << "get select ROI : x=" << roi_x << ", y=" << roi_y << ", width=" << roi_w << ", height=" << roi_h;

    return 0;
}

void Camera::aiTest()
{
    return;
}

void Camera::igonoreAction(int index)
{ // 忽略某个动作,todo: (std::vector<bool> index)作为传递参数较好
    actionGroup[index] = false;
}

void Camera::enableROIDetection(bool enable)
{
    m_enableROIDetection = enable;
}

void Camera::setRoiX(int x)
{
    roi_x = x;
}

void Camera::setRoiY(int y)
{
    roi_y = y;
}

void Camera::setRoiW(int w)
{
    roi_w = w;
}

void Camera::setRoiH(int h)
{
    roi_h = h;
}
