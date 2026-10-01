#include "selectitemstablewigget.h"

#include <QComboBox>
#include <QScrollBar>
#include <QHeaderView>

#include "tgobjects/tgobject.h"
#include "tgobjects/tgbotmanager.h"

SelectItemsTableWidget::SelectItemsTableWidget(TgBotManager &bot_manager, SITW_TYPE type, int rows_num, QWidget *parent)
    : QTableWidget(parent)
    , bot_manager_(bot_manager)
    , content_type_(type)
{
    setColumnCount(3);
    setHorizontalHeaderLabels({"Действие", "ID элемента", "Текст"});
    setRowCount(rows_num);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    setSelectionMode(QAbstractItemView::SingleSelection);

    switch(content_type_) {
    case SITW_TYPE::Messages:
        for(int i = 0; i < this->rowCount(); ++i) {
            QComboBox* cb_command = new QComboBox(this);
            cb_command->addItems({" - ", "Отправить сообщение", "Записать тэг"});
            QObject::connect(cb_command, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 0, cb_command);
            QComboBox* cb_messages = new QComboBox(this);
            cb_messages->setEnabled(false);
            QObject::connect(cb_messages, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 1, cb_messages);

            QTableWidgetItem* text_item = new QTableWidgetItem("");
            text_item->setFlags(Qt::NoItemFlags);
            setItem(i, 2, text_item);
        }
        break;
    case SITW_TYPE::InlineButtons:
        for(int i = 0; i < this->rowCount(); ++i) {
            QComboBox* cb_command = new QComboBox(this);
            cb_command->addItems({" - ", "Встроенная кнопка"});
            QObject::connect(cb_command, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 0, cb_command);
            QComboBox* cb_messages = new QComboBox(this);
            cb_messages->setEnabled(false);
            QObject::connect(cb_messages, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 1, cb_messages);

            QTableWidgetItem* text_item = new QTableWidgetItem("");
            text_item->setFlags(Qt::NoItemFlags);
            setItem(i, 2, text_item);
        }
        break;
    case SITW_TYPE::WaitAnswerTag:
        for(int i = 0; i < this->rowCount(); ++i) {
            QComboBox* cb_command = new QComboBox(this);
            cb_command->addItems({" - ", "Записать ответ в тэг"});
            QObject::connect(cb_command, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 0, cb_command);
            QComboBox* cb_messages = new QComboBox(this);
            cb_messages->setEnabled(false);
            QObject::connect(cb_messages, &QComboBox::currentIndexChanged, this, &SelectItemsTableWidget::sl_table_item_changed);
            setCellWidget(i, 1, cb_messages);

            QTableWidgetItem* text_item = new QTableWidgetItem("");
            text_item->setFlags(Qt::NoItemFlags);
            setItem(i, 2, text_item);
        }
        break;
    }
}

void SelectItemsTableWidget::sl_table_item_changed(int cb_index)
{
    switch(content_type_) {
    case SITW_TYPE::Messages:      update_content_messages_(); break;
    case SITW_TYPE::InlineButtons: update_content_buttons_(); break;
    case SITW_TYPE::WaitAnswerTag: update_content_wait_answer_tag_(); break;
    }
}

void SelectItemsTableWidget::ReadCommandContent(const TGTrigger* command)
{
    if(content_type_ != SITW_TYPE::Messages) return;
    update_content_messages_();
    ResetContent();
    int row_index = 0;
    if(!command) return;

    for(const auto& mes_ptr: command->GetMessages()) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(row_index, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(row_index, 1));
        if(!cb_com || !cb_mes) return;
        cb_com->setCurrentIndex(1);
        cb_mes->setCurrentText(QString::fromStdString(mes_ptr->GetId()));
        item(row_index, 2)->setText(QString::fromStdString(mes_ptr->GetText()));
        ++row_index;
    }

    for(const auto & [id, val]: command->GetIdTagsWSetValues()) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(row_index, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(row_index, 1));
        if(!cb_com || !cb_mes || !bot_manager_.GetTGParent()->TagManager()) return;
        cb_com->setCurrentIndex(2);
        cb_mes->setCurrentText(QString("%1: %2").arg(id).arg(bot_manager_.GetTGParent()->TagManager()->GetTagOfId(id)->GetTagName()));
        item(row_index, 2)->setText(DATATAG::toString(val));
        item(row_index, 2)->setTextAlignment(Qt::AlignCenter);
        item(row_index, 2)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        ++row_index;
    }
}

