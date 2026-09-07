#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <iostream>
#include <QColorDialog>
#include <QScreen>
#include <QLineEdit>
#include <QVideoWidget>
#include <QFile>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;

    initActionControls();
    loadROIParametersToUI();
    initSaveAllVideosControl();

    cam->moveToThread(THREAD1_cam1); // 将Worker对象移到新线程中执行
    // 相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run); // 启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit); // 停止线程，线程那边触发会停止（finished），可以再次用start启动
    // connect(cam, &Camera::finished, cam, &QObject::deleteLater); // 在空闲时间删除线程对象，执行后将不能在用start方法启动线程
    connect(cam, &Camera::send_connectstate, this, &MainWindow::receive_connectstate, Qt::QueuedConnection);
    connect(cam, &Camera::updateLabelState, this, &MainWindow::updateLabelState, Qt::QueuedConnection);
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

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 设置窗口居中（带边界限制）
    QRect screenRect = QApplication::primaryScreen()->availableGeometry();
    QRect frameRect = frameGeometry();
    // 计算居中位置
    int x = screenRect.center().x() - frameRect.width() / 2;
    int y = screenRect.center().y() - frameRect.height() / 2;
    // 限制边界，确保窗口不超出屏幕
    x = qMax(screenRect.left(), qMin(x, screenRect.right() - frameRect.width()));
    y = qMax(screenRect.top(), qMin(y, screenRect.bottom() - frameRect.height()));
    move(x, y);
}

void MainWindow::modifyActionParameter()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("Action");
    settings.setValue("EnableAction", ui->checkBox_enableAction->isChecked());
    settings.setValue("ActionAffectsResult", ui->checkBox_actionAffectsResult->isChecked());
    settings.setValue("EnableOrder", ui->checkBox_enableOrder->isChecked());
    // 保存每个动作的勾选状态
    for (int i = 0; i < actionItems.size(); i++)
    {
        settings.setValue(QString("Action%1").arg(i + 1), actionItems[i].second->isChecked());
    }
    // 保存顺序列表
    QStringList orderList;
    for (int i = 0; i < ui->listWidget_order->count(); i++)
    {
        orderList.append(ui->listWidget_order->item(i)->text());
    }
    settings.setValue("OrderList", orderList);
    settings.endGroup();
}

QVector<int> MainWindow::getEnabledActions()
{
    QVector<int> enabledActions;
    if (!ui->checkBox_enableAction->isChecked())
        return enabledActions;
    for (int i = 0; i < actionItems.size(); i++)
    {
        if (actionItems[i].second->isChecked())
        {
            enabledActions.append(i);
        }
    }
    return enabledActions;
}

QVector<int> MainWindow::getOrderedActions()
{
    QVector<int> orderedActions;
    if (!ui->checkBox_enableAction->isChecked() || !ui->checkBox_enableOrder->isChecked())
        return orderedActions;
    for (int i = 0; i < ui->listWidget_order->count(); i++)
    {
        for (int j = 0; j < actionItems.size(); j++)
        {
            if (actionItems[j].first == ui->listWidget_order->item(i)->text())
            {
                orderedActions.append(j);
                break;
            }
        }
    }
    return orderedActions;
}

void MainWindow::updateOrderStatusLabels(QVector<int> &orderedActions)
{
    QVector<QLabel *> statusLabels = {
        ui->label_OrderStatus_1,
        ui->label_OrderStatus_2,
        ui->label_OrderStatus_3,
        ui->label_OrderStatus_4,
        ui->label_OrderStatus_5};
    for (int i = 0; i < statusLabels.size(); i++)
    {
        int orderIndex = orderedActions.indexOf(i);
        bool isInOrder = orderIndex >= 0;
        statusLabels[i]->setText(isInOrder ? QString::number(orderIndex + 1) : "x");
        statusLabels[i]->setStyleSheet(QString("QLabel {"
                                               "    background-color: #1e1e1e;"
                                               "    color: %1;"
                                               "    border: 1px solid %2;"
                                               "    border-radius: 4px;"
                                               "    padding: 8px;"
                                               "}")
                                           .arg(isInOrder ? "#00ff00" : "#888888")
                                           .arg(isInOrder ? "#2e7d32" : "#333"));
    }
}

void MainWindow::updateActionConfig()
{
    modifyActionParameter();
    QVector<int> orderedActions = getOrderedActions();
    updateOrderStatusLabels(orderedActions);
    cam->setActionConfig(ui->checkBox_enableAction->isChecked(), getEnabledActions(), ui->checkBox_actionAffectsResult->isChecked(), orderedActions);
}

