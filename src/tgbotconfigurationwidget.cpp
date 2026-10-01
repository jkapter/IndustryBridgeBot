#include "tgbotconfigurationwidget.h"
#include "ui_tgbotconfigurationwidget.h"

#include "tgmessageconfigurationwidget.h"
#include "tgcommandconfigurationwidget.h"
#include "tginlinebuttonsconfigurationwidget.h"
#include "tgeventsconfigurationwidget.h"
#include "tgobjects/tgbotmanager.h"

TgBotConfigurationWidget::TgBotConfigurationWidget(TgBotManager* bot_manager, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TgBotConfigurationWidget)
    , tg_bot_manager_(bot_manager)
{
    ui->setupUi(this);

    qRegisterMetaType<std::string>("std::string");

    //------ messages ---------------------------------------

    messages_tree_ = new TGObjectTreeModel(*bot_manager, TGOBJECT_TYPE::MESSAGES, this);
    ui->tvTGMessages->setHeaderHidden(true);
    ui->tvTGMessages->setModel(messages_tree_);
    ui->tvTGMessages->setSelectionMode(QAbstractItemView::SingleSelection);
    QObject::connect(ui->tvTGMessages->selectionModel(), &QItemSelectionModel::currentChanged, messages_tree_, &TGObjectTreeModel::sl_tree_selection_changed);
    QObject::connect(messages_tree_, &TGObjectTreeModel::sg_set_item_selected, ui->tvTGMessages->selectionModel(), &QItemSelectionModel::setCurrentIndex);

    message_widget_ = new TGMessageConfigurationWidget(*bot_manager, this);
    QObject::connect(messages_tree_, &TGObjectTreeModel::sg_change_tree_item, message_widget_, &TGMessageConfigurationWidget::sl_change_object_data, Qt::QueuedConnection);
    QObject::connect(message_widget_, &TGMessageConfigurationWidget::sg_tgobject_changed, messages_tree_, &TGObjectTreeModel::sl_tgobject_changed, Qt::QueuedConnection);
    ui->frMessageData->layout()->addWidget(message_widget_);

    QObject::connect(ui->tbAddTGMessage, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_message);
    QObject::connect(ui->tbAddTGMessageWaitAnswer, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_message_wait_answer);
    QObject::connect(ui->tbSaveMessage, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_save_message);
    QObject::connect(ui->tbDeleteMessage, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_delete_message);

    //------ commands ---------------------------------------

    commands_tree_ = new TGObjectTreeModel(*bot_manager, TGOBJECT_TYPE::COMMANDS, this);
    ui->tvAllCommands->setHeaderHidden(true);
    ui->tvAllCommands->setModel(commands_tree_);
    ui->tvAllCommands->setSelectionMode(QAbstractItemView::SingleSelection);
    QObject::connect(ui->tvAllCommands->selectionModel(), &QItemSelectionModel::currentChanged, commands_tree_, &TGObjectTreeModel::sl_tree_selection_changed);
    QObject::connect(commands_tree_, &TGObjectTreeModel::sg_set_item_selected, ui->tvAllCommands->selectionModel(), &QItemSelectionModel::setCurrentIndex);

    command_widget_ = new TGCommandConfigurationWidget(*bot_manager, this);
    QObject::connect(commands_tree_, &TGObjectTreeModel::sg_change_tree_item, command_widget_, &TGCommandConfigurationWidget::sl_change_object_data, Qt::QueuedConnection);
    QObject::connect(command_widget_, &TGCommandConfigurationWidget::sg_tgobject_changed, commands_tree_, &TGObjectTreeModel::sl_tgobject_changed, Qt::QueuedConnection);
    ui->frCommandData->layout()->addWidget(command_widget_);

    QObject::connect(ui->tbAddTGCommand, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_command);
    QObject::connect(ui->tbSaveCommands, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_save_commands);
    QObject::connect(ui->tbDeleteCommand, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_delete_command);
    QObject::connect(ui->tbAddTGCommandToMainMenu, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_command_to_main_menu);

    //------ inline buttons -----------------------------------

    buttons_tree_ = new TGObjectTreeModel(*bot_manager, TGOBJECT_TYPE::BUTTONS, this);
    ui->tvAllInlineButtons->setHeaderHidden(true);
    ui->tvAllInlineButtons->setModel(buttons_tree_);
    ui->tvAllInlineButtons->setSelectionMode(QAbstractItemView::SingleSelection);
    QObject::connect(ui->tvAllInlineButtons->selectionModel(), &QItemSelectionModel::currentChanged, buttons_tree_, &TGObjectTreeModel::sl_tree_selection_changed);
    QObject::connect(buttons_tree_, &TGObjectTreeModel::sg_set_item_selected, ui->tvAllInlineButtons->selectionModel(), &QItemSelectionModel::setCurrentIndex);

    button_widget_ = new TGInlineButtonsConfigurationWidget(*bot_manager, this);
    QObject::connect(buttons_tree_, &TGObjectTreeModel::sg_change_tree_item, button_widget_, &TGInlineButtonsConfigurationWidget::sl_change_object_data, Qt::QueuedConnection);
    QObject::connect(button_widget_, &TGInlineButtonsConfigurationWidget::sg_tgobject_changed, buttons_tree_, &TGObjectTreeModel::sl_tgobject_changed, Qt::QueuedConnection);
    ui->frInlineButtonData->layout()->addWidget(button_widget_);

    QObject::connect(ui->tbAddTGInlineButton, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_inline_button);
    QObject::connect(ui->tbSaveInlineButtons, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_save_inline_buttons);
    QObject::connect(ui->tbDeleteInlineButton, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_delete_inline_button);

    //------ events ----------------------------------------

    events_tree_ = new TGObjectTreeModel(*bot_manager, TGOBJECT_TYPE::EVENTS, this);
    ui->tvAllEvents->setHeaderHidden(true);
    ui->tvAllEvents->setModel(events_tree_);
    ui->tvAllEvents->setSelectionMode(QAbstractItemView::SingleSelection);
    QObject::connect(ui->tvAllEvents->selectionModel(), &QItemSelectionModel::currentChanged, events_tree_, &TGObjectTreeModel::sl_tree_selection_changed);
    QObject::connect(events_tree_, &TGObjectTreeModel::sg_set_item_selected, ui->tvAllEvents->selectionModel(), &QItemSelectionModel::setCurrentIndex);

    events_widget_ = new TGEventsConfigurationWidget(*bot_manager, this);
    QObject::connect(events_tree_, &TGObjectTreeModel::sg_change_tree_item, events_widget_, &TGEventsConfigurationWidget::sl_change_object_data, Qt::QueuedConnection);
    QObject::connect(events_widget_, &TGEventsConfigurationWidget::sg_tgobject_changed, events_tree_, &TGObjectTreeModel::sl_tgobject_changed, Qt::QueuedConnection);
    ui->frEventsData->layout()->addWidget(events_widget_);

    QObject::connect(ui->tbAddTGEvent, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_event);
    QObject::connect(ui->tbAddScheduleEvent, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_add_new_scheduled_event);
    QObject::connect(ui->tbSaveEvents, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_save_events);
    QObject::connect(ui->tbDeleteEvent, &QAbstractButton::clicked, this, &TgBotConfigurationWidget::sl_delete_event);

    ui->tabWidget->setCurrentIndex(0);
}

