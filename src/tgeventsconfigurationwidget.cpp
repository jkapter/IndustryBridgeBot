#include "tgeventsconfigurationwidget.h"
#include "ui_tgeventsconfigurationwidget.h"

#include "selectitemstablewigget.h"
#include "tgobjects/tgbotmanager.h"
#include "tgobjects/tgobject.h"
#include "datatag.h"

using namespace Qt::StringLiterals;

TGEventsConfigurationWidget::TGEventsConfigurationWidget(TgBotManager& tg_bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TGEventsConfigurationWidget)
    , tg_bot_manager_(tg_bot_manager)
{
    ui->setupUi(this);

    message_to_event_ = new SelectItemsTableWidget(tg_bot_manager_, SITW_TYPE::Messages, 10, this);
    message_to_scheduled_event_ = new SelectItemsTableWidget(tg_bot_manager_, SITW_TYPE::Messages, 10, this);
    ui->gbEventsActions->layout()->addWidget(message_to_event_);
    ui->gbScheduledEventsActions->layout()->addWidget(message_to_scheduled_event_);
    message_to_event_->ResetContent();
    message_to_scheduled_event_->ResetContent();

    QObject::connect(ui->twOPCTagsEvents, &QTableWidget::cellDoubleClicked, this, &TGEventsConfigurationWidget::sl_opc_table_messages_double_click);
    QObject::connect(ui->cbScheduledEventType, &QComboBox::currentIndexChanged, this, &TGEventsConfigurationWidget::sl_make_scheduled_events_layout);

    clear_event_data_();

}

TGEventsConfigurationWidget::~TGEventsConfigurationWidget()
{
    delete ui;
}

void TGEventsConfigurationWidget::sl_change_object_data(const std::string &current, const std::string &previous)
{
    if(!previous.empty()) {
        current_event_ = tg_bot_manager_.GetTGEvent(previous);
        current_scheduled_event_ = tg_bot_manager_.GetTGScheduledEvent(previous);

        if(current_event_) {
            save_to_current_event_();
        }
        if(current_scheduled_event_) {
            save_to_current_scheduled_event_();
        }
    }

    if(tg_bot_manager_.GetTGEvent(current)) {
        ui->swEventsData->setCurrentIndex(1);
        load_data_from_event_(current);
        return;
    }

    if(tg_bot_manager_.GetTGScheduledEvent(current)) {
        ui->swEventsData->setCurrentIndex(0);
        load_data_from_scheduled_event_(current);
        return;
    }

    clear_event_data_();
    clear_scheduled_event_data_();
}

void TGEventsConfigurationWidget::sl_opc_table_messages_double_click(int row, int column)
{
    QTableWidget* tbl = ui->twOPCTagsEvents;
    if(tbl->item(row, 0)) {
        ui->leTagNameEvent->setText(QString("%1: %2").arg(tbl->item(row,0)->text(), tbl->item(row, 1)->text()));
    }
    bool b = false;
    size_t tag_id = tbl->item(row, 0) ? tbl->item(row,0)->text().toInt(&b) : -1;
    std::shared_ptr<DataTag> opc_tag = nullptr;
    if(b) {
        opc_tag = tg_bot_manager_.GetTGParent()->TagManager()->GetTagOfId(tag_id);
    }

    ui->leTagCompareValue->setText(u"0"_s);
    ui->leTagCompareValueHysterezis->setText(u"0"_s);
    ui->leTagCompareValue->setValidator(get_event_values_validator(opc_tag.get()));
    ui->leTagCompareValueHysterezis->setValidator(get_event_values_validator(opc_tag.get()));
    load_data_from_event_(ui->leEventID->text().toStdString());
}

void TGEventsConfigurationWidget::clear_event_data_()
{
    ui->leEventID->setText("");
    ui->leEventID->setEnabled(false);
    ui->leTagCompareValue->setText("");
    ui->leTagCompareValue->setEnabled(false);
    ui->leTagCompareValueHysterezis->setText("");
    ui->leTagCompareValueHysterezis->setEnabled(false);
    ui->leTagNameEvent->setText("");
    ui->leTagNameEvent->setEnabled(false);
    ui->cbTGEventConfCompareType->setCurrentIndex(0);
    ui->cbTGEventConfCompareType->setEnabled(false);
    ui->cbTGEventConfUserLevel->setCurrentIndex(0);
    ui->cbTGEventConfUserLevel->setEnabled(false);

    message_to_event_->ResetContent();
    ui->twOPCTagsEvents->clearContents();
    ui->twOPCTagsEvents->update();
}

