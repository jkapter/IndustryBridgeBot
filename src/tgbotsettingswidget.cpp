#include "tgbotsettingswidget.h"
#include "ui_tgbotsettingswidget.h"

#include <QMessageBox>

#include "hintinputdialog.h"
#include "plaintextconsole.h"
#include "tgobjects/tgbotmanager.h"

using namespace Qt::StringLiterals;

TgBotSettingsWidget::TgBotSettingsWidget(TgBotManager* bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TgBotSettingsWidget)
    , bot_manager_(bot_manager)
{
    ui->setupUi(this);

    console_ = new PlainTextConsole(this);
    console_->setMaximumBlockCount(100);
    console_->AddDTToMessage(true);
    ui->frConsoleOutput->setLayout(new QVBoxLayout());
    ui->frConsoleOutput->layout()->addWidget(console_);

    QObject::connect(bot_manager_, &TgBotManager::sg_bot_thread_state_changed, this, &TgBotSettingsWidget::sl_bot_status_changed);
    QObject::connect(bot_manager_, &TgBotManager::sg_send_message_to_console, this, &TgBotSettingsWidget::sl_console_output_message);
    QObject::connect(ui->cbRestartAppOnError, &QCheckBox::checkStateChanged, this, &TgBotSettingsWidget::sg_change_auto_restart_app_checkbox);
    QObject::connect(ui->cbStartAppHiddenTray, &QCheckBox::checkStateChanged, this, &TgBotSettingsWidget::sg_change_start_app_mode_checkbox);
    QObject::connect(ui->pbStartBot, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_pb_startbot_clicked);
    QObject::connect(ui->pbStopBot, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_pb_stopbot_clicked);
    QObject::connect(ui->cbAutoRestartbot, &QCheckBox::checkStateChanged, this, &TgBotSettingsWidget::sl_cb_autorestartbot_statechanged);
    QObject::connect(ui->tbRefreshUsers, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_refreshusers_clicked);
    QObject::connect(ui->tbVerifyUser, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_verifyuser_clicked);
    QObject::connect(ui->tbSetAdmin, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_setadmin_clicked);
    QObject::connect(ui->tbResetAdmin, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_resetadmin_clicked);
    QObject::connect(ui->tbSendMessage, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_sendmessage_clicked);
    QObject::connect(ui->tbSendToAll, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_sendtoall_clicked);
    QObject::connect(ui->tbBanUser, &QAbstractButton::clicked, this, &TgBotSettingsWidget::sl_tb_banuser_clicked);
    QObject::connect(ui->tabBotSettings, &QTabWidget::currentChanged, this, &TgBotSettingsWidget::sl_tab_botsettings_current_changed);
    QObject::connect(ui->cbAddBotNameToChannelMessages, &QCheckBox::checkStateChanged, this, &TgBotSettingsWidget::sl_cb_addbotnametochannelmessages_check_state_changed);

    ui->tabBotSettings->setCurrentIndex(0);
    ui->cbAutoRestartbot->setChecked(bot_manager_->IsAutoRestart());
}

TgBotSettingsWidget::~TgBotSettingsWidget()
{
    delete ui;
    ui = nullptr;
}

void TgBotSettingsWidget::sl_set_autorestart_app_checkbox(bool b)
{
    ui->cbRestartAppOnError->setChecked(b);
}

void TgBotSettingsWidget::sl_set_start_app_on_tray_checkbox(bool b)
{
    ui->cbStartAppHiddenTray->setChecked(b);
}

void TgBotSettingsWidget::resizeEvent(QResizeEvent* event) {
    users_table_update_();
}

void TgBotSettingsWidget::showEvent(QShowEvent *event) {
    sl_bot_status_changed();
    users_table_update_();
    ui->cbAutoRestartbot->setChecked(bot_manager_->IsAutoRestart());
    ui->leBotNameForChannel->setText(bot_manager_->GetBotNameForChannels());
    if(ui->leBotNameForChannel->text().length() > 0) {
        ui->cbAddBotNameToChannelMessages->setChecked(true);
    }
}