TgBotConfigurationWidget::~TgBotConfigurationWidget()
{
    delete ui;
}

void TgBotConfigurationWidget::showEvent(QShowEvent *event)
{
    int wdt = ui->spltMessageConfig->width();
    ui->spltMessageConfig->setSizes({2*wdt/5, 3*wdt/5});
    ui->spltCommandsConfig->setSizes({2*wdt/5, 3*wdt/5});
    ui->spltInlineButtons->setSizes({2*wdt/5, 3*wdt/5});
    ui->spltEventsConfig->setSizes({2*wdt/5, 3*wdt/5});
}

void TgBotConfigurationWidget::sl_add_new_message()
{
    auto cur_index = ui->tvTGMessages->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        message_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGMessage> new_message_ptr = std::unique_ptr<TGMessage>(new TGMessage(tg_bot_manager_->GetTGParent()));
    const std::string& new_id = new_message_ptr->GetId();
    tg_bot_manager_->AddTGMessage(std::move(new_message_ptr));

    messages_tree_->sl_tgobject_changed(new_id, {});
}

void TgBotConfigurationWidget::sl_add_new_message_wait_answer()
{
    auto cur_index = ui->tvTGMessages->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        message_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGMessageWaitAnswer> new_message_ptr = std::unique_ptr<TGMessageWaitAnswer>(new TGMessageWaitAnswer(tg_bot_manager_->GetTGParent()));
    const std::string& new_id = new_message_ptr->GetId();
    tg_bot_manager_->AddTGMessageWaitAnswer(std::move(new_message_ptr));

    messages_tree_->sl_tgobject_changed(new_id, {});
}


