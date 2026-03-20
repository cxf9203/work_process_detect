#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
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
    void receive_connectstate(bool state);
signals:
    void SetStopThreadC1(); // send signal to stop thread

private slots:
    void on_start_clicked();

    void on_stop_clicked();

private:
    Ui::MainWindow *ui;
    QThread *THREAD1_cam1;
    Camera *cam; // camera thread
};
#endif // MAINWINDOW_H
