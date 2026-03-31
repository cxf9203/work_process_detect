#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <iostream>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    //创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;
    cam->moveToThread(THREAD1_cam1);  // 将Worker对象移到新线程中执行
    //相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run);//启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit);//停止线程，线程那边触发会停止（finished），可以再次用start启动
    //connect(cam, &Camera::finished, cam, &QObject::deleteLater);//在空闲时间删除线程对象，执行后将不能在用start方法启动线程
    ////---------------------------------------------------------------------------------------------------------------------------------------------//
    connect(this, &MainWindow::SetStopThreadC1, cam, &Camera::ExecuteMianToThread, Qt::DirectConnection);//向线程发送信号//线程终止条件设置函数
    //connect(cam,&Camera::sendImgToAutoMain,this,&MainWindow::receiveslotAutoImg,Qt::DirectConnection);
    //connect(cam,&Camera::resetSystem,this,&MainWindow::resetSystem,Qt::DirectConnection);
    connect(cam,&Camera::send_connectstate,this,&MainWindow::receive_connectstate,Qt::QueuedConnection);
    connect(cam,&Camera::updateButtonState,this,&MainWindow::updateButtonState,Qt::DirectConnection);
    //connect(cam,&Camera::triggerAlarm,this,&MainWindow::triggerAlarm,Qt::DirectConnection);
    connect(cam,&Camera::sendQImgToAutoMain,this,&MainWindow::receiveslotQImg,Qt::QueuedConnection);
    
    
    //connect(cam,&Camera::sendQStringtoMain,this,&MainWindow::receiveQStringtoMain,Qt::DirectConnection);
    connect(cam,&Camera::finishedthread,this,&MainWindow::receivefinish);
    connect(this,&MainWindow::destroyed,cam,&Camera::deleteLater,Qt::DirectConnection);
}

MainWindow::~MainWindow()
{
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
void MainWindow::receiveslotQImg(QImage img){
    ui->label->setPixmap(QPixmap::fromImage(img));

}
void MainWindow::receivefinish(){
    qDebug()<<"finished thread";
    cam->closeDevice();//关闭相机线程
}
void MainWindow::updateButtonState(bool p1Detected,bool p2Detected,bool p3Detected){
    ui->btn_proc1->setEnabled(p1Detected);
    ui->btn_proc2->setEnabled(p2Detected);
    ui->btn_proc3->setEnabled(p3Detected);

    if (p1Detected)
        ui->btn_proc1->setStyleSheet("background-color: green;");
    if (p2Detected)
        ui->btn_proc2->setStyleSheet("background-color: green;");
    if (p3Detected)
        ui->btn_proc3->setStyleSheet("background-color: green;");
}
void MainWindow::receive_connectstate(bool state){
    if(state){
        ui->btn_proc4->setStyleSheet("background-color: green; color: white;");
        ui->btn_proc4->setText(QString::fromLocal8Bit ("已连接"));
    }else{
        ui->btn_proc4->setStyleSheet("background-color: red; color: white;");
        ui->btn_proc4->setText(QString::fromLocal8Bit ("未连接"));
    }
}
void MainWindow::on_btn_setRoi_clicked()
{
    //设定识别ROI
    cam->setRoi();
}

void MainWindow::on_pushButton_clicked()
{
    qDebug()<<"output1";
}

void MainWindow::on_pushButton_2_clicked()
{
    qDebug()<<"output2";
}

void MainWindow::on_pushButton_3_clicked()
{
    qDebug()<<"output3";
}
