#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QThread>
#include <iostream>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QColorDialog>
#include <QScreen>
#include <QLineEdit>
#include <QVideoWidget>
#include <QFile>
#include <QHeaderView>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    initTopMostButton();

    // 创建相机1
    THREAD1_cam1 = new QThread();
    cam = new Camera;

    initModelSettingControls();
    initNumberSettings();
    initROIControls();
    initLuosiROIControls();
    initActionControls();
    initSaveSettings();
    loadStatsFromCsv();

    cam->moveToThread(THREAD1_cam1); // 将Worker对象移到新线程中执行
    // 相机1槽函数
    connect(THREAD1_cam1, &QThread::started, cam, &Camera::run); // 启动线程调用线程类里面的主函数
    connect(cam, &Camera::finished, THREAD1_cam1, &QThread::quit); // 停止线程，线程那边触发会停止（finished），可以再次用start启动
    // connect(cam, &Camera::finished, cam, &QObject::deleteLater); // 在空闲时间删除线程对象，执行后将不能在用start方法启动线程
    connect(cam, &Camera::send_connectstate, this, &MainWindow::receive_connectstate, Qt::QueuedConnection);
    connect(cam, &Camera::updateLabelState, this, &MainWindow::updateLabelState, Qt::QueuedConnection);
    connect(cam, &Camera::logFileChanged, this, &MainWindow::onLogFileChanged, Qt::QueuedConnection);
    connect(cam, &Camera::updateStatistics, this, &MainWindow::updateStatistics, Qt::QueuedConnection);
    connect(cam, &Camera::sendQImgToAutoMain, this, &MainWindow::receiveslotQImg, Qt::QueuedConnection);
    connect(cam, &Camera::updateActionState, this, &MainWindow::getActionState, Qt::QueuedConnection);
    connect(cam, &Camera::sendNumber, this, &MainWindow::receiveNumber, Qt::QueuedConnection);
    connect(cam, &Camera::sendQStringtoMain, this, &MainWindow::receiveQStringtoMain, Qt::QueuedConnection);
    connect(cam, &Camera::finishedthread, this, &MainWindow::receivefinish);
    connect(this, &MainWindow::destroyed, cam, &Camera::deleteLater, Qt::QueuedConnection);

    // 启动相机1
    on_btn_start_clicked();
}

MainWindow::~MainWindow()
{
    cam->closeDevice();
    delete ui;
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    if (initialCentered)
        return;
    initialCentered = true;
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

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (historyWindow)
        historyWindow->move(qMax(0, width() - historyWindow->width()), 0);
    if (pinBtn)
        pinBtn->move(width() - pinBtn->width() - 6, 4);
}

void MainWindow::applyTopMost(bool checked)
{
    if (!pinBtn)
        return;

    bool wasVisible = isVisible();
    setWindowFlag(Qt::WindowStaysOnTopHint, checked);
    if (wasVisible)
        show();

    if (csvDialog)
    {
        bool dlgVisible = csvDialog->isVisible();
        csvDialog->setWindowFlag(Qt::WindowStaysOnTopHint, checked);
        if (dlgVisible)
            csvDialog->show();
    }

    pinBtn->setStyleSheet(checked
                              ? "QToolButton { background-color: #2e7d32; color: white; border: 1px solid #4caf50;"
                                "border-radius: 4px; font-weight: bold; }"
                                "QToolButton:hover { background-color: #388e3c; }"
                                "QToolButton:pressed { background-color: #1b5e20; }"
                              : "QToolButton { background-color: #555; color: #aaa; border: 1px solid #666;"
                                "border-radius: 4px; }"
                                "QToolButton:hover { background-color: #666; color: #fff; }"
                                "QToolButton:pressed { background-color: #444; }");
}

void MainWindow::initTopMostButton()
{
    pinBtn = new QToolButton(this);
    pinBtn->setObjectName("pinBtn");
    pinBtn->setText(QStringLiteral("置顶"));
    pinBtn->setCheckable(true);
    pinBtn->setAutoRaise(true);
    pinBtn->setFixedSize(50, 28);
    pinBtn->move(width() - pinBtn->width() - 6, 4);

    QSettings settings(iniFilePath, QSettings::IniFormat);
    bool topMost = settings.value("topMost").toBool();

    applyTopMost(topMost);
    pinBtn->setChecked(topMost);

    connect(pinBtn, &QToolButton::toggled, this, [this](bool checked)
            {
        applyTopMost(checked);

        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.setValue("topMost", checked); });
}

void MainWindow::setGroupLocked(QGroupBox *group, QCheckBox *lockBox, bool locked)
{
    const auto widgets = group->findChildren<QWidget *>();
    for (QWidget *w : widgets)
    {
        if (w != lockBox)
            w->setEnabled(!locked);
    }
}

void MainWindow::initModelSettingControls()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("Model");
    bool lockModelSetting = settings.value("lockModelSetting").toBool();
    int sliderValue = settings.value("probabilityThreshold").toInt();
    settings.endGroup();

    ui->cb_lockModelSetting->setChecked(lockModelSetting);
    ui->slider_prob_threshold->setValue(sliderValue);
    float threshold = sliderValue / 100.0f;
    ui->label_prob_threshold_display->setText(QString("%1").arg(threshold, 0, 'f', 2));
    cam->setProbabilityThreshold(threshold);
    setGroupLocked(ui->groupBox_modelSetting, ui->cb_lockModelSetting, lockModelSetting);

    // 锁定模型设置
    connect(ui->cb_lockModelSetting, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("Model");
        settings.setValue("lockModelSetting", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_modelSetting, ui->cb_lockModelSetting, checked); });

    // 概率阈值滑块
    connect(ui->slider_prob_threshold, &QSlider::valueChanged, [this](int value)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("Model");
        settings.setValue("probabilityThreshold", value);
        settings.endGroup();

        float t = value / 100.0f;
        ui->label_prob_threshold_display->setText(QString("%1").arg(t, 0, 'f', 2));
        cam->setProbabilityThreshold(t); });
}

void MainWindow::applyNumberSettings()
{
    int chilunNum = ui->spinBox_chilunNum->value();
    int luosiNum = ui->spinBox_luosiNum->value();
    bool chilunLock = ui->cb_chilunLock->isChecked();
    bool luosiLock = ui->cb_luosiLock->isChecked();
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("NumberSettings");
    settings.setValue("chilunNum", chilunNum);
    settings.setValue("luosiNum", luosiNum);
    settings.setValue("chilunLock", chilunLock);
    settings.setValue("luosiLock", luosiLock);
    settings.endGroup();

    cam->setNumberConfig(chilunNum, luosiNum, chilunLock, luosiLock);
}