void MainWindow::loadActionConfig()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("Action");
    bool enableAction = settings.value("EnableAction").toBool();
    bool enableOrder = settings.value("EnableOrder").toBool();
    // 更新控件状态
    ui->checkBox_enableAction->setChecked(enableAction);
    // 加载每个动作的勾选状态
    for (int i = 0; i < actionItems.size(); i++)
    {
        actionItems[i].second->setChecked(settings.value(QString("Action%1").arg(i + 1)).toBool());
        actionItems[i].second->setEnabled(enableAction);
    }
    ui->checkBox_actionAffectsResult->setChecked(settings.value("ActionAffectsResult").toBool());
    ui->checkBox_actionAffectsResult->setEnabled(enableAction);
    ui->checkBox_enableOrder->setChecked(enableOrder);
    ui->checkBox_enableOrder->setEnabled(enableAction);
    bool orderEnabled = enableAction && enableOrder;
    ui->listWidget_order->setEnabled(orderEnabled);
    ui->btn_addToOrder->setEnabled(orderEnabled);
    ui->btn_removeFromOrder->setEnabled(orderEnabled);
    ui->btn_clearOrder->setEnabled(orderEnabled);
    // 加载顺序列表
    ui->listWidget_order->clear();
    ui->listWidget_order->addItems(settings.value("OrderList").toStringList());
    settings.endGroup();

    updateActionConfig();
}

void MainWindow::initActionControls()
{
    // 动作配置
    actionItems = {
        {QString::fromLocal8Bit("1.左上螺丝"), ui->checkBox_action1},
        {QString::fromLocal8Bit("2.右上螺丝"), ui->checkBox_action2},
        {QString::fromLocal8Bit("3.左下螺丝"), ui->checkBox_action3},
        {QString::fromLocal8Bit("4.右下螺丝"), ui->checkBox_action4},
        {QString::fromLocal8Bit("5.放齿轮"), ui->checkBox_action5}};

    // 加载动作配置
    loadActionConfig();

    // 启用动作检测
    connect(ui->checkBox_enableAction, &QCheckBox::toggled, [this](bool checked)
    {
        // 启用/禁用动作复选框
        for (const auto& pair : actionItems) pair.second->setEnabled(checked);
        ui->checkBox_actionAffectsResult->setEnabled(checked);
        ui->checkBox_enableOrder->setEnabled(checked);
        bool orderChecked = checked && ui->checkBox_enableOrder->isChecked();
        ui->listWidget_order->setEnabled(orderChecked);
        ui->btn_addToOrder->setEnabled(orderChecked);
        ui->btn_removeFromOrder->setEnabled(orderChecked);
        ui->btn_clearOrder->setEnabled(orderChecked);
        updateActionConfig();
    });

    // 取消勾选时自动从顺序列表移除
    for (const auto &pair : actionItems)
    {
        connect(pair.second, &QCheckBox::toggled, [this, pair](bool checked)
        {
            if (!checked) {
                // 取消勾选时，从顺序列表中移除
                for (int j = 0; j < ui->listWidget_order->count(); j++) {
                    if (ui->listWidget_order->item(j)->text() == pair.first) {
                        delete ui->listWidget_order->takeItem(j);
                        break;
                    }
                }
            }
            updateActionConfig();
        });
    }

    // 动作检测影响结果
    connect(ui->checkBox_actionAffectsResult, &QCheckBox::toggled, [this](bool checked) { updateActionConfig(); });

    // 启用顺序检测
    connect(ui->checkBox_enableOrder, &QCheckBox::toggled, [this](bool checked)
    {
        ui->listWidget_order->setEnabled(checked);
        ui->btn_addToOrder->setEnabled(checked);
        ui->btn_removeFromOrder->setEnabled(checked);
        ui->btn_clearOrder->setEnabled(checked);
        updateActionConfig();
    });

    // 添加选中到顺序列表
    connect(ui->btn_addToOrder, &QPushButton::clicked, [this]()
    {
        // 收集所有勾选的动作
        QStringList selectedActions;
        for (const auto& pair : actionItems) {
            if (pair.second->isChecked()) selectedActions.append(pair.first);
        }
        if (selectedActions.isEmpty()) {
            QMessageBox::information(this, QString::fromLocal8Bit("提示"), QString::fromLocal8Bit("请先勾选需要检测的动作"));
            return;
        }
        // 添加到顺序列表（去重）
        for (const QString& action : selectedActions) {
            if (ui->listWidget_order->findItems(action, Qt::MatchExactly).isEmpty()) {
                ui->listWidget_order->addItem(action);
            }
        }
        updateActionConfig();
    });

    // 移除选中的动作
    connect(ui->btn_removeFromOrder, &QPushButton::clicked, [this]()
    {
        int row = ui->listWidget_order->currentRow();
        if (row >= 0) {
            delete ui->listWidget_order->takeItem(row);
            updateActionConfig();
        }
    });

    // 清空顺序列表
    connect(ui->btn_clearOrder, &QPushButton::clicked, [this]()
    {
        ui->listWidget_order->clear();
        updateActionConfig();
    });

    // 拖拽排序时更新
    connect(ui->listWidget_order->model(), &QAbstractItemModel::rowsMoved, [this]() { updateActionConfig(); });
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

    // 更新 Camera 参数
    cam->enableROIDetection(enableROI);
    cam->setRoiX(roi_x);
    cam->setRoiY(roi_y);
    cam->setRoiW(roi_w);
    cam->setRoiH(roi_h);
    cam->setRoiColor(roiColorStr);
    cam->setRoiOpacity(roi_opacity / 100.0f);
    cam->setRoiLineWidth(roi_line_width);
}