void SelectItemsTableWidget::SetMessagesToCommand(TGTrigger* command)
{
    if(!command) return;
    if(content_type_ != SITW_TYPE::Messages) return;
    command->ClearMessages();
    command->ClearTagsToWrite();
    for(int i = 0; i < rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(i, 1));
        if(!cb_com || !cb_mes) continue;
        if(cb_com->currentIndex() == 1) {
            std::string id = cb_mes->currentText().toStdString();
            auto mes = bot_manager_.GetTGMessage(id);
            if(mes) command->AddTGMessage(mes);
        }
        if(cb_com->currentIndex() == 2) {
            QString id = cb_mes->currentText();
            bool b = false;

            size_t id_tag = id.left(id.indexOf(':')).toULongLong(&b);
            if(b) {
                command->AddDataTagWValue(id_tag, item(i, 2)->text());
            }
        }
    }
}

void SelectItemsTableWidget::ReadMessageContent(const TGMessage *message)
{
    if(content_type_ != SITW_TYPE::InlineButtons) return;
    update_content_buttons_();
    ResetContent();
    int row_index = 0;
    if(!message) return;
    auto buttons = message->GetButtons();

    for(const auto& btn_ptr: buttons) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(row_index, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(row_index, 1));
        if(!cb_com || !cb_mes) return;
        cb_com->setCurrentIndex(1);
        cb_mes->setCurrentText(QString::fromStdString(btn_ptr->GetId()));
        item(row_index, 2)->setText(QString::fromStdString(btn_ptr->GetButtonName()));
        ++row_index;
    }
}

void SelectItemsTableWidget::ResetContent()
{
    for(int i = 0; i < rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(i, 1));
        if(!cb_com || !cb_mes) continue;
        cb_com->setCurrentIndex(0);
        cb_mes->setCurrentIndex(0);
        item(i, 2)->setText("");
        item(i, 2)->setTextAlignment(Qt::AlignLeft);
        item(i, 2)->setFlags(Qt::NoItemFlags);
    }
}

void SelectItemsTableWidget::SetButtonsToMessage(TGMessage *message)
{
    if(!message) return;
    if(content_type_ != SITW_TYPE::InlineButtons) return;
    message->ClearInlineButtons();
    for(int i = 0; i < rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(i, 1));
        if(!cb_com || !cb_mes) continue;
        if(cb_com->currentIndex() == 1) {
            std::string id = cb_mes->currentText().toStdString();
            auto btn = bot_manager_.GetTGInlineButton(id);
            if(!btn) continue;
            message->AddTGInlineButton(btn);
        }
    }
}

void SelectItemsTableWidget::ReadWaitAnswerTagContent(const TGMessageWaitAnswer *message)
{
    if(content_type_ != SITW_TYPE::WaitAnswerTag) return;
    update_content_wait_answer_tag_();
    ResetContent();
    if(!message || !bot_manager_.GetTGParent()->TagManager()) return;

    size_t tag_id = message->GetDataTagID();
    auto tag_ptr = bot_manager_.GetTGParent()->TagManager()->GetTagOfId(tag_id);
    if(!tag_ptr) return;

    QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(0, 0));
    QComboBox* cb_tag = qobject_cast<QComboBox*>(cellWidget(0, 1));
    if(!cb_com || !cb_tag) return;
    cb_com->setCurrentIndex(1);
    cb_tag->setCurrentText(QString("%1: %2").arg(tag_id).arg(tag_ptr->GetTagName()));
}

void SelectItemsTableWidget::SetWaitAnswerTagToMessage(TGMessageWaitAnswer *message)
{
    if(!message) return;
    if(content_type_ != SITW_TYPE::WaitAnswerTag) return;

    QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(0, 0));
    QComboBox* cb_tag = qobject_cast<QComboBox*>(cellWidget(0, 1));
    if(!cb_com || !cb_tag) return;

    if(cb_com->currentIndex() == 1) {
        QString id_str = cb_tag->currentText();
        bool b = false;
        size_t tag_id = id_str.left(id_str.indexOf(':')).toULongLong(&b);
        if(b) {
            message->SetDataTag(tag_id);
        }
    } else {
        message->SetDataTag(0);
    }
}

void SelectItemsTableWidget::showEvent(QShowEvent *ev)
{
    switch(content_type_) {
    case SITW_TYPE::Messages:      update_content_messages_(); break;
    case SITW_TYPE::InlineButtons: update_content_buttons_(); break;
    case SITW_TYPE::WaitAnswerTag: update_content_wait_answer_tag_(); break;
    }
    set_column_width_();
}

void SelectItemsTableWidget::resizeEvent(QResizeEvent *ev)
{
    set_column_width_();
}

void SelectItemsTableWidget::set_column_width_()
{
    int w = width() - 1;
    if(verticalScrollBar() && verticalScrollBar()->isVisible()) {
        w -= verticalScrollBar()->width();
    }
    if(verticalHeader()) {
        w -= verticalHeader()->width();
    }
    setColumnWidth(0, w/5);
    setColumnWidth(1, 2*w/5);
    setColumnWidth(2, 2*w/5);
    update();
}