void TgBotSettingsWidget::sl_bot_status_changed() {
    if(!ui) return;
    if(bot_manager_->BotIsWorking()) {
        ui->lbBotStatus->setText(u"Бот запущен"_s);
        ui->lbBotStatus->setStyleSheet(u"color: white; background-color: darkgreen; border: 2px solid #519999; border-radius: 6px; padding: 5px;"_s);
    } else {
        ui->lbBotStatus->setText("Бот остановлен");
        ui->lbBotStatus->setStyleSheet(u"color: white; background-color: darkred; border: 2px solid #519999; border-radius: 6px; padding: 5px;"_s);
    }
}

void TgBotSettingsWidget::sl_pb_startbot_clicked()
{
    *console_ << "Бот: команда старт";
    bot_manager_->StartBot();
}

void TgBotSettingsWidget::sl_pb_stopbot_clicked()
{
    *console_ << "Бот: команда стоп";
    bot_manager_->StopBot();
}

void TgBotSettingsWidget::sl_console_output_message(QString mes) {
    *console_ << mes;
}


void TgBotSettingsWidget::sl_cb_autorestartbot_statechanged(int arg1)
{
    bool st = ui->cbAutoRestartbot->isChecked();
    bot_manager_->SetAutoRestartBot(st);
}

void TgBotSettingsWidget::users_table_update_() {
    QTableWidget* tbl = ui->tblUsers;
    size_t w = tbl->width() - 25;
    for(int i = 0; i < 6; ++i) {
        tbl->setColumnWidth(i, 100);
    }
    tbl->setColumnWidth(1, 80);
    if(w < 700) {
        tbl->setColumnWidth(6, 100);
    } else {
        tbl->setColumnWidth(6, w - 580);
    }
    auto users_data = bot_manager_->GetUsers();
    tbl->clearContents();
    tbl->setRowCount(users_data.size());
    size_t r = 0;
    for(const auto& user: users_data) {
        QString user_type;
        switch(user->type) {
        case USER_TYPE::ADMIN       : user_type = QString("Администратор"); break;
        case USER_TYPE::REGISTRD    : user_type = QString("Зарегистрированный"); break;
        case USER_TYPE::BANNED      : user_type = QString("Забанен"); break;
        case USER_TYPE::CHANNEL     : user_type = QString("Канал"); break;
        default                     : user_type = QString("");
        }
        tbl->setItem(r, 0, new QTableWidgetItem(QString::number(user->id)));
        QWidget* chb_item = new QWidget(tbl);
        QHBoxLayout* hbox_la = new QHBoxLayout(chb_item);
        QCheckBox* cb_active = new QCheckBox(chb_item);
        hbox_la->addWidget(cb_active);
        hbox_la->setAlignment(Qt::AlignCenter);
        hbox_la->setContentsMargins(0, 0, 0, 0);
        chb_item->setLayout(hbox_la);
        cb_active->setChecked(user->userActive);
        cb_active->setEnabled(false);
        tbl->setCellWidget(r, 1, chb_item);
        tbl->setItem(r, 2, new QTableWidgetItem(QString::fromStdString(user->username)));
        tbl->setItem(r, 3, new QTableWidgetItem(QString::fromStdString(user->firstName)));
        tbl->setItem(r, 4, new QTableWidgetItem(QString::fromStdString(user->lastName)));
        tbl->setItem(r, 5, new QTableWidgetItem(user_type));
        tbl->setItem(r, 6, new QTableWidgetItem(user->lastMessageDT.toString("dd.MM.yyyy hh:mm:ss")));
        ++r;
    }
}

void TgBotSettingsWidget::sl_tb_refreshusers_clicked()
{
    users_table_update_();
}


