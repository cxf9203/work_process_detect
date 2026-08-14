#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
#include <QListWidget>
#include <QPushButton>
#include <QMediaPlayer>
#include <QCheckBox>
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

public slots:
    void modifyActionParameter(); // 修改动作参数
    QVector<int> getEnabledActions(); // 获取启用的动作
    QVector<int> getOrderedActions(); // 获取顺序检测的动作
    void updateOrderStatusLabels(QVector<int> &orderedActions); // 更新顺序状态标签
    void updateActionConfig(); // 更新动作配置
    void loadActionConfig(); // 加载动作配置
    void initActionControls(); // 初始化动作控件
    void loadROIParametersToUI();
    void receiveslotQImg(QImage img);
    void receivefinish();
    void updateLabelState(bool p1Detected, bool p2Detected, bool p3Detected);
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
    void modifyROIParameter(QString parameterName, QVariant newValue);

private slots:
    void on_start_clicked();
    void on_stop_clicked();
    void on_btn_connectPLC_clicked();
    void on_btn_disconnectPLC_clicked();
    void on_btn_history_rst_clicked();
    void on_btn_today_rst_clicked();
    void on_btn_history_clicked();
    void on_btn_settings_clicked();
    void on_cb_enableROI_toggled(bool value);
    void on_spinBox_roi_x_valueChanged(int value);
    void on_spinBox_roi_y_valueChanged(int value);
    void on_spinBox_roi_w_valueChanged(int value);
    void on_spinBox_roi_h_valueChanged(int value);
    void on_btn_colorPicker_clicked();
    void on_slider_opacity_valueChanged(int value);
    void on_slider_line_width_valueChanged(int value);

private:
    Ui::MainWindow *ui;
    QThread *THREAD1_cam1;
    Camera *cam; // camera thread
    QString iniFilePath = "D:\\jiance\\work_process.ini"; // 工序检测配置文件
    QString statFilePath = "D:\\jiance\\work_process_statistics.ini"; // 统计数据配置
    QString currentRoiColor;
    long long int history_good_number;
    long long int history_number;
    double history_good_rates;
    long long int today_good_number;
    long long int today_number;
    double today_good_rates;
    QVector<QPair<QString, QCheckBox *>> actionItems;
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
