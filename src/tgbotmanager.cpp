#include "tgbotmanager.h"

#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDateTime>
#include <QDebug>
#include <QThread>
#include <QApplication>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonDocument>
#include <QMessageBox>

#include "tgbot/tools/StringTools.h"
#include "tgbot/Bot.h"

#include "tgbotthreadwrapper.h"
#include "datatag.h"
#include "sourcedrivermanager.h"

QByteArray TGHELPER::EncryptDecryptTokenString(const QByteArray& text, const std::string& key)
{
    QByteArray ret_array;

    int text_length = text.length();
    int key_length = key.length();
    if(key_length == 0 || text_length == 0) return ret_array;

    ret_array.resize(text_length);

    for(int i = 0; i < text_length; ++i) {
        ret_array[i] = text.at(i) ^ key.at(i % key_length);
    }
    return ret_array;
}

std::string TGHELPER::ReadTokenFromFile(const QString &filename)
{
    QFile file(filename);
    if(file.open(QIODeviceBase::ReadOnly)) {
        QByteArray raw_array = file.readAll();
        if(raw_array.size() > static_cast<qsizetype>(PREFIX_POSTFIX_KEY_LENGTH*2)) {
            QByteArray key_array((raw_array.size() - PREFIX_POSTFIX_KEY_LENGTH*2) / 3, '\0');
            raw_array = std::move(EncryptDecryptTokenString(raw_array, secure_key_string));
            for(size_t i = 0; i < raw_array.size() - PREFIX_POSTFIX_KEY_LENGTH*2; i+=3) {
                key_array[i/3] = raw_array.at(PREFIX_POSTFIX_KEY_LENGTH + i);
            }
            return key_array.toStdString();
        }
    }
    return {};
}

bool TGHELPER::WriteTokenToFile(const QString &filename, const std::string& token_raw)
{
    QFile file(filename);
    std::string prefix_str = StringTools::generateRandomString(PREFIX_POSTFIX_KEY_LENGTH);
    std::string postfix_str = StringTools::generateRandomString(PREFIX_POSTFIX_KEY_LENGTH);
    std::string key_mesh_str = StringTools::generateRandomString(token_raw.length()*2);
    QByteArray meshed_array(token_raw.length()*3, '\0');
    meshed_array.insert(0, prefix_str.data(), PREFIX_POSTFIX_KEY_LENGTH);
    for(size_t i{0}, j{PREFIX_POSTFIX_KEY_LENGTH}; i < token_raw.length(); ++i, j+=3) {
        meshed_array[j] = token_raw.at(i);
        meshed_array[j+1] = key_mesh_str.at(i*2);
        meshed_array[j+2] = key_mesh_str.at(i*2 + 1);
    }
    meshed_array.insert(meshed_array.size() - 1, postfix_str.data(), PREFIX_POSTFIX_KEY_LENGTH);
    if(file.open(QIODeviceBase::WriteOnly)) {
        int res = file.write(EncryptDecryptTokenString(meshed_array, secure_key_string));
        return res != (-1);
    }
    return false;
}

//===============================================================
//============ T G B O T U S E R ================================
//===============================================================
TgBotUser::TgBotUser(TgBot::User& other) {
    copy_fields_(other);
}
TgBotUser::TgBotUser(TgBot::Message::Ptr message) {
    if(message && message->from) {
        copy_fields_(*message->from.get());
        return;
    }
    if(message && message->chat) {
        copy_fields_(*message->chat.get());
        return;
    }

    id = 0;
}
void TgBotUser::copy_fields_(TgBot::User other) {
    addedToAttachmentMenu = other.addedToAttachmentMenu;
    canConnectToBusiness = other.canConnectToBusiness;
    canJoinGroups = other.canJoinGroups;
    canReadAllGroupMessages = other.canReadAllGroupMessages;
    firstName = other.firstName;
    id = other.id;
    isBot = other.isBot;
    isPremium = other.isPremium;
    languageCode = other.languageCode;
    lastName = other.lastName;
    supportsInlineQueries = other.supportsInlineQueries;
    username = other.username;
}
void TgBotUser::copy_fields_(TgBot::Chat chat)
{
    id = chat.id;
    if(chat.type == TgBot::Chat::Type::Channel) {
        type = USER_TYPE::CHANNEL;
    }
    username = chat.username;
    firstName = chat.title;

    addedToAttachmentMenu = false;
    canConnectToBusiness = false;
    canReadAllGroupMessages = false;
    isBot = false;
    isPremium = false;
    languageCode = "";
    lastName = chat.lastName;
}
void TgBotUser::UpdateData(TgBot::User& other) {
    copy_fields_(other);
}
void TgBotUser::UpdateData(TgBot::Message::Ptr message) {
    if(message && message->from) {
        copy_fields_(*message->from.get());
        return;
    }
    if(message && message->chat) {
        copy_fields_(*message->chat.get());
    }
}
//===============================================================
//============ T G B O T M A N A G E R ==========================
//===============================================================

TgBotManager::TgBotManager(const std::string& bot_token, SourceDriverManager& driver_manager)
    : QObject()
    , bot_token_(bot_token)
    , driver_manager_(&driver_manager)
    , check_events_timer_(new QTimer(this))
    , check_restart_timer_(new QTimer(this))
{
    qInfo() << QString("TgBotManager конструктор.");

    QObject::connect(check_events_timer_, SIGNAL(timeout()), this, SLOT(sl_check_events_timer_out()));
    QObject::connect(check_restart_timer_, SIGNAL(timeout()), this, SLOT(sl_check_restart_timer_out()));

    tg_parent_ = std::make_unique<TGParent>(*driver_manager.TagRegistry());

    check_restart_timer_->setInterval(RESTART_BOT_PERIOD_);
    check_restart_timer_->start();

    restore_user_data_from_file_();
    restore_tg_data_from_file_();

    if(auto_restart_bot_) {
        QTimer::singleShot(1000, this, &TgBotManager::StartBot);
    }
}

TgBotManager::TgBotManager(SourceDriverManager& dm)
    : TgBotManager(TGHELPER::ReadTokenFromFile(QString("token.dat")), dm)
{}

TgBotManager::~TgBotManager() {
    emit sg_stop_bot_thread();
    while(bot_started_ || bot_trying_connect_api_) {
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    };

    QDir app_dir(qApp->applicationDirPath());
    bool autosave_flag = app_dir.exists("autosave");
    if(!autosave_flag) {
        autosave_flag = app_dir.mkdir("autosave");
        if(autosave_flag) {
            qInfo() << QString("Папка автосохранения конфигурационных файлов успешно создана.");
        } else {
            qWarning() << QString("Не удалось создать папку для автосохранения конфигурационных файлов.");
        }
    }

    if(autosave_flag) {
        QString path = QString("%1/autosave").arg(app_dir.absolutePath());
        qInfo() << QString("Автосохранение кофигурационных файлов TgBotManager в папке %1").arg(path);
        SaveDataToJson(path);
        QString users_file = QString("%1%2").arg(app_dir.absolutePath(), "/users.json");
        save_user_data_to_file_(users_file);
    } else {
        qWarning() << "Автосохранение кофигурационных файлов TgBotManager не удалось.";
    }

    qInfo() << QString("TgBotManager деструктор завершен");
}

