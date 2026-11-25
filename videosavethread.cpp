#include "videosavethread.h"
#include <QDir>
#include <QDateTime>
#include <QDebug>

VideoSaveThread::VideoSaveThread()
{
    // 将本工作对象移至专用线程，使槽在该线程的事件循环中执行
    moveToThread(&m_thread);
    m_thread.start();

    // 对象创建时清理上次运行残留的临时录制目录
    QDir(baseVideoPath + "temp/").removeRecursively();
}

VideoSaveThread::~VideoSaveThread()
{
    m_thread.quit();
    m_thread.wait();
}

void VideoSaveThread::shutdown()
{
    m_thread.quit();
    m_thread.wait();
    // 保存线程已停止，将对象移回调用方线程，避免跨线程析构
    moveToThread(QThread::currentThread());
}

void VideoSaveThread::setRecordVideo(bool enable)
{
    recordVideoEnabled = enable;
}

void VideoSaveThread::setSaveErrorVideo(bool enable)
{
    saveErrorVideoEnabled = enable;
}

void VideoSaveThread::setForceSaveVideo(bool enable)
{
    forceSaveVideo = enable;
}

bool VideoSaveThread::isRecordEnabled()
{
    return recordVideoEnabled;
}

bool VideoSaveThread::recording()
{
    return isRecording;
}

void VideoSaveThread::doStartRecording(const cv::Mat &originalFrame, const cv::Mat &resultFrame, const QString &timestamp)
{
    if (isRecording)
        return;

    currentVideoTimestamp = timestamp;

    currentTempDir = baseVideoPath + "temp/" + currentVideoTimestamp + "/";
    QDir().mkpath(currentTempDir);

    // 原图视频
    QString originalPath = currentTempDir + "original.avi";
    originalVideoWriter.open(originalPath.toStdString(), cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), 15, originalFrame.size());

    // 结果图视频
    QString resultPath = currentTempDir + "result.avi";
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

        // 打开失败，清掉本段空目录
        QDir(currentTempDir).removeRecursively();
        currentTempDir.clear();
    }
}

void VideoSaveThread::doWriteFrame(const cv::Mat &originalFrame, const cv::Mat &resultFrame)
{
    if (!isRecording)
        return;

    originalVideoWriter.write(originalFrame);
    resultVideoWriter.write(resultFrame);
}

void VideoSaveThread::doStopRecording(bool save)
{
    if (!isRecording)
        return;

    // 关闭视频写入
    if (originalVideoWriter.isOpened())
        originalVideoWriter.release();
    if (resultVideoWriter.isOpened())
        resultVideoWriter.release();

    QString originalPath = currentTempDir + "original.avi";
    QString resultPath = currentTempDir + "result.avi";

    auto copyToPairDirs = [&](const QString &saveDir, const QString &origSubDir, const QString &resSubDir, const QString &logTag)
    {
        QString originalDir = saveDir + origSubDir + "/";
        QString resultDir = saveDir + resSubDir + "/";
        QDir(originalDir).mkpath(".");
        QDir(resultDir).mkpath(".");

        if (QFile::exists(originalPath))
            QFile::copy(originalPath, originalDir + currentVideoTimestamp + ".avi");
        if (QFile::exists(resultPath))
            QFile::copy(resultPath, resultDir + currentVideoTimestamp + ".avi");

        emit sendQStringtoMain(logTag + currentVideoTimestamp);
    };

    if (saveErrorVideoEnabled && save)
        copyToPairDirs(baseVideoPath + QDateTime::currentDateTime().toString("yyyyMMdd") + "/", "original_error", "result_error", "Error videos saved: ");

    if (recordVideoEnabled && forceSaveVideo)
        copyToPairDirs(baseVideoPath + "/force_save/", "original_force", "result_force", "Force saved videos: ");

    // 删除本次录制的临时子目录
    if (!currentTempDir.isEmpty())
    {
        QDir(currentTempDir).removeRecursively();
        currentTempDir.clear();
    }

    isRecording = false;
    qDebug() << "Stopped recording:" << originalPath << "and" << resultPath;
}
