#ifndef VIDEOSAVETHREAD_H
#define VIDEOSAVETHREAD_H

#include <QThread>
#include <QCoreApplication>
#include <opencv2/opencv.hpp>

// 视频录制/保存线程。
// 录制开始、逐帧写入、停止保存及临时目录清理均在专用线程内执行，避免阻塞相机主循环。
// 对外通过信号触发，槽在本线程事件循环中执行。
class VideoSaveThread : public QObject
{
    Q_OBJECT
public:
    VideoSaveThread();
    ~VideoSaveThread();
    void shutdown(); // 关闭线程
    void setRecordVideo(bool enable); // 设置是否录制视频
    void setSaveErrorVideo(bool enable); // 设置是否保存错误视频
    void setForceSaveVideo(bool enable); // 设置是否强制保存视频
    bool isRecordEnabled(); // 是否录制视频
    bool recording(); // 是否正在录制视频

public slots:
    void doStartRecording(const cv::Mat &originalFrame, const cv::Mat &resultFrame, const QString &timestamp); // 开始录制视频
    void doWriteFrame(const cv::Mat &originalFrame, const cv::Mat &resultFrame); // 写入视频帧
    void doStopRecording(bool save); // 停止录制视频

signals:
    void sendQStringtoMain(const QString &message);

private:
    QThread m_thread;
    bool recordVideoEnabled = false; // 是否录制视频
    bool saveErrorVideoEnabled = false; // 是否保存错误视频
    bool forceSaveVideo = false; // 是否强制保存视频
    cv::VideoWriter originalVideoWriter;
    cv::VideoWriter resultVideoWriter;
    bool isRecording = false;
    QString currentVideoTimestamp;
    QString currentTempDir;
    QString baseVideoPath = QCoreApplication::applicationDirPath() + "/saved_videos/";
};

Q_DECLARE_METATYPE(cv::Mat)

#endif // VIDEOSAVETHREAD_H
