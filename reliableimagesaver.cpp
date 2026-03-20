#include "reliableimagesaver.h"
#include <QThread>

ReliableImageSaver::ReliableImageSaver(QObject* parent)
    : QObject(parent)
    , m_isRunning(true)
    , m_compressionQuality(80)  // 默认压缩质量90
{
    // 启动保存线程
    m_saveThread = std::thread(&ReliableImageSaver::saveWorker, this);
    qDebug() << "ReliableImageSaver: save thread has started";
}

ReliableImageSaver::~ReliableImageSaver()
{
    stop();
}

bool ReliableImageSaver::addImage(const cv::Mat& image, const QString& filename, const QString& folder)
{
    std::lock_guard<std::mutex> lock(m_queueMutex);

    // 检查队列状态
    if (m_saveQueue.size() >= MAX_QUEUE_SIZE) {
        qWarning() << "ReliableImageSaver: save queue has alread full, cannot add new image" << filename;
        return false;
    }

    SaveTask task;
    task.image = image.clone(); // 深拷贝确保数据安全
    task.filename = filename;
    task.folder = folder;
    task.timestamp = QDateTime::currentDateTime();

    m_saveQueue.push(task);
    size_t currentSize = m_saveQueue.size();
    m_condition.notify_one();

    qDebug() << "ReliableImageSaver: image has enter queue" << filename << "queue size is :" << currentSize;

    // 发送队列状态变化信号
    emit queueStatusChanged(currentSize);

    return true;
}

void ReliableImageSaver::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_isRunning = false;
    }
    m_condition.notify_all();

    if (m_saveThread.joinable()) {
        m_saveThread.join();
    }

    qDebug() << "ReliableImageSaver: image saver stopped";
}

size_t ReliableImageSaver::getQueueSize()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_saveQueue.size();
}

bool ReliableImageSaver::isQueueHealthy()
{
    std::lock_guard<std::mutex> lock(m_queueMutex);
    return m_saveQueue.size() < WARNING_QUEUE_SIZE;
}

void ReliableImageSaver::setBasePath(const QString& path)
{
    m_basePath = path;
    // 立即创建目录结构
    ensureDirectoryStructure();
    qDebug() << "ReliableImageSaver save path is: " << path;
}

void ReliableImageSaver::saveWorker()
{
    qDebug() << "ReliableImageSaver: saver thread starting ...";

    while (true) {
        SaveTask task;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_condition.wait(lock, [this]() {
                return !m_saveQueue.empty() || !m_isRunning;
            });

            // 检查是否应该退出
            if (!m_isRunning && m_saveQueue.empty()) {
                break;
            }

            // 获取任务
            if (!m_saveQueue.empty()) {
                task = m_saveQueue.front();
                m_saveQueue.pop();

                // 发送队列状态变化信号
                emit queueStatusChanged(m_saveQueue.size());
            } else {
                continue;
            }
        }

        // 执行保存操作（带重试机制）
        if (!task.image.empty()) {
            bool success = saveImageWithRetry(task, 3); // 重试3次
            if (success) {
                QString fullPath = m_basePath + "/" + task.folder + "/" + task.filename;
                qDebug() << "ReliableImageSaver: save success" << task.filename;
                emit imageSaved(fullPath);
            } else {
                qCritical() << "ReliableImageSaver: save failed:" << task.filename;
                QString fullPath = m_basePath + "/" + task.folder + "/" + task.filename;
                emit imageSaveFailed(fullPath, "save failed, up to most  save numbers");
            }
        }
    }

    qDebug() << "ReliableImageSaver: saver thread stopped";
}

bool ReliableImageSaver::saveImageWithRetry(const SaveTask& task, int maxRetries)
{
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        if (saveImageToDisk(task)) {
            return true;
        }
        if (attempt < maxRetries) {
            qWarning() << "ReliableImageSaver: save failed number now is: " << attempt << "times" << task.filename;
            QThread::msleep(100 * attempt); // 递增延迟
        }
    }
    return false;
}

bool ReliableImageSaver::saveImageToDisk(const SaveTask& task)
{
    // 构建完整路径
    QString fullPath = m_basePath + "/" + task.folder;
    QDir dir;
    if (!dir.exists(fullPath)) {
        if (!dir.mkpath(fullPath)) {
            qCritical() << "ReliableImageSaver: create folder failed" << fullPath;
            return false;
        }
    }

    QString filePath = fullPath + "/" + task.filename;
    std::string savePathStd = filePath.toStdString();
    // 设置压缩参数
    std::vector<int> compression_params;
    compression_params.push_back(cv::IMWRITE_JPEG_QUALITY);
    compression_params.push_back(m_compressionQuality);

    // 尝试保存
    bool success = cv::imwrite(savePathStd, task.image,compression_params);
    if (!success) {
        qWarning() << "ReliableImageSaver: cv::imwrite failed:" << filePath;
    }

    return success;
}

void ReliableImageSaver::ensureDirectoryStructure()
{
    if (m_basePath.isEmpty()) return;

    QDir dir;
    QStringList folders = {"raw_images", "processed_images", "failed_images"};
    for (const QString& folder : folders) {
        QString fullPath = m_basePath + "/" + folder;
        if (!dir.exists(fullPath)) {
            if (dir.mkpath(fullPath)) {
                qDebug() << "ReliableImageSaver: create folder" << fullPath;
            } else {
                qWarning() << "ReliableImageSaver: create folder failed:" << fullPath;
            }
        }
    }
}
void ReliableImageSaver::setCompressionQuality(int quality) {
    if (quality >= 0 && quality <= 100) {
        m_compressionQuality = quality;
    }
}
