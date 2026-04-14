#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <iostream>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;
    cam->moveToThread(THREAD1_cam1); // 将Worker对象移到新线程中执行
    // 相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run);   // 启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit); // 停止线程，线程那边触发会停止（finished），可以再次用start启动
    // connect(cam, &Camera::finished, cam, &QObject::deleteLater);//在空闲时间删除线程对象，执行后将不能在用start方法启动线程
    ////---------------------------------------------------------------------------------------------------------------------------------------------//
    connect(this, &MainWindow::SetStopThreadC1, cam, &Camera::ExecuteMianToThread, Qt::QueuedConnection); // 向线程发送信号//线程终止条件设置函数
    // connect(cam,&Camera::sendImgToAutoMain,this,&MainWindow::receiveslotAutoImg,Qt::DirectConnection);
    // connect(cam,&Camera::resetSystem,this,&MainWindow::resetSystem,Qt::DirectConnection);
    connect(cam, &Camera::send_connectstate, this, &MainWindow::receive_connectstate, Qt::QueuedConnection);
    connect(cam, &Camera::updateButtonState, this, &MainWindow::updateButtonState, Qt::QueuedConnection);
    // connect(cam,&Camera::triggerAlarm,this,&MainWindow::triggerAlarm,Qt::DirectConnection);
    connect(cam, &Camera::sendQImgToAutoMain, this, &MainWindow::receiveslotQImg, Qt::QueuedConnection);
    connect(cam, &Camera::updateActionState, this, &MainWindow::getActionState, Qt::QueuedConnection);
    connect(cam, &Camera::sendNumber, this, &MainWindow::receiveNumber, Qt::QueuedConnection);
    connect(cam, &Camera::sendQStringtoMain, this, &MainWindow::receiveQStringtoMain, Qt::QueuedConnection);
    connect(cam, &Camera::finishedthread, this, &MainWindow::receivefinish);
    connect(this, &MainWindow::destroyed, cam, &Camera::deleteLater, Qt::QueuedConnection);
    // 启动相机1
    THREAD1_cam1->start();
}

MainWindow::~MainWindow()
{
    cam->closeDevice();
    delete ui;
}

void MainWindow::on_start_clicked()
{
    THREAD1_cam1->start();
}

void MainWindow::on_stop_clicked()
{
    cam->stop_camera();
}

void MainWindow::receiveslotQImg(QImage img)
{
    ui->label->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::receivefinish()
{
    qDebug() << "finished thread";
    cam->closeDevice(); // 关闭相机线程
}

void MainWindow::updateButtonState(bool p1Detected, bool p2Detected, bool p3Detected)
{
    ui->btn_proc1->setEnabled(p1Detected);
    ui->btn_proc2->setEnabled(p2Detected);
    ui->btn_proc3->setEnabled(p3Detected);
    // 如果ok显示绿色，ng显示红色
    if (p1Detected)
    {
        ui->btn_proc1->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->btn_proc1->setStyleSheet("background-color: red;");
    }
    if (p2Detected)
    {
        ui->btn_proc2->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->btn_proc2->setStyleSheet("background-color: red;");
    }
    if (p3Detected)
    {
        ui->btn_proc3->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->btn_proc3->setStyleSheet("background-color: red;");
    }
}

void MainWindow::receive_connectstate(bool state)
{
    if (state)
    {
        ui->btn_proc4->setStyleSheet("background-color: green; color: white;");
        ui->btn_proc4->setText(QString::fromLocal8Bit("已连接"));
    }
    else
    {
        ui->btn_proc4->setStyleSheet("background-color: red; color: white;");
        ui->btn_proc4->setText(QString::fromLocal8Bit("未连接"));
    }
}

void MainWindow::on_btn_setRoi_clicked()
{
    // 设定识别ROI
    cam->setRoi();
}

void MainWindow::on_pushButton_clicked()
{
    if (baojing_flag)
    {
        qDebug() << "output1";
        cam->setD(0, 1);
        baojing_flag = !baojing_flag;
    }
    else
    {
        qDebug() << "output1";
        cam->setD(0, 0);
        baojing_flag = !baojing_flag;
    }
}

void MainWindow::on_pushButton_2_clicked()
{
    qDebug() << "output2";
    cam->setD(2, 1);
}

void MainWindow::on_pushButton_3_clicked()
{
    qDebug() << "output3";
    cam->setD(4, 1);
}

void MainWindow::on_pushButton_4_clicked()
{
    qDebug() << "ai test";
    cam->aiTest();
}

void MainWindow::getActionState(std::vector<bool> actionState)
{
    // qDebug() << "getActionState" << "luosi_left_bottom", "luosi_left_top", "luosi_right_bottom", "luosi_right_top", "place_chilun";
    if (actionState[0])
    {
        ui->label_4->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->label_4->setStyleSheet("background-color: red;");
    }
    if (actionState[1])
    {
        ui->label_5->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->label_5->setStyleSheet("background-color: red;");
    }
    if (actionState[2])
    {
        ui->label_6->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->label_6->setStyleSheet("background-color: red;");
    }
    if (actionState[3])
    {
        ui->label_7->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->label_7->setStyleSheet("background-color: red;");
    }
    if (actionState[4])
    {
        ui->label_8->setStyleSheet("background-color: green;");
    }
    else
    {
        ui->label_8->setStyleSheet("background-color: red;");
    }
}

void MainWindow::on_checkBox_toggled(bool checked)
{
    qDebug() << "current state is" << checked;
    if (checked)
    {
        cam->igonoreAction(4);
    }
}

void MainWindow::receiveQStringtoMain(QString s)
{
    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timeStamp = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");

    // 构建带有时间戳的日志消息
    QString logMessage = "[" + timeStamp + "] " + s;

    // 将日志消息添加到 QTextBrowser 中
    ui->textBrowser->append(logMessage);

    // 滚动到文本末尾，确保最新的日志消息可见
    ui->textBrowser->moveCursor(QTextCursor::End);
}

void MainWindow::receiveNumber(QString str_chilun_num, QString str_luosi_num)
{
    ui->lb_luosi_num->setText(str_luosi_num);
    ui->lb_chilun_num->setText(str_chilun_num);
}
