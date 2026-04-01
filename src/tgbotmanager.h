#ifndef TGBOTMANAGER_H
#define TGBOTMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QMessageBox>

#include "tgbot/types/User.h"
#include "tgbot/types/Message.h"
#include "tgbot/types/CallbackQuery.h"

#include "tgobject.h"
#include "tgbot/Bot.h"
#include "datatagregistry.h"

class DataTag;
class SourceDriverManager;
class TGTriggerUserCommand;
class TGTriggerTagValue;
class TGMessage;
class TGParent;
class QTimer;
class TGScheduledEvent;
class TGButtonWCallback;
class TgParent;
class QMessageBox;


namespace TGHELPER {
const std::string secure_key_string = "dyzsxpRl8souDS8CNLuyAolMD4NTPQRtpH4xZXLv2WlrtUv8al";
const size_t PREFIX_POSTFIX_KEY_LENGTH = 100;
QByteArray EncryptDecryptTokenString(const QByteArray& text, const std::string& key);
bool WriteTokenToFile(const QString& filename, const std::string& token_raw);
std::string ReadTokenFromFile(const QString& filename);
}

class TgBotUser: public TgBot::User {
public:
    TgBotUser() = default;
    TgBotUser(TgBot::User& other);
    TgBotUser(TgBot::Message::Ptr message);
    int64_t chatId = 0;
    QDateTime lastMessageDT = QDateTime();
    USER_TYPE type = USER_TYPE::NEW_USER;
    bool userActive = true;
    void UpdateData(TgBot::User& other);
    void UpdateData(TgBot::Message::Ptr messag);

private:
    void copy_fields_(TgBot::User other);
    void copy_fields_(TgBot::Chat chat_ptr);
};

class TgBotManager: public QObject
{
    Q_OBJECT
public:
    explicit TgBotManager(const std::string& bot_token, SourceDriverManager& driver_manager);
    TgBotManager(SourceDriverManager& driver_manager);
    virtual ~TgBotManager();

    bool BotIsWorking() const;
    void StartBot();
    void StopBot();
    bool IsAutoRestart() const;
    void SetAutoRestartBot(bool b);
    const std::string& GetToken() const;

    void AddOrUpdateUser(TgBot::Message::Ptr message);
    bool RegisterUser(int64_t user_id);
    bool UnRegisterUser(int64_t user_id);
    bool SetAdmin(int64_t user_id);
    bool ResetAdmin(int64_t user_id);
    bool BanUser(int64_t user_id);
    USER_TYPE CheckUser(int64_t user_id) const;
    std::vector<TgBotUser const *> GetUsers(USER_TYPE type = USER_TYPE::UNDEFINED);
    void SendMessageToUsers(const std::vector<size_t>& id, QString message) const;

    void SetBotNameForChannels(QString name);
    QString GetBotNameForChannels() const;

    bool CheckUniqueID(const std::string& id) const;
    TGParent* GetTGParent() const;

    void DeleteTGObject(const std::string& id);
    void UpdateTGObjectID(const std::string& id);

    std::vector<TGMessage*> GetTGMessages() const;
    TGMessage* GetTGMessage(const std::string& id) const;
    TGMessage* GetTGMessage(const std::string&& id) const;
    void AddTGMessage(std::unique_ptr<TGMessage>&& message_u_ptr);


    std::vector<TGTriggerUserCommand*> GetTGCommands() const;
    TGTriggerUserCommand* GetTGCommand(const std::string& id) const;
    void AddTGCommand(std::unique_ptr<TGTriggerUserCommand>&& command_ptr);

    std::vector<TGTriggerTagValue*> GetTGEvents() const;
    TGTriggerTagValue* GetTGEvent(const std::string& id) const;
    TGScheduledEvent* GetTGScheduledEvent(const std::string& id) const;
    std::vector<TGScheduledEvent*> GetTGScheduledEvents() const;
    void AddTGEvent(std::unique_ptr<TGTriggerTagValue>&& event);
    void AddTGScheduledEvent(std::unique_ptr<TGScheduledEvent>&& event);