void TgBotConfigurationWidget::sl_delete_message()
{
    auto cur_index = ui->tvTGMessages->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        tg_bot_manager_->DeleteTGObject(item_id);
        messages_tree_->sl_tgobject_changed({}, item_id);
    }
}

void TgBotConfigurationWidget::sl_save_message()
{
    auto cur_index = ui->tvTGMessages->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        auto mes_ptr = tg_bot_manager_->GetTGMessage(item_id);
        if(mes_ptr) {
            mes_ptr->SetObjectInWork(true);
            message_widget_->sl_change_object_data(item_id, item_id);
        }
    }
}

void TgBotConfigurationWidget::sl_add_new_command()
{
    auto cur_index = ui->tvAllCommands->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        command_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGTriggerUserCommand> new_command_ptr = std::unique_ptr<TGTriggerUserCommand>(new TGTriggerUserCommand(tg_bot_manager_->GetTGParent()));
    const std::string& new_id = new_command_ptr->GetId();
    tg_bot_manager_->AddTGCommand(std::move(new_command_ptr));

    commands_tree_->sl_tgobject_changed(new_id, {});
}

void TgBotConfigurationWidget::sl_delete_command()
{
    auto cur_index = ui->tvAllCommands->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        tg_bot_manager_->DeleteTGObject(item_id);
        commands_tree_->sl_tgobject_changed({}, item_id);
    }
}

void TgBotConfigurationWidget::sl_save_commands()
{
    auto cur_index = ui->tvAllCommands->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        auto com_ptr = tg_bot_manager_->GetTGCommand(item_id);
        if(com_ptr) {
            com_ptr->SetObjectInWork(true);
            command_widget_->sl_change_object_data(item_id, item_id);
        }
    }
}

void TgBotConfigurationWidget::sl_add_command_to_main_menu()
{
    auto cur_index = ui->tvAllCommands->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        auto com_ptr = tg_bot_manager_->GetTGCommand(item_id);
        if(com_ptr) {
            com_ptr->IncludeToMainMenu(!com_ptr->IncludedToMainMenu());
            command_widget_->sl_change_object_data(item_id, item_id);
        }
    }
}

void TgBotConfigurationWidget::sl_add_new_event()
{
    auto cur_index = ui->tvAllEvents->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        events_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGTriggerTagValue> new_event_ptr = std::unique_ptr<TGTriggerTagValue>(new TGTriggerTagValue(tg_bot_manager_->GetTGParent()));
    const std::string& new_id = new_event_ptr->GetId();
    tg_bot_manager_->AddTGEvent(std::move(new_event_ptr));

    events_tree_->sl_tgobject_changed(new_id, {});
}


void TgBotConfigurationWidget::sl_delete_event()
{
    auto cur_index = ui->tvAllEvents->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        tg_bot_manager_->DeleteTGObject(item_id);
        events_tree_->sl_tgobject_changed({}, item_id);
    }
}