void MainWindow::initSaveAllVideosControl()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    bool saveAllVideos = settings.value("saveAllVideos").toBool();
    ui->checkBox_saveAllVideos->setChecked(saveAllVideos);
    cam->setSaveAllVideos(saveAllVideos);
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

void MainWindow::updateLabelState(bool p1Detected, bool p2Detected, bool p3Detected)
{
    QLabel *labels[] = {ui->lb_proc1, ui->lb_proc2, ui->lb_proc3};
    bool states[] = {p1Detected, p2Detected, p3Detected};

    for (int i = 0; i < 3; ++i)
    {
        // 如果ok显示绿色，ng显示红色
        labels[i]->setStyleSheet(QString("QLabel {"
                                         "    background-color: %1;"
                                         "    color: #ffffff;"
                                         "    border: 1px solid #555;"
                                         "    border-radius: 6px;"
                                         "}")
                                     .arg(states[i] ? "#2e7d32" : "#c62828"));
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

void MainWindow::on_btn_history_clicked()
{
    // 如果窗口已经存在，则不再创建新的窗口
    if (historyWindow)
    {
        return;
    }

    // 创建独立窗口
    historyWindow = new QWidget(this);
    historyWindow->setObjectName("historyWindow");
    historyWindow->setMinimumSize(680, height());
    historyWindow->setStyleSheet("QWidget { background-color: #1e1e1e; }"
                                 "QWidget#historyWindow { border: 2px solid #444; border-radius: 8px; }");
    historyWindow->setAttribute(Qt::WA_DeleteOnClose);

    // 设置窗口位置
    historyWindow->move(qMax(0, width() - historyWindow->width()), 0);

    // 窗口关闭时清空指针和释放资源
    connect(historyWindow, &QWidget::destroyed, [this]()
    {
        if (player)
        {
            player->disconnect();
            player->stop();
            player->deleteLater();
            player = nullptr;
        }
        historyWindow = nullptr;
        listContainer = nullptr;
        listWidget = nullptr;
        currentPageRecordLabel = nullptr;
        prevPageBtn = nullptr;
        pageInfoLabel = nullptr;
        nextPageBtn = nullptr;
        jumpLineEdit = nullptr;
        recordCountLabel = nullptr;
        videoContainer = nullptr;
        videoTitleLabel = nullptr;
        timeLabel = nullptr;
        videoSlider = nullptr;
        speedCombo = nullptr;
    });

    QVBoxLayout *mainLayout = new QVBoxLayout(historyWindow);

    // 标题
    QLabel *titleLabel = new QLabel(QString::fromLocal8Bit("历史记录"), historyWindow);
    titleLabel->setStyleSheet("color: #ffffff; font-size: 18px; font-weight: bold; padding: 10px;");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 列表容器（默认显示）
    listContainer = new QWidget(historyWindow);
    QVBoxLayout *listContainerLayout = new QVBoxLayout(listContainer);
    listContainerLayout->setContentsMargins(0, 0, 0, 0);
    listContainerLayout->setSpacing(0);

    // 列表
    listWidget = new QListWidget(listContainer);
    listWidget->setStyleSheet("QListWidget { background-color: #1e1e1e; border: 1px solid #444; border-radius: 5px; }"
                              "QListWidget::item { padding: 5px; border-bottom: 1px solid #333; }"
                              "QListWidget::item:selected { background-color: #2d2d2d; }");
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    listContainerLayout->addWidget(listWidget, 1);

    // 分页控制栏
    QHBoxLayout *paginationLayout = new QHBoxLayout();
    paginationLayout->setContentsMargins(5, 5, 5, 5);

    auto createBtn = [&](const QString &text, std::function<void()> onClick)
    {
        QPushButton *btn = new QPushButton(text, listContainer);
        btn->setStyleSheet("QPushButton { background-color: #424242; color: white; border-radius: 4px; padding: 4px 16px; }"
                           "QPushButton:hover { background-color: #616161; }"
                           "QPushButton:pressed { background-color: #1a1a1a; }"
                           "QPushButton:disabled { background-color: #2d2d2d; color: #666; }");
        connect(btn, &QPushButton::clicked, onClick);
        return btn;
    };

    // 当前页记录数
    currentPageRecordLabel = new QLabel(QString::fromLocal8Bit("本页 0 条 (每页最多 %1 条)").arg(PAGE_SIZE), listContainer);
    currentPageRecordLabel->setStyleSheet("color: #00ff00; font-size: 12px; min-width: 200px;");
    currentPageRecordLabel->setAlignment(Qt::AlignCenter);

    // 上一页按钮
    prevPageBtn = createBtn(QString::fromLocal8Bit("上一页"), [this]() { if (currentPage > 0) { loadPage(--currentPage); } });

    // 页码信息
    pageInfoLabel = new QLabel("1 / 1", listContainer);
    pageInfoLabel->setStyleSheet("color: #ffffff; font-size: 13px; min-width: 80px;");
    pageInfoLabel->setAlignment(Qt::AlignCenter);

    // 下一页按钮
    nextPageBtn = createBtn(QString::fromLocal8Bit("下一页"), [this]() { if (currentPage < totalPages - 1) { loadPage(++currentPage); } });

    // 跳转功能
    QHBoxLayout *jumpLayout = new QHBoxLayout();
    jumpLayout->setSpacing(4);

    jumpLineEdit = new QLineEdit(listContainer);
    jumpLineEdit->setPlaceholderText(QString::fromLocal8Bit("页数"));
    jumpLineEdit->setStyleSheet("QLineEdit { background-color: #333; color: white; border: 1px solid #555; border-radius: 3px; padding: 3px 5px; width: 60px; }"
                                "QLineEdit:focus { border: 1px solid #2e7d32; }");

    QPushButton *jumpBtn = new QPushButton(QString::fromLocal8Bit("跳转"), listContainer);
    jumpBtn->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; border-radius: 3px; padding: 3px 6px; width: 35px; }"
                           "QPushButton:hover { background-color: #388e3c; }"
                           "QPushButton:pressed { background-color: #1b5e20; }");
    connect(jumpBtn, &QPushButton::clicked, [this]()
    {
        if (!jumpLineEdit) return;
        bool ok;
        int page = jumpLineEdit->text().toInt(&ok);
        if (ok && page >= 1 && page <= totalPages)
        {
            currentPage = page - 1;
            loadPage(currentPage);
            jumpLineEdit->clear();
        }
        else
        {
            // 输入无效，清空并提示
            jumpLineEdit->clear();
            jumpLineEdit->setPlaceholderText("1-" + QString::number(totalPages));
        }
    });
    // 回车键触发跳转
    connect(jumpLineEdit, &QLineEdit::returnPressed, jumpBtn, &QPushButton::click);

    jumpLayout->addWidget(jumpLineEdit);
    jumpLayout->addWidget(jumpBtn);

    // 总记录数信息
    recordCountLabel = new QLabel(QString::fromLocal8Bit("共 0 条"), listContainer);
    recordCountLabel->setStyleSheet("color: #00ff00; font-size: 12px; min-width: 70px;");
    recordCountLabel->setAlignment(Qt::AlignCenter);

    paginationLayout->addWidget(currentPageRecordLabel);
    paginationLayout->addWidget(prevPageBtn);
    paginationLayout->addWidget(pageInfoLabel);
    paginationLayout->addWidget(nextPageBtn);
    paginationLayout->addSpacing(10);
    paginationLayout->addLayout(jumpLayout);
    paginationLayout->addSpacing(10);
    paginationLayout->addWidget(recordCountLabel);

    listContainerLayout->addLayout(paginationLayout);

    mainLayout->addWidget(listContainer, 1);

    // 加载日志列表
    loadHistoryList();

    // 视频容器（默认隐藏）
    videoContainer = new QWidget(historyWindow);
    videoContainer->setVisible(false);

    QVBoxLayout *videoLayout = new QVBoxLayout(videoContainer);

    // 视频标题
    videoTitleLabel = new QLabel(videoContainer);
    videoTitleLabel->setFixedHeight(40);
    videoTitleLabel->setStyleSheet("QLabel {"
                                   "    color: #00ff00;"
                                   "    font-size: 14px;"
                                   "    font-weight: bold;"
                                   "    padding: 8px 12px;"
                                   "    background-color: #2d2d2d;"
                                   "    border-radius: 4px;"
                                   "}");
    videoTitleLabel->setAlignment(Qt::AlignCenter);
    videoTitleLabel->setText(QString::fromLocal8Bit("请选择视频查看"));
    videoLayout->addWidget(videoTitleLabel);

    // 视频播放器
    player = new QMediaPlayer(historyWindow);
    QVideoWidget *videoWidget = new QVideoWidget(historyWindow);
    videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    player->setVideoOutput(videoWidget);

    // 视频容器样式
    QWidget *videoContainerWidget = new QWidget(historyWindow);
    videoContainerWidget->setStyleSheet("background-color: #000000; border: 2px solid #2e7d32; border-radius: 8px;");
    QVBoxLayout *containerLayout = new QVBoxLayout(videoContainerWidget);
    containerLayout->addWidget(videoWidget);
    videoLayout->addWidget(videoContainerWidget);

    // 进度条和时间显示
    QHBoxLayout *progressLayout = new QHBoxLayout();
    progressLayout->setContentsMargins(5, 0, 5, 0);

    timeLabel = new QLabel("00:00 / 00:00", videoContainer);
    timeLabel->setStyleSheet("color: #ffffff; font-size: 12px; min-width: 130px;");
    timeLabel->setAlignment(Qt::AlignCenter);

    videoSlider = new QSlider(Qt::Horizontal, videoContainer);
    videoSlider->setRange(0, 1000);
    videoSlider->setValue(0);
    videoSlider->setPageStep(0);
    videoSlider->setStyleSheet("QSlider::groove:horizontal { height: 6px; background: #555; border-radius: 3px; }"
                               "QSlider::handle:horizontal { background: #2e7d32; width: 14px; margin: -4px 0; border-radius: 7px; }"
                               "QSlider::handle:horizontal:hover { background: #388e3c; }"
                               "QSlider::sub-page:horizontal { background: #2e7d32; border-radius: 3px; }");

    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updateVideoPosition);
    connect(videoSlider, &QSlider::sliderPressed, this, &MainWindow::onSliderPressed);
    connect(videoSlider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(videoSlider, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);

    // 速度控制
    QLabel *speedLabel = new QLabel(QString::fromLocal8Bit("速度:"), videoContainer);
    speedLabel->setStyleSheet("color: #ffffff; font-size: 12px;");

    speedCombo = new QComboBox(videoContainer);
    // 速度列表 0.1 ~ 2.0 步进 0.1
    for (int i = 1; i <= 20; i++)
    {
        qreal speed = i / 10.0;
        speedCombo->addItem(QString::number(speed, 'f', 1) + "x", speed);
    }
    int index = speedCombo->findData(0.5);
    speedCombo->setCurrentIndex(index >= 0 ? index : 0);
    speedCombo->setStyleSheet("QComboBox {"
                              "    background-color: #333;"
                              "    color: #ffffff;"
                              "    border: 1px solid #555;"
                              "    border-radius: 4px;"
                              "    padding: 4px 8px;"
                              "    min-width: 60px;"
                              "}"
                              "QComboBox:hover { border: 1px solid #777; }"
                              "QComboBox::drop-down { border: none; }"
                              "QComboBox QAbstractItemView {"
                              "    background-color: #333;"
                              "    color: #ffffff;"
                              "    selection-background-color: #2e7d32;"
                              "}");

    connect(speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index)
    {
        if (player)
            player->setPlaybackRate(speedCombo->itemData(index).toReal());
    });

    progressLayout->addWidget(timeLabel);
    progressLayout->addWidget(videoSlider, 1);
    progressLayout->addWidget(speedLabel);
    progressLayout->addWidget(speedCombo);
    videoLayout->addLayout(progressLayout);

    // 视频控制栏
    QHBoxLayout *videoControlLayout = new QHBoxLayout();
    QPushButton *videoPlayBtn = new QPushButton(QString::fromLocal8Bit("播放"), videoContainer);
    QPushButton *videoPauseBtn = new QPushButton(QString::fromLocal8Bit("暂停"), videoContainer);
    QPushButton *videoStopBtn = new QPushButton(QString::fromLocal8Bit("停止"), videoContainer);
    QPushButton *videoCloseBtn = new QPushButton(QString::fromLocal8Bit("返回列表"), videoContainer);

    videoPlayBtn->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; border-radius: 4px; padding: 6px 16px; }"
                                "QPushButton:hover { background-color: #66bb6a; }"
                                "QPushButton:pressed { background-color: #1b5e20; }");
    videoPauseBtn->setStyleSheet("QPushButton { background-color: #f57c00; color: white; border-radius: 4px; padding: 6px 16px; }"
                                 "QPushButton:hover { background-color: #ffa726; }"
                                 "QPushButton:pressed { background-color: #e65100; }");
    videoStopBtn->setStyleSheet("QPushButton { background-color: #c62828; color: white; border-radius: 4px; padding: 6px 16px; }"
                                "QPushButton:hover { background-color: #ef5350; }"
                                "QPushButton:pressed { background-color: #b71c1c; }");
    videoCloseBtn->setStyleSheet("QPushButton { background-color: #424242; color: white; border-radius: 4px; padding: 6px 16px; }"
                                 "QPushButton:hover { background-color: #78909c; }"
                                 "QPushButton:pressed { background-color: #212121; }");

    connect(videoPlayBtn, &QPushButton::clicked, player, &QMediaPlayer::play);
    connect(videoPauseBtn, &QPushButton::clicked, player, &QMediaPlayer::pause);
    connect(videoStopBtn, &QPushButton::clicked, player, &QMediaPlayer::stop);
    connect(videoCloseBtn, &QPushButton::clicked, [this]() { showHistoryList(); });

    videoControlLayout->addWidget(videoPlayBtn);
    videoControlLayout->addWidget(videoPauseBtn);
    videoControlLayout->addWidget(videoStopBtn);
    videoControlLayout->addStretch();
    videoControlLayout->addWidget(videoCloseBtn);
    videoLayout->addLayout(videoControlLayout);

    mainLayout->addWidget(videoContainer, 1);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(QString::fromLocal8Bit("关闭"), historyWindow);
    closeBtn->setStyleSheet("QPushButton { background-color: #c62828; color: white; border-radius: 5px; padding: 8px 20px; }"
                            "QPushButton:hover { background-color: #ef5350; }"
                            "QPushButton:pressed { background-color: #b71c1c; }");
    connect(closeBtn, &QPushButton::clicked, historyWindow, &QWidget::close);
    mainLayout->addWidget(closeBtn);

    historyWindow->show();
}