void TgBotManager::screen_symbols_(std::string& text, const std::string& symbols) const
{
    for(auto it = text.begin(); it < text.end(); ++it) {
        if(symbols.find(*it) != symbols.npos) {
            if(it == text.begin() || *(it-1) != '\\') {
                it = text.insert(it, '\\');
                ++it;
            }
        }
    }
}

void TgBotManager::AddOrUpdateUser(TgBot::Message::Ptr message) {
    if(!message || !message->chat) {
        qCritical() << "Получено пустое сообщение (NULL) или отсутствует id чата";
        return;
    }

    if(CheckUser(message->chat->id) == USER_TYPE::NEW_USER) {
        std::string mes = "Подключен новый пользователь: ";
        mes += message->chat->username;
        mes += " ";
        mes += message->chat->firstName;
        mes += " ";
        mes += message->chat->lastName;
        mes += ", тип = ";
        switch(message->chat->type) {
            case TgBot::Chat::Type::Channel: mes += "Channel"; break;
            case TgBot::Chat::Type::Group: mes += "Group"; break;
            case TgBot::Chat::Type::Private: mes += "Private"; break;
            case TgBot::Chat::Type::Supergroup: mes += "Supergroup";
        }

        emit sg_send_message_to_console(QString::fromStdString(mes));
        qInfo() << QString::fromStdString(mes);

        screen_symbols_(mes, screened_symbols_);
        users_.emplace(message->chat->id, TgBotUser(message));
        users_.at(message->chat->id).type = USER_TYPE::UNREGISTERED;
        tg_parent_->AddOrUpdateChatID(message->chat->id, USER_TYPE::UNREGISTERED);
        for(const auto& it: GetUsers(USER_TYPE::ADMIN)) {
            tg_parent_->BotSendMessage(it->chatId, mes);
        }
    } else {

        users_.at(message->chat->id).UpdateData(message);
        qInfo() << QString("Получено сообщение или команда от пользователя ID[%1] Username[%2] [%3] [%4]")
                       .arg(static_cast<quint64>(message->chat->id))
                       .arg(QString::fromStdString(users_.at(message->chat->id).username)
                            , QString::fromStdString(users_.at(message->chat->id).firstName)
                            , QString::fromStdString(users_.at(message->chat->id).lastName));
    }
    users_.at(message->chat->id).chatId = message->chat->id;
    users_.at(message->chat->id).lastMessageDT = QDateTime::currentDateTime();
}