void TgBotConfigurationWidget::sl_save_events()
{
    auto cur_index = ui->tvAllEvents->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        auto ev_ptr = tg_bot_manager_->GetTGEvent(item_id);
        auto sch_ev_ptr = tg_bot_manager_->GetTGScheduledEvent(item_id);
        if(ev_ptr) {
            ev_ptr->SetObjectInWork(true);
            events_widget_->sl_change_object_data(item_id, item_id);
        }
        if(sch_ev_ptr) {
            sch_ev_ptr->SetObjectInWork(true);
            events_widget_->sl_change_object_data(item_id, item_id);
        }
    }
}

void TgBotConfigurationWidget::sl_add_new_inline_button()
{
    auto cur_index = ui->tvAllInlineButtons->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        button_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGButtonWCallback> new_button_ptr = std::unique_ptr<TGButtonWCallback>(new TGButtonWCallback(tg_bot_manager_->GetTGParent()));
    const std::string& new_id = new_button_ptr->GetId();
    tg_bot_manager_->AddTGInlineButton(std::move(new_button_ptr));

    buttons_tree_->sl_tgobject_changed(new_id, {});
}

void TgBotConfigurationWidget::sl_delete_inline_button()
{
    auto cur_index = ui->tvAllInlineButtons->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        tg_bot_manager_->DeleteTGObject(item_id);
        buttons_tree_->sl_tgobject_changed({}, item_id);
    }
}

void TgBotConfigurationWidget::sl_save_inline_buttons()
{
    auto cur_index = ui->tvAllInlineButtons->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        auto btn_ptr = tg_bot_manager_->GetTGInlineButton(item_id);
        if(btn_ptr) {
            btn_ptr->SetObjectInWork(true);
            button_widget_->sl_change_object_data(item_id, item_id);
        }
    }
}

void TgBotConfigurationWidget::sl_add_new_scheduled_event()
{
    auto cur_index = ui->tvAllEvents->selectionModel()->currentIndex();
    if(cur_index.isValid() && cur_index.parent().isValid()) {
        auto item_id = static_cast<TGTreeItem*>(cur_index.internalPointer())->GetId();
        events_widget_->sl_change_object_data(item_id, item_id);
    }

    std::unique_ptr<TGScheduledEvent> new_event_ptr = std::unique_ptr<TGScheduledEvent>(new TGScheduledEvent(tg_bot_manager_->GetTGParent(), PeriodicalTask::Period::HOURLY));
    const std::string& new_id = new_event_ptr->GetId();
    tg_bot_manager_->AddTGScheduledEvent(std::move(new_event_ptr));

    events_tree_->sl_tgobject_changed(new_id, {});
}

//=================================================================================
//============ T G T R E E I T E M ================================================
//=================================================================================

TGTreeItem::TGTreeItem(const std::string& id , TGTreeItem* parent)
    : parent_(parent)
    , tgobject_id_(id)
{
}

TGTreeItem *TGTreeItem::Child(int row) const
{
    if(row < 0 || row >= static_cast<int>(child_ids_.size())) return nullptr;
    auto it = child_ids_.cbegin();
    std::advance(it, row);
    return childs_.at(&(*it)).get();
}

TGTreeItem *TGTreeItem::Child(const std::string &id) const
{
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it == child_ids_.end()) return nullptr;
    return childs_.at(&(*it)).get();
}

int TGTreeItem::ChildCount() const
{
    return child_ids_.size();
}

QVariant TGTreeItem::Data() const
{
    return QString::fromStdString(tgobject_id_);
}

int TGTreeItem::Row() const
{
    if(!parent_) return 0;
    auto it = std::find(parent_->child_ids_.begin(), parent_->child_ids_.end(), tgobject_id_);
    return std::distance(parent_->child_ids_.begin(), it);
}

TGTreeItem *TGTreeItem::ParentItem()
{
    return parent_;
}

const std::string &TGTreeItem::GetId() const
{
    return tgobject_id_;
}

