#include "camera.h"
#include<QDebug>
#include<QImage>
#include <QImageReader>
#include<QTimer>
#include<QThread>
#include<string>
#include <QSettings>
#include <cmath> // For std::atan and std::abs
#include<QDateTime>
std::queue<cv::Mat> Camera::gImage;
cv::Mat g_BGRImage;
LONG g_nPort;
LONG nUser =1;
namespace fs = std::filesystem;
QMutex queueMutex;
//数据解码回调函数，
//功能：将YV_12格式的视频数据流转码为可供opencv处理的BGR类型的图片数据，并实时显示。
void CALLBACK DecCBFun(long nPort, char* pBuf, long nSize, FRAME_INFO* pFrameInfo, long nUser, long nReserved2)
{
//    std::cout << nUser << std::endl;
//    if (nUser == 1) {
//        std::cout << "camera" << std::endl;
//    }
    if (pFrameInfo->nType == T_YV12)
    {
        //std::cout << "the frame infomation is T_YV12" << std::endl;
        if (g_BGRImage.empty())
        {
            g_BGRImage.create(pFrameInfo->nHeight, pFrameInfo->nWidth, CV_8UC3);
        }
        cv::Mat YUVImage(pFrameInfo->nHeight + pFrameInfo->nHeight / 2, pFrameInfo->nWidth, CV_8UC1, (unsigned char*)pBuf);

        cv::cvtColor(YUVImage, g_BGRImage, cv::COLOR_YUV2BGR_YV12);
        //cv::imshow("RGBImage1", g_BGRImage);
        //cv::waitKey(15);
        QMutexLocker locker(&queueMutex);
        if(Camera::gImage.size()>1){
            Camera::gImage.pop();
        }
        Camera::gImage.push(g_BGRImage);//处理速度不够快的话，检查内存溢出问题


        YUVImage.~Mat();
    }
}
//实时视频码流数据获取 回调函数
void CALLBACK g_RealDataCallBack_V30(LONG lPlayHandle, DWORD dwDataType, BYTE* pBuffer, DWORD dwBufSize, void* pUser)
{
    if (dwDataType == NET_DVR_STREAMDATA)//码流数据
    {
        if (dwBufSize > 0 && g_nPort != -1)
        {
            if (!PlayM4_InputData(g_nPort, pBuffer, dwBufSize))
            {
                std::cout << "fail input data" << std::endl;
            }
            else
            {
                //std::cout << "success input data" << std::endl;
            }

        }
    }
}
Camera::Camera(QObject *parent)
{
    initCamera();
}
Camera::~Camera(){
    //销毁事件回调指针

    while (!Camera::gImage.empty()) {
            // 释放 cv::Mat 对象占用的内存
            Camera::gImage.front().release();
            // 从队列中移除该元素
            Camera::gImage.pop();
        }
}
void Camera::initCamera(){//初始化相机参数

}
void Camera::run(){
    YoloV8 yoloV8(onnxModelPath, config);//加载深度学习模型
    // 初始化
    NET_DVR_Init();
    // 设置连接时间与重连时间
    NET_DVR_SetConnectTime(2000, 1);
    NET_DVR_SetReconnect(10000, true);

    //------------------------------
    // 登录
    pLoginInfo = { 0 };
    lpDeviceInfo = { 0 };

    pLoginInfo.bUseAsynLogin = 0;  // 同步登录方式
    char* sDeviceAddress, * sUserName, * sPassword;
    wPort = 8000;
    // 修改后
    char ip[] = "192.168.31.105"; // 栈上创建可修改副本
    //char ip[] = "192.168.1.64"; // 厂里
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
    if (lUserID < 0) {
        std::cout << "注册失败！\n";
        system("pause");

    }
    else {
        std::cout << "注册成功！" << std::endl;
        Sleep(1000);  // 显示注册相关信息
    }

    if (PlayM4_GetPort(&g_nPort))            //获取播放库通道号
    {
        if (PlayM4_SetStreamOpenMode(g_nPort, STREAME_REALTIME))      //设置流模式
        {
            if (PlayM4_OpenStream(g_nPort, NULL, 0, 1024 * 1024))         //打开流
            {
                if (PlayM4_SetDecCallBackExMend(g_nPort, DecCBFun, NULL, 0, nUser))//NULL 替换为nUser了
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
    Sleep(1000);  // 显示播放端口打开情况

    //启动预览并设置回调数据流
    struPlayInfo = { 0 };
    struPlayInfo.hPlayWnd = NULL; //窗口为空，设备SDK不解码只取流
    struPlayInfo.lChannel = 1; //Channel number 设备通道
    struPlayInfo.dwStreamType = 0;// 码流类型，0-主码流，1-子码流，2-码流3，3-码流4, 4-码流5,5-码流6,7-码流7,8-码流8,9-码流9,10-码流10
    struPlayInfo.dwLinkMode = 0;// 0：TCP方式,1：UDP方式,2：多播方式,3 - RTP方式，4-RTP/RTSP,5-RSTP/HTTP
    struPlayInfo.bBlocked = 0; //0-非阻塞取流, 1-阻塞取流, 如果阻塞SDK内部connect失败将会有5s的超时才能够返回,不适合于轮询取流操作.


    qDebug() << "Camera" << id << "opened successfully";
    if (NET_DVR_RealPlay_V40(lUserID, &struPlayInfo, g_RealDataCallBack_V30, NULL))//开始取流
    {
        //cv::namedWindow("RGBImage2");
    }
     try
     {
        this->Camera_thread_flag = false;
         //等待结束
         int fps;
         double t = 0;


        while(true){
            //QThread::msleep(1000);
             if (Camera_thread_flag){
                    break;
             }

             if(Camera::gImage.empty()){
                 continue;
             }

             t = (double)cv::getTickCount();
             QMutexLocker locker(&queueMutex);
             BGR_image = Camera::gImage.front();
             Camera::gImage.pop();
             //帧率计算


            //图像处理
            output = false;
            // 处理检测到的工序
            bool detectedP1 = false, detectedP2 = false, detectedP3 = false;
            try {
                // Run inference 推理
                //qDebug()<<"run inference";
                const auto objects = yoloV8.detectObjects(BGR_image);
                // Draw the bounding boxes on the image
                yoloV8.drawObjectLabels(BGR_image, objects,2);//绘制框
                std::vector<int> classCount = YoloV8.getclassnumer;
                int chilun_num = classCount[0];
                int keti_num = classCount[1];
                int luosi_num = classCount[2];
                //"chilun",   "keti" ,"luosi"
                std::cout<<"class0"<<chilun_num<<"class1"<<keti_num<<"class2"<<luosi_num<<std::endl;
                cur_keti = keti_num;
                if (cur_keti > 0){
                    if (chilun_num == CHILUN_NUM ){
                        //满了 plc res_flag置1
                        //cv2.putText(image, "OK", (10, 120), cv::FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                        chilun_flag = true;
                    }
                        
                    if (luosi_num == LUOSI_NUM){
                        //满了 plc res_flag置1
                        //cv2.putText(image, "OK", (10, 130), cv::FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2)
                        luosi_flag = true;
                    }  
                }
                last_keti = cur_keti;
                if (chilun_flag && !luosi_flag){
                    //绘制消息框
                    cv::putText(BGR_image, "chilun OK",  cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi not yet",  cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1,cv::Scalar(0, 255, 0), 2);
                }
                
                if (luosi_flag && !chilun_flag){
                    //绘制消息框
                    cv::putText(BGR_image, "chilun not yet",  cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK",  cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                }
                    
                if (chilun_flag && luosi_flag){
                    //绘制消息框
                    cv::putText(BGR_image, "chilun OK",  cv::Point(10, 190), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "luosi OK",  cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                    cv::putText(BGR_image, "ALL OK",  cv::Point(10, 290), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                }
                    
                if (cur_keti==0 && last_keti==1){
                    //keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                    //并检查是否漏装对齐
                    if (!chilun_flag){
                        //绘制消息框
                        cv::putText(BGR_image, "chilun miss",  cv::Point(10, 210), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        //PLC 报警
                    }
                        
                    if (!luosi_flag){
                        //绘制消息框 
                        cv::putText(BGR_image, "luosi miss",  cv::Point(10, 230), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
                        //PLC 报警
                    }
                        
                }
                    
                if (cur_keti==0 && last_keti==0){
                    //keti消失，chilun_flag和luosi_flag置0
                    chilun_flag = false;
                    luosi_flag = false;
                }

            } catch (...) {
                // 处理所有异常的逻辑
               std::cerr << "An unknown exception occurred during image processing." << std::endl;
               sendQStringtoMain("An unknown exception occurred during image processing.");
            }
            QImage a = cvMat2QImage(BGR_image);
            QImage IMG =a.scaled(300, 300, Qt::KeepAspectRatio);
            sendQImgToAutoMain(IMG);

    }

    //结束停止采集
    //发送停采命令
    NET_DVR_Logout(lUserID);
    NET_DVR_Cleanup();
    emit finishedthread();
    //注销采集回调
    //注销远端设备事件
    //释放资源
  }

  catch(std::exception&e)
  {
   qDebug()<<"error info: "<<e.what() ;
  }
  //反初始化库
  //销毁事件回调指针

}
void Camera::ExecuteMianToThread(){
    stop_camera();
    qDebug() << "shoudao " << "\n";
 //   DestroyedHandle();
    emit finished();
}
void Camera::stop_camera(){
    this->Camera_thread_flag=true;

}
void Camera::closeDevice(){//关闭设备
    // ch:关闭设备 | Close device

   qDebug("Closed");

   emit finished();
}
bool Camera::imageProcess(cv::Mat image){
    return true;
}
QImage Camera::cvMat2QImage(const cv::Mat& mat){
    // 8-bits unsigned, NO. OF CHANNELS = 1
    if(mat.type() == CV_8UC1)
    {
        qDebug()<<" CV_8UC1";
        QImage qimage(mat.cols, mat.rows, QImage::Format_Indexed8);
        // Set the color table (used to translate colour indexes to qRgb values)
        qimage.setColorCount(256);
        for(int i = 0; i < 256; i++)
        {
            qimage.setColor(i, qRgb(i, i, i));
        }
        // Copy input Mat
        uchar *pSrc = mat.data;
        for(int row = 0; row < mat.rows; row ++)
        {
            uchar *pDest = qimage.scanLine(row);
            memcpy(pDest, pSrc, mat.cols);
            pSrc += mat.step;
        }
        return qimage;
    }
    // 8-bits unsigned, NO. OF CHANNELS = 3
    else if(mat.type() == CV_8UC3)
    {   //qDebug()<<" CV_8UC3";
        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
        return image.rgbSwapped();
    }
    else if(mat.type() == CV_8UC4)
    {
        // Copy input Mat
        const uchar *pSrc = (const uchar*)mat.data;
        // Create QImage with same dimensions as input Mat
        QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_ARGB32);
        return image.copy();
    }
    else
    {
        return QImage();
    }
}