    std::vector<TGButtonWCallback*> GetTGInlineButtons() const;
    TGButtonWCallback* GetTGInlineButton(const std::string& id) const;
    TGButtonWCallback* GetTGInlineButton(const std::string&& id) const;
    void AddTGInlineButton(std::unique_ptr<TGButtonWCallback>&& button);

    bool SaveDataToJson() const;
    bool SaveDataToJson(const QString& filename) const;
signals:
    void sg_send_message_to_console(QString mes);
    void sg_bot_thread_state_changed();
    void sg_stop_bot_thread();
    void sg_restart_application_cmd(bool auto_restart = false);

private slots:
    void sl_bot_finished();
    void sl_bot_started();
    void sl_bot_throw_exception(QString what);
    void sl_check_events_timer_out();
    void sl_check_restart_timer_out();
    void sl_try_to_connect_api_successful();

private:
    std::string bot_token_;

    bool auto_restart_bot_ = false;
    bool bot_started_ = false;
    bool bot_trying_connect_api_ = false;

    SourceDriverManager* driver_manager_;

    std::unique_ptr<TGParent> tg_parent_ = nullptr;
    QTimer* check_events_timer_ = nullptr;
    QTimer* check_restart_timer_ = nullptr;
    std::unique_ptr<QMessageBox> error_message_ = nullptr;

    std::optional<QString> bot_name_for_channels_;

    int restart_counter_ = 0;

    const int CHECK_EVENTS_PERIOD_ = 2000;
    const int RESTART_BOT_PERIOD_ = 10000;
    const int TIME_TO_RESTART_APP_ON_COMM_FAIL_ = 150;
    const std::string screened_symbols_= ".=-()";


    std::unordered_map<int64_t, TgBotUser> users_;

    std::unordered_map<std::string, std::unique_ptr<TGMessage>> id_to_text_messages_;
    std::unordered_map<std::string, std::unique_ptr<TGTriggerUserCommand>> id_to_commands_;
    std::unordered_map<std::string, std::unique_ptr<TGTriggerTagValue>> id_to_events_;
    std::unordered_map<std::string, std::unique_ptr<TGButtonWCallback>> id_to_inline_buttons_;
    std::unordered_map<std::string, std::unique_ptr<TGScheduledEvent>> id_to_scheduled_events_;
    std::unordered_map<std::string, std::function<void(const TgBot::CallbackQuery::Ptr)>> id_to_callback_;
    std::unordered_map<std::string, std::function<void(const TgBot::CallbackQuery::Ptr)>> admin_tools_to_callback_;
    std::unordered_map<QString, std::pair<std::shared_ptr<DataTag>, size_t>> server_to_sample_and_nomber_tag_;
    std::function<bool()> check_opc_status_ = [](){return false;};

    bool opc_comm_status_previous_scan_ = false;
    int opc_comm_failed_time_ = 0;


    void process_callbacks_(const TgBot::CallbackQuery::Ptr cb_query);
    void make_admin_tools_();
    void make_users_processing_();
    void make_commands_processing_();
    void make_callback_data_();
    void make_opc_communication_event_();
    void make_new_bot_();
    void initialize_bot_();
    bool save_user_data_to_file_(const QString& filename) const;
    bool save_tg_data_to_file_(const QString& filename) const;
    bool restore_user_data_from_file_();
    bool restore_tg_data_from_file_();
    void screen_symbols_(std::string& text, const std::string& symbols) const;

    std::unique_ptr<TGMessage> parse_message_from_json_(QJsonObject obj);
    std::unique_ptr<TGTriggerUserCommand> parse_user_command_from_json_(QJsonObject obj);
    std::unique_ptr<TGTriggerTagValue> parse_tag_trigger_from_json_(QJsonObject obj);
    std::unique_ptr<TGButtonWCallback> parse_inline_button_from_json_wo_messages_(QJsonObject obj);
    std::unique_ptr<TGScheduledEvent> parse_scheduled_events_from_json_(QJsonObject obj);
    bool parse_inline_button_from_json_add_messages_(QJsonObject obj);
};

#endif // TGBOTMANAGER_H