bool TGTreeItem::AppendChild(std::unique_ptr<TGTreeItem> &&child, int row)
{
    if(row == -1) {
        child_ids_.push_back(child->tgobject_id_);
        childs_[&child_ids_.back()] = std::move(child);
        return true;
    }
    if(row < 0 || row > static_cast<int>(child_ids_.size())) {
        return false;
    }

    auto it_pos = child_ids_.begin();
    std::advance(it_pos, row);
    auto it = child_ids_.insert(it_pos, child->tgobject_id_);
    childs_[&(*it)] = std::move(child);
    return true;
}

bool TGTreeItem::AppendChild(const std::string& id, int row)
{
    return AppendChild(std::make_unique<TGTreeItem>(id, this), row);
}

bool TGTreeItem::DeleteChildRecursively(const std::string& id)
{
    bool b = false;
    for(auto& [it, child_ptr]: childs_) {
        b = b || child_ptr->DeleteChildRecursively(id);
    }
    auto it = std::find(child_ids_.begin(), child_ids_.end(), id);
    if(it != child_ids_.end()) {
        childs_.erase(&(*it));
        child_ids_.erase(it);
        b = true;
    }
    return b;
}

bool TGTreeItem::DeleteChild(int row)
{
    if(row >= 0 && row < static_cast<int>(child_ids_.size())) {
        auto it = child_ids_.begin();
        std::advance(it, row);
        childs_.erase(&(*it));
        child_ids_.erase(it);
        return true;
    }
    return false;
}

//=================================================================================
//============ T G T R E E M O D E L ==============================================
//=================================================================================
TGObjectTreeModel::TGObjectTreeModel(TgBotManager &bot_manager, TGOBJECT_TYPE tree_type, QObject *parent)
    : QAbstractItemModel(parent)
    , bot_manager_(bot_manager)
    , tree_type_(tree_type)
{
    switch(tree_type) {
    case TGOBJECT_TYPE::MESSAGES:   prepare_data_tree_messages_(); break;
    case TGOBJECT_TYPE::EVENTS:     prepare_data_tree_events_(); break;
    case TGOBJECT_TYPE::COMMANDS:   prepare_data_tree_commands_(); break;
    case TGOBJECT_TYPE::BUTTONS:    prepare_data_tree_inline_buttons_(); break;
    }
    build_tree_items_();
}

void TGObjectTreeModel::prepare_data_tree_messages_()
{
    headers_.clear();
    header_to_ids_.clear();
    headers_.push_back(std::string("Несохраненные сообщения"));
    headers_.push_back(std::string("Текстовые сообщения"));
    headers_.push_back(std::string("Текстовые сообщения с тэгами"));
    headers_.push_back(std::string("Текстовые сообщения с кнопками"));
    headers_.push_back(std::string("Текстовые сообщения с тэгом и кнопками"));
    headers_.push_back(std::string("Сообщения с ожиданием ответа"));
    for(const auto& it: headers_) {
        header_to_ids_[&(it)] = {};
    }
    int index_header = 0;
    for(const auto& it: bot_manager_.GetTGMessages()) {
        if(!it->IsInWork()) {
            //несохраненные сообщения индекс = 0
            index_header = 0;
        } else if(dynamic_cast<TGMessageWaitAnswer*>(it) != nullptr) {
            //сообщения с ожиданием ответа (с тэгами или без -- одна категория) индекс = 5
            index_header = 5;
        } else if(!it->HasTags() && !it->HasButtons()) {
            //сообщения индекс = 1
            index_header = 1;
        } else if(it->HasTags() && !it->HasButtons()) {
            //сообщения с тэгом индекс = 2
            index_header = 2;
        } else if(!it->HasTags() && it->HasButtons()) {
            //сообщения с кнопками индекс = 3
            index_header = 3;
        } else {
            //сообщения с кнопками и тэгами индекс = 4
            index_header = 4;
        }
        header_to_ids_.at(&headers_.at(index_header)).insert(it->GetId());
    }
}