void MainWindow::initNumberSettings()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("NumberSettings");
    bool lockNumberSettings = settings.value("lockNumberSettings").toBool();
    int chilunNum = settings.value("chilunNum").toInt();
    int luosiNum = settings.value("luosiNum").toInt();
    bool chilunLock = settings.value("chilunLock").toBool();
    bool luosiLock = settings.value("luosiLock").toBool();
    settings.endGroup();

    ui->cb_lockNumberSettings->setChecked(lockNumberSettings);
    ui->spinBox_chilunNum->setValue(chilunNum);
    ui->spinBox_luosiNum->setValue(luosiNum);
    ui->cb_chilunLock->setChecked(chilunLock);
    ui->cb_luosiLock->setChecked(luosiLock);
    cam->setNumberConfig(chilunNum, luosiNum, chilunLock, luosiLock);
    setGroupLocked(ui->groupBox_numberSettings, ui->cb_lockNumberSettings, lockNumberSettings);

    // 锁定数量设置
    connect(ui->cb_lockNumberSettings, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("NumberSettings");
        settings.setValue("lockNumberSettings", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_numberSettings, ui->cb_lockNumberSettings, checked); });

    // 数量参数变化时应用
    connect(ui->spinBox_chilunNum, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::applyNumberSettings);
    connect(ui->spinBox_luosiNum, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::applyNumberSettings);
    connect(ui->cb_chilunLock, &QCheckBox::toggled, this, &MainWindow::applyNumberSettings);
    connect(ui->cb_luosiLock, &QCheckBox::toggled, this, &MainWindow::applyNumberSettings);
}

void MainWindow::modifyROIParameter(QString parameterName, QVariant newValue)
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("ROI");
    settings.setValue(parameterName, newValue);
    settings.endGroup();
}

QString MainWindow::colorButtonStyle(const QColor &color)
{
    int h, s, v, a;
    color.getHsv(&h, &s, &v, &a);
    const QColor disabled = QColor::fromHsv(h, s, v / 2, a);
    return QString("QPushButton { background-color: %1; border: 1px solid #888; }"
                   "QPushButton:disabled { background-color: %2; }")
        .arg(color.name(), disabled.name());
}

void MainWindow::initROIControls()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("ROI");
    bool lockROI = settings.value("lockROI").toBool();
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

    ui->cb_lockROI->setChecked(lockROI);
    ui->cb_enableROI->setChecked(enableROI);
    ui->spinBox_roi_x->setValue(roi_x);
    ui->spinBox_roi_y->setValue(roi_y);
    ui->spinBox_roi_w->setValue(roi_w);
    ui->spinBox_roi_h->setValue(roi_h);
    ui->btn_colorPicker->setStyleSheet(colorButtonStyle(QColor(roiColorStr)));
    ui->slider_opacity->setValue(roi_opacity);
    ui->label_opacity_display->setText(QString("%1%").arg(roi_opacity));
    ui->slider_line_width->setValue(roi_line_width);
    ui->label_line_width_display->setText(QString("%1").arg(roi_line_width));
    cam->enableROIDetection(enableROI);
    cam->setRoiX(roi_x);
    cam->setRoiY(roi_y);
    cam->setRoiW(roi_w);
    cam->setRoiH(roi_h);
    cam->setRoiColor(roiColorStr);
    cam->setRoiOpacity(roi_opacity / 100.0f);
    cam->setRoiLineWidth(roi_line_width);
    setGroupLocked(ui->groupBox_ROI, ui->cb_lockROI, lockROI);

    // 锁定ROI设置
    connect(ui->cb_lockROI, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("ROI");
        settings.setValue("lockROI", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_ROI, ui->cb_lockROI, checked); });

    // 启用ROI检测
    connect(ui->cb_enableROI, &QCheckBox::toggled, [this](bool value)
            {
        modifyROIParameter("EnableROI", value);
        cam->enableROIDetection(value); });

    // ROI坐标/尺寸变化
    connect(ui->spinBox_roi_x, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v)
            {
        modifyROIParameter("RoiX", v);
        cam->setRoiX(v); });
    connect(ui->spinBox_roi_y, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v)
            {
        modifyROIParameter("RoiY", v);
        cam->setRoiY(v); });
    connect(ui->spinBox_roi_w, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v)
            {
        modifyROIParameter("RoiW", v);
        cam->setRoiW(v); });
    connect(ui->spinBox_roi_h, QOverload<int>::of(&QSpinBox::valueChanged), [this](int v)
            {
        modifyROIParameter("RoiH", v);
        cam->setRoiH(v); });

    // ROI颜色选择
    connect(ui->btn_colorPicker, &QPushButton::clicked, [this]()
            {
        QColor color = QColorDialog::getColor(QColor(currentRoiColor), this, QStringLiteral("选择检测框颜色"));
        if (!color.isValid())
            return;
        QString colorName = color.name();
        modifyROIParameter("RoiColor", colorName);
        currentRoiColor = colorName;
        ui->btn_colorPicker->setStyleSheet(colorButtonStyle(QColor(colorName)));
        cam->setRoiColor(colorName); });

    // ROI透明度
    connect(ui->slider_opacity, &QSlider::valueChanged, [this](int value)
            {
        modifyROIParameter("RoiOpacity", value);
        ui->label_opacity_display->setText(QString("%1%").arg(value));
        cam->setRoiOpacity(value / 100.0f); });

    // ROI线宽
    connect(ui->slider_line_width, &QSlider::valueChanged, [this](int value)
            {
        modifyROIParameter("RoiLineWidth", value);
        ui->label_line_width_display->setText(QString("%1").arg(value));
        cam->setRoiLineWidth(value); });
}

QString MainWindow::formatLuosiROIText(const LuosiROIConfig &cfg, int index)
{
    return QStringLiteral("ROI %1 [%2]  (%3,%4,%5,%6)  %7%/%8  达标:%9%10")
        .arg(index + 1)
        .arg(cfg.enabled ? QStringLiteral("启用") : QStringLiteral("禁用"))
        .arg(cfg.rect.x())
        .arg(cfg.rect.y())
        .arg(cfg.rect.width())
        .arg(cfg.rect.height())
        .arg(cfg.opacity * 100)
        .arg(cfg.lineWidth)
        .arg(cfg.targetNum)
        .arg(cfg.lockReached ? QStringLiteral("锁") : QStringLiteral(""));
}

void MainWindow::updateLuosiROIItemText(int row)
{
    if (row < 0 || row >= luosiROIList.size())
        return;
    const LuosiROIConfig &cfg = luosiROIList[row];
    QListWidgetItem *item = ui->listWidget_luosiROI->item(row);
    item->setText(formatLuosiROIText(cfg, row));
    item->setForeground(QBrush(cfg.enabled ? cfg.color : QColor(160, 160, 160)));
}

