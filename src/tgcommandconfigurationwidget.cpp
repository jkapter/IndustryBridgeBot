#include "tgcommandconfigurationwidget.h"
#include "ui_tgcommandconfigurationwidget.h"

#include "selectitemstablewigget.h"
#include "tgobjects/tgbotmanager.h"
#include "tgobjects/tgobject.h"

TGCommandConfigurationWidget::TGCommandConfigurationWidget(TgBotManager& tg_bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGCommandConfigurationWidget)
    , tg_bot_manager_(tg_bot_manager)
{
    ui->setupUi(this);

    message_to_command_ = new SelectItemsTableWidget(tg_bot_manager_, SITW_TYPE::Messages, 10, this);
    ui->gbCommandsActions->layout()->addWidget(message_to_command_);
    message_to_command_->ResetContent();

    clear_command_data_();
}

TGCommandConfigurationWidget::~TGCommandConfigurationWidget()
{
    delete ui;
}

void TGCommandConfigurationWidget::sl_change_object_data(const std::string &current, const std::string &previous)
{
    if(!previous.empty()) {
        current_command_ = tg_bot_manager_.GetTGCommand(previous);
        save_to_current_command_();
    }
    load_data_from_command_(current);
}

void TGCommandConfigurationWidget::clear_command_data_()
{
    ui->leCommandID->setText("");
    ui->leCommandID->setEnabled(false);
    ui->leCommandText->setText("");
    ui->leCommandText->setEnabled(false);
    ui->ptCommandHelp->setPlainText("");
    ui->ptCommandHelp->setEnabled(false);
    message_to_command_->ResetContent();
}

void TGCommandConfigurationWidget::save_to_current_command_()
{
    if(!current_command_) return;
    std::string prev = current_command_->GetId();

    if(tg_bot_manager_.CheckUniqueID(ui->leCommandID->text().toStdString())) {
        current_command_->SetId(ui->leCommandID->text().toStdString());
        tg_bot_manager_.UpdateTGObjectID(prev);
    }

    current_command_->SetCommand(ui->leCommandText->text().toStdString(), ui->ptCommandHelp->toPlainText().toStdString());
    message_to_command_->SetMessagesToCommand(current_command_);

    emit sg_tgobject_changed(current_command_->GetId(), prev);
}

void TGCommandConfigurationWidget::load_data_from_command_(const std::string &id)
{
    current_command_ = tg_bot_manager_.GetTGCommand(id);
    if(!current_command_) {
        clear_command_data_();
        return;
    }

    ui->leCommandID->setText(QString::fromStdString(current_command_->GetId()));
    ui->leCommandID->setEnabled(true);
    auto [com, description] = current_command_->GetCommandAndDescription();
    ui->leCommandText->setText(QString::fromStdString(std::move(com)));
    ui->leCommandText->setEnabled(true);
    ui->ptCommandHelp->setPlainText(QString::fromStdString(std::move(description)));
    ui->ptCommandHelp->setEnabled(true);

    if(current_command_->HasMessages()) {
        message_to_command_->ReadCommandContent(current_command_);
    } else {
        message_to_command_->ResetContent();
    }
}