bool TgBotManager::RegisterUser(int64_t user_id) {
    if(CheckUser(user_id) == USER_TYPE::UNREGISTERED) {
        users_.at(user_id).type = USER_TYPE::REGISTRD;
        QString log_string = QString("Зарегистрирован пользователь: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    if(CheckUser(user_id) == USER_TYPE::BANNED) {
        users_.at(user_id).type = USER_TYPE::UNREGISTERED;
        QString log_string = QString("Отменена регистрация пользователя: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

USER_TYPE TgBotManager::CheckUser(int64_t user_id) const {
    if(users_.count(user_id) > 0) {
        return users_.at(user_id).type;
    }
    return USER_TYPE::NEW_USER;
}

bool TgBotManager::SetAdmin(int64_t user_id) {
    if(CheckUser(user_id) != USER_TYPE::NEW_USER) {
        users_.at(user_id).type = USER_TYPE::ADMIN;
        QString log_string = QString("Назначен администратор: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

bool TgBotManager::ResetAdmin(int64_t user_id) {
    if(CheckUser(user_id) == USER_TYPE::ADMIN) {
        users_.at(user_id).type = USER_TYPE::UNREGISTERED;
        QString log_string = QString("Отменена регистрация администратора: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

bool TgBotManager::BanUser(int64_t user_id) {
    if(CheckUser(user_id) != USER_TYPE::NEW_USER
        && CheckUser(user_id) != USER_TYPE::BANNED
        && CheckUser(user_id) != USER_TYPE::ADMIN) {
        users_.at(user_id).type = USER_TYPE::BANNED;
        tg_parent_->AddOrUpdateChatID(user_id, USER_TYPE::BANNED);
        QString log_string = QString("Забанен пользователь: %1").arg(QString::fromStdString(users_.at(user_id).username));
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;
        return true;
    }
    return false;
}

void TgBotManager::make_admin_tools_() {
    admin_tools_to_callback_.clear();
    server_to_sample_and_nomber_tag_.clear();

    for(const auto& tag: tg_parent_->TagManager()->GetAllTags()) {
        if(!server_to_sample_and_nomber_tag_.contains(tag->GetEndpointName())) {
            server_to_sample_and_nomber_tag_[tag->GetEndpointName()] = {tag, 1};
        } else {
            ++server_to_sample_and_nomber_tag_.at(tag->GetEndpointName()).second;
        }
    }

    TgBot::InlineKeyboardMarkup::Ptr main_admin_kb(new TgBot::InlineKeyboardMarkup);
    TgBot::InlineKeyboardButton::Ptr btn(new TgBot::InlineKeyboardButton);
    btn->text = "Все пользователи";
    btn->callbackData = "admin_tools_get_all_users";
    main_admin_kb->inlineKeyboard.push_back({btn});
    admin_tools_to_callback_["admin_tools_get_all_users"] = [this](const TgBot::CallbackQuery::Ptr query) {
        std::vector<TgBotUser const *> users = GetUsers();
        QString mes = "Все пользователи:\n";
        for(const auto& it: users) {
            mes.append(QString("*%1* %2 %3 %4: %5 \n").arg(it->id).arg(QString::fromStdString(it->username)
                                                                       ,QString::fromStdString(it->firstName)
                                                                       , QString::fromStdString(it->lastName)
                                                                       ,it->lastMessageDT.toString("dd.MM.yyyy hh:mm:ss")));
        }
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
    };

    btn = TgBot::InlineKeyboardButton::Ptr(new TgBot::InlineKeyboardButton());
    btn->text = "ОРС статус";
    btn->callbackData = "admin_opc_data";
    main_admin_kb->inlineKeyboard.push_back({btn});

    admin_tools_to_callback_["admin_opc_data"] = [this](const TgBot::CallbackQuery::Ptr query) {
        QString mes = "Данные OPC:\n";
        if(driver_manager_->InWork()) {
            mes.append("OPC клиент запущен. \n");
        } else {
            mes.append("OPC клиент остановлен. \n");
        }

        for(const auto& [server, pair_tag_n]: server_to_sample_and_nomber_tag_) {
            mes.append(QString("Сервер [%1] количество тэгов [%2], статус связи: 0x%3 [%4]\n")
                           .arg(server)
                           .arg(pair_tag_n.second)
                           .arg(DataTag::QualityToString(pair_tag_n.first->GetTagQuality())));
        }
        std::string std_mes = mes.toStdString();
        screen_symbols_(std_mes, screened_symbols_);
        tg_parent_->BotSendMessage(query->message->chat->id, std_mes);
    };

    btn = TgBot::InlineKeyboardButton::Ptr(new TgBot::InlineKeyboardButton());
    btn->text = "Перезапуск OPC клиента";
    btn->callbackData = "admin_opc_restart";
    main_admin_kb->inlineKeyboard.push_back({btn});
    admin_tools_to_callback_["admin_opc_restart"] = [this](const TgBot::CallbackQuery::Ptr query) {
        driver_manager_->StartPeriodReading();
        tg_parent_->BotSendMessage(query->message->chat->id, "Перезапуск ОРС клиента");
    };

    btn = TgBot::InlineKeyboardButton::Ptr(new TgBot::InlineKeyboardButton());
    btn->text = "Рестарт приложения";
    btn->callbackData = "admin_app_restart";
    main_admin_kb->inlineKeyboard.push_back({btn});
    admin_tools_to_callback_["admin_app_restart"] = [this](const TgBot::CallbackQuery::Ptr query) {
        tg_parent_->BotSendMessage(query->message->chat->id, "Перезапуск приложения");
        emit sg_restart_application_cmd();
    };

    tg_parent_->Bot()->getEvents().onCommand("admin", [this, main_admin_kb](TgBot::Message::Ptr message) {
        QString log_string{"Получена команда администрирования."};
        emit sg_send_message_to_console(log_string);
        qInfo() << log_string;

        if(!message || !message->chat) {
            qCritical() << "Пустой указатель на сообщение в команде администрирования";
            return;
        }

        if(CheckUser(message->chat->id) == USER_TYPE::ADMIN) {
            tg_parent_->BotSendMessage(message->chat->id, "Доступные команды:", main_admin_kb);
        } else {
            tg_parent_->BotSendMessage(message->chat->id, "Вы не администратор");
        }
    });
}

void TgBotManager::process_callbacks_(const TgBot::CallbackQuery::Ptr cb_query)
{
    if(id_to_callback_.count(cb_query->data) > 0) {
        auto cb_func = id_to_callback_.at(cb_query->data);
        cb_func(cb_query);
    } else {
        tg_parent_->BotSendMessage(cb_query->message->chat->id, "Неизвестная команда.");
    }
}

std::vector<TgBotUser const *> TgBotManager::GetUsers(USER_TYPE type) {
    std::vector<TgBotUser const *> ret_vec;

    for(const auto& it: tg_parent_->GetInactiveUsers()) {
        if(users_.count(it) > 0) {
            users_.at(it).userActive = false;
        }
    }

    for(const auto& [id, it]: users_) {
        if(it.type == type || type == USER_TYPE::UNDEFINED) {
            ret_vec.push_back(&it);
        }
    }
    return ret_vec;
}

bool TgBotManager::CheckUniqueID(const std::string &id) const
{
    bool b = true;
    b = b && (id_to_text_messages_.count(id) == 0);
    b = b && (id_to_commands_.count(id) == 0);
    b = b && (id_to_events_.count(id) == 0);
    b = b && (id_to_inline_buttons_.count(id) == 0);
    b = b && (id_to_scheduled_events_.count(id) == 0);
    return b;
}

TGParent *TgBotManager::GetTGParent() const
{
    return tg_parent_.get();
}

std::vector<TGMessage*> TgBotManager::GetTGMessages() const
{
    std::vector<TGMessage*> ret_vec;
    ret_vec.reserve(id_to_text_messages_.size());

    for(auto& [id, mes_ptr]: id_to_text_messages_) {
        ret_vec.push_back(mes_ptr.get());
    }

    return ret_vec;
}


TGMessage* TgBotManager::GetTGMessage(const std::string &id) const
{
    if(id_to_text_messages_.count(id) > 0) {
        return id_to_text_messages_.at(id).get();
    }
    return nullptr;
}

TGMessage* TgBotManager::GetTGMessage(const std::string &&id) const
{
    std::string id_int = std::move(id);
    return GetTGMessage(id_int);
}

void TgBotManager::AddTGMessage(std::unique_ptr<TGMessage>&& message_u_ptr)
{
    id_to_text_messages_[message_u_ptr->GetId()] = std::move(message_u_ptr);
}

void TgBotManager::DeleteTGObject(const std::string &id)
{
    id_to_text_messages_.erase(id);
    for(auto& [it, cmd]: id_to_commands_) {
        cmd->DeleteMessage(id);
    }
    for(auto& [it, event]: id_to_events_) {
        event->DeleteMessage(id);
    }
    for(auto& [it, btn]: id_to_inline_buttons_) {
        btn->DeleteMessage(id);
    }
    for(auto& [it, event]: id_to_scheduled_events_) {
        event->DeleteMessage(id);
    }

    id_to_commands_.erase(id);
    id_to_events_.erase(id);
    id_to_inline_buttons_.erase(id);
    id_to_scheduled_events_.erase(id);
}

void TgBotManager::UpdateTGObjectID(const std::string &id)
{
    if(id_to_text_messages_.count(id) > 0) {
        id_to_text_messages_[id_to_text_messages_.at(id)->GetId()] = std::move(id_to_text_messages_.at(id));
        id_to_text_messages_.erase(id);
    }

    if(id_to_commands_.count(id) > 0) {
        id_to_commands_[id_to_commands_.at(id)->GetId()] = std::move(id_to_commands_.at(id));
        id_to_commands_.erase(id);
    }
    if(id_to_events_.count(id) > 0) {
        id_to_events_[id_to_events_.at(id)->GetId()] = std::move(id_to_events_.at(id));
        id_to_events_.erase(id);
    }
    if(id_to_inline_buttons_.count(id) > 0) {
        id_to_inline_buttons_[id_to_inline_buttons_.at(id)->GetId()] = std::move(id_to_inline_buttons_.at(id));
        id_to_inline_buttons_.erase(id);
    }
    if(id_to_scheduled_events_.count(id)) {
        id_to_scheduled_events_[id_to_scheduled_events_.at(id)->GetId()] = std::move(id_to_scheduled_events_.at(id));
        id_to_scheduled_events_.erase(id);
    }
}

std::vector<TGTriggerUserCommand*> TgBotManager::GetTGCommands() const
{
    std::vector<TGTriggerUserCommand*> ret_vec;
    ret_vec.reserve(id_to_commands_.size());

    for(auto& [id, com_ptr]: id_to_commands_) {
        ret_vec.push_back(com_ptr.get());
    }

    return ret_vec;
}

void TgBotManager::AddTGCommand(std::unique_ptr<TGTriggerUserCommand>&& command_ptr)
{
    if(command_ptr) {
        id_to_commands_[command_ptr->GetId()] = std::move(command_ptr);
    }
}

TGTriggerUserCommand* TgBotManager::GetTGCommand(const std::string &id) const
{
    if(id_to_commands_.count(id) > 0) {
        return id_to_commands_.at(id).get();
    }
    return nullptr;
}

void TgBotManager::AddTGEvent(std::unique_ptr<TGTriggerTagValue>&& event)
{
    if(event) {
        id_to_events_[event->GetId()] = std::move(event);
    }
}

void TgBotManager::AddTGScheduledEvent(std::unique_ptr<TGScheduledEvent>&& event)
{
    if(event) {
        id_to_scheduled_events_[event->GetId()] = std::move(event);
    }
}

TGScheduledEvent* TgBotManager::GetTGScheduledEvent(const std::string &id) const
{
    if(id_to_scheduled_events_.count(id) > 0) {
        return id_to_scheduled_events_.at(id).get();
    }
    return nullptr;
}

std::vector<TGTriggerTagValue*> TgBotManager::GetTGEvents() const
{
    std::vector<TGTriggerTagValue*> ret_vec;
    ret_vec.reserve(id_to_events_.size());

    for(auto& [id, ev_ptr]: id_to_events_) {
        ret_vec.push_back(ev_ptr.get());
    }

    return ret_vec;
}

std::vector<TGScheduledEvent*> TgBotManager::GetTGScheduledEvents() const
{
    std::vector<TGScheduledEvent*> ret_vec;
    ret_vec.reserve(id_to_scheduled_events_.size());

    for(auto& [id, ev_ptr]: id_to_scheduled_events_) {
        ret_vec.push_back(ev_ptr.get());
    }

    return ret_vec;
}

TGTriggerTagValue* TgBotManager::GetTGEvent(const std::string &id) const
{
    if(id_to_events_.count(id) > 0) {
        return id_to_events_.at(id).get();
    }
    return nullptr;
}

void TgBotManager::AddTGInlineButton(std::unique_ptr<TGButtonWCallback>&& button)
{
    if(button) {
        id_to_inline_buttons_[button->GetId()] = std::move(button);
    }
}

std::vector<TGButtonWCallback*> TgBotManager::GetTGInlineButtons() const
{
    std::vector<TGButtonWCallback*> ret_vec;
    ret_vec.reserve(id_to_inline_buttons_.size());

    for(auto& [id, btn_ptr]: id_to_inline_buttons_) {
        ret_vec.push_back(btn_ptr.get());
    }

    return ret_vec;
}

TGButtonWCallback* TgBotManager::GetTGInlineButton(const std::string &id) const
{
    if(id_to_inline_buttons_.count(id) > 0) {
        return id_to_inline_buttons_.at(id).get();
    }
    return nullptr;
}

TGButtonWCallback* TgBotManager::GetTGInlineButton(const std::string &&id) const
{
    std::string id_int = std::move(id);
    return GetTGInlineButton(id_int);
}

bool TgBotManager::SaveDataToJson(const QString& folder_path) const {
    QString users_file = QString("%1%2").arg(folder_path, "/users.json");
    QString tgdata_file = QString("%1%2").arg(folder_path, "/tgdata.json");
    return save_user_data_to_file_(users_file) && save_tg_data_to_file_(tgdata_file);
}

bool TgBotManager::SaveDataToJson() const
{
    return save_user_data_to_file_("users.json") && save_tg_data_to_file_("tgdata.json");
}

void TgBotManager::make_users_processing_() {
    tg_parent_->Bot()->getEvents().onAnyMessage([this](TgBot::Message::Ptr message) {
        AddOrUpdateUser(message);
    });
}

void TgBotManager::make_commands_processing_()
{
    std::vector<TgBot::BotCommand::Ptr> main_menu;
    for(const auto& [id, com_ptr]: id_to_commands_) {
        com_ptr->RegisterTrigger();
        if(com_ptr->IncludedToMainMenu()) {
            main_menu.push_back(com_ptr->GetTGCommand());
        }
    }
    if(tg_parent_->Bot() && !main_menu.empty()) {
        tg_parent_->Bot()->getApi().setMyCommands(main_menu);
    }
}

void TgBotManager::make_callback_data_()
{
    id_to_callback_.clear();
    for(const auto& [id, btn_ptr]: id_to_inline_buttons_) {
        auto [id_ret, functor] = btn_ptr->GetCallBack();
        id_to_callback_[id_ret] = functor;
    }
    for(const auto & [id, functor]: admin_tools_to_callback_) {
        id_to_callback_[id] = functor;
    }
    tg_parent_->Bot()->getEvents().onCallbackQuery([this](const TgBot::CallbackQuery::Ptr cb_query){
        process_callbacks_(cb_query);
    });
}

void TgBotManager::make_opc_communication_event_()
{
    if(server_to_sample_and_nomber_tag_.size() > 0) {
        check_opc_status_ = [this]() {
            bool b = false;
            for(const auto& [server, pair_tag_n]: server_to_sample_and_nomber_tag_) {
                if(b) continue;
                b = pair_tag_n.first->GetTagQuality() != DataTag::DataQuality::GOOD;
            }
            if(!b) return false;
            if(b && opc_comm_status_previous_scan_) return true;

            QString mes{"Ошибка связи OPC.\n"};
            for(const auto& [server, pair_tag_n]: server_to_sample_and_nomber_tag_) {
                mes.append(QString("Сервер [%1] статус связи: 0x%2\n")
                    .arg(server, DataTag::QualityToString(pair_tag_n.first->GetTagQuality())));
            }

            std::string std_mes = mes.toStdString();
            screen_symbols_(std_mes, screened_symbols_);

            for(const auto& it: GetUsers(USER_TYPE::ADMIN)) {
                tg_parent_->BotSendMessage(it->chatId, std_mes);
            }
            return true;
        };
    }
}

void TgBotManager::make_new_bot_()
{
    if(bot_started_) {
        emit sg_stop_bot_thread();
        while(bot_started_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
    }

    tg_parent_->InitializeBot(bot_token_);
}

bool TgBotManager::BotIsWorking() const {
    return bot_started_;
}

void TgBotManager::initialize_bot_() {

    if(!tg_parent_->Bot()) return;

    for(const auto& [id, user]: users_) {
        tg_parent_->AddOrUpdateChatID(user.chatId, user.type);
    }

    if(bot_name_for_channels_.has_value()) {
        tg_parent_->SetBotNameForChannel(bot_name_for_channels_.value());
    }

    try {
        tg_parent_->Bot()->getApi();
        make_admin_tools_();
        make_opc_communication_event_();
        make_users_processing_();
        make_commands_processing_();
        make_callback_data_();

    } catch (std::exception &e) {
        sl_bot_throw_exception(e.what());
        return;
    }

    emit sg_send_message_to_console(QString("Бот инициализирован"));
    qInfo() << QString("Бот инициализирован");

}

void TgBotManager::StartBot() {

    make_new_bot_();

    if(tg_parent_->Bot()) {
        QThread* bot_thread = new QThread(this);
        TgBotWorker* bot_worker = new TgBotWorker(tg_parent_->Bot());
        QObject::connect(bot_thread, &QThread::started, bot_worker, &TgBotWorker::sl_try_to_connect_api);
        QObject::connect(bot_worker, &TgBotWorker::sg_emit_text_message, this, &TgBotManager::sg_send_message_to_console);
        QObject::connect(bot_worker, &TgBotWorker::sg_emit_exception, this, &TgBotManager::sl_bot_throw_exception);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_ok, bot_thread, &QThread::quit);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_fail, bot_thread, &QThread::quit);
        QObject::connect(bot_worker, &TgBotWorker::sg_try_connection_ok, this, &TgBotManager::sl_try_to_connect_api_successful);
        QObject::connect(bot_thread, &QThread::finished, this, [this](){bot_trying_connect_api_ = false;});
        QObject::connect(bot_thread, &QThread::finished, bot_thread, &QObject::deleteLater, Qt::QueuedConnection);
        QObject::connect(this, &TgBotManager::sg_stop_bot_thread, bot_worker, &TgBotWorker::sl_stop_bot);
        QObject::connect(this, &TgBotManager::sg_stop_bot_thread, this, [this]{tg_parent_->InterruptBotHttpClient();});
        QObject::connect(bot_thread, &QThread::finished, [bot_worker] {delete bot_worker;});

        bot_trying_connect_api_ = true;

        bot_worker->moveToThread(bot_thread);
        bot_thread->start();
    }
}

void TgBotManager::sl_try_to_connect_api_successful()
{
    bot_trying_connect_api_ = false;
    initialize_bot_();

    if(tg_parent_->Bot()) {
        QThread* bot_thread = new QThread(this);
        TgBotWorker* bot_worker = new TgBotWorker(tg_parent_->Bot());
        QObject::connect(bot_thread, SIGNAL(started()), this, SIGNAL(sg_bot_thread_state_changed()));
        QObject::connect(bot_thread, SIGNAL(started()), this, SLOT(sl_bot_started()));
        QObject::connect(bot_thread, SIGNAL(started()), bot_worker, SLOT(sl_process()));
        QObject::connect(bot_worker, SIGNAL(sg_emit_text_message(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
        QObject::connect(bot_worker, SIGNAL(sg_emit_exception(QString)), this, SLOT(sl_bot_throw_exception(QString)));
        QObject::connect(bot_worker, SIGNAL(sg_finished()), bot_thread, SLOT(quit()));
        QObject::connect(bot_thread, SIGNAL(finished()), bot_thread, SLOT(deleteLater()), Qt::QueuedConnection);
        QObject::connect(bot_thread, SIGNAL(finished()), this, SLOT(sl_bot_finished()), Qt::QueuedConnection);
        QObject::connect(bot_thread, SIGNAL(finished()), this, SIGNAL(sg_bot_thread_state_changed()), Qt::QueuedConnection);
        QObject::connect(this, SIGNAL(sg_stop_bot_thread()), bot_worker, SLOT(sl_stop_bot()));
        QObject::connect(bot_thread, &QThread::finished, [bot_worker] {delete bot_worker;});

        bot_started_ = true;

        bot_worker->moveToThread(bot_thread);
        bot_thread->start();
    }
}


void TgBotManager::StopBot() {
    emit sg_stop_bot_thread();
    for(const auto& it: tg_parent_->GetInactiveUsers()) {
        if(users_.count(it) > 0) {
            users_.at(it).userActive = false;
        }
    }
}

bool TgBotManager::IsAutoRestart() const {
    return auto_restart_bot_;
}

void TgBotManager::SetAutoRestartBot(bool b) {
    auto_restart_bot_ = b;
    emit sg_send_message_to_console(QString("Изменен автозапуск бота: %1").arg(b));
    qInfo() << QString("Изменен автозапуск бота: %1").arg(b ? "АВТО" : "РУЧН");
}

void TgBotManager::sl_bot_finished() {
    bot_started_ = false;
    emit sg_send_message_to_console(QString("Бот остановлен."));
    qInfo() << QString("Бот остановлен.");
    check_events_timer_->stop();
}

void TgBotManager::sl_bot_started()
{
    emit sg_send_message_to_console(QString("Бот запущен."));
    check_events_timer_->setInterval(CHECK_EVENTS_PERIOD_);
    check_events_timer_->start();
    error_message_.reset(nullptr);
}


void TgBotManager::sl_bot_throw_exception(QString what)
{
    error_message_.reset(new QMessageBox(QMessageBox::Critical, "Ошибка!"
                                         , QString("Ошибка инициализации Бота, проверьте токен!\nПерезапустите приложение.\n %1").arg(what)
                                         , QMessageBox::Ok
                                         , nullptr
                                         , Qt::Dialog | Qt::MSWindowsFixedSizeDialogHint | Qt::WindowStaysOnTopHint));
    error_message_->setWindowModality(Qt::NonModal);
    error_message_->show();
    emit sg_stop_bot_thread();
    emit sg_send_message_to_console(QString("Бот: получено исключение - %1").arg(what));
}

void TgBotManager::sl_check_events_timer_out()
{
    for(const auto& [id, event_ptr]: id_to_events_) {
        if(event_ptr->CheckTrigger()) {
            event_ptr->Execute();
        }
    }

    for(const auto& [id, event_ptr]: id_to_scheduled_events_) {
        if(event_ptr->CheckTrigger()) {
            event_ptr->Execute();
        }
    }

    opc_comm_status_previous_scan_ = check_opc_status_();

    if(opc_comm_status_previous_scan_) {
        opc_comm_failed_time_ += CHECK_EVENTS_PERIOD_;
    } else {
        opc_comm_failed_time_ = 0;
    }

    if(opc_comm_failed_time_ > CHECK_EVENTS_PERIOD_ * TIME_TO_RESTART_APP_ON_COMM_FAIL_) {
        emit sg_restart_application_cmd(true);
        opc_comm_failed_time_ = 0;
    }
}

void TgBotManager::sl_check_restart_timer_out() {
    if(bot_started_ || bot_trying_connect_api_) {
        return;
    }

    if(auto_restart_bot_) {
        ++restart_counter_;
        emit sg_send_message_to_console(QString("Бот: автоматический рестарт."));
        qWarning() << QString("Бот: автоматический рестарт. Попытка %1").arg(restart_counter_ - 1);
        StartBot();
    }
}

const std::string& TgBotManager::GetToken() const {
    return bot_token_;
}

void TgBotManager::SendMessageToUsers(const std::vector<size_t>& id, QString message) const {
    if(!bot_started_) return;
    for(const auto& it: id) {
        if(users_.count(it) == 0) continue;
        tg_parent_->BotSendMessage(users_.at(it).chatId, message.toStdString());
    }
}

void TgBotManager::SetBotNameForChannels(QString name)
{
    if(name.length() > 0) {
        bot_name_for_channels_ = name;
    } else {
        bot_name_for_channels_.reset();
    }
}

QString TgBotManager::GetBotNameForChannels() const
{
    return bot_name_for_channels_.has_value() ? bot_name_for_channels_.value() : QString();
}

bool TgBotManager::save_user_data_to_file_(const QString& filename) const {
    QFile file(filename);
    if(file.open(QIODeviceBase::WriteOnly)) {
        QJsonArray users_array;
        for(const auto& [id, user]: users_) {
            QJsonObject obj;
            obj.insert("first_name", QString::fromStdString(user.firstName));
            obj.insert("last_name", QString::fromStdString(user.lastName));
            obj.insert("username", QString::fromStdString(user.username));
            obj.insert("chat_id", user.chatId);
            obj.insert("type", tg_user_type_to_qstring(user.type));
            obj.insert("id", id);
            obj.insert("user_active", user.userActive);
            users_array.append(obj);
        }
        QJsonDocument doc(users_array);
        bool res = file.write(doc.toJson()) != (-1);
        qInfo() << QString("Результат записи данных пользователей в файл %1: %2").arg(filename, res ? "OK" : "ERROR");
        return res;
    }
    qWarning() << QString("Ошибка открытия файла %1 для записи").arg(filename);
    return false;
}

bool TgBotManager::save_tg_data_to_file_(const QString& filename) const
{
    QFile file(filename);
    if(file.open(QIODeviceBase::WriteOnly)) {
        QJsonObject main_object;
        if(bot_name_for_channels_.has_value()) {
            main_object.insert("bot_name_for_channels", bot_name_for_channels_.value());
        }

        QJsonArray messages_array;
        for(const auto& [id, mes_ptr]: id_to_text_messages_) {
            if(mes_ptr->IsInWork()) messages_array.append(mes_ptr->SaveToJson());
        };
        main_object.insert("messages", messages_array);

        QJsonArray buttons_array;
        for(const auto& [id, btn_ptr]: id_to_inline_buttons_) {
            if(btn_ptr->IsInWork()) buttons_array.append(btn_ptr->SaveToJson());
        }
        main_object.insert("inline_buttons", buttons_array);

        QJsonArray commands_array;
        for(const auto& [it, com_ptr]: id_to_commands_) {
            if(com_ptr->IsInWork()) commands_array.append(com_ptr->SaveToJson());
        }
        main_object.insert("commands", commands_array);

        QJsonArray events_array;
        for(const auto& [it, event_ptr]: id_to_events_) {
            if(event_ptr->IsInWork()) events_array.append(event_ptr->SaveToJson());
        }
        main_object.insert("events", events_array);

        QJsonArray scheduled_events_array;
        for(const auto& [it, event_ptr]: id_to_scheduled_events_) {
            if(event_ptr->IsInWork()) scheduled_events_array.append(event_ptr->SaveToJson());
        }
        main_object.insert("scheduled_events", scheduled_events_array);

        QJsonDocument json_doc(main_object);
        qint64 res_write = file.write(json_doc.toJson());
        file.close();
        qInfo() << QString("Результат записи данных пользователей в файл %1: %2").arg(filename, res_write ? "OK" : "ERROR");
        return res_write != (-1);
    }
    qWarning() << QString("Ошибка открытия файла %1 для записи").arg(filename);
    return false;
}

bool TgBotManager::restore_user_data_from_file_() {
    QFile file("users.json");
    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);
        std::unordered_map<int64_t, TgBotUser> users_temp;

        if(json_error.error != QJsonParseError::NoError) {
            qWarning() << QString("Ошибка парсинга файла users.json: %1").arg(json_error.errorString());
            return false;
        }

        if(input_doc.isArray()) {
            for(int i = 0; i < input_doc.array().size(); ++i) {
                QJsonObject item_obj = input_doc.array().at(i).toObject();
                if((item_obj.contains("first_name") && item_obj.value("first_name").isString())
                    && (item_obj.contains("last_name") && item_obj.value("last_name").isString())
                    && (item_obj.contains("username") && item_obj.value("username").isString())
                    && (item_obj.contains("chat_id") && item_obj.value("chat_id").isDouble())
                    && (item_obj.contains("type") && item_obj.value("type").isString())
                    && (item_obj.contains("id") && item_obj.value("id").isDouble())) {

                    TgBotUser user_tmp;
                    user_tmp.chatId = static_cast<int64_t>(item_obj.value("chat_id").toInteger());
                    user_tmp.firstName = item_obj.value("first_name").toString().toStdString();
                    user_tmp.lastName = item_obj.value("last_name").toString().toStdString();
                    user_tmp.username = item_obj.value("username").toString().toStdString();
                    user_tmp.type = tg_user_type_from_qstring(item_obj.value("type").toString());
                    user_tmp.id = static_cast<int64_t>(item_obj.value("id").toInteger());

                    users_temp.emplace(static_cast<int64_t>(item_obj.value("id").toInteger()), user_tmp);
                } else {
                    qWarning() << "Файл настроек users.json поврежден!";
                    return false;
                }
            }
            users_.swap(users_temp);
            emit sg_send_message_to_console("Данные пользователей прочитаны.");
            qInfo() << "Данные пользователей прочитаны из users.json";
            return true;
        }
    }

    return false;
}

bool TgBotManager::restore_tg_data_from_file_()
{
    id_to_inline_buttons_.clear();
    id_to_text_messages_.clear();
    id_to_events_.clear();
    id_to_commands_.clear();
    id_to_scheduled_events_.clear();

    QFile file("tgdata.json");
    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);

        if(json_error.error != QJsonParseError::NoError) {
            emit sg_send_message_to_console(QString("Ошибка парсинга файла данных: %1.").arg(json_error.errorString()));
            qWarning() << QString("Ошибка парсинга файла данных: %1.").arg(json_error.errorString());
            return false;
        }

        bool res = true;

        if((input_doc.isObject())
            && (input_doc.object().contains("messages") && input_doc.object().value("messages").isArray())
            && (input_doc.object().contains("inline_buttons") && input_doc.object().value("inline_buttons").isArray())
            && (input_doc.object().contains("commands") && input_doc.object().value("commands").isArray())
            && (input_doc.object().contains("events") && input_doc.object().value("events").isArray())
            && (input_doc.object().contains("scheduled_events") && input_doc.object().value("scheduled_events").isArray())) {

            QJsonArray buttons_obj = input_doc.object().value("inline_buttons").toArray();
            for(int i = 0; i < buttons_obj.size(); ++i) {
                auto btn_ptr = parse_inline_button_from_json_wo_messages_(buttons_obj.at(i).toObject());
                if(btn_ptr) {
                    id_to_inline_buttons_.emplace(btn_ptr->GetId(), std::move(btn_ptr));
                } else {
                    qWarning() << QString("Ошибка парсинга кнопки i=(%1) из tgdata.json").arg(i);
                    res = false;
                }
            }

            QJsonArray tmp_array = input_doc.object().value("messages").toArray();
            for(int i = 0; i < tmp_array.size(); ++i) {
                auto mes_ptr = parse_message_from_json_(tmp_array.at(i).toObject());
                if(mes_ptr) {
                    id_to_text_messages_.emplace(mes_ptr->GetId(), std::move(mes_ptr));
                } else {
                    qWarning() << QString("Ошибка парсинга сообщения i=(%1) из tgdata.json").arg(i);
                }
            }

            for(int i = 0; i < buttons_obj.size(); ++i) {
                parse_inline_button_from_json_add_messages_(buttons_obj.at(i).toObject());
            }

            tmp_array = input_doc.object().value("commands").toArray();
            for(int i = 0; i < tmp_array.size(); ++i) {
                auto com_ptr = parse_user_command_from_json_(tmp_array.at(i).toObject());
                if(com_ptr) {
                    id_to_commands_.emplace(com_ptr->GetId(), std::move(com_ptr));
                } else {
                    qWarning() << QString("Ошибка парсинга команды i=(%1) из tgdata.json").arg(i);
                    res = false;
                }
            }

            tmp_array = input_doc.object().value("events").toArray();
            for(int i = 0; i < tmp_array.size(); ++i) {
                auto event_ptr = parse_tag_trigger_from_json_(tmp_array.at(i).toObject());
                if(event_ptr) {
                    id_to_events_.emplace(event_ptr->GetId(), std::move(event_ptr));
                } else {
                    qWarning() << QString("Ошибка парсинга события i=(%1) из tgdata.json").arg(i);
                    res = false;
                }
            }

            tmp_array = input_doc.object().value("scheduled_events").toArray();
            for(int i = 0; i < tmp_array.size(); ++i) {
                auto event_ptr = parse_scheduled_events_from_json_(tmp_array.at(i).toObject());
                if(event_ptr) {
                    id_to_scheduled_events_.emplace(event_ptr->GetId(), std::move(event_ptr));
                } else {
                    qWarning() << QString("Ошибка парсинга события i=(%1) из tgdata.json").arg(i);
                    res = false;
                }
            }
        };

        if(input_doc.isObject()
            && input_doc.object().contains("bot_name_for_channels")
            && input_doc.object().value("bot_name_for_channels").isString()) {
            bot_name_for_channels_ = input_doc.object().value("bot_name_for_channels").toString();
        }

        emit sg_send_message_to_console("Файл данных прочитан.");
        qInfo() << QString("Файл данных tgdata.json прочитан");
        return res;
    }
    emit sg_send_message_to_console(QString("Неверный формат данных файла \"tgdata.json\""));
    qWarning() << QString("Неверный формат данных файла tgdata.json");
    return false;
}

std::unique_ptr<TGMessage> TgBotManager::parse_message_from_json_(QJsonObject obj)
{
    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("text") && obj.value("text").isString())
        && (obj.contains("inline_buttons") && obj.value("inline_buttons").isArray())) {
        std::unique_ptr<TGMessage> ret_mes = std::unique_ptr<TGMessage>(new TGMessage(tg_parent_.get()));
        ret_mes->SetId(obj.value("id").toString().toStdString());
        ret_mes->SetText(obj.value("text").toString().toStdString());

        bool b = true;
        if(obj.contains("in_work") && obj.value("in_work").isBool()) {
            b = obj.value("in_work").toBool();
        }
        ret_mes->SetObjectInWork(b);

        for(int i = 0; i < obj.value("inline_buttons").toArray().size(); ++i) {
            if(obj.value("inline_buttons").toArray().at(i).isString()
                && GetTGInlineButton(obj.value("inline_buttons").toArray().at(i).toString().toStdString())) {
                auto btn_ptr = GetTGInlineButton(obj.value("inline_buttons").toArray().at(i).toString().toStdString());
                ret_mes->AddTGInlineButton(btn_ptr);
            } else {
                return nullptr;
            }
        }
        return ret_mes;
    }
    return nullptr;
}

std::unique_ptr<TGTriggerUserCommand> TgBotManager::parse_user_command_from_json_(QJsonObject obj)
{
    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("command") && obj.value("command").isString())
        && (obj.contains("description") && obj.value("description").isString())
        && (obj.contains("main_menu") && obj.value("main_menu").isBool())
        && (obj.contains("messages") && obj.value("messages").isArray())
        && (obj.contains("auth_level") && obj.value("auth_level").isString())) {

        std::unique_ptr<TGTriggerUserCommand> ret_cmd = std::unique_ptr<TGTriggerUserCommand>(new TGTriggerUserCommand(tg_parent_.get()));
        ret_cmd->SetId(obj.value("id").toString().toStdString());

        bool b = true;
        if(obj.contains("in_work") && obj.value("in_work").isBool()) {
            b = obj.value("in_work").toBool();
        }
        ret_cmd->SetObjectInWork(b);

        ret_cmd->SetCommand(obj.value("command").toString().toStdString(), obj.value("description").toString().toStdString());
        ret_cmd->IncludeToMainMenu(obj.value("main_menu").toBool());
        ret_cmd->SetAuthorizationLevel(tg_user_type_from_qstring(obj.value("auth_level").toString()));
        for(int i = 0; i < obj.value("messages").toArray().size(); ++i) {
            if(obj.value("messages").toArray().at(i).isString()
                && (GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString()))) {

                auto mes_ptr = GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString());
                ret_cmd->AddTGMessage(mes_ptr);
            } else {
                return nullptr;
            }
        }
        return ret_cmd;
    }
    return nullptr;
}

std::unique_ptr<TGTriggerTagValue> TgBotManager::parse_tag_trigger_from_json_(QJsonObject obj)
{

    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("comparision_type") && obj.value("comparision_type").isString())
        && (obj.contains("tag_id") && obj.value("tag_id").isDouble())
        && (obj.contains("messages") && obj.value("messages").isArray())
        && (obj.contains("auth_level") && obj.value("auth_level").isString())
        && (obj.contains("hysterezis") && obj.value("hysterezis").isDouble())) {

        std::unique_ptr<TGTriggerTagValue> ret_trg = std::unique_ptr<TGTriggerTagValue>(new TGTriggerTagValue(tg_parent_.get()));
        ret_trg->SetId(obj.value("id").toString().toStdString());

        bool b = true;
        if(obj.contains("in_work") && obj.value("in_work").isBool()) {
            b = obj.value("in_work").toBool();
        }
        ret_trg->SetObjectInWork(b);

        QString comp_type_qstr = obj.value("comparision_type").toString();
        COMPARE_TYPE comp_type;
        ValueVariant val, hys;
        if(comp_type_qstr == "EQUAL") {
            comp_type = COMPARE_TYPE::EQUAL;
        } else if(comp_type_qstr == "LESS") {
            comp_type = COMPARE_TYPE::LESS;
        } else if(comp_type_qstr == "GREATER") {
            comp_type = COMPARE_TYPE::GREATER;
        } else if(comp_type_qstr == "CHANGED") {
            comp_type = COMPARE_TYPE::CHANGED;
            val = 0;
            hys = 0;
        } else if(comp_type_qstr == "NOT_EQUAL") {
            comp_type = COMPARE_TYPE::NOT_EQUAL;
        } else {
            return nullptr;
        }

        if(obj.contains("int_val") && obj.value("int_val").isDouble()) {
            val = static_cast<int64_t>(obj.value("int_val").toInteger());
            hys = static_cast<int64_t>(obj.value("hysterezis").toInteger());
        }

        if(obj.contains("double_val") && obj.value("double_val").isDouble()) {
            val = static_cast<double>(obj.value("double_val").toDouble());
            hys = static_cast<double>(obj.value("hysterezis").toDouble());
        }

        ret_trg->SetTagTrigger(static_cast<size_t>(obj.value("tag_id").toInteger()), comp_type, val, hys);
        ret_trg->SetAuthorizationLevel(tg_user_type_from_qstring(obj.value("auth_level").toString()));

        for(int i = 0; i < obj.value("messages").toArray().size(); ++i) {
            if(obj.value("messages").toArray().at(i).isString()
                && (GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString()))) {

                auto mes_ptr = GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString());
                ret_trg->AddTGMessage(mes_ptr);
            } else {
                return nullptr;
            }
        }
        return ret_trg;
    }
    return nullptr;
}

