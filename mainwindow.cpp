#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QDebug>
#include <iostream>
#include <QColorDialog>
#include <QScreen>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 设置窗口居中（带边界限制）
    QRect screenRect = QApplication::primaryScreen()->availableGeometry();
    // 计算居中位置
    int x = screenRect.center().x() - width() / 2;
    int y = screenRect.center().y() - height() / 2;
    // 限制边界，确保窗口不超出屏幕
    x = qMax(screenRect.left(), qMin(x, screenRect.right() - width()));
    y = qMax(screenRect.top(), qMin(y, screenRect.bottom() - height()));
    move(x, y);

    // 创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;
    loadROIParametersToUI();

    cam->moveToThread(THREAD1_cam1); // 将Worker对象移到新线程中执行
    // 相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run); // 启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit); // 停止线程，线程那边触发会停止（finished），可以再次用start启动
    // connect(cam, &Camera::finished, cam, &QObject::deleteLater); // 在空闲时间删除线程对象，执行后将不能在用start方法启动线程
    connect(cam, &Camera::send_connectstate, this, &MainWindow::receive_connectstate, Qt::QueuedConnection);
    connect(cam, &Camera::updateButtonState, this, &MainWindow::updateButtonState, Qt::QueuedConnection);
    connect(cam, &Camera::updateStatistics, this, &MainWindow::updateStatistics, Qt::QueuedConnection);
    connect(cam, &Camera::sendQImgToAutoMain, this, &MainWindow::receiveslotQImg, Qt::QueuedConnection);
    connect(cam, &Camera::updateActionState, this, &MainWindow::getActionState, Qt::QueuedConnection);
    connect(cam, &Camera::sendNumber, this, &MainWindow::receiveNumber, Qt::QueuedConnection);
    connect(cam, &Camera::sendQStringtoMain, this, &MainWindow::receiveQStringtoMain, Qt::QueuedConnection);
    connect(cam, &Camera::finishedthread, this, &MainWindow::receivefinish);
    connect(this, &MainWindow::destroyed, cam, &Camera::deleteLater, Qt::QueuedConnection);

    QSettings statSettings(statFilePath, QSettings::IniFormat);
    // 获取今天的日期（只保留年月日）
    QString today = QDate::currentDate().toString("yyyy-MM-dd");
    // 读取上一次保存的日期
    QString last_date = statSettings.value("last_date").toString();
    // 如果日期不同，重置计数
    if (last_date != today)
    {
        last_date = today;
        today_good_number = today_number = today_good_rates = 0;
        // 将变量写入 QSettings 对象
        statSettings.setValue("last_date", last_date);
        statSettings.setValue("today_good_number", today_good_number);
        statSettings.setValue("today_number", today_number);
        statSettings.setValue("today_good_rates", today_good_rates);
    }
    else
    {
        today_good_number = statSettings.value("today_good_number").toInt();
        today_number = statSettings.value("today_number").toInt();
        today_good_rates = statSettings.value("today_good_rates").toDouble();
    }

    history_good_number = statSettings.value("history_good_number").toInt();
    history_number = statSettings.value("history_number").toInt();
    history_good_rates = statSettings.value("history_good_rates").toDouble();

    ui->history_good_number->setText(QString::number(history_good_number));
    ui->history_number->setText(QString::number(history_number));
    ui->history_good_rates->setText(QString::number(history_good_rates, 'f', 2) + " %");
    ui->today_good_number->setText(QString::number(today_good_number));
    ui->today_number->setText(QString::number(today_number));
    ui->today_good_rates->setText(QString::number(today_good_rates, 'f', 2) + " %");

    // 启动相机1
    on_start_clicked();
}

MainWindow::~MainWindow()
{
    cam->closeDevice();
    delete ui;
}

void MainWindow::loadROIParametersToUI()
{
    // 从 INI 文件读取 ROI 配置
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("ROI");
    // 读取 ROI 参数
    bool enableROI = settings.value("EnableROI").toBool();
    int roi_x = settings.value("RoiX").toInt();
    int roi_y = settings.value("RoiY").toInt();
    int roi_w = settings.value("RoiW").toInt();
    int roi_h = settings.value("RoiH").toInt();
    QString roiColorStr = settings.value("RoiColor").toString();
    currentRoiColor = roiColorStr;
    int roi_opacity = settings.value("RoiOpacity").toInt();
    int roi_line_width = settings.value("RoiLineWidth").toInt();
    settings.endGroup();

    // 更新 UI
    ui->cb_enableROI->setChecked(enableROI);
    ui->spinBox_roi_x->setValue(roi_x);
    ui->spinBox_roi_y->setValue(roi_y);
    ui->spinBox_roi_w->setValue(roi_w);
    ui->spinBox_roi_h->setValue(roi_h);
    // 初始化颜色预览样式
    ui->label_color_preview->setStyleSheet(QString("QLabel { color: %1; }").arg(roiColorStr));
    // 初始化透明度滑块
    ui->slider_opacity->setValue(roi_opacity);
    ui->label_opacity_display->setText(QString("%1%").arg(roi_opacity));
    // 初始化线宽滑块
    ui->slider_line_width->setValue(roi_line_width);
    ui->label_line_width_display->setText(QString("%1").arg(roi_line_width));
}

void MainWindow::on_start_clicked()
{
    if (!THREAD1_cam1->isRunning())
    {
        THREAD1_cam1->start();
        ui->start->setDisabled(true);
        ui->stop->setDisabled(false);
    }
}

void MainWindow::on_stop_clicked()
{
    if (THREAD1_cam1->isRunning())
    {
        cam->stop_camera();
        ui->start->setDisabled(false);
        ui->stop->setDisabled(true);
    }
}

