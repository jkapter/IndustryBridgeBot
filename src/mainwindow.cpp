#include "./ui_mainwindow.h"

#include <QCloseEvent>
#include <QJsonObject>
#include <QMenu>
#include <Qfile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QPropertyAnimation>

#include "mainwindow.h"
#include "sourcedrivers/copcclient.h"
#include "sourcedrivers/sourcedrivermanager.h"
#include "tgobjects/tgbotmanager.h"
#include "opcbrowsewidget.h"
#include "tgbotsettingswidget.h"
#include "opcvaluesviewer.h"
#include "tgbotconfigurationwidget.h"

int const MainWindow::EXIT_CODE_REBOOT = 8888;
int const MainWindow::EXIT_CODE_USER_CMD = 8887;

using namespace Qt::StringLiterals;

MainWindow::~MainWindow()
{
    qInfo() << u"MainWindow: деструктор"_s;
    write_settings_to_file_(qApp->applicationDirPath());
    delete ui;
}

MainWindow::MainWindow(TgBotManager* bot_manager, SourceDriverManager* driver_manager, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tg_bot_manager_(bot_manager)
    , source_data_manager_(driver_manager)
{
    if(!tg_bot_manager_ || !source_data_manager_) {
        qCritical() << QString("Исключение при инициализации приложения: TgBotManager=%1, OPCDataManager=%2")
                           .arg(tg_bot_manager_ ? QString("ОК") : QString("NULL")
                           , source_data_manager_ ? QString("ОК") : QString("NULL"));
        throw std::logic_error("uninitialized managers");
    }

    ui->setupUi(this);

    QFile style_file(u":/styles/styles.qss"_s);
    if(style_file.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream style_stream(&style_file);
        qApp->setStyleSheet(style_stream.readAll());
    } else {
        qWarning() << u"Не удается открыть файл стилей."_s;
    }

    status_bar_opc_da_label_ = new QLabel(u"OPC DA"_s, this);
    status_bar_opc_ua_label_ = new QLabel(u"OPC UA"_s, this);

    status_bar_bot_label_ = new QLabel(u"Телеграм Бот"_s, this);
    status_bar_bot_label_->setStyleSheet(u"background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';"_s);

    status_bar_message_label_ = new QLabel(this);

    ui->statusbar->addPermanentWidget(status_bar_opc_da_label_, 1);
    ui->statusbar->addPermanentWidget(status_bar_opc_ua_label_, 1);
    ui->statusbar->addPermanentWidget(status_bar_bot_label_, 1);
    ui->statusbar->addPermanentWidget(status_bar_message_label_, 3);

    setTrayIconActions();
    showTrayIcon();

    read_settings_();

    QObject::connect(source_data_manager_, &SourceDriverManager::sg_data_driver_status_changed, this, &MainWindow::sl_status_bar_opc_label_change_text);

    qInfo() << u"Инициализация Телеграмм Бота"_s;

    QObject::connect(tg_bot_manager_, &TgBotManager::sg_bot_thread_state_changed, this, &MainWindow::sl_status_bar_bot_label_change_text);
    QObject::connect(tg_bot_manager_, &TgBotManager::sg_restart_application_cmd, this, &MainWindow::sl_restart_app_cmd, Qt::QueuedConnection);
    QObject::connect(tg_bot_manager_, &TgBotManager::sg_bot_error, this, &MainWindow::sl_bot_error);

    source_data_manager_->SetPeriodReading(opc_period_reading_);

    if(opc_was_running_) {
        source_data_manager_->StartPeriodReading();
    }

    tg_bot_manager_->SetAutoRestartBot(tg_bot_auto_restart_);
    qInfo() << QString("Установлен автоматический рестарт телеграмм бота: %1").arg(tg_bot_auto_restart_ ? "ДА" : "НЕТ");

    OpcBrowseWidget* opc_browse_wdg = new OpcBrowseWidget(source_data_manager_, this);
    ui->swAppPages->addWidget(opc_browse_wdg);

    OPCValuesViewer* opc_viewer_wdg = new OPCValuesViewer(source_data_manager_, this);
    ui->swAppPages->addWidget(opc_viewer_wdg);
    QObject::connect(opc_browse_wdg, &OpcBrowseWidget::sg_send_message_to_console, opc_viewer_wdg, &OPCValuesViewer::sl_get_message_to_console);

    TgBotSettingsWidget* tg_settings_wdg = new TgBotSettingsWidget(&(*tg_bot_manager_), this);
    ui->swAppPages->addWidget(tg_settings_wdg);
    QObject::connect(tg_settings_wdg, &TgBotSettingsWidget::sg_change_auto_restart_app_checkbox, this, &MainWindow::sl_auto_restart_app_checkbox_changed);
    QObject::connect(tg_settings_wdg, &TgBotSettingsWidget::sg_change_start_app_mode_checkbox, this, &MainWindow::sl_start_app_mode_checkbox_changed);
    tg_settings_wdg->sl_set_autorestart_app_checkbox(app_auto_restart_);
    tg_settings_wdg->sl_set_start_app_on_tray_checkbox(start_app_on_tray_);

    TgBotConfigurationWidget* tg_config_wdg = new TgBotConfigurationWidget(tg_bot_manager_, this);
    ui->swAppPages->addWidget(tg_config_wdg);

    ui->frLeftMenuButtonsBar->setMaximumWidth(40);

    ui->swAppPages->setCurrentIndex(1);

    QObject::connect(opc_browse_wdg, &OpcBrowseWidget::sg_set_main_window_status_bar_message, this, &MainWindow::sl_status_bar_message_label_change_text);
    QObject::connect(opc_viewer_wdg, &OPCValuesViewer::sg_set_main_window_status_bar_message, this, &MainWindow::sl_status_bar_message_label_change_text);
    QObject::connect(ui->pbMainMenu, &QAbstractButton::clicked, this, &MainWindow::sl_pb_main_menu_clicked);
    QObject::connect(ui->pbCloseApp, &QAbstractButton::clicked, this, &MainWindow::sl_pb_close_app_clicked);
    QObject::connect(ui->pbOPCBrowsePage, &QAbstractButton::clicked, this, &MainWindow::sl_pb_opcbrowse_page_clicked);
    QObject::connect(ui->pbOPCManagePage, &QAbstractButton::clicked, this, &MainWindow::sl_pb_opcmanage_page_clicked);
    QObject::connect(ui->pbTGConfigPage, &QAbstractButton::clicked, this, &MainWindow::sl_pb_tgconfig_page_clicked);
    QObject::connect(ui->pbSaveDataFiles, &QAbstractButton::clicked, this, &MainWindow::sl_pb_savedatafiles_clicked);
    QObject::connect(ui->pbTgSettingsPage, &QAbstractButton::clicked, this, &MainWindow::sl_pb_tgsettings_page_clicked);

    {
        if(source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCDA)->PeriodicReadingOn()) {
            status_bar_opc_da_label_->setStyleSheet(u"background-color: #E2F3F0; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';"_s);
        } else {
            status_bar_opc_da_label_->setStyleSheet(u"background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';"_s);
        }
    }

    {
        if(source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCUA)->PeriodicReadingOn()) {
            status_bar_opc_ua_label_->setStyleSheet(u"background-color: #E2F3F0; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';"_s);
        } else {
            status_bar_opc_ua_label_->setStyleSheet(u"background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';"_s);
        }
    }

    ui->lbMainMenu1->setVisible(false);
    ui->lbMainMenu2->setVisible(false);
    ui->lbMainMenu3->setVisible(false);
    ui->lbMainMenu4->setVisible(false);
    ui->lbMainMenu5->setVisible(false);
    ui->lbMainMenu6->setVisible(false);
}