std::unique_ptr<TGButtonWCallback> TgBotManager::parse_inline_button_from_json_wo_messages_(QJsonObject obj)
{
    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("name") && obj.value("name").isString())
        && (obj.contains("messages") && obj.value("messages").isArray())) {

        std::unique_ptr<TGButtonWCallback> ret_btn = std::unique_ptr<TGButtonWCallback>(new TGButtonWCallback(tg_parent_.get()));
        ret_btn->SetId(obj.value("id").toString().toStdString());
        ret_btn->SetButtonName(obj.value("name").toString().toStdString());

        bool b = true;
        if(obj.contains("in_work") && obj.value("in_work").isBool()) {
            b = obj.value("in_work").toBool();
        }
        ret_btn->SetObjectInWork(b);

        if(obj.contains("tag_to_set") && obj.value("tag_to_set").isArray()) {

            for(const auto& tag_obj: obj.value("tag_to_set").toArray()) {
                if(!tag_obj.isObject() || !tag_obj.toObject().contains("tag_id") || !tag_obj.toObject().value("tag_id").isDouble()) continue;
                ValueVariant val;
                if(tag_obj.toObject().contains("int_value")) {
                    val = tag_obj.toObject().value("int_value").toInteger();
                } else if(tag_obj.toObject().contains("double_value")) {
                    val = tag_obj.toObject().value("double_value").toDouble();
                } else if(tag_obj.toObject().contains("string_value")) {
                    val = tag_obj.toObject().value("string_value").toString();
                } else {
                    val = 0;
                }
                ret_btn->AddOPCTagWValue(tag_obj.toObject().value("tag_id").toInteger(), val);
            }
        }
        return ret_btn;
    }
    return nullptr;
}