void TGEventsConfigurationWidget::clear_scheduled_event_data_()
{
    ui->leScheduledEventID->setText("");
    ui->leScheduledEventID->setEnabled(false);
    ui->cbScheduledEventDoW->setCurrentIndex(0);
    ui->cbScheduledEventDoW->setEnabled(false);
    ui->cbScheduledEventMonth->setCurrentIndex(0);
    ui->cbScheduledEventMonth->setEnabled(false);
    ui->cbScheduledEventType->setCurrentIndex(0);
    ui->cbScheduledEventType->setEnabled(false);
    ui->cbTGScheduledEventConfUserLevel->setCurrentIndex(0);
    ui->cbTGScheduledEventConfUserLevel->setEnabled(false);
    ui->sbScheduledEventDay->setValue(0);
    ui->sbScheduledEventDay->setEnabled(false);
    ui->sbScheduledEventHour->setValue(0);
    ui->sbScheduledEventHour->setEnabled(false);
    ui->sbScheduledEventMinute->setValue(0);
    ui->sbScheduledEventMinute->setEnabled(false);

    message_to_scheduled_event_->ResetContent();
}

void TGEventsConfigurationWidget::save_to_current_event_()
{
    if(!current_event_) return;
    std::string prev = current_event_->GetId();

    if(tg_bot_manager_.CheckUniqueID(ui->leEventID->text().toStdString())) {
        current_event_->SetId(ui->leEventID->text().toStdString());
        tg_bot_manager_.UpdateTGObjectID(prev);
    }

    QString tag_str = ui->leTagNameEvent->text();
    qsizetype id_ind = tag_str.indexOf(':');
    size_t tag_id = 0;
    bool b = false;
    if(id_ind > 0 && id_ind < tag_str.size()) {
        tag_str = tag_str.left(id_ind);
        tag_id = tag_str.toInt(&b);
    }
    std::shared_ptr<DataTag> opc_tag = nullptr;
    if(b) {
        opc_tag = tg_bot_manager_.GetTGParent()->TagManager()->GetTagOfId(tag_id);
    }

    if(opc_tag) {
        COMPARE_TYPE type;
        switch(ui->cbTGEventConfCompareType->currentIndex()) {
        case 0: type = COMPARE_TYPE::EQUAL; break;
        case 1: type = COMPARE_TYPE::NOT_EQUAL; break;
        case 2: type = COMPARE_TYPE::GREATER; break;
        case 3: type = COMPARE_TYPE::LESS; break;
        case 4: type = COMPARE_TYPE::CHANGED; break;
        default: type = COMPARE_TYPE::EQUAL;
        }

        USER_TYPE us_type;
        switch(ui->cbTGEventConfUserLevel->currentIndex()) {
        case 1: us_type = USER_TYPE::REGISTRD; break;
        case 2: us_type = USER_TYPE::CHANNEL; break;
        case 3: us_type = USER_TYPE::ADMIN; break;
        default: us_type = USER_TYPE::UNDEFINED;
        }
        current_event_->SetAuthorizationLevel(us_type);

        ValueVariant val, hyst;
        QString val_str = ui->leTagCompareValue->text();
        QString hyst_str = ui->leTagCompareValueHysterezis->text();

        if(opc_tag->ValueIsReal()) {
            val = val_str.toDouble();
            hyst = hyst_str.toDouble();
        }
        if(opc_tag->ValueIsInteger() || opc_tag->ValueIsBool() || opc_tag->ValueIsUnsignedInteger()) {
            val = val_str.toLongLong();
            hyst = hyst_str.toLongLong();
        }
        if(opc_tag->ValueIsString()) {
            val = val_str;
            hyst = hyst_str;
        }
        current_event_->SetTagTrigger(tag_id, type, val, hyst);
    }

    message_to_event_->SetMessagesToCommand(current_event_);
    emit sg_tgobject_changed(current_event_->GetId(), prev);
}