void TGObjectTreeModel::prepare_data_tree_commands_()
{
    headers_.clear();
    header_to_ids_.clear();
    headers_.push_back(std::string("Несохраненные команды"));
    headers_.push_back(std::string("Команды главного меню"));
    headers_.push_back(std::string("Команды вне меню"));
    for(const auto& it: headers_) {
        header_to_ids_[&it] = {};
    }
    int index_header = 0;
    for(const auto& it: bot_manager_.GetTGCommands()) {
        if(!it->IsInWork()) {
            //несохраненные команды индекс = 0
            index_header = 0;
        } else if(it->IncludedToMainMenu()) {
            //команды меню индекс = 1
            index_header = 1;
        } else {
            //команды вне меню индекс = 2
            index_header = 2;
        }
        header_to_ids_.at(&headers_.at(index_header)).insert(it->GetId());
    }
}

void TGObjectTreeModel::prepare_data_tree_events_()
{
    headers_.clear();
    header_to_ids_.clear();
    headers_.push_back(std::string("Несохраненные события"));
    headers_.push_back(std::string("Несохраненные периодические события"));
    headers_.push_back(std::string("События по изменению тэга"));
    headers_.push_back(std::string("Периодические события"));
    for(const auto& it: headers_) {
        header_to_ids_[&it] = {};
    }
    int index_header = 0;
    for(const auto& it: bot_manager_.GetTGEvents()) {
        if(!it->IsInWork()) {
            //несохраненные события индекс = 0
            index_header = 0;
        } else {
            //События по изменению тэга индекс = 2
            index_header = 2;
        }
        header_to_ids_.at(&headers_.at(index_header)).insert(it->GetId());
    }
    for(const auto& it: bot_manager_.GetTGScheduledEvents()) {
        if(!it->IsInWork()) {
            //несохраненные события индекс = 1
            index_header = 1;
        } else {
            //Периодические события индекс = 3
            index_header = 3;
        }
        header_to_ids_.at(&headers_.at(index_header)).insert(it->GetId());
    }
}

void TGObjectTreeModel::prepare_data_tree_inline_buttons_()
{
    headers_.clear();
    header_to_ids_.clear();
    headers_.push_back(std::string("Несохраненные кнопки"));
    headers_.push_back(std::string("Встраиваемые кнопки"));

    for(const auto& it: headers_) {
        header_to_ids_[&it] = {};
    }
    int index_header = 0;
    for(const auto& it: bot_manager_.GetTGInlineButtons()) {
        if(!it->IsInWork()) {
            //несохраненные кнопки индекс = 0
            index_header = 0;
        } else {
            //кнопки индекс = 1
            index_header = 1;
        }
        header_to_ids_.at(&headers_.at(index_header)).insert(it->GetId());
    }
}

void TGObjectTreeModel::add_new_item_message_(const std::string &id)
{
    auto mes_ptr = bot_manager_.GetTGMessage(id);
    if(!mes_ptr) return;
    int parent_index = 0;
    if(!mes_ptr->IsInWork()) {
        //несохраненные сообщения индекс = 0
        parent_index = 0;
    } else if(dynamic_cast<TGMessageWaitAnswer*>(mes_ptr) != nullptr) {
        //сообщения с ожиданием ответа индекс = 5
        parent_index = 5;
    } else if(!mes_ptr->HasTags() && !mes_ptr->HasButtons()) {
        //сообщения индекс = 1
        parent_index = 1;
    } else if(mes_ptr->HasTags() && !mes_ptr->HasButtons()) {
        //сообщения с тэгом индекс = 2
        parent_index = 2;
    } else if(!mes_ptr->HasTags() && mes_ptr->HasButtons()) {
        //сообщения с кнопками индекс = 3
        parent_index = 3;
    } else {
        //сообщения с кнопками и тэгами индекс = 4
        parent_index = 4;
    }

    auto [it, res_ins] = header_to_ids_.at(&headers_.at(parent_index)).insert(mes_ptr->GetId());
    if(!res_ins) return;

    QModelIndex parent_insert = createIndex(parent_index, 0, root_item_->Child(parent_index));
    int row_insert = std::distance(header_to_ids_.at(&headers_.at(parent_index)).begin(), it);

    beginInsertRows(parent_insert, row_insert, row_insert);
    root_item_->Child(parent_index)->AppendChild(*it, row_insert);
    endInsertRows();
}