void MainWindow::saveLuosiROIToINI()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    // 先读取组级开关，避免 remove 整组后丢失
    settings.beginGroup("LuosiROI");
    bool lockLuosiROI = settings.value("lockLuosiROI").toBool();
    bool enabled = settings.value("enabled").toBool();
    bool useReachedCount = settings.value("useReachedCount").toBool();
    settings.endGroup();

    settings.remove("LuosiROI");
    settings.beginGroup("LuosiROI");
    settings.setValue("lockLuosiROI", lockLuosiROI);
    settings.setValue("enabled", enabled);
    settings.setValue("useReachedCount", useReachedCount);
    settings.setValue("count", luosiROIList.size());
    for (int i = 0; i < luosiROIList.size(); ++i)
    {
        QString p = QString("%1_").arg(i);
        const LuosiROIConfig &cfg = luosiROIList[i];
        settings.setValue(p + "enabled", cfg.enabled);
        settings.setValue(p + "rect", cfg.rect);
        settings.setValue(p + "color", cfg.color.name());
        settings.setValue(p + "opacity", static_cast<int>(cfg.opacity * 100));
        settings.setValue(p + "lineWidth", cfg.lineWidth);
        settings.setValue(p + "targetNum", cfg.targetNum);
        settings.setValue(p + "lockReached", cfg.lockReached);
    }
    settings.endGroup();
}

void MainWindow::syncLuosiROIToCamera()
{
    cam->setLuosiROIs(luosiROIList);
}

void MainWindow::commitLuosiROIChange(int row)
{
    updateLuosiROIItemText(row);
    saveLuosiROIToINI();
    syncLuosiROIToCamera();
}

void MainWindow::updateLuosiROIEditState()
{
    bool sel = ui->listWidget_luosiROI->currentRow() >= 0;

    QWidget *labels[] = {
        ui->label_luosiROI_x,
        ui->label_luosiROI_y,
        ui->label_luosiROI_w,
        ui->label_luosiROI_h,
        ui->label_luosiROI_color,
        ui->label_luosiROI_opacity,
        ui->label_luosiROI_lineWidth,
        ui->label_luosiROI_targetNum};

    QWidget *edits[] = {
        ui->btn_copyLuosiROI,
        ui->btn_removeLuosiROI,
        ui->cb_luosiROI_enabled,
        ui->spinBox_luosiROI_x,
        ui->spinBox_luosiROI_y,
        ui->spinBox_luosiROI_w,
        ui->spinBox_luosiROI_h,
        ui->btn_luosiROI_color,
        ui->spinBox_luosiROI_opacity,
        ui->spinBox_luosiROI_lineWidth,
        ui->spinBox_luosiROI_targetNum,
        ui->cb_luosiROI_lockReached};
    if (!sel)
    {
        QWidget *fw = QApplication::focusWidget();
        if (fw && ui->groupBox_luosiROI->isAncestorOf(fw))
            fw->clearFocus();
    }
    for (auto *w : labels)
        w->setEnabled(sel);
    for (auto *w : edits)
        w->setEnabled(sel);

    for (auto *w : edits)
        w->blockSignals(true);
    LuosiROIConfig cfg = sel ? luosiROIList[ui->listWidget_luosiROI->currentRow()] : LuosiROIConfig();
    ui->cb_luosiROI_enabled->setChecked(cfg.enabled);
    ui->spinBox_luosiROI_x->setValue(cfg.rect.x());
    ui->spinBox_luosiROI_y->setValue(cfg.rect.y());
    ui->spinBox_luosiROI_w->setValue(cfg.rect.width());
    ui->spinBox_luosiROI_h->setValue(cfg.rect.height());
    ui->btn_luosiROI_color->setStyleSheet(colorButtonStyle(cfg.color));
    ui->spinBox_luosiROI_opacity->setValue(cfg.opacity);
    ui->spinBox_luosiROI_lineWidth->setValue(cfg.lineWidth);
    ui->spinBox_luosiROI_targetNum->setValue(cfg.targetNum);
    ui->cb_luosiROI_lockReached->setChecked(cfg.lockReached);
    for (auto *w : edits)
        w->blockSignals(false);
}

void MainWindow::reselectLuosiROI(int row)
{
    ui->listWidget_luosiROI->blockSignals(true);
    ui->listWidget_luosiROI->clear();
    for (int i = 0; i < luosiROIList.size(); ++i)
    {
        const LuosiROIConfig &cfg = luosiROIList[i];
        QListWidgetItem *item = new QListWidgetItem(formatLuosiROIText(cfg, i));
        item->setForeground(QBrush(cfg.enabled ? cfg.color : QColor(160, 160, 160)));
        ui->listWidget_luosiROI->addItem(item);
    }
    if (row >= 0)
        ui->listWidget_luosiROI->setCurrentRow(row);
    ui->listWidget_luosiROI->blockSignals(false);
    updateLuosiROIEditState();
}

