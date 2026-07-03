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
    qDebug() << "Cleanup started - folderPath:" << folderPath << "keepDays:" << keepDays << "maxCount:" << maxCount;

    if (maxCount <= 0 && keepDays <= 0)
    {
        qDebug() << "Cleanup skipped: no valid limits (keepDays and maxCount are both <= 0)";
        return 0;
    }

    // 递归收集所有图像文件
    QList<QPair<QString, QDateTime>> allFiles;
    std::function<void(const QString &)> collectFiles = [&](const QString &path)
    {
        QDir dir(path);

        // 添加当前目录的图像文件
        for (const QFileInfo &file : dir.entryInfoList(m_imageExtensions, QDir::Files))
        {
            allFiles.append(qMakePair(file.absoluteFilePath(), file.lastModified()));
        }

        // 递归子目录
        for (const QFileInfo &subDir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            collectFiles(subDir.absoluteFilePath());
        }
    };

    collectFiles(folderPath);

    if (allFiles.isEmpty())
    {
        qDebug() << "Cleanup skipped: no image files found in" << folderPath;
        return 0;
    }

    qDebug() << "Collected" << allFiles.size() << "image files from" << folderPath;

    // 按修改时间排序（最新的在前）
    std::sort(allFiles.begin(), allFiles.end(), [](const auto &a, const auto &b) { return a.second > b.second; });

    int deletedCount = 0;

    // 标记文件为待删除
    QSet<QString> filesToDelete;

    // 处理数量限制
    if (maxCount > 0)
    {
        qDebug() << "Count limit: keeping up to" << maxCount << "newest files";

        for (int i = maxCount; i < allFiles.size(); ++i)
        {
            filesToDelete.insert(allFiles[i].first);
            qDebug() << "Marked for deletion (count limit):" << QFileInfo(allFiles[i].first).fileName() << "(#" << i + 1 << "/" << allFiles.size() << ")";
        }
    }

    // 处理时间限制
    if (keepDays > 0)
    {
        QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-keepDays);
        qDebug() << "Time limit: keeping files modified after" << cutoffDate.toString("yyyy-MM-dd hh:mm:ss");

        for (int i = 0; i < allFiles.size(); ++i)
        {
            if (allFiles[i].second < cutoffDate)
            {
                filesToDelete.insert(allFiles[i].first);
                qDebug() << "Marked for deletion (time limit):" << QFileInfo(allFiles[i].first).fileName() << "modified:" << allFiles[i].second.toString("yyyy-MM-dd hh:mm:ss");
            }
        }
    }

    qDebug() << "Total files marked for deletion:" << filesToDelete.size();

    // 执行删除
    for (const QString &filePath : filesToDelete)
    {
        if (QFile::remove(filePath))
        {
            deletedCount++;
            emit fileDeleted(filePath);
        }
        else
        {
            emit cleanupError(QString("Failed to delete file: %1").arg(filePath));
        }
    }

    // 删除空文件夹（递归删除）
    std::function<void(const QString &)> removeEmptyDirs = [&](const QString &path)
    {
        QDir dir(path);
        // 递归处理子目录
        for (const QFileInfo &subDir : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot))
        {
            removeEmptyDirs(subDir.absoluteFilePath());
        }
        // 删除空目录
        if (path != folderPath && dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty())
        {
            if (dir.rmdir(path))
            {
                qDebug() << "Removed empty folder:" << path;
            }
        }
    };

    removeEmptyDirs(folderPath);

    qDebug() << "Cleanup completed for" << folderPath << ": deleted" << deletedCount << "files,"
             << "original count:" << allFiles.size()
             << ", remaining:" << (allFiles.size() - deletedCount) << endl;

    return deletedCount;
}