void TGEventsConfigurationWidget::save_to_current_scheduled_event_()
{
    if(!current_scheduled_event_) return;
    std::string prev = current_scheduled_event_->GetId();

    if(tg_bot_manager_.CheckUniqueID(ui->leScheduledEventID->text().toStdString())) {
        current_scheduled_event_->SetId(ui->leScheduledEventID->text().toStdString());
        tg_bot_manager_.UpdateTGObjectID(prev);
    }

    PeriodicalTask scheduled_task;
    switch(ui->cbScheduledEventType->currentIndex()) {
    case 0: //HOURLY
        scheduled_task.SetPeriod(PeriodicalTask::Period::HOURLY);
        scheduled_task.SetTime(QTime(0, ui->sbScheduledEventMinute->value()));
        break;
    case 1: //DAYLY
        scheduled_task.SetPeriod(PeriodicalTask::Period::DAILY);
        scheduled_task.SetTime(QTime(ui->sbScheduledEventHour->value(), ui->sbScheduledEventMinute->value()));
        break;
    case 2: //WEEKLY
        scheduled_task.SetPeriod(PeriodicalTask::Period::WEEKLY);
        scheduled_task.SetDayOfWeek(ui->cbScheduledEventDoW->currentIndex() + 1);
        scheduled_task.SetTime(QTime(ui->sbScheduledEventHour->value(), ui->sbScheduledEventMinute->value()));
        break;
    case 3: //MONTHLY
        scheduled_task.SetPeriod(PeriodicalTask::Period::MONTHLY);
        scheduled_task.SetTime(QTime(ui->sbScheduledEventHour->value(), ui->sbScheduledEventMinute->value()));
        scheduled_task.SetDate(QDate(QDateTime::currentDateTime().date().year(), 1, ui->sbScheduledEventDay->value()));
        break;
    case 4: //YEARLY
        scheduled_task.SetPeriod(PeriodicalTask::Period::YEARLY);
        scheduled_task.SetTime(QTime(ui->sbScheduledEventHour->value(), ui->sbScheduledEventMinute->value()));
        scheduled_task.SetDate(QDate(QDateTime::currentDateTime().date().year(), ui->cbScheduledEventMonth->currentIndex() + 1, ui->sbScheduledEventDay->value()));
        break;
    }

    current_scheduled_event_->SetTask(scheduled_task);

    USER_TYPE us_type;
    switch(ui->cbTGScheduledEventConfUserLevel->currentIndex()) {
    case 1: us_type = USER_TYPE::REGISTRD; break;
    case 2: us_type = USER_TYPE::CHANNEL; break;
    case 3: us_type = USER_TYPE::ADMIN; break;
    default: us_type = USER_TYPE::UNDEFINED;
    }
    current_scheduled_event_->SetAuthorizationLevel(us_type);
    message_to_scheduled_event_->SetMessagesToCommand(current_scheduled_event_);
    emit sg_tgobject_changed(current_scheduled_event_->GetId(), prev);
}

void TGEventsConfigurationWidget::load_data_from_event_(const std::string &id)
{
    current_event_ = tg_bot_manager_.GetTGEvent(id);
    if(!current_event_) {
        clear_event_data_();
        return;
    }

    ui->leEventID->setText(QString::fromStdString(current_event_->GetId()));
    ui->leEventID->setEnabled(true);
    ui->cbTGEventConfCompareType->setEnabled(true);
    ui->leTagCompareValue->setEnabled(true);
    ui->leTagCompareValueHysterezis->setEnabled(true);
    ui->cbTGEventConfUserLevel->setEnabled(true);

    auto [tag_id, type, ival, ihys] = current_event_->GetTagTrigger();
    if(tag_id > 0) {
        auto tag_ptr = tg_bot_manager_.GetTGParent()->TagManager()->GetTagOfId(tag_id);
        ui->leTagNameEvent->setText(QString("%1: %2").arg(tag_id).arg(tag_ptr->GetTagName()));

        int cb_index = 0;

        switch(type) {
        case COMPARE_TYPE::EQUAL: cb_index = 0; break;
        case COMPARE_TYPE::NOT_EQUAL: cb_index = 1; break;
        case COMPARE_TYPE::GREATER: cb_index = 2; break;
        case COMPARE_TYPE::LESS: cb_index = 3; break;
        case COMPARE_TYPE::CHANGED: cb_index = 4; break;
        }

        ui->leTagCompareValue->setEnabled(type != COMPARE_TYPE::CHANGED);
        ui->leTagCompareValueHysterezis->setEnabled(type != COMPARE_TYPE::CHANGED);

        ui->cbTGEventConfCompareType->setCurrentIndex(cb_index);

        ui->leTagCompareValue->setText(DATATAG::toString(ival));
        ui->leTagCompareValue->setValidator(get_event_values_validator(tag_ptr.get()));
        ui->leTagCompareValueHysterezis->setText(DATATAG::toString(ihys));
        ui->leTagCompareValueHysterezis->setValidator(get_event_values_validator(tag_ptr.get()));
    }

    int auth_ind;
    switch(current_event_->GetAuthorizationLevel()) {
    case USER_TYPE::ADMIN: auth_ind = 3; break;
    case USER_TYPE::CHANNEL: auth_ind = 2; break;
    case USER_TYPE::REGISTRD: auth_ind = 1; break;
    default: auth_ind = 0;
    }
    ui->cbTGEventConfUserLevel->setCurrentIndex(auth_ind);
    message_to_event_->ReadCommandContent(current_event_);
    fill_opc_tags_table_(ui->twOPCTagsEvents);
    opc_table_set_column_width_(ui->twOPCTagsEvents);
}