void MainWindow::initLuosiROIControls()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("LuosiROI");
    bool lockLuosiROI = settings.value("lockLuosiROI").toBool();
    bool enabled = settings.value("enabled").toBool();
    bool useReachedCount = settings.value("useReachedCount").toBool();
    int count = settings.value("count").toInt();
    for (int i = 0; i < count; ++i)
    {
        QString p = QString("%1_").arg(i);
        LuosiROIConfig cfg;
        cfg.enabled = settings.value(p + "enabled").toBool();
        cfg.rect = settings.value(p + "rect").toRect();
        cfg.color = QColor(settings.value(p + "color").toString());
        cfg.opacity = settings.value(p + "opacity").toInt() / 100.0f;
        cfg.lineWidth = settings.value(p + "lineWidth").toInt();
        cfg.targetNum = settings.value(p + "targetNum").toInt();
        cfg.lockReached = settings.value(p + "lockReached").toBool();
        luosiROIList.append(cfg);
    }
    settings.endGroup();

    ui->cb_lockLuosiROI->setChecked(lockLuosiROI);
    ui->cb_enableLuosiROI->setChecked(enabled);
    ui->cb_luosiROI_useReachedCount->setChecked(useReachedCount);
    cam->enableLuosiROI(enabled);
    cam->setLuosiUseReachedCount(useReachedCount);
    setGroupLocked(ui->groupBox_luosiROI, ui->cb_lockLuosiROI, lockLuosiROI);
    reselectLuosiROI(-1);
    syncLuosiROIToCamera();

    // 锁定螺丝ROI设置
    connect(ui->cb_lockLuosiROI, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("LuosiROI");
        settings.setValue("lockLuosiROI", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_luosiROI, ui->cb_lockLuosiROI, checked);
        if (!checked)
            updateLuosiROIEditState(); });

    // 启用螺丝ROI检测
    connect(ui->cb_enableLuosiROI, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("LuosiROI");
        settings.setValue("enabled", checked);
        settings.endGroup();

        cam->enableLuosiROI(checked); });

    // 使用达标数量
    connect(ui->cb_luosiROI_useReachedCount, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("LuosiROI");
        settings.setValue("useReachedCount", checked);
        settings.endGroup();

        cam->setLuosiUseReachedCount(checked); });

    // 添加ROI
    connect(ui->btn_addLuosiROI, &QPushButton::clicked, [this]()
            {
        luosiROIList.append(LuosiROIConfig());
        reselectLuosiROI(luosiROIList.size() - 1);
        saveLuosiROIToINI();
        syncLuosiROIToCamera(); });

    // 复制ROI
    connect(ui->btn_copyLuosiROI, &QPushButton::clicked, [this]()
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList.append(luosiROIList[row]);
        reselectLuosiROI(luosiROIList.size() - 1);
        saveLuosiROIToINI();
        syncLuosiROIToCamera(); });

    // 删除ROI
    connect(ui->btn_removeLuosiROI, &QPushButton::clicked, [this]()
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList.removeAt(row);
        reselectLuosiROI(qMin(row, luosiROIList.size() - 1));
        saveLuosiROIToINI();
        syncLuosiROIToCamera(); });

    // 选中ROI时刷新编辑区
    connect(ui->listWidget_luosiROI, &QListWidget::currentRowChanged, this, &MainWindow::updateLuosiROIEditState);

    // ROI启用状态
    connect(ui->cb_luosiROI_enabled, &QCheckBox::toggled, [this](bool checked)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList[row].enabled = checked;
        commitLuosiROIChange(row); });

    // ROI坐标X
    connect(ui->spinBox_luosiROI_x, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        const QRect &r = luosiROIList[row].rect;
        luosiROIList[row].rect = QRect(value, r.y(), r.width(), r.height());
        commitLuosiROIChange(row); });

    // ROI坐标Y
    connect(ui->spinBox_luosiROI_y, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        const QRect &r = luosiROIList[row].rect;
        luosiROIList[row].rect = QRect(r.x(), value, r.width(), r.height());
        commitLuosiROIChange(row); });

    // ROI宽度
    connect(ui->spinBox_luosiROI_w, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        const QRect &r = luosiROIList[row].rect;
        luosiROIList[row].rect = QRect(r.x(), r.y(), value, r.height());
        commitLuosiROIChange(row); });

    // ROI高度
    connect(ui->spinBox_luosiROI_h, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        const QRect &r = luosiROIList[row].rect;
        luosiROIList[row].rect = QRect(r.x(), r.y(), r.width(), value);
        commitLuosiROIChange(row); });

    // ROI颜色选择
    connect(ui->btn_luosiROI_color, &QPushButton::clicked, [this]()
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        QColor c = QColorDialog::getColor(luosiROIList[row].color, this, QStringLiteral("选择ROI颜色"));
        if (!c.isValid())
            return;
        luosiROIList[row].color = c;
        ui->btn_luosiROI_color->setStyleSheet(colorButtonStyle(c));
        commitLuosiROIChange(row); });

    // ROI透明度
    connect(ui->spinBox_luosiROI_opacity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [this](double value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList[row].opacity = static_cast<float>(value);
        commitLuosiROIChange(row); });

    // ROI线宽
    connect(ui->spinBox_luosiROI_lineWidth, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList[row].lineWidth = value;
        commitLuosiROIChange(row); });

    // ROI目标数量
    connect(ui->spinBox_luosiROI_targetNum, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList[row].targetNum = value;
        commitLuosiROIChange(row); });

    // 达标后锁存
    connect(ui->cb_luosiROI_lockReached, &QCheckBox::toggled, [this](bool checked)
            {
        int row = ui->listWidget_luosiROI->currentRow();
        if (row < 0)
            return;
        luosiROIList[row].lockReached = checked;
        commitLuosiROIChange(row); });
}

void MainWindow::modifyActionParameter()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("Action");
    settings.setValue("EnableAction", ui->checkBox_enableAction->isChecked());
    settings.setValue("IncludeActionInResult", ui->checkBox_includeActionInResult->isChecked());
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
    cam->setActionConfig(ui->checkBox_enableAction->isChecked(), getEnabledActions(), ui->checkBox_includeActionInResult->isChecked(), orderedActions);
}

void MainWindow::initActionControls()
{
    // 动作配置
    actionItems = {
        {QStringLiteral("1.左上螺丝"), ui->checkBox_action1},
        {QStringLiteral("2.右上螺丝"), ui->checkBox_action2},
        {QStringLiteral("3.左下螺丝"), ui->checkBox_action3},
        {QStringLiteral("4.右下螺丝"), ui->checkBox_action4},
        {QStringLiteral("5.放齿轮"), ui->checkBox_action5}};

    // 加载动作配置
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("Action");
    bool lockActionControl = settings.value("lockActionControl").toBool();
    bool enableAction = settings.value("EnableAction").toBool();
    bool enableOrder = settings.value("EnableOrder").toBool();
    // 更新控件状态
    ui->cb_lockActionControl->setChecked(lockActionControl);
    ui->checkBox_enableAction->setChecked(enableAction);
    // 加载每个动作的勾选状态
    for (int i = 0; i < actionItems.size(); i++)
    {
        actionItems[i].second->setChecked(settings.value(QString("Action%1").arg(i + 1)).toBool());
        actionItems[i].second->setEnabled(enableAction);
    }
    ui->checkBox_includeActionInResult->setChecked(settings.value("IncludeActionInResult").toBool());
    ui->checkBox_includeActionInResult->setEnabled(enableAction);
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

    setGroupLocked(ui->groupBox_actionControl, ui->cb_lockActionControl, lockActionControl);
    updateActionConfig();

    // 锁定动作配置
    connect(ui->cb_lockActionControl, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("Action");
        settings.setValue("lockActionControl", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_actionControl, ui->cb_lockActionControl, checked);
        if (!checked)
        {
            bool enableAction = ui->checkBox_enableAction->isChecked();
            for (const auto& pair : actionItems) pair.second->setEnabled(enableAction);
            ui->checkBox_includeActionInResult->setEnabled(enableAction);
            ui->checkBox_enableOrder->setEnabled(enableAction);
            bool orderEnabled = enableAction && ui->checkBox_enableOrder->isChecked();
            ui->listWidget_order->setEnabled(orderEnabled);
            ui->btn_addToOrder->setEnabled(orderEnabled);
            ui->btn_removeFromOrder->setEnabled(orderEnabled);
            ui->btn_clearOrder->setEnabled(orderEnabled);
        } });

    // 启用动作检测
    connect(ui->checkBox_enableAction, &QCheckBox::toggled, [this](bool checked)
            {
        // 启用/禁用动作复选框
        for (const auto& pair : actionItems) pair.second->setEnabled(checked);
        ui->checkBox_includeActionInResult->setEnabled(checked);
        ui->checkBox_enableOrder->setEnabled(checked);
        bool orderEnabled = checked && ui->checkBox_enableOrder->isChecked();
        ui->listWidget_order->setEnabled(orderEnabled);
        ui->btn_addToOrder->setEnabled(orderEnabled);
        ui->btn_removeFromOrder->setEnabled(orderEnabled);
        ui->btn_clearOrder->setEnabled(orderEnabled);
        updateActionConfig(); });

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
            updateActionConfig(); });
    }

    // 动作检测影响结果
    connect(ui->checkBox_includeActionInResult, &QCheckBox::toggled, this, &MainWindow::updateActionConfig);

    // 启用顺序检测
    connect(ui->checkBox_enableOrder, &QCheckBox::toggled, [this](bool checked)
            {
        ui->listWidget_order->setEnabled(checked);
        ui->btn_addToOrder->setEnabled(checked);
        ui->btn_removeFromOrder->setEnabled(checked);
        ui->btn_clearOrder->setEnabled(checked);
        updateActionConfig(); });

    // 添加选中到顺序列表
    connect(ui->btn_addToOrder, &QPushButton::clicked, [this]()
            {
        // 收集所有勾选的动作
        QStringList selectedActions;
        for (const auto& pair : actionItems) {
            if (pair.second->isChecked()) selectedActions.append(pair.first);
        }
        if (selectedActions.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先勾选需要检测的动作"));
            return;
        }
        // 添加到顺序列表（去重）
        for (const QString& action : selectedActions) {
            if (ui->listWidget_order->findItems(action, Qt::MatchExactly).isEmpty()) {
                ui->listWidget_order->addItem(action);
            }
        }
        updateActionConfig(); });

    // 移除选中的动作
    connect(ui->btn_removeFromOrder, &QPushButton::clicked, [this]()
            {
        int row = ui->listWidget_order->currentRow();
        if (row >= 0) {
            delete ui->listWidget_order->takeItem(row);
            updateActionConfig();
        } });

    // 清空顺序列表
    connect(ui->btn_clearOrder, &QPushButton::clicked, [this]()
            {
        ui->listWidget_order->clear();
        updateActionConfig(); });

    // 拖拽排序时更新
    connect(ui->listWidget_order->model(), &QAbstractItemModel::rowsMoved, this, &MainWindow::updateActionConfig);
}