void MainWindow::showTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    QIcon trayImage(u":/img/TreeTelegram_bright.png"_s);
    trayIcon -> setIcon(trayImage);
    trayIcon->setToolTip(u"OPC DA Telegram bot \n Телеграм-бот c OPC DA"_s);
    trayIcon -> setContextMenu(trayIconMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);
    trayIcon -> show();
}

void MainWindow::trayActionExecute()
{
    showNormal();
    activateWindow();
}

void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
    //case QSystemTrayIcon::
        trayActionExecute();
        break;
    default:
        break;
    }
}

void MainWindow::setTrayIconActions()
{
    minimizeAction = new QAction(u"Свернуть"_s, this);
    restoreAction = new QAction(u"Восстановить"_s, this);
    quitAction = new QAction(u"Выход"_s, this);

    connect (minimizeAction, &QAction::triggered, this, &QWidget::hide);
    connect (restoreAction, &QAction::triggered, this, &QWidget::showNormal);
    connect (quitAction, &QAction::triggered, this, &MainWindow::sl_close_app_from_tray);

    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction (minimizeAction);
    trayIconMenu->addAction (restoreAction);
    trayIconMenu->addAction (quitAction);
}

void MainWindow::sl_close_app_from_tray() {
    bFirstClosed_ = true;
    close();
    qApp->exit(EXIT_CODE_USER_CMD);
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange)
    {
        if (isMinimized())
        {
            hide();

            if(!bFirstMinimized_) {
                QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(QSystemTrayIcon::Information);
                trayIcon->showMessage(u"OPC DA Telegram bot"_s, u"Приложение свернуто в трей и продолжает работать."_s, icon, 2000);
                qInfo() << u"Приложение свернуто в трей"_s;
                bFirstMinimized_ = true;
            }
        }
    }
}

