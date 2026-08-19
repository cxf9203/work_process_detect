#include <QDir>
#include "camera.h"
#include <QDebug>
#include <QMutex>
#include <QImage>
#include <QImageReader>
#include <QThread>
#include <string>
#include <QSettings>
#include <cmath>
#include <QDateTime>

std::queue<cv::Mat> Camera::gImage;
LONG g_nPort = -1; // 初始化为-1表示未获取端口
LONG nUser = 1;
QMutex queueMutex;

// 数据解码回调函数，
// 功能：将YV_12格式的视频数据流转码为可供opencv处理的BGR类型的图片数据，并实时显示。
void CALLBACK DecCBFun(long nPort, char *pBuf, long nSize, FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
{
    if (pFrameInfo->nType == T_YV12)
    {
        // std::cout << "the frame infomation is T_YV12" << std::endl;
        cv::Mat YUVImage(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char *)pBuf);
        cv::Mat bgrImage;
        cv::cvtColor(YUVImage, bgrImage, cv::COLOR_YUV2BGR_YV12);
        // cv::imshow("RGBImage1", bgrImage);
        // cv::waitKey(15);
        QMutexLocker locker(&queueMutex);
        if (Camera::gImage.size() > 1)
        {
            Camera::gImage.pop();
        }
        Camera::gImage.push(bgrImage);
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
}

void Camera::initCamera()
{
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
            std::cout << "注册失败！错误码: " << dwErr << std::endl;
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
            if (PlayM4_SetStreamOpenMode(g_nPort, STREAME_REALTIME)) // 设置流模式
                if (PlayM4_OpenStream(g_nPort, NULL, 0, 1024 * 1024)) // 打开流
                    if (PlayM4_SetDecCallBackExMend(g_nPort, DecCBFun, NULL, 0, nUser))
                        if (PlayM4_Play(g_nPort, NULL))
                            std::cout << "success to set play mode" << std::endl;
                        else
                            std::cout << "fail to set play mode" << std::endl;
                    else
                        std::cout << "fail to set dec callback " << std::endl;
                else
                    std::cout << "fail to open stream" << std::endl;
            else
                std::cout << "fail to set stream open mode" << std::endl;
        else
            std::cout << "fail to get port" << std::endl;
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
            std::cout << "预览失败！错误码: " << dwErr << std::endl;
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
                break;
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
                QMutexLocker locker(&queueMutex);
                if (Camera::gImage.empty())
                    continue;
                BGR_image = Camera::gImage.front().clone();
                Camera::gImage.pop();
            }
            // 图像处理
            output = false;
            // 处理检测到的工序
            try
            {
                // 创建原图的副本进行处理，避免后续处理修改原图数据
                cv::Mat original_image = BGR_image.clone();
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
                if (enableAction)
                    processActionDetection(yoloV8->getActionFlag());
                emit updateActionState(actionGroup);
                // 将当前帧的壳体检测结果添加到滑动窗口
                bool current_keti_detected = keti_num > 0;
                keti_history.push_back(current_keti_detected);
                // 保持滑动窗口大小为 KETI_WINDOW_SIZE
                if (keti_history.size() > KETI_WINDOW_SIZE)
                    keti_history.pop_front();
                // 计算滑动窗口中检测到壳体的帧数
                int keti_count = std::count(keti_history.begin(), keti_history.end(), true);
                // 根据阈值确定最终的壳体状态
                cur_keti = keti_count >= KETI_THRESHOLD ? 1 : 0;

                if (cur_keti > 0)
                {
                    if (chilun_num == CHILUN_NUM)
                        chilun_flag = true;

                    if (luosi_num == LUOSI_NUM)
                        luosi_flag = true;

                    cv::putText(BGR_image, chilun_flag ? "chilun OK" : "chilun miss", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, chilun_flag ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
                    cv::putText(BGR_image, luosi_flag ? "luosi OK" : "luosi miss", cv::Point(10, 240), cv::FONT_HERSHEY_SIMPLEX, 1, luosi_flag ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
                    int yPos = 290;
                    // 显示动作检测状态
                    if (enableAction && !enabledActions.isEmpty())
                    {
                        for (int i : enabledActions)
                        {
                            if (i < static_cast<int>(actionGroup.size()))
                            {
                                cv::putText(BGR_image, QString("A%1:%2").arg(i + 1).arg(actionGroup[i] ? "OK" : "NO").toStdString(),
                                            cv::Point(10, yPos), cv::FONT_HERSHEY_SIMPLEX, 1,
                                            actionGroup[i] ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255), 2);
                                yPos += 30;
                            }
                        }
                    }
                    if (chilun_flag && luosi_flag && isActionsCompleted())
                    {
                        cv::putText(BGR_image, "ALL OK", cv::Point(10, yPos + 20), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // setD(2, 1); // 绿灯
                    }

                    emit updateLabelState(chilun_flag, luosi_flag, chilun_flag && luosi_flag); // 齿轮/螺丝/总体
                }

                if (cur_keti == 1 && last_keti == 0)
                {
                    startRecording(original_image, BGR_image);
                }

                if (isRecording)
                {
                    originalVideoWriter.write(original_image);
                    resultVideoWriter.write(BGR_image);
                }

                if (cur_keti == 0 && last_keti == 1)
                {
                    bool hasError = !chilun_flag || !luosi_flag || !isActionsCompleted();
                    if (hasError)
                    {
                        setD(0, 1); // PLC 报警

                        // 保存错误日志
                        QStringList errorDetails;
                        if (!chilun_flag)
                            errorDetails.append("chilun MISS");
                        if (!luosi_flag)
                            errorDetails.append("luosi MISS");
                        if (!isActionsCompleted())
                            errorDetails.append("actions NG");
                        QString logMsg = QString("ERROR | %1").arg(errorDetails.join(", "));
                        saveErrorLog(logMsg);
                    }

                    stopRecording(hasError);
                    emit updateStatistics(!hasError);
                }

                if (cur_keti == 0 && last_keti == 0)
                {
                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = luosi_flag = false;
                    // reset actionGroup and buttonState
                    actionGroup = {false, false, false, false, false};
                    emit updateLabelState(false, false, false); // 齿轮/螺丝/总体
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
        stopRecording(false);

        if (useLocalVideo)
            emit sendQStringtoMain("Local video closed");
        else
            emit sendQStringtoMain("Camera closed");
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

void Camera::processActionDetection(const std::vector<bool> &actions)
{
    if (!enabledActions.isEmpty() && !orderedActions.isEmpty())
    {
        // ===== 顺序检测模式 =====
        // 收集所有检测到的动作
        QVector<int> detected;
        for (int i = 0; i < static_cast<int>(actions.size()); i++)
        {
            if (actions[i])
                detected.append(i);
        }

        if (!detected.isEmpty())
        {
            // 1. 处理顺序列表中的动作（取最靠前的位置）
            int bestPos = -1, bestAction = -1;
            for (int action : detected)
            {
                int pos = orderedActions.indexOf(action);
                if (pos >= 0 && (bestPos == -1 || pos < bestPos))
                {
                    bestPos = pos;
                    bestAction = action;
                }
            }
            if (bestAction >= 0 && (bestPos == 0 || actionGroup[orderedActions[bestPos - 1]]))
            {
                actionGroup[bestAction] = true;
            }

            // 2. 处理其他启用的动作（不在顺序列表中，无顺序要求）
            for (int action : detected)
            {
                if (!orderedActions.contains(action) && enabledActions.contains(action))
                {
                    actionGroup[action] = true;
                }
            }
        }
    }
    else
    {
        // ===== 非顺序检测模式 =====
        // 检测所有启用的动作
        for (int i : enabledActions)
        {
            if (i < static_cast<int>(actions.size()) && actions[i])
            {
                actionGroup[i] = true;
            }
        }
    }
}

void Camera::setActionConfig(bool enable, const QVector<int> &enabled, bool affectsResult, const QVector<int> &ordered)
{
    enableAction = enable;
    enabledActions = enabled;
    actionAffectsResult = affectsResult;
    orderedActions = ordered;
}

bool Camera::isActionsCompleted()
{
    if (enableAction && !enabledActions.isEmpty() && actionAffectsResult)
    {
        for (int i : enabledActions)
        {
            if (i < static_cast<int>(actionGroup.size()) && !actionGroup[i])
                return false;
        }
    }
    return true;
}

void Camera::setSaveAllVideos(bool enable)
{
    saveAllVideos = enable;
}

void Camera::startRecording(const cv::Mat &originalFrame, const cv::Mat &resultFrame)
{
    if (isRecording)
        return;

    currentVideoTimestamp = QDateTime::currentDateTime().toString("yyyyMMddhhmmss_zzz");

    QString tempDir = baseVideoPath + "temp/";
    QDir(tempDir).mkpath(".");

    // 原图视频
    QString originalPath = tempDir + "original.avi";
    originalVideoWriter.open(originalPath.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, originalFrame.size());

    // 结果图视频
    QString resultPath = tempDir + "result.avi";
    resultVideoWriter.open(resultPath.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, resultFrame.size());

    if (originalVideoWriter.isOpened() && resultVideoWriter.isOpened())
    {
        isRecording = true;
        qDebug() << "Started recording:" << originalPath << "and" << resultPath;
    }
    else
    {
        if (originalVideoWriter.isOpened())
            originalVideoWriter.release();
        if (resultVideoWriter.isOpened())
            resultVideoWriter.release();
    }
}

void Camera::stopRecording(bool save)
{
    if (!isRecording)
        return;

    // 关闭视频写入
    if (originalVideoWriter.isOpened())
        originalVideoWriter.release();
    if (resultVideoWriter.isOpened())
        resultVideoWriter.release();

    QString tempDir = baseVideoPath + "temp/";
    QString originalPath = tempDir + "original.avi";
    QString resultPath = tempDir + "result.avi";

    if (saveAllVideos)
    {
        QString saveDir = baseVideoPath + "/all_videos/";
        QString originalAllDir = saveDir + "/original_all/";
        QString resultAllDir = saveDir + "/result_all/";
        QDir(originalAllDir).mkpath(".");
        QDir(resultAllDir).mkpath(".");

        QFile::copy(originalPath, originalAllDir + currentVideoTimestamp + ".avi");
        QFile::copy(resultPath, resultAllDir + currentVideoTimestamp + ".avi");

        emit sendQStringtoMain("All videos saved: " + currentVideoTimestamp);
    }

    if (save)
    {
        QString saveDir = baseVideoPath + QDateTime::currentDateTime().toString("yyyyMMdd");
        QString originalErrorDir = saveDir + "/original_error/";
        QString resultErrorDir = saveDir + "/result_error/";
        QDir(originalErrorDir).mkpath(".");
        QDir(resultErrorDir).mkpath(".");

        QFile::rename(originalPath, originalErrorDir + currentVideoTimestamp + ".avi");
        QFile::rename(resultPath, resultErrorDir + currentVideoTimestamp + ".avi");

        emit sendQStringtoMain("Error videos saved: " + currentVideoTimestamp);
    }

    // 删除临时目录
    QDir(tempDir).removeRecursively();

    isRecording = false;
}

void Camera::saveErrorLog(const QString &message)
{
    QFile file(baseVideoPath + "/error_log.txt");
    if (file.open(QIODevice::Append | QIODevice::Text))
    {
        QTextStream out(&file);
        QString ts = currentVideoTimestamp;
        QString formattedTime = ts.mid(0, 4) + "-" + ts.mid(4, 2) + "-" + ts.mid(6, 2) + " " + ts.mid(8, 2) + ":" + ts.mid(10, 2) + ":" + ts.mid(12, 2) + "." + ts.mid(15, 3);
        out << formattedTime << " | " << message << "\n";
        file.close();
    }
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
{
    qDebug("Closed");
    emit finished();
}