void MainWindow::updateSaveVideoControlsEnabled()
{
    bool canEdit = !ui->cb_lockSaveSettings->isChecked() && ui->checkBox_recordVideo->isChecked();
    ui->checkBox_saveErrorVideo->setEnabled(canEdit);
    ui->checkBox_forceSaveVideo->setEnabled(canEdit);
}

void MainWindow::initSaveSettings()
{
    QSettings settings(iniFilePath, QSettings::IniFormat);
    settings.beginGroup("SaveSettings");
    bool lockSaveSettings = settings.value("lockSaveSettings").toBool();
    bool recordVideo = settings.value("recordVideo").toBool();
    bool saveErrorVideo = settings.value("saveErrorVideo").toBool();
    bool forceSaveVideo = settings.value("forceSaveVideo").toBool();
    bool saveErrorLog = settings.value("saveErrorLog").toBool();
    QString dayStartStr = settings.value("dayStart").toString();
    dayStartTime = QTime::fromString(dayStartStr, "HH:mm");
    settings.endGroup();

    ui->cb_lockSaveSettings->setChecked(lockSaveSettings);
    ui->checkBox_recordVideo->setChecked(recordVideo);
    ui->checkBox_saveErrorVideo->setChecked(saveErrorVideo);
    ui->checkBox_forceSaveVideo->setChecked(forceSaveVideo);
    ui->checkBox_saveErrorLog->setChecked(saveErrorLog);
    ui->timeEdit_dayStart->setTime(QTime::fromString(dayStartStr, "HH:mm"));
    cam->setRecordVideo(recordVideo);
    cam->setSaveErrorVideo(saveErrorVideo);
    cam->setForceSaveVideo(forceSaveVideo);
    cam->setSaveErrorLog(saveErrorLog);
    setGroupLocked(ui->groupBox_saveSettings, ui->cb_lockSaveSettings, lockSaveSettings);
    updateSaveVideoControlsEnabled();

    // 锁定保存设置
    connect(ui->cb_lockSaveSettings, &QCheckBox::toggled, [this](bool checked)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("lockSaveSettings", checked);
        settings.endGroup();

        setGroupLocked(ui->groupBox_saveSettings, ui->cb_lockSaveSettings, checked);
        updateSaveVideoControlsEnabled(); });

    // 录制视频开关
    connect(ui->checkBox_recordVideo, &QCheckBox::toggled, [this](bool value)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("recordVideo", value);
        settings.endGroup();

        cam->setRecordVideo(value);
        updateSaveVideoControlsEnabled(); });

    // 保存异常视频
    connect(ui->checkBox_saveErrorVideo, &QCheckBox::toggled, [this](bool value)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("saveErrorVideo", value);
        settings.endGroup();

        cam->setSaveErrorVideo(value); });

    // 强制保存视频
    connect(ui->checkBox_forceSaveVideo, &QCheckBox::toggled, [this](bool value)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("forceSaveVideo", value);
        settings.endGroup();

        cam->setForceSaveVideo(value); });

    // 保存异常日志
    connect(ui->checkBox_saveErrorLog, &QCheckBox::toggled, [this](bool value)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("saveErrorLog", value);
        settings.endGroup();

        cam->setSaveErrorLog(value); });

    // 当日起始时间
    connect(ui->timeEdit_dayStart, &QTimeEdit::timeChanged, [this](const QTime &time)
            {
        QSettings settings(iniFilePath, QSettings::IniFormat);
        settings.beginGroup("SaveSettings");
        settings.setValue("dayStart", time.toString("HH:mm"));
        settings.endGroup();

        dayStartTime = time;
        loadStatsFromCsv(); });
}

// 统计 CSV 内部标记行
static const QString kCsvHeader = QStringLiteral("日期,合格数,检测数,合格率");
static const QString kCsvResetMarker = QStringLiteral("--- 历史重置 ---");
static const QString kCsvHistoryMarker = QStringLiteral("历史累计");

void MainWindow::ensureStatCsvExists()
{
    QFile file(statFilePath);
    if (file.exists())
    {
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            QString firstLine = in.readLine().trimmed();
            file.close();
            if (firstLine == kCsvHeader)
                return;
        }
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "ensureStatCsvExists: cannot create" << statFilePath << file.errorString();
        return;
    }
    QTextStream out(&file);
    out << kCsvHeader << "\n";
    file.close();
}

QString MainWindow::statisticalDate()
{
    QDateTime now = QDateTime::currentDateTime();
    QDate date = now.date();
    if (now.time() < dayStartTime)
        date = date.addDays(-1);
    return date.toString("yyyy/MM/dd");
}

bool MainWindow::isValidCsvRow(const QString &line)
{
    const QStringList p = line.split(',');
    const QString first = p[0].trimmed();
    if (first == kCsvResetMarker || first == kCsvHistoryMarker)
        return true;
    if (p.size() < 4)
        return false;
    return QDate::fromString(first, "yyyy/MM/dd").isValid();
}

void MainWindow::fillCsvTable()
{
    if (!csvDialog)
        return;
    QTableWidget *table = csvDialog->findChild<QTableWidget *>("csvTableWidget");
    if (!table)
        return;

    table->setRowCount(0);

    QFile file(statFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&file);
    in.readLine();

    int row = 0;
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || !isValidCsvRow(line))
            continue;
        QStringList parts = line.split(',');

        table->insertRow(row);

        if (parts[0] == kCsvResetMarker)
        {
            for (int c = 0; c < 4; c++)
            {
                QTableWidgetItem *it = new QTableWidgetItem(c == 0 ? kCsvResetMarker : "");
                it->setBackground(QColor(60, 30, 30));
                it->setForeground(QColor(255, 150, 80));
                table->setItem(row, c, it);
            }
        }
        else if (parts[0] == kCsvHistoryMarker)
        {
            QStringList disp = {kCsvHistoryMarker, parts.value(1), parts.value(2), parts.value(3)};
            for (int c = 0; c < 4; c++)
            {
                QTableWidgetItem *it = new QTableWidgetItem(disp[c]);
                it->setBackground(QColor(20, 60, 30));
                it->setForeground(QColor(0, 255, 100));
                table->setItem(row, c, it);
            }
        }
        else
        {
            for (int c = 0; c < 4; c++)
                table->setItem(row, c, new QTableWidgetItem(parts.value(c)));
        }
        row++;
    }
    file.close();
}