std::unique_ptr<TGScheduledEvent> TgBotManager::parse_scheduled_events_from_json_(QJsonObject obj)
{
    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("scheduled_task") && obj.value("scheduled_task").isObject())
        && (obj.contains("messages") && obj.value("messages").isArray())
        && (obj.contains("auth_level") && obj.value("auth_level").isString()))
    {
        std::unique_ptr<TGScheduledEvent> ret_event = std::unique_ptr<TGScheduledEvent>(new TGScheduledEvent(tg_parent_.get(), PeriodicalTask::Period::HOURLY));
        auto opt_task = PeriodicalTask::FromJson(obj.value("scheduled_task").toObject());
        if(!opt_task.has_value()) return nullptr;

        ret_event->SetTask(opt_task.value());
        ret_event->SetId(obj.value("id").toString().toStdString());
        ret_event->SetAuthorizationLevel(tg_user_type_from_qstring(obj.value("auth_level").toString()));

        bool b = true;
        if(obj.contains("in_work") && obj.value("in_work").isBool()) {
            b = obj.value("in_work").toBool();
        }
        ret_event->SetObjectInWork(b);

        for(int i = 0; i < obj.value("messages").toArray().size(); ++i) {
            if(obj.value("messages").toArray().at(i).isString()
                && (GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString()))) {
                auto mes_ptr = GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString());
                ret_event->AddTGMessage(mes_ptr);

            } else {
                return nullptr;
            }
        }
        return ret_event;
    }
    return nullptr;
}

bool TgBotManager::parse_inline_button_from_json_add_messages_(QJsonObject obj)
{
    if((obj.contains("id") && obj.value("id").isString())
        && (obj.contains("name") && obj.value("name").isString())
        && (obj.contains("messages") && obj.value("messages").isArray())) {
        auto btn_ptr = GetTGInlineButton(obj.value("id").toString().toStdString());
        for(int i = 0; i < obj.value("messages").toArray().size(); ++i) {
            if(obj.value("messages").toArray().at(i).isString()
                && (GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString()))) {

                auto mes_ptr = GetTGMessage(obj.value("messages").toArray().at(i).toString().toStdString());
                btn_ptr->AddTGMessage(mes_ptr);
            } else {
                return false;
            }
        }
        return true;
    }
    return false;
}
