#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <iostream>
#include <QColorDialog>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;
    // 初始化ROI参数
    ui->cb_enableROI->setChecked(cam->m_enableROIDetection);
    ui->spinBox_roi_x->setValue(cam->roi_x);
    ui->spinBox_roi_y->setValue(cam->roi_y);
    ui->spinBox_roi_w->setValue(cam->roi_w);
    ui->spinBox_roi_h->setValue(cam->roi_h);
    // 初始化颜色预览样式
    ui->label_color_preview->setStyleSheet(QString("QLabel { color: rgb(%1, %2, %3); }").arg(cam->roi_color_r).arg(cam->roi_color_g).arg(cam->roi_color_b));
    // 初始化透明度滑块
    ui->slider_opacity->setValue(static_cast<int>(cam->roi_opacity * 100));
    ui->label_opacity_display->setText(QString("%1%").arg(static_cast<int>(cam->roi_opacity * 100)));
    // 初始化线宽滑块
    ui->slider_line_width->setValue(cam->roi_line_width);
    ui->label_line_width_display->setText(QString("%1").arg(cam->roi_line_width));

    cam->moveToThread(THREAD1_cam1); // 将Worker对象移到新线程中执行
    // 相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run);   // 启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit); // 停止线程，线程那边触发会停止（finished），可以再次用start启动
    // connect(cam, &Camera::finished, cam, &QObject::deleteLater);//在空闲时间删除线程对象，执行后将不能在用start方法启动线程
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
    QPushButton *buttons[] = {ui->btn_proc1, ui->btn_proc2, ui->btn_proc3};
    bool states[] = {p1Detected, p2Detected, p3Detected};

    for (int i = 0; i < 3; ++i)
    {
        buttons[i]->setEnabled(states[i]);
        // 如果ok显示绿色，ng显示红色
        buttons[i]->setStyleSheet(states[i] ? "background-color: green;" : "background-color: red;");
    }
}

void MainWindow::receive_connectstate(bool state)
{
    ui->btn_proc4->setStyleSheet(state ? "background-color: green; color: white;" : "background-color: red; color: white;");
    ui->btn_proc4->setText(QString::fromLocal8Bit(state ? "已连接" : "未连接"));
}

void MainWindow::on_btn_setRoi_clicked()
{
    // 设定识别ROI
    // cam->setRoi();

    if (ui->stackedWidget->currentIndex() == 0)
        ui->stackedWidget->setCurrentIndex(1);
    else
        ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_clicked()
{
    qDebug() << "output1";
    cam->setD(0, baojing_flag ? 1 : 0);
    baojing_flag = !baojing_flag;
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

    QLabel *labels[] = {ui->label_4, ui->label_5, ui->label_6, ui->label_7, ui->label_8};

    // 循环更新颜色
    for (int i = 0; i < 5; i++)
    {
        labels[i]->setStyleSheet(actionState[i] ? "background-color: green;" : "background-color: red;");
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
    static int colorIndex = 0;
    static QStringList colors = {
        "#00ff00", // 绿色
        "#00ffff", // 青色
        "#ffff00", // 黄色
        "#ff44ff", // 紫色
        "#ffa500", // 橙色
        "#ffffff", // 白色
    };

    // 获取当前时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timeStamp = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");

    // // 构建带有时间戳的日志消息
    // QString logMessage = "[" + timeStamp + "] " + s;
    // 构建带有时间戳的日志消息，并根据当前颜色索引设置颜色
    QString logMessage = QString("<font color=\"%1\">[%2] %3</font>").arg(colors[colorIndex++ % colors.size()]).arg(timeStamp).arg(s);

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

void MainWindow::on_cb_enableROI_toggled(bool checked)
{
    qDebug() << "ROI detection state is" << checked;
    cam->enableROIDetection(checked);
}

void MainWindow::on_spinBox_roi_x_valueChanged(int value)
{
    cam->setRoiX(value);
}

void MainWindow::on_spinBox_roi_y_valueChanged(int value)
{
    cam->setRoiY(value);
}

void MainWindow::on_spinBox_roi_w_valueChanged(int value)
{
    cam->setRoiW(value);
}

void MainWindow::on_spinBox_roi_h_valueChanged(int value)
{
    cam->setRoiH(value);
}

void MainWindow::on_btn_colorPicker_clicked()
{
    QColor color = QColorDialog::getColor(Qt::green, this, QString::fromLocal8Bit("选择检测框颜色"));
    if (color.isValid())
    {
        ui->label_color_preview->setStyleSheet(QString("QLabel { color: %1; }").arg(color.name()));
        cam->setRoiColor(color.red(), color.green(), color.blue());
    }
}

void MainWindow::on_slider_opacity_valueChanged(int value)
{
    ui->label_opacity_display->setText(QString("%1%").arg(value));
    cam->setRoiOpacity(value / 100.0f);
}

void MainWindow::on_slider_line_width_valueChanged(int value)
{
    ui->label_line_width_display->setText(QString("%1").arg(value));
    cam->setRoiLineWidth(value);
}