void MainWindow::saveTodayRowToCsv()
{
    ensureStatCsvExists();

    QString todayStr = statisticalDate();
    double todayRate = today_number > 0 ? static_cast<double>(today_good_number) / today_number * 100.0 : 0.0;
    QString todayRow = QString("%1,%2,%3,%4").arg(todayStr).arg(today_good_number).arg(today_number).arg(todayRate, 0, 'f', 2);

    QFile file(statFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "saveTodayRowToCsv: cannot read" << statFilePath << file.errorString();
        return;
    }
    QTextStream in(&file);
    QString header = in.readLine();
    QStringList rows;
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(kCsvHistoryMarker) || !isValidCsvRow(line))
            continue;
        rows << line;
    }
    file.close();

    int resetIdx = -1;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i].startsWith(kCsvResetMarker))
        {
            resetIdx = i;
            break;
        }

    // 查找今日行：标记后优先，其次标记前（日期回退场景）
    int idx = -1;
    for (int i = (resetIdx >= 0 ? resetIdx + 1 : 0); i < rows.size(); ++i)
    {
        QStringList p = rows[i].split(',');
        if (p.size() >= 4 && p[0] == todayStr)
        {
            idx = i;
            break;
        }
    }
    if (idx < 0 && resetIdx >= 0)
        for (int i = 0; i < resetIdx; ++i)
        {
            QStringList p = rows[i].split(',');
            if (p.size() >= 4 && p[0] == todayStr)
            {
                idx = i;
                break;
            }
        }

    if (idx >= 0)
    {
        if (resetIdx >= 0 && idx < resetIdx)
        {
            // 回退到标记前：把该日期及其之后的所有旧数据行一起移到标记下方继续使用
            int count = resetIdx - idx;
            QStringList block;
            for (int i = 0; i < count; ++i)
                block << rows.takeAt(idx);
            block[0] = todayRow;
            resetIdx -= count;
            int pos = resetIdx + 1;
            for (const QString &r : block)
                rows.insert(pos++, r);
        }
        else
            rows[idx] = todayRow;
    }
    else
    {
        // 未找到：按日期升序插入当前统计周期
        int pos = resetIdx >= 0 ? resetIdx + 1 : 0;
        for (; pos < rows.size(); ++pos)
        {
            QString first = rows[pos].split(',').value(0);
            if (first == kCsvResetMarker)
                continue;
            if (first > todayStr)
                break;
        }
        rows.insert(pos, todayRow);
    }

    // 历史累计 = 重置标记之后所有数据行之和
    long long sumGood = 0, sumTotal = 0;
    bool passed = resetIdx < 0;
    for (const QString &r : rows)
    {
        QStringList p = r.split(',');
        if (p[0] == kCsvResetMarker)
        {
            passed = true;
            continue;
        }
        if (!passed || p.size() < 4)
            continue;
        sumGood += p[1].toLongLong();
        sumTotal += p[2].toLongLong();
    }
    double sumRate = sumTotal > 0 ? static_cast<double>(sumGood) / sumTotal * 100.0 : 0.0;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "saveTodayRowToCsv: cannot write" << statFilePath << file.errorString();
        return;
    }
    QTextStream out(&file);
    out << header << "\n";
    for (const QString &r : rows)
        out << r << "\n";
    out << QString("%1,%2,%3,%4\n").arg(kCsvHistoryMarker).arg(sumGood).arg(sumTotal).arg(sumRate, 0, 'f', 2);
    file.close();

    if (csvDialog && csvDialog->isVisible())
        fillCsvTable();
}

void MainWindow::loadStatsFromCsv()
{
    QString todayStr = statisticalDate();
    lastStatDate = todayStr;

    ensureStatCsvExists();

    QFile file(statFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "loadStatsFromCsv: cannot open" << statFilePath << file.errorString();
        return;
    }
    QTextStream in(&file);
    in.readLine();

    QStringList rows;
    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(kCsvHistoryMarker) || !isValidCsvRow(line))
            continue;
        rows << line;
    }
    file.close();

    int resetIdx = -1;
    for (int i = 0; i < rows.size(); ++i)
        if (rows[i].startsWith(kCsvResetMarker))
        {
            resetIdx = i;
            break;
        }

    history_good_number = history_number = today_good_number = today_number = today_good_rates = 0;
    bool passed = resetIdx < 0;
    bool foundToday = false, todayBeforeReset = false;
    int todayIdx = -1;

    for (int i = 0; i < rows.size(); ++i)
    {
        QStringList p = rows[i].split(',');
        if (p[0] == kCsvResetMarker)
        {
            passed = true;
            continue;
        }
        if (p.size() < 4)
            continue;
        if (passed)
        {
            history_good_number += p[1].toLongLong();
            history_number += p[2].toLongLong();
        }
        if (p[0] == todayStr)
        {
            foundToday = true;
            todayBeforeReset = !passed;
            todayIdx = i;
            today_good_number = p[1].toLongLong();
            today_number = p[2].toLongLong();
        }
    }

    // 今日行位于历史重置标记之前：把该日期及其之后的旧数据一并计入历史，并移到标记下方
    if (foundToday && todayBeforeReset)
    {
        history_good_number += today_good_number;
        history_number += today_number;
        for (int i = todayIdx + 1; i < resetIdx; ++i)
        {
            QStringList q = rows[i].split(',');
            if (q.size() < 4)
                continue;
            history_good_number += q[1].toLongLong();
            history_number += q[2].toLongLong();
        }
        saveTodayRowToCsv();
    }
    else if (!foundToday)
    {
        saveTodayRowToCsv();
    }

    history_good_rates = history_number > 0 ? static_cast<double>(history_good_number) / history_number * 100.0 : 0.0;
    today_good_rates = today_number > 0 ? static_cast<double>(today_good_number) / today_number * 100.0 : 0.0;

    ui->history_good_number->setText(QString::number(history_good_number));
    ui->history_number->setText(QString::number(history_number));
    ui->history_good_rates->setText(QString::number(history_good_rates, 'f', 2) + " %");
    ui->today_good_number->setText(QString::number(today_good_number));
    ui->today_number->setText(QString::number(today_number));
    ui->today_good_rates->setText(QString::number(today_good_rates, 'f', 2) + " %");
}

void MainWindow::receiveslotQImg(QImage img)
{
    ui->label_video->setPixmap(QPixmap::fromImage(img));
}

