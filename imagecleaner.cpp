#include "imagecleaner.h"
#include <algorithm>

ImageCleaner::ImageCleaner(QObject *parent)
    : QObject(parent)
{
}

bool ImageCleaner::cleanupOldImages(const QString &folderPath, int keepDays, int maxCount)
{
    QDir dir(folderPath);
    if (!dir.exists())
    {
        emit cleanupError(QString("folder not exist: %1").arg(folderPath));
        return false;
    }

    emit cleanupStarted(folderPath);

    int deletedCount = cleanupFolder(folderPath, keepDays, maxCount);

    qint64 folderSize = getFolderSize(folderPath);
    emit cleanupFinished(folderPath, deletedCount, folderSize);

    qDebug() << "clean image finished:" << folderPath
             << "clean files number:" << deletedCount
             << "current folder size:" << folderSize / (1024 * 1024) << "MB";

    return true;
}

bool ImageCleaner::cleanupMultipleFolders(const QStringList &folderPaths, int keepDays, int maxCount)
{
    bool allSuccess = true;

    for (const QString &folderPath : folderPaths)
    {
        if (!cleanupOldImages(folderPath, keepDays, maxCount))
        {
            allSuccess = false;
        }
        // 可选：添加短暂延迟避免磁盘IO过载
        QThread::msleep(100);
    }

    return allSuccess;
}

qint64 ImageCleaner::getFolderSize(const QString &folderPath)
{
    qint64 totalSize = 0;
    QDir dir(folderPath);

    // 获取所有文件（包括子目录）
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QFileInfo &fileInfo : files)
    {
        if (fileInfo.isDir())
        {
            totalSize += getFolderSize(fileInfo.absoluteFilePath());
        }
        else
        {
            // 检查是否是图像文件
            QString suffix = fileInfo.suffix().toLower();
            if (m_imageExtensions.contains("*." + suffix))
            {
                totalSize += fileInfo.size();
            }
        }
    }

    return totalSize;
}

int ImageCleaner::getFileCount(const QString &folderPath)
{
    int count = 0;
    QDir dir(folderPath);

    // 获取所有图像文件
    QStringList filters = m_imageExtensions;
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
    count += files.count();

    // 递归子目录
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &dirInfo : dirs)
    {
        count += getFileCount(dirInfo.absoluteFilePath());
    }

    return count;
}

qint64 ImageCleaner::getAvailableSpace(const QString &path)
{
    QStorageInfo storage(path);
    return storage.bytesAvailable();
}

qint64 ImageCleaner::getTotalSpace(const QString &path)
{
    QStorageInfo storage(path);
    return storage.bytesTotal();
}

int ImageCleaner::cleanupFolder(const QString &folderPath, int keepDays, int maxCount)
{
    QDir dir(folderPath);
    int deletedCount = 0;

    // 获取所有图像文件
    QStringList filters = m_imageExtensions;
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time); // 按时间排序

    QDateTime cutoffDate;
    if (keepDays > 0)
    {
        cutoffDate = QDateTime::currentDateTime().addDays(-keepDays);
    }

    // 处理文件数量限制
    if (maxCount > 0 && files.count() > maxCount)
    {
        int filesToDelete = files.count() - maxCount;
        for (int i = files.count() - 1; i >= maxCount && filesToDelete > 0; --i, --filesToDelete)
        {
            const QFileInfo &fileInfo = files[i];
            if (QFile::remove(fileInfo.absoluteFilePath()))
            {
                deletedCount++;
                emit fileDeleted(fileInfo.absoluteFilePath());
                qDebug() << "delete old files(number limit):" << fileInfo.fileName();
            }
        }
    }

    // 处理时间限制
    if (keepDays > 0)
    {
        for (const QFileInfo &fileInfo : files)
        {
            // 如果文件早于截止日期，删除它
            if (fileInfo.lastModified() < cutoffDate)
            {
                if (QFile::remove(fileInfo.absoluteFilePath()))
                {
                    deletedCount++;
                    emit fileDeleted(fileInfo.absoluteFilePath());
                    qDebug() << "delete old files(number limit):" << fileInfo.fileName()
                             << "modify time:" << fileInfo.lastModified().toString("yyyy-MM-dd hh:mm:ss");
                }
                else
                {
                    emit cleanupError(QString("cannot delte files: %1").arg(fileInfo.absoluteFilePath()));
                }
            }
        }
    }

    // 递归清理子目录
    QFileInfoList dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &dirInfo : dirs)
    {
        deletedCount += cleanupFolder(dirInfo.absoluteFilePath(), keepDays, maxCount);
    }

    // 删除空文件夹
    dirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &dirInfo : dirs)
    {
        QDir subDir(dirInfo.absoluteFilePath());
        if (subDir.isEmpty())
        {
            if (dir.rmdir(dirInfo.fileName()))
            {
                qDebug() << "delete folder:" << dirInfo.absoluteFilePath();
            }
        }
    }

    return deletedCount;
}