void SelectItemsTableWidget::update_content_messages_()
{
    for(int i = 0; i < this->rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(i, 1));
        const QSignalBlocker blocker_1(cb_mes);
        const QSignalBlocker blocker_2(cb_com);
        if(!cb_com || !cb_mes) return;

        if(currentRow() == i) {
            item(i, 2)->setText("");
        }

        switch(cb_com->currentIndex()) {
        case 0:
            cb_mes->setCurrentIndex(0);
            cb_mes->setEnabled(false);
            item(i, 2)->setText("");
            break;

        case 1:
            {
            std::string id = cb_mes->currentText().toStdString();
            cb_mes->clear();
            QStringList mes_ids;

            auto messages = bot_manager_.GetTGMessages();

            for(size_t j = 0; j < messages.size(); ++j) {
                mes_ids.push_back(QString::fromStdString(messages.at(j)->GetId()));
            }

            mes_ids.sort();
            mes_ids.push_front(" - ");

            cb_mes->addItems(mes_ids);
            cb_mes->setCurrentText(QString::fromStdString(id));
            cb_mes->setEnabled(true);
            auto mes = bot_manager_.GetTGMessage(id);
            if(!mes) break;
            item(i, 2)->setText(QString::fromStdString(mes->GetText()));
            item(i, 2)->setTextAlignment(Qt::AlignLeft);
            item(i, 2)->setFlags(Qt::NoItemFlags);
            }
            break;
        case 2:
            {
            QString tag_str = cb_mes->currentText();
            cb_mes->clear();
            QStringList tag_list;
            QMap<size_t, QString> tags_id_to_names;

            for(const auto& [id, tag_ptr]: bot_manager_.GetTGParent()->TagManager()->GetIdToTagsMap()) {
                tag_list.push_back(QString("%1: %2").arg(id).arg(tag_ptr->GetTagName()));
            }
            tag_list.push_front(" - ");

            cb_mes->addItems(tag_list);

            for(int i = 0; i < tag_list.size(); ++i) {
                cb_mes->setItemData(i, tag_list.at(i), Qt::ToolTipRole);
            }

            cb_mes->setCurrentText(tag_str);
            cb_mes->setEnabled(true);
            item(i, 2)->setTextAlignment(Qt::AlignCenter);
            item(i, 2)->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

            }
            break;
        }
    }
}

void SelectItemsTableWidget::update_content_buttons_()
{
    for(int i = 0; i < this->rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_mes = qobject_cast<QComboBox*>(cellWidget(i, 1));
        const QSignalBlocker blocker_1(cb_mes);
        const QSignalBlocker blocker_2(cb_com);
        if(!cb_com || !cb_mes) return;
        if(cb_com->currentIndex() == 0) {
            cb_mes->setCurrentIndex(0);
            cb_mes->setEnabled(false);
            item(i, 2)->setText("");
        } else {
            std::string id = cb_mes->currentText().toStdString();
            cb_mes->clear();
            QStringList mes_ids;
            mes_ids.push_back(" - ");
            auto buttons = bot_manager_.GetTGInlineButtons();
            std::sort(buttons.begin(), buttons.end());
            for(size_t j = 0; j < buttons.size(); ++j) {
                mes_ids.push_back(QString::fromStdString(buttons.at(j)->GetId()));
            }
            cb_mes->addItems(mes_ids);
            cb_mes->setCurrentText(QString::fromStdString(id));
            auto btn = bot_manager_.GetTGInlineButton(id);
            if(btn) {
                item(i, 2)->setText(QString::fromStdString(btn->GetButtonName()));
            }
            cb_mes->setEnabled(true);
        }
    }
}

void SelectItemsTableWidget::update_content_wait_answer_tag_()
{
    for(int i = 0; i < this->rowCount(); ++i) {
        QComboBox* cb_com = qobject_cast<QComboBox*>(cellWidget(i, 0));
        QComboBox* cb_tag = qobject_cast<QComboBox*>(cellWidget(i, 1));
        const QSignalBlocker blocker_1(cb_tag);
        const QSignalBlocker blocker_2(cb_com);
        if(!cb_com || !cb_tag) return;

        if(cb_com->currentIndex() == 0) {
            cb_tag->setCurrentIndex(0);
            cb_tag->setEnabled(false);
            item(i, 2)->setText("");
        } else {
            QString tag_str = cb_tag->currentText();
            cb_tag->clear();
            QStringList tag_list;
            tag_list.push_back(" - ");

            for(const auto& [id, tag_ptr]: bot_manager_.GetTGParent()->TagManager()->GetIdToTagsMap()) {
                tag_list.push_back(QString("%1: %2").arg(id).arg(tag_ptr->GetTagName()));
            }

            cb_tag->addItems(tag_list);
            cb_tag->setCurrentText(tag_str);
            cb_tag->setEnabled(true);
        }
    }
}