void MainWindow::receiveslotQImg(QImage img)
{
    ui->label->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::receivefinish()
{
    ui->start->setDisabled(false);
    ui->stop->setDisabled(true);
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

void MainWindow::updateStatistics(bool result)
{
    QSettings statSettings(statFilePath, QSettings::IniFormat);

    if (result)
    {
        history_good_number++;
        today_good_number++;
    }
    history_number++;
    today_number++;
    history_good_rates = (static_cast<double>(history_good_number) / history_number) * 100;
    today_good_rates = (static_cast<double>(today_good_number) / today_number) * 100;
    // 将变量写入 QSettings 对象
    statSettings.setValue("history_good_number", history_good_number);
    statSettings.setValue("history_number", history_number);
    statSettings.setValue("history_good_rates", history_good_rates);
    statSettings.setValue("today_good_number", today_good_number);
    statSettings.setValue("today_number", today_number);
    statSettings.setValue("today_good_rates", today_good_rates);

    ui->history_good_number->setText(QString::number(history_good_number));
    ui->history_number->setText(QString::number(history_number));
    ui->history_good_rates->setText(QString::number(history_good_rates, 'f', 2) + " %");
    ui->today_good_number->setText(QString::number(today_good_number));
    ui->today_number->setText(QString::number(today_number));
    ui->today_good_rates->setText(QString::number(today_good_rates, 'f', 2) + " %");
}

void MainWindow::on_btn_connectPLC_clicked()
{
    cam->connectPLC();
}

void MainWindow::on_btn_disconnectPLC_clicked()
{
    cam->disconnectPLC();
}

void MainWindow::on_btn_history_rst_clicked()
{
    QSettings statSettings(statFilePath, QSettings::IniFormat);

    history_good_number = history_number = history_good_rates = 0;

    // 将变量写入 QSettings 对象
    statSettings.setValue("history_good_number", history_good_number);
    statSettings.setValue("history_number", history_number);
    statSettings.setValue("history_good_rates", history_good_rates);
    ui->history_good_number->setText(QString::number(history_good_number));
    ui->history_number->setText(QString::number(history_number));
    ui->history_good_rates->setText(QString::number(history_good_rates, 'f', 2) + " %");
}

void MainWindow::on_btn_today_rst_clicked()
{
    QSettings statSettings(statFilePath, QSettings::IniFormat);

    today_good_number = today_number = today_good_rates = 0;

    // 将变量写入 QSettings 对象
    statSettings.setValue("today_good_number", today_good_number);
    statSettings.setValue("today_number", today_number);
    statSettings.setValue("today_good_rates", today_good_rates);
    ui->today_good_number->setText(QString::number(today_good_number));
    ui->today_number->setText(QString::number(today_number));
    ui->today_good_rates->setText(QString::number(today_good_rates, 'f', 2) + " %");
}

void MainWindow::receive_connectstate(bool state)
{
    ui->btn_connectPLC->setEnabled(!state);
    ui->btn_disconnectPLC->setEnabled(state);
    ui->btn_proc4->setStyleSheet(state ? "background-color: green; color: white;" : "background-color: red; color: white;");
    ui->btn_proc4->setText(QString::fromLocal8Bit(state ? "已连接" : "未连接"));
}

void MainWindow::on_btn_setRoi_clicked()
{
    if (ui->stackedWidget->currentIndex() != 1)
        ui->stackedWidget->setCurrentIndex(1);
    else
        ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::getActionState(std::vector<bool> actionState)
{
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
    cam->igonoreAction(4, checked);
}

void MainWindow::receiveQStringtoMain(QString s)
{
    static int colorIndex = 0;
    static QStringList colors = {
        "#00ff00", // 绿色
        "#00ffff", // 青色
        "#ffff00", // 黄色
        "#ff00ff", // 紫色
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

void MainWindow::modifyROIParameter(const QString &parameterName, const QVariant &newValue)
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("ROI");
    settings.setValue(parameterName, newValue);
    settings.endGroup();
}

void MainWindow::on_cb_enableROI_toggled(bool value)
{
    modifyROIParameter("EnableROI", value);
    cam->enableROIDetection(value);
}

void MainWindow::on_spinBox_roi_x_valueChanged(int value)
{
    modifyROIParameter("RoiX", value);
    cam->setRoiX(value);
}

void MainWindow::on_spinBox_roi_y_valueChanged(int value)
{
    modifyROIParameter("RoiY", value);
    cam->setRoiY(value);
}

void MainWindow::on_spinBox_roi_w_valueChanged(int value)
{
    modifyROIParameter("RoiW", value);
    cam->setRoiW(value);
}

void MainWindow::on_spinBox_roi_h_valueChanged(int value)
{
    modifyROIParameter("RoiH", value);
    cam->setRoiH(value);
}

void MainWindow::on_btn_colorPicker_clicked()
{
    QColor color = QColorDialog::getColor(QColor(currentRoiColor), this, QString::fromLocal8Bit("选择检测框颜色"));
    if (color.isValid())
    {
        QString colorName = color.name();
        modifyROIParameter("RoiColor", colorName);
        currentRoiColor = colorName;
        ui->label_color_preview->setStyleSheet(QString("QLabel { color: %1; }").arg(colorName));
        cam->setRoiColor(colorName);
    }
}

void MainWindow::on_slider_opacity_valueChanged(int value)
{
    modifyROIParameter("RoiOpacity", value);
    ui->label_opacity_display->setText(QString("%1%").arg(value));
    cam->setRoiOpacity(value / 100.0f);
}

void MainWindow::on_slider_line_width_valueChanged(int value)
{
    modifyROIParameter("RoiLineWidth", value);
    ui->label_line_width_display->setText(QString("%1").arg(value));
    cam->setRoiLineWidth(value);
}
