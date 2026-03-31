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
void onMouse(int event, int x, int y, int flags, void* param) {
    static cv::Point pt1, pt2;
    cv::Mat* img = static_cast<cv::Mat*>(param);
    
    if (event == cv::EVENT_LBUTTONDOWN) {
        pt1 = cv::Point(x, y);
    }
    else if (event == cv::EVENT_LBUTTONUP) {
        pt2 = cv::Point(x, y);
        roi_rect = cv::Rect(pt1, pt2);
        roi_selected = true;
        
        // 在原图上绘制矩形
        cv::Mat display_img = img->clone();
        cv::rectangle(display_img, roi_rect, cv::Scalar(0, 255, 0), 2);
        cv::imshow("Select ROI", display_img);
    }
    else if (event == cv::EVENT_MOUSEMOVE && (flags & cv::EVENT_FLAG_LBUTTON)) {
        cv::Mat display_img = img->clone();
        cv::rectangle(display_img, pt1, cv::Point(x, y), cv::Scalar(0, 255, 0), 2);
        cv::imshow("Select ROI", display_img);
    }
}
// 数据解码回调函数，
// 功能：将YV_12格式的视频数据流转码为可供opencv处理的BGR类型的图片数据，并实时显示。
void CALLBACK DecCBFun(long nPort, char *pBuf, long nSize, FRAME_INFO *pFrameInfo, long nUser, long nReserved2)
{
    //    std::cout << nUser << std::endl;
    //    if (nUser == 1) {
    //        std::cout << "camera" << std::endl;
    //    }
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
    //ctx = modbus_new_tcp("192.168.1.11", 502);
    ctx = modbus_new_tcp("192.168.31.99", 2001);//西门子smart 200 厂里是192.168.1.11
    if (ctx == NULL)
    {
        qDebug() << "cannot create modbus";
        return;
    }

    // 连接到Modbus服务器
    if (modbus_connect(ctx) == -1)
    {
        qDebug() << "connect to server fail";
        modbus_free(ctx);

        emit send_connectstate(false);
        return;
    }
    emit send_connectstate(true);
    YoloV8 yoloV8(onnxModelPath, config); // 加载深度学习模型
    // 初始化
    NET_DVR_Init();
    // 设置连接时间与重连时间
    NET_DVR_SetConnectTime(2000, 1);
    NET_DVR_SetReconnect(10000, true);

    //------------------------------
    // 登录
    pLoginInfo = {0};
    lpDeviceInfo = {0};

    pLoginInfo.bUseAsynLogin = 0; // 同步登录方式
    char *sDeviceAddress, *sUserName, *sPassword;
    wPort = 8000;
    // 修改后
    char ip[] = "192.168.31.105"; // 栈上创建可修改副本
    // char ip[] = "192.168.1.64"; // 厂里
    sDeviceAddress = ip;
    char admin[] = "admin";
    sUserName = admin;
    char pwd[] = "CXF643200";
    sPassword = pwd;
    strcpy_s(pLoginInfo.sDeviceAddress, sDeviceAddress);
    strcpy_s(pLoginInfo.sUserName, sUserName);
    strcpy_s(pLoginInfo.sPassword, sPassword);
    pLoginInfo.wPort = wPort;

    lUserID = NET_DVR_Login_V40(&pLoginInfo, &lpDeviceInfo);
    if (lUserID < 0)
    {
        std::cout << "注册失败！\n";
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

    if (NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL)) // 开始取流
    {
        // cv::namedWindow("RGBImage2");
    }
    try
    {
        this->Camera_thread_flag = false;
        // 等待结束
        int fps;
        double t = 0;

        while (true)
        {
            QThread::msleep(10);
            if (Camera_thread_flag)
            {
                break;
            }

            if (Camera::gImage.empty())
            {
                continue;
            }

            t = (double)cv::getTickCount();
            QMutexLocker locker(&queueMutex);
            BGR_image = Camera::gImage.front();
            Camera::gImage.pop();
            // 帧率计算
            // 图像处理
            output = false;
            // 处理检测到的工序
            try
            {
                // Run inference 推理
                // qDebug()<<"run inference";
                const auto objects = yoloV8.detectObjects(BGR_image);
                // Draw the bounding boxes on the image
                yoloV8.drawObjectLabels(BGR_image, objects); // 绘制框
                std::vector<int> classCount = yoloV8.getclassnumer();
                int chilun_num = classCount[0];
                int keti_num = classCount[1];
                int luosi_num = classCount[2];
                //检查动作是否有做到了（瞬时动作可以消失）
                std::vector<bool> tempAction = yoloV8.getActionFlag();
                if (tempAction[0]){
                    actionGroup[0] = true;
                }
                if (tempAction[1]){
                    actionGroup[1] = true;
                }
                if (tempAction[2]){
                    actionGroup[2] = true;
                }
                if (tempAction[3]){
                    actionGroup[3] = true;
                }
                if (tempAction[4]){
                    actionGroup[4] = true;
                }
                //"chilun",   "keti" ,"luosi"
                std::cout << "class0" << chilun_num << "class1" << keti_num << "class2" << luosi_num << std::endl;
                cur_keti = keti_num;
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
                last_keti = cur_keti;
                emit updateActionState(actionGroup);
                if (chilun_flag && !luosi_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi not yet", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(true, false, false); // 齿轮/螺丝/ 总体
                }

                if (luosi_flag && !chilun_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun not yet", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(false, true, false); // 齿轮/螺丝/ 总体
                }

                if (chilun_flag && luosi_flag)
                {
                    // 绘制消息框
                    cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "ALL OK", cv::Point(10, 290), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    emit updateButtonState(true, true, true); // 齿轮/螺丝/ 总体
                    // PLC 接收
                    setD(2,1);//绿灯 
                }

                if (cur_keti == 0 && last_keti == 1)
                {
                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                    // 并检查是否漏装对齐
                    if (!chilun_flag)
                    {
                        // 绘制消息框
                        cv::putText(BGR_image, "chilun miss", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // PLC 报警
                        setD(0,1);//置位
                    }

                    if (!luosi_flag)
                    {
                        // 绘制消息框
                        cv::putText(BGR_image, "luosi miss", cv::Point(10, 230), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        // PLC 报警
                        setD(0,1);//置位
                    }
                }

                if (cur_keti == 0 && last_keti == 0)
                {
                    // keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                    //reset actionGroup and buttonState
                    actionGroup = {false,false,false,false,false};
                    emit updateButtonState(false, true, false); // 齿轮/螺丝/ 总体
                    //复位PLC输出
                    setD(0,0);//复位报警
                    setD(2,0);//复位绿灯
                }
            }
            catch (...)
            {
                // 处理所有异常的逻辑
                std::cerr << "An unknown exception occurred during image processing." << std::endl;
                sendQStringtoMain("An unknown exception occurred during image processing.");
            }
            QImage a = cvMat2QImage(BGR_image);
            QImage IMG = a.scaled(300, 300, Qt::KeepAspectRatio);
            sendQImgToAutoMain(IMG);
        }

        // 结束停止采集
        //  先停止预览
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
    //   DestroyedHandle();
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
void Camera::set32D(int address,int32_t value){//设置32位D
    // 确保value在int32_t的范围内
    if (value < INT32_MIN || value > INT32_MAX) {
        std::cerr << "Value out of range for int32_t" << std::endl;
    }

    // 将32位整数分割为两个16位部分
    uint16_t high = static_cast<uint16_t>((value >> 16) & 0xFFFF); // 取高16位
    uint16_t low = static_cast<uint16_t>(value & 0xFFFF); // 取低16位
    rc =modbus_write_register(ctx,address,low);
    rc =modbus_write_register(ctx,address+1,high);
}
void Camera::setD(int address,int value){//设置16位 D
    rc =modbus_write_register(ctx,address,value);
}
int Camera::setRoi(){
    //获取一张BGR_image，显示出来让用户进行手动框选，然后保存框选的坐标保存到roi_x,roi_y,roi_w,roi_h中
    //然后根据框选的坐标进行图像处理
    //test 实际使用时注释
    cv::Mat BGR_image = cv::imread("C:\\Users\\chenxinfeng\\Desktop\\异物图片 裁剪后\\22.bmp");
    if (BGR_image.empty()) {
        qDebug() << "cannot load image" ;
        return -1;
    }
    
    // 显示图像并设置鼠标回调
    cv::namedWindow("Select ROI", cv::WINDOW_NORMAL);
    cv::setMouseCallback("Select ROI", onMouse, &BGR_image);
    cv::imshow("Select ROI", BGR_image);
    
    qDebug() << "select roi by mouse..." ;
    cv::waitKey(0);
    
    if (roi_selected) {
        // 保存ROI坐标
        int roi_x = roi_rect.x;
        int roi_y = roi_rect.y;
        int roi_w = roi_rect.width;
        int roi_h = roi_rect.height;
        
        qDebug() << "select ROI : x=" << roi_x << ", y=" << roi_y 
             << ", width=" << roi_w << ", height=" << roi_h ;
        
        // 提取ROI区域
        cv::Mat roi_image = BGR_image(roi_rect);
        
        // 显示ROI区域
        cv::imshow("Selected ROI", roi_image);
        cv::waitKey(0);
    } else {
        qDebug()<< "not select roi yet" ;
    }
    qDebug() << "get select ROI : x=" << roi_x << ", y=" << roi_y 
             << ", width=" << roi_w << ", height=" << roi_h ;
    
    return 0;

}
void Camera::aiTest(){
    YoloV8 yoloV8(onnxModelPath, config); // 加载深度学习模型
    //test 实际使用时注释
    qDebug() << "cannot load image" ;
    //load video
    std::string video_path ="video\\work_process.mp4" ;
    cv::VideoCapture cap(video_path);
    if (!cap.isOpened()) {
        qDebug() << "cannot open video" ;
        return;
    }
    cv::Mat frame;
    cv::Mat image ;
    
    while (cap.read(frame)) {
        // 对每一帧进行处理
        cv::resize(frame, image, cv::Size(640, 480));
        //模型预测
        const auto objects = yoloV8.detectObjects(image);
        // Draw the bounding boxes on the image
        yoloV8.drawObjectLabels(image, objects); // 绘制框
        std::vector<int> classCount = yoloV8.getclassnumer();
        int chilun_num = classCount[0];
        int keti_num = classCount[1];
        int luosi_num = classCount[2];
        //检查动作是否有做到了（瞬时动作可以消失）
        std::vector<bool> tempAction = yoloV8.getActionFlag();
        if (tempAction[0]){
            actionGroup[0] = true;
        }
        if (tempAction[1]){
            actionGroup[1] = true;
        }
        if (tempAction[2]){
            actionGroup[2] = true;
        }
        if (tempAction[3]){
            actionGroup[3] = true;
        }
        if (tempAction[4]){
            actionGroup[4] = true;
        }
        //"chilun",   "keti" ,"luosi"
        std::cout << "class0" << chilun_num << "class1" << keti_num << "class2" << luosi_num << std::endl;
        cur_keti = keti_num;
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
        last_keti = cur_keti;
        emit updateActionState(actionGroup);
        if (chilun_flag && !luosi_flag)
        {
            // 绘制消息框
            cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::putText(BGR_image, "luosi not yet", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            emit updateButtonState(true, false, false); // 齿轮/螺丝/ 总体
        }

        if (luosi_flag && !chilun_flag)
        {
            // 绘制消息框
            cv::putText(BGR_image, "chilun not yet", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            emit updateButtonState(false, true, false); // 齿轮/螺丝/ 总体
        }

        if (chilun_flag && luosi_flag)
        {
            // 绘制消息框
            cv::putText(BGR_image, "chilun OK", cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::putText(BGR_image, "luosi OK", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            cv::putText(BGR_image, "ALL OK", cv::Point(10, 290), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            emit updateButtonState(true, true, true); // 齿轮/螺丝/ 总体
            // PLC 接收
            //setD(1,1);//绿灯 
        }

        if (cur_keti == 0 && last_keti == 1)
        {
            // keti消失，chilun_flag和luosi_flag置0
            chilun_flag = false;
            luosi_flag = false;
            // 并检查是否漏装对齐
            if (!chilun_flag)
            {
                // 绘制消息框
                cv::putText(BGR_image, "chilun miss", cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                // PLC 报警
                //setD(0,2);//置位
            }

            if (!luosi_flag)
            {
                // 绘制消息框
                cv::putText(BGR_image, "luosi miss", cv::Point(10, 230), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                // PLC 报警
                //setD(0,2);//置位
            }
        }

        if (cur_keti == 0 && last_keti == 0)
        {
            // keti消失，chilun_flag和luosi_flag置0
            chilun_flag = false;
            luosi_flag = false;
            //reset actionGroup and buttonState
            actionGroup = {false,false,false,false,false};
            emit updateButtonState(false, true, false); // 齿轮/螺丝/ 总体
            //复位PLC输出
            //setD(0,0);//复位报警
            //setD(2,0);//复位绿灯
        }
        cv::imshow("video", image);
        cv::waitKey(5);
        

    }
    cap.release();
    cv::destroyAllWindows();



     
}


