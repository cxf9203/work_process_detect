#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDateTime>
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
    void loadROIParametersToUI();
    // camera1 receive
    void receiveslotQImg(QImage img);
    void receivefinish();
    void updateButtonState(bool p1Detected, bool p2Detected, bool p3Detected);
    void updateStatistics(bool result);
    void receiveNumber(QString str_chilun_num, QString str_luosi_num);
    void modifyROIParameter(const QString &parameterName, const QVariant &newValue);
    void receive_connectstate(bool state);
    void getActionState(std::vector<bool> actionState);
    void receiveQStringtoMain(QString s);

private slots:
    void on_start_clicked();
    void on_stop_clicked();
    void on_btn_history_rst_clicked();
    void on_btn_today_rst_clicked();
    void on_btn_setRoi_clicked();
    void on_checkBox_toggled(bool checked);
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
};
#endif // MAINWINDOW_H