void MainWindow::closeEvent(QCloseEvent * event)
{
    auto ev = event->type();
    if(isVisible() && ev == QEvent::Close){
        event->ignore();
        hide();

        if(!bFirstClosed_) {
            QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::MessageIcon(QSystemTrayIcon::Information);
            trayIcon->showMessage(u"OPC DA Telegram bot"_s, u"Приложение свернуто в трей и продолжает работать."_s, icon, 1000);
            qInfo() << u"Приложение свернуто в трей"_s;
            bFirstClosed_ = true;
        }
    }
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    int fh = ui->content->height() - 70 > 0 ? ui->content->height() - 70 : ui->content->height();
    ui->frLeftMenuButtonsBar->setFixedHeight(fh);
}

void MainWindow::showEvent(QShowEvent *ev)
{
    int fh = ui->content->height() - 70 > 0 ? ui->content->height() - 70 : ui->content->height();
    ui->frLeftMenuButtonsBar->setFixedHeight(fh);
}

void MainWindow::sl_pb_main_menu_clicked()
{
    QPropertyAnimation *animation = new QPropertyAnimation(ui->frLeftMenuButtonsBar, "maximumWidth");
    animation->setDuration(500);
    QObject::connect(animation, &QAbstractAnimation::finished, this, &MainWindow::sl_animation_main_menu_finished);

    int fh = ui->content->height() - 70 > 0 ? ui->content->height() - 70 : ui->content->height();
    ui->frLeftMenuButtonsBar->setFixedHeight(fh);

    if(ui->frLeftMenuButtonsBar->maximumWidth() > 40) {
        animation->setStartValue(220);
        animation->setEndValue(40);
    } else {
        animation->setStartValue(40);
        animation->setEndValue(220);
        ui->lbMainMenu1->setVisible(true);
        ui->lbMainMenu2->setVisible(true);
        ui->lbMainMenu3->setVisible(true);
        ui->lbMainMenu4->setVisible(true);
        ui->lbMainMenu5->setVisible(true);
        ui->lbMainMenu6->setVisible(true);
    }
    animation->start();
}

void MainWindow::sl_animation_main_menu_finished() {
    if(ui->frLeftMenuButtonsBar->maximumWidth() == 40) {
        ui->lbMainMenu1->setVisible(false);
        ui->lbMainMenu2->setVisible(false);
        ui->lbMainMenu3->setVisible(false);
        ui->lbMainMenu4->setVisible(false);
        ui->lbMainMenu5->setVisible(false);
        ui->lbMainMenu6->setVisible(false);
    }
}

void MainWindow::sl_pb_close_app_clicked()
{
    this->sl_close_app_from_tray();
}

void MainWindow::sl_pb_opcbrowse_page_clicked()
{
    ui->swAppPages->setCurrentIndex(0);
}

void MainWindow::sl_pb_opcmanage_page_clicked()
{
    ui->swAppPages->setCurrentIndex(1);
}


