#ifndef IMAGECLEANER_H
#define IMAGECLEANER_H

#include <QObject>
#include <QString>
#include <QDir>
#include <QFile>
#include <QDateTime>
#include <QDebug>
#include <QStorageInfo>
#include <QThread>

class ImageCleaner : public QObject
{
    Q_OBJECT
public:
    explicit ImageCleaner(QObject *parent = nullptr);
    // 清理指定文件夹中的旧图像
    // folderPath: 要清理的文件夹路径
    // keepDays: 保留最近几天的图像（0=删除所有）
    // maxCount: 最大保留文件数量（0=无限制）
    bool cleanupOldImages(const QString &folderPath, int keepDays = 30, int maxCount = 0);
    // 清理多个文件夹
    bool cleanupMultipleFolders(const QStringList &folderPaths, int keepDays = 30, int maxCount = 0);
    // 获取文件夹信息
    qint64 getFolderSize(const QString &folderPath);
    int getFileCount(const QString &folderPath);
    // 获取磁盘空间信息
    qint64 getAvailableSpace(const QString &path);
    qint64 getTotalSpace(const QString &path);

signals:
    void cleanupStarted(const QString &folder);
    void fileDeleted(const QString &filepath);
    void cleanupFinished(const QString &folder, int deletedCount, qint64 freedSpace);
    void cleanupError(const QString &error);

private:
    // 支持的图像格式
    QStringList m_imageExtensions = {"*.jpg", "*.jpeg", "*.png", "*.bmp", "*.tiff", "*.tif"};
    // 内部清理函数
    int cleanupFolder(const QString &folderPath, int keepDays, int maxCount);
};

#endif // IMAGECLEANER_H
