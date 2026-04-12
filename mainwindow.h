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
    // camera1 receive
    void receiveslotQImg(QImage img);
    void receivefinish();
    void updateButtonState(bool p1Detected, bool p2Detected, bool p3Detected);
    void receiveNumber(QString str_chilun_num,QString str_luosi_num);
    void receive_connectstate(bool state);
    void getActionState(std::vector<bool> actionState);
    void receiveQStringtoMain(QString s);
signals:
    void SetStopThreadC1(); // send signal to stop thread

private slots:
    void on_start_clicked();

    void on_stop_clicked();

    void on_btn_setRoi_clicked();

    void on_pushButton_clicked();

    void on_pushButton_2_clicked();

    void on_pushButton_3_clicked();

    void on_pushButton_4_clicked();

    void on_checkBox_toggled(bool checked);

private:
    Ui::MainWindow *ui;
    QThread *THREAD1_cam1;
    Camera *cam; // camera thread
    bool baojing_flag = true;
};
#endif // MAINWINDOW_H