void TGObjectTreeModel::add_new_item_command_(const std::string &id)
{
    auto com_ptr = bot_manager_.GetTGCommand(id);
    if(!com_ptr) return;
    int parent_index = 0;
    if(!com_ptr->IsInWork()) {
        //несохраненные команды индекс = 0
        parent_index = 0;
    } else if(com_ptr->IncludedToMainMenu()) {
        //команды меню индекс = 1
        parent_index = 1;
    } else {
        //команды вне меню индекс = 2
        parent_index = 2;
    }
    auto [it, res_ins] = header_to_ids_.at(&headers_.at(parent_index)).insert(com_ptr->GetId());
    if(!res_ins) return;

    QModelIndex parent_insert = createIndex(parent_index, 0, root_item_->Child(parent_index));
    int row_insert = std::distance(header_to_ids_.at(&headers_.at(parent_index)).begin(), it);

    beginInsertRows(parent_insert, row_insert, row_insert);
    root_item_->Child(parent_index)->AppendChild(*it, row_insert);
    endInsertRows();
}

void TGObjectTreeModel::add_new_item_event_(const std::string &id)
{
    auto ev_ptr = bot_manager_.GetTGEvent(id);
    auto ev_sch_ptr = bot_manager_.GetTGScheduledEvent(id);
    if(!ev_ptr && !ev_sch_ptr) return;

    std::string event_id;
    if(ev_ptr) {
        event_id = ev_ptr->GetId();
    }
    if(ev_sch_ptr) {
        event_id = ev_sch_ptr->GetId();
    }

    int parent_index = 0;
    if((ev_ptr) && !ev_ptr->IsInWork()) {
        //несохраненные события индекс = 0
        parent_index = 0;
    } else if((ev_sch_ptr) && !ev_sch_ptr->IsInWork()) {
        //несохраненные периодические события индекс = 1
        parent_index = 1;
    } else if(ev_ptr){
        //События по изменению тэга индекс = 2
        parent_index = 2;
    } else if(ev_sch_ptr){
        //периодическое событие вне меню индекс = 3
        parent_index = 3;
    }

    auto [it, res_ins] = header_to_ids_.at(&headers_.at(parent_index)).insert(std::move(event_id));
    if(!res_ins) return;

    QModelIndex parent_insert = createIndex(parent_index, 0, root_item_->Child(parent_index));
    int row_insert = std::distance(header_to_ids_.at(&headers_.at(parent_index)).begin(), it);

    beginInsertRows(parent_insert, row_insert, row_insert);
    root_item_->Child(parent_index)->AppendChild(*it, row_insert);
    endInsertRows();
}

void TGObjectTreeModel::add_new_item_button_(const std::string &id)
{
    auto btn_ptr = bot_manager_.GetTGInlineButton(id);
    if(!btn_ptr) return;
    int parent_index = 0;
    if(!btn_ptr->IsInWork()) {
        //несохраненные кнопки индекс = 0
        parent_index = 0;
    } else {
        //кнопки индекс = 1
        parent_index = 1;
    }
    auto [it, res_ins] = header_to_ids_.at(&headers_.at(parent_index)).insert(btn_ptr->GetId());
    if(!res_ins) return;

    QModelIndex parent_insert = createIndex(parent_index, 0, root_item_->Child(parent_index));
    int row_insert = std::distance(header_to_ids_.at(&headers_.at(parent_index)).begin(), it);

    beginInsertRows(parent_insert, row_insert, row_insert);
    root_item_->Child(parent_index)->AppendChild(*it, row_insert);
    endInsertRows();
}

