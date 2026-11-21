#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QListWidget>
#include <QPushButton>
#include <QMediaPlayer>
#include <QCheckBox>
#include <QGroupBox>
#include <QToolButton>
#include <QLabel>
#include <QComboBox>
#include "camera.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void showEvent(QShowEvent *event);
    void resizeEvent(QResizeEvent *event);

public slots:
    void applyTopMost(bool checked); // 应用窗口置顶状态
    void initTopMostButton(); // 初始化置顶按钮
    void setGroupLocked(QGroupBox *group, QCheckBox *lockBox, bool locked); // 设置分组锁定状态
    void initModelSettingControls(); // 初始化模型设置分组
    void modifyROIParameter(QString parameterName, QVariant newValue); // 修改 ROI 参数
    QString colorButtonStyle(const QColor &color); // 生成色块按钮样式（禁用态显示暗化原色）
    void initROIControls(); // 初始化 ROI 分组
    void modifyActionParameter(); // 修改动作参数
    QVector<int> getEnabledActions(); // 获取启用的动作
    QVector<int> getOrderedActions(); // 获取顺序检测的动作
    void updateOrderStatusLabels(QVector<int> &orderedActions); // 更新顺序状态标签
    void updateActionConfig(); // 更新动作配置
    void initActionControls(); // 初始化动作控件
    void initForceSaveVideoControl(); // 初始化强制保存视频复选框
    void receiveslotQImg(QImage img);
    void receivefinish();
    void updateLabelState(bool p1Detected, bool p2Detected, bool p3Detected);
    void onLogFileChanged();
    void updateStatistics(bool result);
    void loadHistoryList();
    void showEmptyLabel();
    void loadPage(int page);
    void updatePaginationControls();
    QWidget *createHistoryItem(int recordNumber, QString logLine, QString originalPath, QString resultPath);
    void showVideo(int recordNumber, QString type, QString logInfo, QString path);
    void updateVideoPosition(qint64 position);
    void onSliderPressed();
    void onSliderReleased();
    void onSliderMoved(int value);
    QString formatTime(qint64 ms);
    void showHistoryList();
    void receive_connectstate(bool state);
    void getActionState(std::vector<bool> actionState);
    void receiveQStringtoMain(QString s);
    void receiveNumber(QString str_chilun_num, QString str_luosi_num);

private slots:
    void on_btn_start_clicked();
    void on_btn_stop_clicked();
    void on_btn_connectPLC_clicked();
    void on_btn_disconnectPLC_clicked();
    void on_btn_history_rst_clicked();
    void on_btn_today_rst_clicked();
    void on_btn_history_clicked();
    void on_btn_settings_clicked();
    void on_checkBox_forceSaveVideo_toggled(bool value);

private:
    Ui::MainWindow *ui;
    QThread *THREAD1_cam1;
    Camera *cam; // camera thread
    bool initialCentered = false;
    QString iniFilePath = "D:\\jiance\\work_process.ini"; // 工序检测配置文件
    QString statFilePath = "D:\\jiance\\work_process_statistics.ini"; // 统计数据配置
    long long int history_good_number;
    long long int history_number;
    double history_good_rates;
    long long int today_good_number;
    long long int today_number;
    double today_good_rates;
    QString currentRoiColor;
    QVector<QPair<QString, QCheckBox *>> actionItems;
    QToolButton *pinBtn = nullptr;
    // 历史记录窗口
    QWidget *historyWindow = nullptr;
    QWidget *listContainer = nullptr;
    QListWidget *listWidget = nullptr;
    // 分页相关
    const int PAGE_SIZE = 100;
    int currentPage = 0;
    int totalPages = 1;
    QVector<QString> allLogLines;
    QLabel *currentPageRecordLabel = nullptr;
    QPushButton *prevPageBtn = nullptr;
    QLabel *pageInfoLabel = nullptr;
    QPushButton *nextPageBtn = nullptr;
    QLineEdit *jumpLineEdit = nullptr;
    QLabel *recordCountLabel = nullptr;
    QString logDirPath = QCoreApplication::applicationDirPath() + "/saved_videos/";
    QWidget *videoContainer = nullptr;
    QLabel *videoTitleLabel = nullptr;
    QMediaPlayer *player = nullptr;
    QLabel *timeLabel = nullptr;
    QSlider *videoSlider = nullptr;
    QComboBox *speedCombo = nullptr;
    bool isSliderPressed = false;
};
#endif // MAINWINDOW_H