void TGEventsConfigurationWidget::load_data_from_scheduled_event_(const std::string &id)
{
    current_scheduled_event_ = tg_bot_manager_.GetTGScheduledEvent(id);
    if(!current_scheduled_event_) {
        clear_scheduled_event_data_();
        return;
    }

    ui->leScheduledEventID->setText(QString::fromStdString(current_scheduled_event_->GetId()));
    PeriodicalTask task = current_scheduled_event_->GetTask();
    auto [period, date, time, dow] = task.GetTask();

    sl_make_scheduled_events_layout(static_cast<int>(period));

    ui->leScheduledEventID->setEnabled(true);
    ui->cbScheduledEventType->setCurrentIndex(static_cast<int>(period));
    ui->cbScheduledEventType->setEnabled(true);
    ui->sbScheduledEventMinute->setValue(time.minute());
    ui->sbScheduledEventHour->setValue(time.hour());
    ui->cbScheduledEventDoW->setCurrentIndex(dow - 1);
    ui->sbScheduledEventDay->setValue(date.day());
    ui->cbScheduledEventMonth->setCurrentIndex(date.month() - 1);

    int auth_ind;
    switch(current_scheduled_event_->GetAuthorizationLevel()) {
    case USER_TYPE::ADMIN: auth_ind = 3; break;
    case USER_TYPE::CHANNEL: auth_ind = 2; break;
    case USER_TYPE::REGISTRD: auth_ind = 1; break;
    default: auth_ind = 0;
    }
    ui->cbTGScheduledEventConfUserLevel->setCurrentIndex(auth_ind);
    ui->cbTGScheduledEventConfUserLevel->setEnabled(true);
    message_to_scheduled_event_->ReadCommandContent(current_scheduled_event_);
}

void TGEventsConfigurationWidget::showEvent(QShowEvent *event)
{
    fill_opc_tags_table_(ui->twOPCTagsEvents);
    opc_table_set_column_width_(ui->twOPCTagsEvents);
    sl_make_scheduled_events_layout(0);
}

void TGEventsConfigurationWidget::resizeEvent(QResizeEvent *event)
{
    opc_table_set_column_width_(ui->twOPCTagsEvents);
}

void TGEventsConfigurationWidget::fill_opc_tags_table_(QTableWidget* tbl) {
    auto map_id_to_tags = tg_bot_manager_.GetTGParent()->TagManager()->GetIdToTagsMap();

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

void TGEventsConfigurationWidget::opc_table_set_column_width_(QTableWidget* tbl) {
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

QValidator* TGEventsConfigurationWidget::get_event_values_validator(DataTag* tag)
{
    if(!tag) return nullptr;
    if(tag->ValueIsReal()) return new QDoubleValidator(-10000.0, 10000.0, 1);
    if(tag->ValueIsInteger()) return new QIntValidator(-10000, 10000);
    if(tag->ValueIsUnsignedInteger()) return new QIntValidator(0, 10000);
    if(tag->ValueIsBool()) return new QIntValidator(0,1);
    return nullptr;
}

void TGEventsConfigurationWidget::sl_make_scheduled_events_layout(int index)
{
    /* INDEX:
     * 0 = HOURLY
     * 1 = DAYLY
     * 2 = WEEKLY
     * 3 = MONTHLY
     * 4 = YEARLY
     */

    ui->lbScheduledEventMinute->setVisible(true);
    ui->lbScheduledEventMinute->setEnabled(true);
    ui->sbScheduledEventMinute->setVisible(true);
    ui->sbScheduledEventMinute->setEnabled(true);

    ui->lbScheduledEventHour->setVisible(index > 0);
    ui->sbScheduledEventHour->setVisible(index > 0);
    ui->lbScheduledEventHour->setEnabled(index > 0);
    ui->sbScheduledEventHour->setEnabled(index > 0);

    ui->lbScheduledEventDay->setVisible(index > 2);
    ui->sbScheduledEventDay->setVisible(index > 2);
    ui->lbScheduledEventDay->setEnabled(index > 2);
    ui->sbScheduledEventDay->setEnabled(index > 2);

    ui->lbScheduledEventDoW->setVisible(index == 2);
    ui->cbScheduledEventDoW->setVisible(index == 2);
    ui->lbScheduledEventDoW->setEnabled(index == 2);
    ui->cbScheduledEventDoW->setEnabled(index == 2);

    ui->lbScheduledEventMonth->setVisible(index > 3);
    ui->cbScheduledEventMonth->setVisible(index > 3);
    ui->lbScheduledEventMonth->setEnabled(index > 3);
    ui->cbScheduledEventMonth->setEnabled(index > 3);

    ui->swEventsData->currentWidget()->update();
}