void TgBotSettingsWidget::sl_tb_verifyuser_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не выбран"_s);
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, u"Информация"_s, u"ID пользователя неверный"_s);
        return;
    }

    if(bot_manager_->RegisterUser(id)) {
        users_table_update_();
    } else {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не найден"_s);
    }
}


void TgBotSettingsWidget::sl_tb_setadmin_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не выбран"_s);
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, u"Информация"_s, u"ID пользователя неверный"_s);
        return;
    }

    if(bot_manager_->SetAdmin(id)) {
        users_table_update_();
    } else {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не найден"_s);
    }
}


void TgBotSettingsWidget::sl_tb_resetadmin_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не выбран"_s);
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, u"Информация"_s, u"ID пользователя неверный"_s);
        return;
    }

    if(bot_manager_->ResetAdmin(id)) {
        users_table_update_();
    } else {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не найден"_s);
    }
}

void TgBotSettingsWidget::show_message_input_dialog_() {
    QString help_text{"*bold text*\n_italic text_\n__underline__\n~strikethrough~\n||spoiler||\n[inline URL](http://www.example.com/)\n[inline mention of a user](tg://user?id=123456789)\n![👍](tg://emoji?id=5368324170671202286)\n"};
    HintInputDialog* input_dialog = new HintInputDialog(help_text, this);
    input_dialog->setWindowFlag(Qt::MSWindowsFixedSizeDialogHint);
    input_dialog->setWindowModality(Qt::WindowModality::WindowModal);
    input_dialog->setWindowTitle(tr("Отправить сообщение пользователю"));
    input_dialog->setGeometry(this->pos().rx() + 200, this->pos().ry() + 100, 400, 500);
    QObject::connect(input_dialog, &HintInputDialog::sg_applied, this, &TgBotSettingsWidget::sl_get_text_message_to_clients);
    input_dialog->show();
}

void TgBotSettingsWidget::sl_tb_sendmessage_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не выбран"_s);
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, u"Информация"_s, u"ID пользователя неверный"_s);
        return;
    }

    temp_clients_.clear();
    temp_clients_.push_back(id);

    show_message_input_dialog_();
}

void TgBotSettingsWidget::sl_get_text_message_to_clients(QString message) {
    bot_manager_->SendMessageToUsers(temp_clients_, message);
}


void TgBotSettingsWidget::sl_tb_sendtoall_clicked()
{
    temp_clients_.clear();
    for(const auto& it: bot_manager_->GetUsers()) {
        if(it->type != USER_TYPE::BANNED) {
            temp_clients_.push_back(it->id);
        }
    }
    show_message_input_dialog_();
}


void TgBotSettingsWidget::sl_tb_banuser_clicked()
{
    auto sel_ranges = ui->tblUsers->selectedRanges();
    if(sel_ranges.isEmpty()) {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не выбран"_s);
        return;
    }

    size_t id;
    bool b;
    id = ui->tblUsers->item(sel_ranges.at(0).topRow(), 0)->text().toLongLong(&b);

    if(!b) {
        QMessageBox::information(this, u"Информация"_s, u"ID пользователя неверный"_s);
        return;
    }

    if(bot_manager_->BanUser(id)) {
        users_table_update_();
    } else {
        QMessageBox::information(this, u"Информация"_s, u"Пользователь не найден"_s);
    }
}


void TgBotSettingsWidget::sl_tab_botsettings_current_changed(int index)
{
    if(index == 1) {
        users_table_update_();
    }
    if(index == 0) {
        ui->cbAutoRestartbot->setChecked(bot_manager_->IsAutoRestart());
    }
}

void TgBotSettingsWidget::sl_cb_addbotnametochannelmessages_check_state_changed(const Qt::CheckState &arg1)
{
    if(!bot_manager_) return;
    if(arg1 == Qt::Checked) {
        bot_manager_->SetBotNameForChannels(ui->leBotNameForChannel->text());
    } else {
        bot_manager_->SetBotNameForChannels("");
    }
}