void MainWindow::loadHistoryList()
{
    if (!listWidget)
        return;

    allLogLines.clear();
    listWidget->clear();

    QFile file(logDirPath + "error_log.txt");
    if (!file.exists())
    {
        showEmptyLabel();
        return;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        while (!in.atEnd())
        {
            QString line = in.readLine();
            if (!line.isEmpty())
                allLogLines.append(line);
        }
        file.close();
    }

    // 计算总页数（从最新开始，反向分页）
    totalPages = (allLogLines.size() + PAGE_SIZE - 1) / PAGE_SIZE;
    currentPage = 0;

    if (totalPages > 0)
        loadPage(currentPage);
    else
        showEmptyLabel();
}

void MainWindow::showEmptyLabel()
{
    QLabel *emptyLabel = new QLabel(QString::fromLocal8Bit("暂无历史记录"), historyWindow);
    emptyLabel->setStyleSheet("color: #888; font-size: 16px;");
    emptyLabel->setAlignment(Qt::AlignCenter);
    QListWidgetItem *item = new QListWidgetItem(listWidget);
    item->setSizeHint(QSize(0, 60));
    listWidget->setItemWidget(item, emptyLabel);
    currentPage = 0;
    totalPages = 1;
    updatePaginationControls();
}

void MainWindow::loadPage(int page)
{
    if (!listWidget || allLogLines.isEmpty())
        return;

    listWidget->clear();

    int totalRecords = allLogLines.size();
    int startIndex = totalRecords - 1 - page * PAGE_SIZE;
    int endIndex = qMax(0, startIndex - PAGE_SIZE + 1);

    for (int i = startIndex; i >= endIndex && i >= 0; i--)
    {
        QString line = allLogLines[i];
        QString ts = line.left(23);
        QString dateDir = ts.mid(0, 4) + ts.mid(5, 2) + ts.mid(8, 2);
        QString videoTs = dateDir + ts.mid(11, 2) + ts.mid(14, 2) + ts.mid(17, 2) + "_" + ts.mid(20, 3);
        QString originalPath = logDirPath + dateDir + "/original_error/" + videoTs + ".avi";
        QString resultPath = logDirPath + dateDir + "/result_error/" + videoTs + ".avi";

        QListWidgetItem *item = new QListWidgetItem(listWidget);
        item->setSizeHint(QSize(0, 45));
        listWidget->setItemWidget(item, createHistoryItem(totalRecords - i, line, originalPath, resultPath));
    }
    updatePaginationControls();
}