bool MainWindow::write_settings_to_file_(const QString& folder_path) const {
    QJsonObject temp_obj;
    bool opc_running = source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCDA)->PeriodicReadingOn() || source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCUA)->PeriodicReadingOn();
    temp_obj.insert("window_h", this->height());
    temp_obj.insert("window_w", this->width());
    temp_obj.insert("window_left", this->geometry().left());
    temp_obj.insert("window_top", this->geometry().top());
    temp_obj.insert("opc_running", opc_running);
    temp_obj.insert("opc_period_reading", source_data_manager_->GetPeriodReading());
    temp_obj.insert("auto_restart_bot", tg_bot_manager_->IsAutoRestart());
    temp_obj.insert("auto_restart_application", app_auto_restart_);
    temp_obj.insert("start_application_on_tray", start_app_on_tray_);

    QJsonDocument output_doc(temp_obj);

    QFile output_file(QString("%1/settings.json").arg(folder_path));
    if(output_file.open(QIODeviceBase::WriteOnly)) {
        output_file.write(output_doc.toJson());
        output_file.close();

        qInfo() << QString("Настройки приложения сохранены в settings.json");

        return true;
    }

    qCritical() << QString("Не удалось открыть settings.json для записи настроек");

    return false;
}

bool MainWindow::read_settings_() {

    QFile input_file("settings.json");
    if(!input_file.open(QIODeviceBase::ReadOnly)) {

        qCritical() << QString("Не найден файл настроек settings.json");

        return false;
    }

    QJsonParseError json_error;
    QJsonDocument input_doc = QJsonDocument::fromJson(input_file.readAll(), &json_error);

    if(json_error.error != QJsonParseError::NoError) {

        qCritical() << QString("Файл настроек settings.json поврежден. Ошибка парсинга: %1").arg(json_error.errorString());

        return false;
    }

    if(!(input_doc.object().contains("window_h") && input_doc.object().contains("window_w")
          && input_doc.object().contains("window_top") && input_doc.object().contains("window_left")
          && input_doc.object().contains("opc_period_reading")
          && input_doc.object().contains("opc_running") && input_doc.object().contains("auto_restart_bot"))) {

        qCritical() << QString("Файл настроек settings.json неверный формат");

        return false;
    }

    if(input_doc.object().value("window_h").isDouble() && input_doc.object().value("window_w").isDouble()
        && input_doc.object().value("window_top").isDouble() && input_doc.object().value("window_left").isDouble()
        && input_doc.object().value("window_h").toDouble() > 150 && input_doc.object().value("window_w").toDouble() > 200) {
        this->setGeometry(QRect(input_doc.object().value("window_left").toInt(), input_doc.object().value("window_top").toInt(),
                                input_doc.object().value("window_w").toInt(), input_doc.object().value("window_h").toInt()));
    } else {

        qCritical() << QString("Файл настроек settings.json ошибка парсинга параметров окна");

        return false;
    }

    if(input_doc.object().value("opc_period_reading").isDouble()) {
        opc_period_reading_ = input_doc.object().value("opc_period_reading").toInt();
    } else {

        qCritical() << QString("Файл настроек settings.json: нет периода чтения");

        return false;
    }

    if(input_doc.object().value("opc_running").isBool()) {
        opc_was_running_ = input_doc.object().value("opc_running").toBool();
        qInfo() << QString("Автоматический старт опроса OPC: %1.").arg(opc_was_running_ ? "ДА" : "НЕТ");
    } else {

        qWarning() << QString("Файл настроек settings.json: нет периода чтения OPC");

        return false;
    }

    if(input_doc.object().value("auto_restart_bot").isBool()) {
        tg_bot_auto_restart_ = input_doc.object().value("auto_restart_bot").toBool();
    } else {

        qWarning() << QString("Файл настроек settings.json: нет флага автостарта бота");

        return false;
    }

    if(input_doc.object().value("auto_restart_application").isBool()) {
        app_auto_restart_ = input_doc.object().value("auto_restart_application").toBool();
    } else {

        qWarning() << QString("Файл настроек settings.json: нет флага автоматического перезапуска приложения");

        return false;
    }

    if(input_doc.object().contains("start_application_on_tray") && input_doc.object().value("start_application_on_tray").isBool()) {
        start_app_on_tray_ = input_doc.object().value("start_application_on_tray").toBool();
    } else {

        qWarning() << QString("Файл настроек settings.json: нет флага старта приложения");

        return false;
    }

    return true;
}