void MainWindow::receivefinish()
{
    ui->btn_start->setDisabled(false);
    ui->btn_stop->setDisabled(true);
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

void MainWindow::onLogFileChanged()
{
    if (historyWindow)
        loadHistoryList();
}

void MainWindow::updateStatistics(bool result)
{
    QString todayStr = statisticalDate();
    if (todayStr != lastStatDate)
    {
        lastStatDate = todayStr;
        QFile file(statFilePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QTextStream in(&file);
            in.readLine();
            today_good_number = today_number = 0;
            while (!in.atEnd())
            {
                QString line = in.readLine().trimmed();
                if (line.isEmpty() || !isValidCsvRow(line))
                    continue;
                QStringList p = line.split(',');
                if (p.size() >= 4 && p[0] == todayStr)
                {
                    today_good_number = p[1].toLongLong();
                    today_number = p[2].toLongLong();
                    break;
                }
            }
            file.close();
        }
    }

    if (result)
    {
        history_good_number++;
        today_good_number++;
    }
    history_number++;
    today_number++;
    history_good_rates = (static_cast<double>(history_good_number) / history_number) * 100;
    today_good_rates = (static_cast<double>(today_good_number) / today_number) * 100;

    saveTodayRowToCsv();

    ui->history_good_number->setText(QString::number(history_good_number));
    ui->history_number->setText(QString::number(history_number));
    ui->history_good_rates->setText(QString::number(history_good_rates, 'f', 2) + " %");
    ui->today_good_number->setText(QString::number(today_good_number));
    ui->today_number->setText(QString::number(today_number));
    ui->today_good_rates->setText(QString::number(today_good_rates, 'f', 2) + " %");
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
    QLabel *emptyLabel = new QLabel(QStringLiteral("暂无历史记录"), historyWindow);
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

    currentPageRecordLabel->setText(QStringLiteral("本页 %1 条 (每页最多 %2 条)").arg(currentPageRecords).arg(PAGE_SIZE));
    prevPageBtn->setEnabled(currentPage > 0);
    pageInfoLabel->setText(QString("%1 / %2").arg(currentPage + 1).arg(totalPages));
    nextPageBtn->setEnabled(currentPage < totalPages - 1);
    jumpLineEdit->setPlaceholderText("1-" + QString::number(totalPages));
    recordCountLabel->setText(QStringLiteral("共 %1 条").arg(totalRecords));
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
    QString originalVideoText = QStringLiteral("原视频");
    QPushButton *btnOriginal = new QPushButton(originalVideoText, itemWidget);
    btnOriginal->setStyleSheet("QPushButton { background-color: #1976d2; color: white; border-radius: 4px; padding: 4px 12px; }"
                               "QPushButton:hover { background-color: #42a5f5; }"
                               "QPushButton:pressed { background-color: #0d47a1; }"
                               "QPushButton:disabled { background-color: #555555; color: #888888; }");
    btnOriginal->setEnabled(!originalPath.isEmpty() && QFile::exists(originalPath));
    connect(btnOriginal, &QPushButton::clicked, [this, recordNumber, originalVideoText, logLine, originalPath]()
            { showVideo(recordNumber, originalVideoText, logLine, originalPath); });

    // 结果视频按钮
    QString resultVideoText = QStringLiteral("结果视频");
    QPushButton *btnResult = new QPushButton(resultVideoText, itemWidget);
    btnResult->setStyleSheet("QPushButton { background-color: #2e7d32; color: white; border-radius: 4px; padding: 4px 12px; }"
                             "QPushButton:hover { background-color: #66bb6a; }"
                             "QPushButton:pressed { background-color: #1b5e20; }"
                             "QPushButton:disabled { background-color: #555555; color: #888888; }");
    btnResult->setEnabled(!resultPath.isEmpty() && QFile::exists(resultPath));
    connect(btnResult, &QPushButton::clicked, [this, recordNumber, resultVideoText, logLine, resultPath]()
            { showVideo(recordNumber, resultVideoText, logLine, resultPath); });

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
        QMessageBox::warning(historyWindow, QStringLiteral("警告"), QStringLiteral("视频文件不存在:\n%1").arg(path));
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
            videoTitleLabel->setText(QStringLiteral("#%1 %2 —— %3").arg(recordNumber).arg(type).arg(logInfo));
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
    ui->lb_proc4->setText(state ? QStringLiteral("已连接") : QStringLiteral("未连接"));
}

void MainWindow::getActionState(std::vector<bool> actionState)
{
    QLabel *labels[] = {ui->label_action1, ui->label_action2, ui->label_action3, ui->label_action4, ui->label_action5};
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

void MainWindow::on_btn_start_clicked()
{
    if (!THREAD1_cam1->isRunning())
    {
        THREAD1_cam1->start();
        ui->btn_start->setDisabled(true);
        ui->btn_stop->setDisabled(false);
    }
}

void MainWindow::on_btn_stop_clicked()
{
    if (THREAD1_cam1->isRunning())
    {
        cam->stop_camera();
        ui->btn_start->setDisabled(false);
        ui->btn_stop->setDisabled(true);
    }
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
    QString todayStr = statisticalDate();

    QFile file(statFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "on_btn_history_rst_clicked: cannot read" << statFilePath << file.errorString();
        return;
    }
    QTextStream in(&file);

    QString header = in.readLine();
    QStringList rows;

    while (!in.atEnd())
    {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(kCsvResetMarker) || line.startsWith(kCsvHistoryMarker) || !isValidCsvRow(line))
            continue;
        rows << line;
    }
    file.close();

    QStringList newRows;
    bool inserted = false;
    for (const QString &row : rows)
    {
        QStringList parts = row.split(',');
        if (!inserted && parts[0] == todayStr)
        {
            newRows << kCsvResetMarker + ",,,";
            inserted = true;
        }
        newRows << row;
    }
    if (!inserted)
    {
        newRows << kCsvResetMarker + ",,,";
        newRows << todayStr + ",0,0,0.00";
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        qDebug() << "on_btn_history_rst_clicked: cannot write" << statFilePath << file.errorString();
        return;
    }
    QTextStream out(&file);
    out << header << "\n";
    for (const QString &row : newRows)
        out << row << "\n";
    file.close();

    saveTodayRowToCsv();
    loadStatsFromCsv();
}

void MainWindow::on_btn_today_rst_clicked()
{
    today_good_number = today_number = today_good_rates = 0;
    saveTodayRowToCsv();
    loadStatsFromCsv();
}

void MainWindow::on_btn_csv_detail_clicked()
{
    if (!csvDialog)
    {
        csvDialog = new QDialog(this);
        csvDialog->setWindowTitle(QStringLiteral("统计数据详情"));
        csvDialog->setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
        csvDialog->resize(560, 380);
        csvDialog->setMinimumSize(560, 380);
        QVBoxLayout *layout = new QVBoxLayout(csvDialog);

        QTableWidget *table = new QTableWidget(csvDialog);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({QStringLiteral("日期"), QStringLiteral("合格数"), QStringLiteral("检测数"), QStringLiteral("合格率")});
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->verticalHeader()->setVisible(false);
        table->setStyleSheet("QTableWidget { background-color: #1e1e1e; color: #ffffff; border: 1px solid #555;"
                             "border-radius: 4px; gridline-color: #444; font-family: Consolas, monospace; font-size: 12px; }"
                             "QHeaderView::section { background-color: #2d2d2d; color: #00ff00; border: 1px solid #555; padding: 4px; font-weight: bold; }");
        table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
        table->setColumnWidth(0, 140);
        table->setColumnWidth(1, 80);
        table->setColumnWidth(2, 80);
        table->setObjectName("csvTableWidget");
        layout->addWidget(table);

        QLabel *pathLabel = new QLabel(QStringLiteral("文件: ") + statFilePath, csvDialog);
        pathLabel->setStyleSheet("QLabel { color: #888; font-size: 11px; padding: 4px; }");
        layout->addWidget(pathLabel);
    }

    csvDialog->show();
    csvDialog->raise();
    csvDialog->activateWindow();

    fillCsvTable();
}

void MainWindow::on_btn_history_clicked()
{
    // 如果窗口已经存在，则刷新数据并置前
    if (historyWindow)
    {
        loadHistoryList();
        historyWindow->show();
        historyWindow->raise();
        historyWindow->activateWindow();
        return;
    }

    // 创建独立窗口
    historyWindow = new QWidget(this);
    historyWindow->setObjectName("historyWindow");
    historyWindow->setMinimumSize(680, height());
    historyWindow->setStyleSheet("QWidget { background-color: #1e1e1e; }"
                                 "QWidget#historyWindow { border: 2px solid #444; border-radius: 8px; }"
                                 "QPushButton { background-color: #3d3d3d; color: #ffffff; border: 1px solid #555; border-radius: 5px; padding: 8px; font-weight: bold; }"
                                 "QPushButton:hover { background-color: #4d4d4d; }"
                                 "QPushButton:pressed { background-color: #2a2a2a; }"
                                 "QPushButton:disabled { background-color: #2d2d2d; color: #888; }");
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
        speedCombo = nullptr; });

    QVBoxLayout *mainLayout = new QVBoxLayout(historyWindow);

    // 标题
    QLabel *titleLabel = new QLabel(QStringLiteral("历史记录"), historyWindow);
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
    currentPageRecordLabel = new QLabel(QStringLiteral("本页 0 条 (每页最多 %1 条)").arg(PAGE_SIZE), listContainer);
    currentPageRecordLabel->setStyleSheet("color: #00ff00; font-size: 12px; min-width: 200px;");
    currentPageRecordLabel->setAlignment(Qt::AlignCenter);

    // 上一页按钮
    prevPageBtn = createBtn(QStringLiteral("上一页"), [this]()
                            { if (currentPage > 0) { loadPage(--currentPage); } });

    // 页码信息
    pageInfoLabel = new QLabel("1 / 1", listContainer);
    pageInfoLabel->setStyleSheet("color: #ffffff; font-size: 13px; min-width: 80px;");
    pageInfoLabel->setAlignment(Qt::AlignCenter);

    // 下一页按钮
    nextPageBtn = createBtn(QStringLiteral("下一页"), [this]()
                            { if (currentPage < totalPages - 1) { loadPage(++currentPage); } });

    // 跳转功能
    QHBoxLayout *jumpLayout = new QHBoxLayout();
    jumpLayout->setSpacing(4);

    jumpLineEdit = new QLineEdit(listContainer);
    jumpLineEdit->setPlaceholderText(QStringLiteral("页数"));
    jumpLineEdit->setStyleSheet("QLineEdit { background-color: #333; color: white; border: 1px solid #555; border-radius: 3px; padding: 3px 5px; width: 60px; }"
                                "QLineEdit:focus { border: 1px solid #2e7d32; }");

    QPushButton *jumpBtn = new QPushButton(QStringLiteral("跳转"), listContainer);
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
        } });
    // 回车键触发跳转
    connect(jumpLineEdit, &QLineEdit::returnPressed, jumpBtn, &QPushButton::click);

    jumpLayout->addWidget(jumpLineEdit);
    jumpLayout->addWidget(jumpBtn);

    // 总记录数信息
    recordCountLabel = new QLabel(QStringLiteral("共 0 条"), listContainer);
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
    videoTitleLabel->setText(QStringLiteral("请选择视频查看"));
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

    connect(player, &QMediaPlayer::positionChanged, this, &MainWindow::updateVideoPosition);
    connect(videoSlider, &QSlider::sliderPressed, this, &MainWindow::onSliderPressed);
    connect(videoSlider, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(videoSlider, &QSlider::sliderMoved, this, &MainWindow::onSliderMoved);

    // 速度控制
    QLabel *speedLabel = new QLabel(QStringLiteral("速度:"), videoContainer);
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
            player->setPlaybackRate(speedCombo->itemData(index).toReal()); });

    progressLayout->addWidget(timeLabel);
    progressLayout->addWidget(videoSlider, 1);
    progressLayout->addWidget(speedLabel);
    progressLayout->addWidget(speedCombo);
    videoLayout->addLayout(progressLayout);

    // 视频控制栏
    QHBoxLayout *videoControlLayout = new QHBoxLayout();
    QPushButton *videoPlayBtn = new QPushButton(QStringLiteral("播放"), videoContainer);
    QPushButton *videoPauseBtn = new QPushButton(QStringLiteral("暂停"), videoContainer);
    QPushButton *videoStopBtn = new QPushButton(QStringLiteral("停止"), videoContainer);
    QPushButton *videoCloseBtn = new QPushButton(QStringLiteral("返回列表"), videoContainer);

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
    connect(videoCloseBtn, &QPushButton::clicked, this, &MainWindow::showHistoryList);

    videoControlLayout->addWidget(videoPlayBtn);
    videoControlLayout->addWidget(videoPauseBtn);
    videoControlLayout->addWidget(videoStopBtn);
    videoControlLayout->addStretch();
    videoControlLayout->addWidget(videoCloseBtn);
    videoLayout->addLayout(videoControlLayout);

    mainLayout->addWidget(videoContainer, 1);

    // 关闭按钮
    QPushButton *closeBtn = new QPushButton(QStringLiteral("关闭"), historyWindow);
    closeBtn->setStyleSheet("QPushButton { background-color: #c62828; color: white; border-radius: 5px; padding: 8px 20px; }"
                            "QPushButton:hover { background-color: #ef5350; }"
                            "QPushButton:pressed { background-color: #b71c1c; }");
    connect(closeBtn, &QPushButton::clicked, historyWindow, &QWidget::close);
    mainLayout->addWidget(closeBtn);

    historyWindow->show();
}

void MainWindow::on_btn_settings_clicked()
{
    if (ui->stackedWidget->currentIndex() != 1)
        ui->stackedWidget->setCurrentIndex(1);
    else
        ui->stackedWidget->setCurrentIndex(0);
}
