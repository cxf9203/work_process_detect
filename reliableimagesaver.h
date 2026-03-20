#ifndef RELIABLEIMAGESAVER_H
#define RELIABLEIMAGESAVER_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QDir>
#include <QDebug>
#include <opencv2/opencv.hpp>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

class ReliableImageSaver : public QObject
{
    Q_OBJECT

public:
    explicit ReliableImageSaver(QObject* parent = nullptr);
    ~ReliableImageSaver();

    // 添加图像到保存队列（同步操作，确保重要图像不被丢弃）
    bool addImage(const cv::Mat& image, const QString& filename, const QString& folder);

    // 停止保存线程
    void stop();

    // 获取当前队列大小
    size_t getQueueSize();

    // 检查队列健康状态
    bool isQueueHealthy();

    // 设置基础保存路径
    void setBasePath(const QString& path);

    // 获取基础保存路径
    QString getBasePath() const { return m_basePath; }

    // 检查是否正在运行
    bool isRunning() const { return m_isRunning; }
    //设置压缩比例
    void setCompressionQuality(int quality) ;
signals:
    // 图像保存成功信号
    void imageSaved(const QString& filepath);

    // 图像保存失败信号
    void imageSaveFailed(const QString& filepath, const QString& error);

    // 队列状态变化信号
    void queueStatusChanged(size_t queueSize);

private:
    struct SaveTask {
        cv::Mat image;
        QString filename;
        QString folder;
        QDateTime timestamp;
    };

    // 保存工作线程函数
    void saveWorker();

    // 带重试机制的保存函数
    bool saveImageWithRetry(const SaveTask& task, int maxRetries);

    // 保存图像到磁盘
    bool saveImageToDisk(const SaveTask& task);

    // 确保目录结构存在
    void ensureDirectoryStructure();

private:
    std::thread m_saveThread;
    std::queue<SaveTask> m_saveQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_condition;
    std::atomic<bool> m_isRunning;
    QString m_basePath;
    int m_compressionQuality; // 压缩质量（0-100，对于JPEG）

    // 队列限制（设置为较大值，因为每秒1张不算快）
    static const size_t MAX_QUEUE_SIZE = 500;       // 最大队列大小
    static const size_t WARNING_QUEUE_SIZE = 400;   // 警告阈值
};

#endif // RELIABLEIMAGESAVER_H
