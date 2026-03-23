#include "tgmessageconfigurationwidget.h"
#include "ui_tgmessageconfigurationwidget.h"

#include "selectitemstablewigget.h"
#include "datatag.h"
#include "tgbotmanager.h"
#include "tgobject.h"

TGMessageConfigurationWidget::TGMessageConfigurationWidget(TgBotManager &tg_bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGMessageConfigurationWidget)
    , tg_bot_manager_(tg_bot_manager)
{
    ui->setupUi(this);

    inline_buttons_to_message_ = new SelectItemsTableWidget(tg_bot_manager_, SITW_TYPE::InlineButtons, 10, this);
    ui->gbInlineButtons->layout()->addWidget(inline_buttons_to_message_);
    inline_buttons_to_message_->ResetContent();

    QObject::connect(ui->twOPCTagsList, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(sl_opc_table_messages_double_click(int,int)));

    clear_message_data_();
}

TGMessageConfigurationWidget::~TGMessageConfigurationWidget()
{
    delete ui;
}

void TGMessageConfigurationWidget::sl_change_object_data(const std::string &current, const std::string &previous)
{
    if(!previous.empty()) {
        current_message_ = tg_bot_manager_.GetTGMessage(previous);
        save_to_current_message_();
    }

    load_data_from_message_(current);
}

void TGMessageConfigurationWidget::showEvent(QShowEvent *event)
{
    opc_table_set_column_width_(ui->twOPCTagsList);
}

void TGMessageConfigurationWidget::resizeEvent(QResizeEvent *event)
{
    opc_table_set_column_width_(ui->twOPCTagsList);
}

void TGMessageConfigurationWidget::fill_opc_tags_table_(QTableWidget* tbl) {
    auto  map_id_to_tags = tg_bot_manager_.GetTGParent()->TagManager()->GetIdToTagsMap();

    tbl->setColumnCount(3);
    tbl->setRowCount(map_id_to_tags.size());
    tbl->setHorizontalHeaderLabels({"ID", "Имя тэга", "Тип"});
    tbl->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter | (Qt::Alignment)Qt::TextWordWrap);
    tbl->verticalHeader()->setVisible(false);
    tbl->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tbl->setWordWrap(true);
    tbl->setSelectionBehavior(QAbstractItemView::SelectRows);

    opc_table_set_column_width_(tbl);
    int r = 0;
    for(const auto& [id, tag]: map_id_to_tags) {
        QTableWidgetItem* it_id, *it_name;
        it_id = new QTableWidgetItem(QString::number(id));
        it_id->setFlags(it_id->flags().setFlag(Qt::ItemIsSelectable, true).setFlag(Qt::ItemIsEditable, false));
        it_id->setTextAlignment(Qt::AlignHCenter);
        it_name = new QTableWidgetItem(tag->GetTagName());
        it_name->setFlags(it_id->flags().setFlag(Qt::ItemIsSelectable, true).setFlag(Qt::ItemIsEditable, false));
        it_name->setTextAlignment(Qt::AlignLeft);
        tbl->setItem(r, 0, it_id);
        tbl->setItem(r, 1, it_name);
        tbl->setItem(r, 2, new QTableWidgetItem(*it_id));
        tbl->item(r, 2)->setText(tag->GetStringType());
        ++r;
    }
}

void TGMessageConfigurationWidget::opc_table_set_column_width_(QTableWidget* tbl) {
    if(!tbl) return;
    int sum_width = tbl->width();
    int w1, w2, w3;
    if(sum_width >= 300) {
        w1 = (20*sum_width) / 100;
        w2 = (60*sum_width) / 100;
        w3 = (20*sum_width) / 100;
    } else {
        w1 = 20;
        w2 = 150;
        w3 = (sum_width - 170);
    }
    tbl->setColumnWidth(0, w1);
    tbl->setColumnWidth(1, w2);
    tbl->setColumnWidth(2, w3 - 2);
}

void TGMessageConfigurationWidget::clear_message_data_()
{
    ui->leMessageID->setText("");
    ui->leMessageID->setEnabled(false);
    ui->ptMessage->setPlainText("");
    ui->ptMessage->setEnabled(false);
    inline_buttons_to_message_->ResetContent();
    ui->twOPCTagsList->clearContents();
    ui->twOPCTagsList->update();
}

void TGMessageConfigurationWidget::save_to_current_message_()
{
    if(!current_message_) return;
    std::string prev = current_message_->GetId();

    if(tg_bot_manager_.CheckUniqueID(ui->leMessageID->text().toStdString())) {
        current_message_->SetId(ui->leMessageID->text().toStdString());
        tg_bot_manager_.UpdateTGObjectID(prev);
    }

    current_message_->SetText(ui->ptMessage->toPlainText().toStdString());
    inline_buttons_to_message_->SetButtonsToMessage(current_message_);

    emit sg_tgobject_changed(current_message_->GetId(), prev);
}

void TGMessageConfigurationWidget::load_data_from_message_(const std::string &id)
{
    current_message_ = tg_bot_manager_.GetTGMessage(id);
    if(!current_message_) {
        clear_message_data_();
        return;
    }

    ui->leMessageID->setText(QString::fromStdString(current_message_->GetId()));
    ui->leMessageID->setEnabled(true);
    ui->ptMessage->setPlainText(QString::fromStdString(current_message_->GetText()));
    ui->ptMessage->setEnabled(true);
    fill_opc_tags_table_(ui->twOPCTagsList);

    if(current_message_->HasTags()) {
        for(const auto& tag_id: current_message_->GetTagIDs()) {
            const auto matched_items = ui->twOPCTagsList->findItems(QString::number(tag_id), Qt::MatchExactly);
            for(const auto& item: matched_items) {
                for(auto c = 0; c < ui->twOPCTagsList->columnCount(); ++c) {
                    if(ui->twOPCTagsList->item(item->row(), c))  {
                        ui->twOPCTagsList->item(item->row(), c)->setBackground(Qt::yellow);
                    }
                }
            }
        }
    }

    if(current_message_->HasButtons()) {
        inline_buttons_to_message_->ReadMessageContent(current_message_);
    } else {
        inline_buttons_to_message_->ResetContent();
    }
}

void TGMessageConfigurationWidget::sl_opc_table_messages_double_click(int row, int column)
{
    QTableWidget* tbl = ui->twOPCTagsList;
    if(tbl->item(row, 0)) {
        QString mes_text = ui->ptMessage->toPlainText();
        ui->ptMessage->setPlainText(QString("%1#TAG:%2#").arg(mes_text, tbl->item(row, 0)->text()));
    }
}
