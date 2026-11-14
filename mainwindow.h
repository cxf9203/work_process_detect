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
    void ensureStatCsvExists(); // 确保统计 CSV 文件存在
    void fillCsvTable(); // 将 CSV 数据填充到详情表格
    void saveTodayRowToCsv(); // 将今日统计行追加写入 CSV
    void loadStatsFromCsv(); // 从 CSV 加载历史/今日统计到 UI
    void initProbabilityThresholdControl(); // 初始化置信度阈值滑块
    void applyNumberSettings(); // 应用齿轮/螺丝标准数设置
    void initNumberSettings(); // 初始化齿轮/螺丝标准数设置
    void modifyROIParameter(QString parameterName, QVariant newValue); // 修改 ROI 参数
    void loadROIParametersToUI(); // 读取 ROI 参数到 UI 控件
    void modifyActionParameter(); // 修改动作参数
    QVector<int> getEnabledActions(); // 获取启用的动作
    QVector<int> getOrderedActions(); // 获取顺序检测的动作
    void updateOrderStatusLabels(QVector<int> &orderedActions); // 更新顺序状态标签
    void updateActionConfig(); // 更新动作配置
    void loadActionConfig(); // 加载动作配置
    void refreshActionControlEnabled(); // 刷新动作控件启用状态
    void initActionControls(); // 初始化动作控件
    void setGroupLocked(QGroupBox *group, QCheckBox *lockBox, bool locked); // 设置分组锁定状态
    void initGroupLocks(); // 初始化分组锁定状态
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
    void on_btn_csv_detail_clicked();
    void on_btn_history_clicked();
    void on_btn_settings_clicked();
    void on_slider_prob_threshold_valueChanged(int value);
    void on_spinBox_chilunNum_valueChanged(int value);
    void on_spinBox_luosiNum_valueChanged(int value);
    void on_cb_chilunLock_toggled(bool checked);
    void on_cb_luosiLock_toggled(bool checked);
    void on_cb_enableROI_toggled(bool value);
    void on_spinBox_roi_x_valueChanged(int value);
    void on_spinBox_roi_y_valueChanged(int value);
    void on_spinBox_roi_w_valueChanged(int value);
    void on_spinBox_roi_h_valueChanged(int value);
    void on_btn_colorPicker_clicked();
    void on_slider_opacity_valueChanged(int value);
    void on_slider_line_width_valueChanged(int value);
    void on_cb_lockModelSetting_toggled(bool checked);
    void on_cb_lockNumberSettings_toggled(bool checked);
    void on_cb_lockROI_toggled(bool checked);
    void on_cb_lockActionControl_toggled(bool checked);
    void on_checkBox_forceSaveVideo_toggled(bool value);

private:
    Ui::MainWindow *ui;
    QThread *THREAD1_cam1;
    Camera *cam; // camera thread
    bool initialCentered = false;
    QString iniFilePath = "D:\\jiance\\work_process.ini"; // 工序检测配置文件
    QString statFilePath = "D:\\jiance\\work_process_statistics.csv"; // 统计数据配置
    QString lastStatDate = QDate::currentDate().toString("yyyy-MM-dd");
    long long int history_good_number;
    long long int history_number;
    double history_good_rates;
    long long int today_good_number;
    long long int today_number;
    double today_good_rates;
    QString currentRoiColor;
    QVector<QPair<QString, QCheckBox *>> actionItems;
    QToolButton *pinBtn = nullptr;
    QDialog *csvDialog = nullptr;
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