void MainWindow::updatePaginationControls()
{
    if (!currentPageRecordLabel || !prevPageBtn || !pageInfoLabel || !nextPageBtn || !jumpLineEdit || !recordCountLabel)
        return;

    // 计算当前页的记录数
    int totalRecords = allLogLines.size();
    int startIndex = totalRecords - 1 - currentPage * PAGE_SIZE;
    int endIndex = qMax(0, startIndex - PAGE_SIZE + 1);
    int currentPageRecords = qMax(0, startIndex - endIndex + 1);

    currentPageRecordLabel->setText(QString::fromLocal8Bit("本页 %1 条 (每页最多 %2 条)").arg(currentPageRecords).arg(PAGE_SIZE));
    prevPageBtn->setEnabled(currentPage > 0);
    pageInfoLabel->setText(QString("%1 / %2").arg(currentPage + 1).arg(totalPages));
    nextPageBtn->setEnabled(currentPage < totalPages - 1);
    jumpLineEdit->setPlaceholderText("1-" + QString::number(totalPages));
    recordCountLabel->setText(QString::fromLocal8Bit("共 %1 条").arg(totalRecords));
}

QWidget *MainWindow::createHistoryItem(int recordNumber, QString logLine, QString originalPath, QString resultPath)
{
    QWidget *itemWidget = new QWidget(listWidget);
    QHBoxLayout *layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(5, 2, 5, 2);

    // 记录号
    QLabel *recordLabel = new QLabel(QString::number(recordNumber), itemWidget);
    recordLabel->setStyleSheet("color: #ffa500; font-size: 12px; font-weight: bold; min-width: 45px;");
    recordLabel->setAlignment(Qt::AlignCenter);

    // 日志内容
    QLabel *logLabel = new QLabel(logLine, itemWidget);
    logLabel->setStyleSheet("color: #00ff00; font-size: 12px;");
    logLabel->setWordWrap(true);

    // 原视频按钮
    QString originalVideoText = QString::fromLocal8Bit("原视频");
    QPushButton *btnOriginal = new QPushButton(originalVideoText, itemWidget);
    btnOriginal->setStyleSheet("QPushButton { background-color: #1976d2; color: white; border-radius: 4px; padding: 4px 12px; }"
                               "QPushButton:hover { background-color: #42a5f5; }"
                               "QPushButton:pressed { background-color: #0d47a1; }"
                               "QPushButton:disabled { background-color: #555555; color: #888888; }");
    btnOriginal->setEnabled(!originalPath.isEmpty() && QFile::exists(originalPath));
    connect(btnOriginal, &QPushButton::clicked, [this, recordNumber, originalVideoText, logLine, originalPath]() { showVideo(recordNumber, originalVideoText, logLine, originalPath); });

    // 结果视频按钮
    QString resultVideoText = QString::fromLocal8Bit("结果视频");
    QPushButton *btnResult = new QPushButton(resultVideoText, itemWidget);
    btnResult->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; border-radius: 4px; padding: 4px 12px; }"
                             "QPushButton:hover { background-color: #66bb6a; }"
                             "QPushButton:pressed { background-color: #1b5e20; }"
                             "QPushButton:disabled { background-color: #555555; color: #888888; }");
    btnResult->setEnabled(!resultPath.isEmpty() && QFile::exists(resultPath));
    connect(btnResult, &QPushButton::clicked, [this, recordNumber, resultVideoText, logLine, resultPath]() { showVideo(recordNumber, resultVideoText, logLine, resultPath); });

    layout->addWidget(recordLabel);
    layout->addWidget(logLabel, 3);
    layout->addWidget(btnOriginal);
    layout->addWidget(btnResult);

    return itemWidget;
}