void MainWindow::sl_status_bar_opc_label_change_text(DataTag::DataSource driver, bool is_connected) {
    QPalette palette;
    switch (driver) {
        using enum DataTag::DataSource;
    case OPCDA:
        if(source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCDA)->PeriodicReadingOn()) {
            status_bar_opc_da_label_->setStyleSheet("background-color: #E2F3F0; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");
        } else {
            status_bar_opc_da_label_->setStyleSheet("background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");
        }
        break;
    case OPCUA:
        if(source_data_manager_->GetDriverPtr(DataTag::DataSource::OPCUA)->PeriodicReadingOn()) {
            status_bar_opc_ua_label_->setStyleSheet("background-color: #E2F3F0; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");
        } else {
            status_bar_opc_ua_label_->setStyleSheet("background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");
        }
        break;
    default: return;
    }
}

void MainWindow::sl_status_bar_bot_label_change_text() {

if(tg_bot_manager_->BotIsWorking()) {
        status_bar_bot_label_->setStyleSheet("background-color: #E2F3F0; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");;
        qInfo() << QString("Старт Бота");
        bot_error_message_.reset();

    } else {
        status_bar_bot_label_->setStyleSheet("background-color: #FCE8E6; color: #2B2B2B; qproperty-alignment: 'AlignHCenter | AlignVCenter';");;
        qInfo() << QString("БОТ остановлен");

    }
}

void MainWindow::sl_status_bar_message_label_change_text(QString message) {
    status_bar_message_label_->setText(message);
}

void MainWindow::sl_bot_error(QString what) {
    bot_error_message_.reset(new QMessageBox(QMessageBox::Critical, "Ошибка!"
                                             , QString("Ошибка инициализации Бота, проверьте токен!\nПерезапустите приложение.\n %1").arg(what)
                                             , QMessageBox::Ok
                                             , nullptr
                                             , Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint));
    bot_error_message_->setWindowModality(Qt::NonModal);
    bot_error_message_->show();
}

void MainWindow::sl_pb_tgsettings_page_clicked()
{
    ui->swAppPages->setCurrentIndex(2);
}

void MainWindow::sl_pb_tgconfig_page_clicked()
{
    ui->swAppPages->setCurrentIndex(3);
}


void MainWindow::sl_pb_savedatafiles_clicked()
{
    QString folder_path = QFileDialog::getExistingDirectory(this, u"Выберите папку для сохранения"_s, QDir::currentPath());
    if(folder_path.size() == 0) {
        QMessageBox msgbox(QMessageBox::Information, u"Ошибка"_s
                           , u"Папка не выбрана, конфигурация не сохранена."_s
                           , QMessageBox::Ok);

        msgbox.exec();
        return;
    }

    QString message;
    message = tg_bot_manager_->SaveDataToJson(folder_path) ? QString("Конфигурация бота сохранена в папке %1").arg(folder_path)
                                                           : QString("Не удалось сохранить конфигурацию бота.");

    qInfo() << message;

    {
        QMessageBox msgbox(QMessageBox::Information, "Сообщение"
                           , message
                           , QMessageBox::Ok);

        msgbox.exec();
    }

    message = source_data_manager_->TagRegistry()->SaveDataToFile(folder_path) ? QString("Конфигурация OPC сохранена в папке %1").arg(folder_path)
                                                           : QString("Не удалось сохранить конфигурацию OPC.");

    qInfo() << message;

    {
        QMessageBox msgbox(QMessageBox::Information, "Сообщение"
                           , message
                           , QMessageBox::Ok);

        msgbox.exec();
    }

    bool b = write_settings_to_file_(folder_path);

    qInfo() << QString("Конфигурация приложения сохранена в папке %1 : %2").arg(folder_path, b ? "OK" : "ERROR");
}

void MainWindow::sl_restart_app_cmd(bool auto_restart)
{
    if(auto_restart && !app_auto_restart_) return;
    qInfo() << u"Перезапуск приложения."_s;
    close();
    qApp->exit(EXIT_CODE_REBOOT);
}

void MainWindow::sl_auto_restart_app_checkbox_changed(Qt::CheckState state)
{
    app_auto_restart_ = state == Qt::Checked;
}

void MainWindow::sl_start_app_mode_checkbox_changed(Qt::CheckState state)
{
    start_app_on_tray_ = state == Qt::Checked;
}