void TGObjectTreeModel::build_tree_items_()
{
    if(root_item_) root_item_.reset();
    root_item_ = std::make_unique<TGTreeItem>(empty_str_, nullptr);

    for(const auto& it: headers_) {
        root_item_->AppendChild(it);
    }

    for(int i = 0; i < root_item_->ChildCount(); ++i) {
        auto parent_ptr = root_item_->Child(i);
        auto par_it = std::find(headers_.begin(), headers_.end(), parent_ptr->GetId());
        if(par_it == headers_.end()) continue;
        for(const auto& it: header_to_ids_.at(&(*par_it))) {
            parent_ptr->AppendChild(it);
        }
    }
}

const std::string &TGObjectTreeModel::get_id_(const QModelIndex &index) const
{
    if (!index.isValid() || !index.parent().isValid()) return empty_str_;
    return static_cast<TGTreeItem*>(index.internalPointer())->GetId();
}

QVariant TGObjectTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    return {};
}

QModelIndex TGObjectTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return {};

    TGTreeItem *parentItem = parent.isValid()
                             ? static_cast<TGTreeItem*>(parent.internalPointer())
                             : root_item_.get();

    if (auto *childItem = parentItem->Child(row))
        return createIndex(row, column, childItem);
    return {};
}

QModelIndex TGObjectTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return {};

    auto *childItem = static_cast<TGTreeItem*>(index.internalPointer());
    TGTreeItem *parentItem = childItem->ParentItem();

    return parentItem != root_item_.get()
               ? createIndex(parentItem->Row(), 0, parentItem) : QModelIndex{};
}

int TGObjectTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    const TGTreeItem *parentItem = parent.isValid()
                                   ? static_cast<const TGTreeItem*>(parent.internalPointer())
                                   : root_item_.get();
    return parentItem->ChildCount();
}

int TGObjectTreeModel::columnCount(const QModelIndex &parent) const
{
    return 1;
}

QVariant TGObjectTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) return {};

    if(role == Qt::DisplayRole) {
        const auto *item = static_cast<const TGTreeItem*>(index.internalPointer());
        return item->Data();
    }
    if(role == Qt::FontRole) {
        QFont font("Segoe UI", 11);
        if(!index.parent().isValid())
            font.setBold(true);

        return font;
    }
    return {};
}

void TGObjectTreeModel::sl_tree_selection_changed(const QModelIndex &current, const QModelIndex &previous)
{
    emit sg_change_tree_item(get_id_(current), get_id_(previous));
}

void TGObjectTreeModel::sl_tgobject_changed(const std::string& current_id, const std::string& previous_id)
{
    TGTreeItem* item_ptr = nullptr;
    for(int i = 0; i < root_item_->ChildCount() && !item_ptr; ++i) {
        item_ptr = root_item_->Child(i)->Child(previous_id);
    }
    if(item_ptr) {
        QModelIndex parent_delete = createIndex(item_ptr->ParentItem()->Row(), 0, item_ptr->ParentItem());
        beginRemoveRows(parent_delete, item_ptr->Row(), item_ptr->Row());
        int row = item_ptr->ParentItem()->Row();
        root_item_->DeleteChildRecursively(previous_id);
        header_to_ids_.at(&headers_.at(row)).erase(previous_id);
        endRemoveRows();
    }

    if(current_id.empty()) return;
    switch(tree_type_) {
    case TGOBJECT_TYPE::MESSAGES:   add_new_item_message_(current_id); break;
    case TGOBJECT_TYPE::COMMANDS:   add_new_item_command_(current_id); break;
    case TGOBJECT_TYPE::EVENTS:     add_new_item_event_(current_id); break;
    case TGOBJECT_TYPE::BUTTONS:    add_new_item_button_(current_id); break;
    }

    item_ptr = nullptr;
    for(int i = 0; i < root_item_->ChildCount() && !item_ptr; ++i) {
        item_ptr = root_item_->Child(i)->Child(current_id);
    }
    if(item_ptr) {
        QModelIndex index_to_select = createIndex(item_ptr->Row(), 0, item_ptr);
        if(current_id != previous_id) {
            emit sg_set_item_selected(index_to_select, QItemSelectionModel::Select);
        }
    }
}