void MainWindow::showVideo(int recordNumber, QString type, QString logInfo, QString path)
{
    if (path.isEmpty() || !QFile::exists(path))
    {
        QMessageBox::warning(historyWindow, QString::fromLocal8Bit("警告"), QString::fromLocal8Bit("视频文件不存在:\n%1").arg(path));
        return;
    }

    // 隐藏列表容器，显示视频
    if (listContainer)
        listContainer->setVisible(false);
    if (videoContainer)
    {
        videoContainer->setVisible(true);
        // 更新视频标题（显示编号 + 类型 + 日志信息）
        if (videoTitleLabel)
            videoTitleLabel->setText(QString::fromLocal8Bit("#%1 %2 —— %3").arg(recordNumber).arg(type).arg(logInfo));
        if (player)
        {
            // 设置视频
            player->setMedia(QUrl::fromLocalFile(path));
            player->pause();
        }
    }
}

void MainWindow::updateVideoPosition(qint64 position)
{
    if (!isSliderPressed)
    {
        qint64 duration = player ? player->duration() : -1;
        if (duration > 0)
        {
            if (timeLabel)
                timeLabel->setText(formatTime(position) + " / " + formatTime(duration));
            if (videoSlider)
                videoSlider->setValue(static_cast<int>(position * 1000 / duration));
        }
    }
}

