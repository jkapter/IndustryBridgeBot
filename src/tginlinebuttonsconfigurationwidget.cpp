#include "tginlinebuttonsconfigurationwidget.h"
#include "ui_tginlinebuttonsconfigurationwidget.h"

#include "selectitemstablewigget.h"
#include "tgobjects/tgbotmanager.h"
#include "tgobjects/tgobject.h"

TGInlineButtonsConfigurationWidget::TGInlineButtonsConfigurationWidget(TgBotManager& tg_bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGInlineButtonsConfigurationWidget)
    , tg_bot_manager_(tg_bot_manager)
{
    ui->setupUi(this);

    message_to_button_ = new SelectItemsTableWidget(tg_bot_manager_, SITW_TYPE::Messages, 10, this);
    ui->gbInlineButtonsActions->layout()->addWidget(message_to_button_);
    message_to_button_->ResetContent();

    clear_button_data_();
}

TGInlineButtonsConfigurationWidget::~TGInlineButtonsConfigurationWidget()
{
    delete ui;
}

void TGInlineButtonsConfigurationWidget::sl_change_object_data(const std::string &current, const std::string &previous)
{
    if(!previous.empty()) {
        current_button_ = tg_bot_manager_.GetTGInlineButton(previous);
        save_to_current_button_();
    }
    load_data_from_button_(current);
}

void TGInlineButtonsConfigurationWidget::clear_button_data_()
{
    ui->leInlineButtonID->setText("");
    ui->leInlineButtonID->setEnabled(false);
    ui->leInlineButtonText->setText("");
    ui->leInlineButtonText->setEnabled(false);
    message_to_button_->ResetContent();
}

void TGInlineButtonsConfigurationWidget::save_to_current_button_()
{
    if(!current_button_) return;
    std::string prev = current_button_->GetId();

    if(tg_bot_manager_.CheckUniqueID(ui->leInlineButtonID->text().toStdString())) {
        current_button_->SetId(ui->leInlineButtonID->text().toStdString());
        tg_bot_manager_.UpdateTGObjectID(prev);
    }

    current_button_->SetButtonName(ui->leInlineButtonText->text().toStdString());
    message_to_button_->SetMessagesToCommand(current_button_);

    emit sg_tgobject_changed(current_button_->GetId(), prev);
}

void TGInlineButtonsConfigurationWidget::load_data_from_button_(const std::string &id)
{
    current_button_ = tg_bot_manager_.GetTGInlineButton(id);
    if(!current_button_) {
        clear_button_data_();
        return;
    }

    ui->leInlineButtonID->setText(QString::fromStdString(current_button_->GetId()));
    ui->leInlineButtonID->setEnabled(true);
    ui->leInlineButtonText->setText(QString::fromStdString(current_button_->GetButtonName()));
    ui->leInlineButtonText->setEnabled(true);

    message_to_button_->ReadCommandContent(current_button_);
}
