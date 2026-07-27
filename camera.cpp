#include "camera.h"
#include <QDebug>
#include <QImage>
#include <QImageReader>
#include <QThread>
#include <string>
#include <QSettings>
#include <cmath>
#include <QDateTime>

std::queue<cv::Mat> Camera::gImage;
cv::Mat g_BGRImage;
LONG g_nPort = -1; // 初始化为-1表示未获取端口
LONG nUser = 1;
QMutex queueMutex;

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
    Camera_thread_flag = false;

    // 链接PLC
    if (!connectPLC())
    {
        // emit finishedthread();
        // return;
    }

    emit sendQStringtoMain("loading AI model...");
    QSettings settings(iniFilePath, QSettings::IniFormat);
    QString modelPath = settings.value("modelPath").toString();
    qDebug() << "modelPath: " << modelPath;
    YoloV8Config config;
    YoloV8 *yoloV8 = nullptr;
    try
    {
        yoloV8 = new YoloV8(modelPath.replace("/", "\\").toStdString(), config); // 加载深度学习模型
        emit sendQStringtoMain("load AI model success");
    }
    catch (const std::exception &e)
    {
        emit sendQStringtoMain(QString("Failed to load AI model: %1").arg(e.what()));
        emit finishedthread();
        return;
    }
    catch (...)
    {
        emit sendQStringtoMain("Failed to load AI model: Unknown error!");
        emit finishedthread();
        return;
    }
    // 获取参数
    getROIParameters();

    bool useLocalVideo = settings.value("useLocalVideo").toBool();
    QString videoPath = settings.value("videoPath").toString();
    QString cameraIp = settings.value("cameraIp").toString();
    int cameraPort = settings.value("cameraPort").toInt();
    QString cameraUsername = settings.value("cameraUsername").toString();
    QString cameraPassword = settings.value("cameraPassword").toString();

    if (useLocalVideo)
    {
        // 本地视频模式
        emit sendQStringtoMain("Using local video mode...");
        if (!cap.open(videoPath.toStdString()))
        {
            emit sendQStringtoMain("Failed to open local video!");
            emit finishedthread();
            return;
        }
        emit sendQStringtoMain("Local video opened successfully");
    }
    else
    {
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

        std::string ip = cameraIp.toStdString();
        std::string username = cameraUsername.toStdString();
        std::string password = cameraPassword.toStdString();

        strcpy_s(lpLoginInfo.sDeviceAddress, ip.c_str());
        strcpy_s(lpLoginInfo.sUserName, username.c_str());
        strcpy_s(lpLoginInfo.sPassword, password.c_str());
        lpLoginInfo.wPort = cameraPort;
        lpLoginInfo.bUseAsynLogin = 0; // 同步登录方式

        lUserID = NET_DVR_Login_V40(&lpLoginInfo, &lpDeviceInfo);
        if (lUserID < 0)
        {
            DWORD dwErr = NET_DVR_GetLastError(); // 获取错误码
            std::cout << "注册失败！错误码: " << dwErr << "\n";
            emit sendQStringtoMain(QString("register fail with camera! Error code: %1").arg(dwErr));
            emit finishedthread();
            return;
        }
        else
        {
            std::cout << "注册成功！" << std::endl;
            emit sendQStringtoMain("register success with camera!");
        }

        if (PlayM4_GetPort(&g_nPort)) // 获取播放库通道号
        {
            if (PlayM4_SetStreamOpenMode(g_nPort, STREAME_REALTIME)) // 设置流模式
            {
                if (PlayM4_OpenStream(g_nPort, NULL, 0, 1024 * 1024)) // 打开流
                {
                    if (PlayM4_SetDecCallBackExMend(g_nPort, DecCBFun, NULL, 0, nUser))
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
        struPlayInfo.hPlayWnd = NULL;  // 窗口句柄设为NULL，表示SDK不解码显示，只取原始码流
        struPlayInfo.lChannel = 1;     // Channel number 设备通道号
        struPlayInfo.dwStreamType = 0; // 码流类型：0-主码流，1-子码流，2-码流3...
        struPlayInfo.dwLinkMode = 0;   // 取流协议：0-TCP，1-UDP，2-多播，3-RTP，4-RTP/RTSP，5-RTSP/HTTP
        struPlayInfo.bBlocked = 0;     // 阻塞模式：0-非阻塞（立即返回），1-阻塞（失败时最多等待5秒，不适合轮询）

        qDebug() << "Camera" << id << "opened successfully";
        emit sendQStringtoMain("Camera " + QString::number(id) + " opened successfully");
        setD(0, 0);
        setD(1, 0);
        setD(2, 0);
        LONG lRealHandle = NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL); // 开始取流
        if (lRealHandle < 0)
        {
            DWORD dwErr = NET_DVR_GetLastError(); // 获取错误码
            std::cout << "预览失败！错误码: " << dwErr << "\n";
            emit sendQStringtoMain(QString("RealPlay failed! Error code: %1").arg(dwErr));
            emit finishedthread();
            return;
        }
        else
        {
            std::cout << "预览成功！句柄: " << lRealHandle << std::endl;
            emit sendQStringtoMain("RealPlay started successfully");
        }
    }

    keti_history.clear(); // 清空队列
    last_keti = 0; // 重置计数器

    try
    {
        while (true)
        {
            QThread::msleep(30); // 延时
            if (Camera_thread_flag)
            {
                break;
            }
            if (useLocalVideo)
            {
                // 从本地视频读取帧
                cap >> BGR_image;
                if (BGR_image.empty())
                {
                    // 视频结束，循环播放
                    qDebug() << "Video ended, restarting...";
                    emit sendQStringtoMain("Video ended, restarting...");

                    // 重新打开视频文件
                    cap.release();
                    if (!cap.open(videoPath.toStdString()))
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
            }
            else
            {
                // 从真实相机获取帧
                if (Camera::gImage.empty())
                {
                    continue;
                }

                QMutexLocker locker(&queueMutex);
                BGR_image = Camera::gImage.front();
                Camera::gImage.pop();
            }
            // 图像处理
            output = false;
            // 处理检测到的工序
            try
            {
                // Run inference 推理
                // qDebug() << "run inference";
                const auto objects = yoloV8->detectObjects(BGR_image);
                // 设置检测区域ROI
                yoloV8->enableROIDetection(m_enableROIDetection);
                cv::Rect detectionROI(roi_x, roi_y, roi_w, roi_h);
                yoloV8->setDetectionROI(detectionROI);
                yoloV8->setRoiColor(roi_color);
                yoloV8->setRoiOpacity(roi_opacity);
                yoloV8->setRoiLineWidth(roi_line_width);
                // Draw the bounding boxes on the image
                yoloV8->drawObjectLabels(BGR_image, objects);
                std::vector<int> classCount = yoloV8->getclassnumer();
                int chilun_num = classCount[0];
                int keti_num = classCount[1];
                int luosi_num = classCount[2];
                QString str_chilun_num = QString::number(chilun_num);
                QString str_luosi_num = QString::number(luosi_num);
                emit sendNumber(str_chilun_num, str_luosi_num);
                // 检查动作是否有做到了（瞬时动作可以消失）
                std::vector<bool> tempAction = yoloV8->getActionFlag();
                for (size_t i = 0; i < tempAction.size(); i++)
                {
                    if (tempAction[i])
                        actionGroup[i] = true;
                }
                emit updateActionState(actionGroup);
                // "chilun", "keti, "luosi"
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

                if (chilun_flag || luosi_flag)
                {
                    cv::putText(BGR_image, chilun_flag ? "chilun OK" : "chilun not yet", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, luosi_flag ? "luosi OK" : "luosi not yet", cv::Point(10, 240), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    if (chilun_flag && luosi_flag)
                    {
                        cv::putText(BGR_image, "ALL OK", cv::Point(10, 290), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // setD(2, 1); // 绿灯
                    }

                    emit updateButtonState(chilun_flag, luosi_flag, chilun_flag && luosi_flag); // 齿轮/螺丝/总体
                }

                if (cur_keti == 0 && last_keti == 1)
                {
                    if (!chilun_flag)
                        cv::putText(BGR_image, "chilun miss", cv::Point(10, 340), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    if (!luosi_flag)
                        cv::putText(BGR_image, "luosi miss", cv::Point(10, 390), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);

                    if (!chilun_flag || !luosi_flag)
                        setD(0, 1); // PLC 报警

                    emit updateStatistics(chilun_flag && luosi_flag);
                }

                if (cur_keti == 0 && last_keti == 0)
                {
                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = luosi_flag = false;
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
            QImage IMG = cvMat2QImage(BGR_image);
            // IMG = IMG.scaled(640, 640, Qt::KeepAspectRatio); // 调整图像大小
            emit sendQImgToAutoMain(IMG);
        }

        if (useLocalVideo)
        {
            // 本地视频模式清理
            cap.release();
        }
        else
        {
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
        }

        emit finishedthread();
        // 注销采集回调
        // 注销远端设备事件
        // 释放资源

        // 关闭相机
        if (useLocalVideo)
        {
            emit sendQStringtoMain("Local video closed");
        }
        else
        {
            emit sendQStringtoMain("Camera closed");
        }
    }
    catch (std::exception &e)
    {
        qDebug() << "error info: " << e.what();
    }
    // 反初始化库
    // 销毁事件回调指针
}

bool Camera::connectPLC()
{
    // 从 INI 文件读取 PLC 配置
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("PLC");
    QString plcIp = settings.value("plcIp").toString();
    int plcPort = settings.value("plcPort").toInt();
    settings.endGroup();

    // 如果已有连接，先断开
    if (ctx != NULL)
    {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = NULL;
    }

    // 创建 Modbus TCP 上下文
    ctx = modbus_new_tcp(plcIp.toStdString().c_str(), plcPort); // 西门子smart 200
    if (ctx == NULL)
    {
        QString errorMsg = QString("PLC context creation failed (%1:%2)").arg(plcIp).arg(plcPort);
        qDebug() << errorMsg;
        emit sendQStringtoMain(errorMsg);
        emit send_connectstate(false);
        return false;
    }

    // 连接到 Modbus 服务器
    if (modbus_connect(ctx) == -1)
    {
        QString errorMsg = QString("PLC connection failed (%1:%2)").arg(plcIp).arg(plcPort);
        qDebug() << errorMsg;
        modbus_free(ctx);
        ctx = NULL;

        emit sendQStringtoMain(errorMsg);
        emit send_connectstate(false);
        return false;
    }
    else
    {
        emit sendQStringtoMain(QString("PLC connected successfully (%1:%2)").arg(plcIp).arg(plcPort));
        emit send_connectstate(true);
        return true;
    }
}

void Camera::disconnectPLC()
{
    if (ctx != NULL)
    {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = NULL;

        emit sendQStringtoMain("disconnect from plc success");
        qDebug() << "disconnect from plc success";
    }
    else
    {
        qDebug() << "plc already disconnected";
        emit sendQStringtoMain("plc already disconnected");
    }
    emit send_connectstate(false);
}

void Camera::getROIParameters()
{
    // 从 INI 文件读取 ROI 配置
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("ROI");
    // 读取 ROI 参数
    m_enableROIDetection = settings.value("EnableROI").toBool();
    roi_x = settings.value("RoiX").toInt();
    roi_y = settings.value("RoiY").toInt();
    roi_w = settings.value("RoiW").toInt();
    roi_h = settings.value("RoiH").toInt();
    roi_color = settings.value("RoiColor").toString();
    roi_opacity = settings.value("RoiOpacity").toInt() / 100.0;
    roi_line_width = settings.value("RoiLineWidth").toInt();
    settings.endGroup();
}

void Camera::stop_camera()
{
    Camera_thread_flag = true;

    // 释放 Modbus 资源
    if (ctx != NULL)
    {
        modbus_close(ctx);
        modbus_free(ctx);
        ctx = NULL;
    }
}

void Camera::closeDevice()
{ // 关闭设备
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
    if (modbus_write_register(ctx, address, value) == -1)
    {
        emit sendQStringtoMain("Failed to write register: " + QString::number(address));
    }
    else
    {
        emit sendQStringtoMain("setD address: " + QString::number(address) + ", value is: " + QString::number(value));
    }
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

void Camera::setRoiColor(QString color)
{
    roi_color = color;
}

void Camera::setRoiOpacity(float opacity)
{
    roi_opacity = opacity;
}

void Camera::setRoiLineWidth(int lineWidth)
{
    roi_line_width = lineWidth;
}