void MainWindow::onSliderPressed()
{
    isSliderPressed = true;
    if (player && player->state() == QMediaPlayer::StoppedState)
    {
        player->play();
        player->pause();
    }
}

void MainWindow::onSliderReleased()
{
    isSliderPressed = false;
    if (player && videoSlider)
    {
        qint64 duration = player->duration();
        if (duration > 0)
        {
            qint64 position = (videoSlider->value() * duration) / 1000;
            player->setPosition(position);
        }
    }
}

void MainWindow::onSliderMoved(int value)
{
    if (player && timeLabel)
    {
        qint64 duration = player->duration();
        if (duration > 0)
        {
            qint64 position = (value * duration) / 1000;
            timeLabel->setText(formatTime(position) + " / " + formatTime(duration));
        }
    }
}

QString MainWindow::formatTime(qint64 ms)
{
    if (ms <= 0)
        return "00:00";
    qint64 seconds = ms / 1000;
    qint64 minutes = seconds / 60;
    seconds = seconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::showHistoryList()
{
    // 显示列表容器，隐藏视频
    if (listContainer)
        listContainer->setVisible(true);
    if (videoContainer)
    {
        videoContainer->setVisible(false);
        if (player)
        {
            player->stop();
            player->setMedia(QUrl()); // 清空媒体
        }
        // 重置进度条
        if (timeLabel)
            timeLabel->setText("00:00 / 00:00");
        if (videoSlider)
            videoSlider->setValue(0);
        // 重置速度
        if (speedCombo)
        {
            int index = speedCombo->findData(0.5);
            speedCombo->setCurrentIndex(index >= 0 ? index : 0);
        }
    }
}

void MainWindow::receive_connectstate(bool state)
{
    ui->btn_connectPLC->setEnabled(!state);
    ui->btn_disconnectPLC->setEnabled(state);
    ui->lb_proc4->setStyleSheet(QString("QLabel {"
                                        "    background-color: %1;"
                                        "    color: #ffffff;"
                                        "    border: 1px solid #555;"
                                        "    border-radius: 6px;"
                                        "}")
                                    .arg(state ? "#2e7d32" : "#c62828"));
    ui->lb_proc4->setText(QString::fromLocal8Bit(state ? "已连接" : "未连接"));
}

void MainWindow::on_btn_settings_clicked()
{
    if (ui->stackedWidget->currentIndex() != 1)
        ui->stackedWidget->setCurrentIndex(1);
    else
        ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::getActionState(std::vector<bool> actionState)
{
    QLabel *labels[] = {ui->label_4, ui->label_5, ui->label_6, ui->label_7, ui->label_8};
    QVector<int> enabledActions = getEnabledActions();
    bool enableAction = ui->checkBox_enableAction->isChecked();
    for (int i = 0; i < 5; i++)
    {
        QString color;
        if (!enableAction || !enabledActions.contains(i))
            color = "#424242"; // 未启用
        else if (actionState[i])
            color = "#2e7d32"; // 已完成
        else
            color = "#c62828"; // 未完成
        labels[i]->setStyleSheet(QString("QLabel {"
                                         "    background-color: %1;"
                                         "    color: #ffffff;"
                                         "    border-radius: 5px;"
                                         "}")
                                     .arg(color));
    }
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

void MainWindow::modifyROIParameter(QString parameterName, QVariant newValue)
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

void MainWindow::on_checkBox_saveAllVideos_toggled(bool value)
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.setValue("saveAllVideos", value);
    cam->setSaveAllVideos(value);
}
