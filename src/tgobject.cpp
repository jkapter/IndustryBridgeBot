#include "tgobject.h"

#include "tgbot/Bot.h"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#include "datatag.h"
#include "datatagregistry.h"

using namespace DATATAG;

QString tg_user_type_to_qstring(USER_TYPE type)
{
    switch(type) {
    case USER_TYPE::ADMIN: return "ADMIN";
    case USER_TYPE::BANNED: return "BANNED";
    case USER_TYPE::NEW_USER: return "NEW_USER";
    case USER_TYPE::UNDEFINED: return "UNDEFINED";
    case USER_TYPE::UNREGISTERED: return "UNREGISTERED";
    case USER_TYPE::REGISTRD: return "REGISTRD";
    case USER_TYPE::CHANNEL: return "CHANNEL";
    }
    return "";
}

USER_TYPE tg_user_type_from_qstring(QString type)
{
    if(type == "ADMIN") return USER_TYPE::ADMIN;
    if(type == "BANNED") return USER_TYPE::BANNED;
    if(type == "NEW_USER") return USER_TYPE::NEW_USER;
    if(type == "UNDEFINED") return USER_TYPE::UNDEFINED;
    if(type == "REGISTRD") return USER_TYPE::REGISTRD;
    if(type == "CHANNEL") return USER_TYPE::CHANNEL;
    if(type == "UNREGISTERED") return USER_TYPE::UNREGISTERED;
    return USER_TYPE::UNDEFINED;
}

//===============================================================
//============ T G B O T P A R E N T ============================
//===============================================================

TGParent::TGParent(DataTagRegistry& tag_registry_ptr)
    : tag_registry_ptr_(&tag_registry_ptr)
    , bot_ptr_(nullptr)
{}

const DataTagRegistry* TGParent::TagManager() const {
    return tag_registry_ptr_;
}

TgBot::Bot* TGParent::Bot() {
    return bot_ptr_.get();
}

void TGParent::AddOrUpdateChatID(int64_t user, USER_TYPE type) {
    for(auto& [t, id_set]: user_permission_to_chat_id_) {
        if(id_set.count(user) > 0 && t != type) {
            id_set.erase(user);
        }
    }
    user_permission_to_chat_id_[type].insert(user);
}

void TGParent::DeleteChatID(int64_t user) {
    for(auto& [type, us]: user_permission_to_chat_id_) {
        us.erase(user);
    }
}

void TGParent::ClearChatIdData()
{
    user_permission_to_chat_id_.clear();
    inactive_users_.clear();
}

void TGParent::ResetTgBotPtr()
{
    bot_ptr_.reset(nullptr);
    ClearChatIdData();
}

void TGParent::InitializeBot(const std::string& token)
{
    bot_ptr_ = std::make_unique<TgBot::Bot>(token);
    ClearChatIdData();
}

std::vector<int64_t> TGParent::GetChatIDs(USER_TYPE min_auth_level) const {
    std::vector<int64_t> ret_vec;

    for(const auto& [type, id_set]: user_permission_to_chat_id_) {
        if(type >= min_auth_level && type != USER_TYPE::BANNED) {
            ret_vec.insert(ret_vec.end(), id_set.begin(), id_set.end());
        }
    }

    return ret_vec;
}

bool TGParent::CheckUserChatId(int64_t chat_id, USER_TYPE min_auth_level) const
{
    for(const auto& [type, id_set]: user_permission_to_chat_id_) {
        if(type >= min_auth_level && type != USER_TYPE::BANNED && id_set.count(chat_id) > 0) {
            return true;
        }
    }
    return false;
}

void TGParent::BotSendMessage(int64_t chat_id, const std::string& text, TgBot::InlineKeyboardMarkup::Ptr inline_buttons)
{
    if(bot_ptr_) {
        std::string text_to_send(text);
        if(user_permission_to_chat_id_.count(USER_TYPE::CHANNEL) > 0
            && user_permission_to_chat_id_.at(USER_TYPE::CHANNEL).count(chat_id) > 0
            && bot_name_for_channel_.has_value()) {

            text_to_send.insert(0, QString("%1\r\n").arg(bot_name_for_channel_.value()).toStdString());
        }
        try {
            bot_ptr_->getApi().sendMessage(chat_id, text_to_send, nullptr, nullptr, inline_buttons, "MarkdownV2");
            inactive_users_.erase(chat_id);
        } catch (std::exception& e) {
            qCritical() << QString("Получено исключение при отправке сообщения пользователю id = [%1]: [%2]")
                               .arg(chat_id)
                               .arg(e.what());
            inactive_users_.insert(chat_id);
        }
    } else {
        qCritical() << "Указатель на TgBot не инициализирован!";
    }
}

const std::unordered_set<int64_t>& TGParent::GetInactiveUsers() const
{
    return inactive_users_;
}

void TGParent::SetBotNameForChannel(QString name)
{
    if(name.length() > 0) {
        bot_name_for_channel_ = name;
    } else {
        bot_name_for_channel_.reset();
    }
}

//=============================================
//===== T G O B J E C T =======================
//=============================================

TGObject::TGObject(TGParent* parent)
    : parent_(parent)
{
    GenerateRandomId();
}

TGParent* TGObject::GetTGParent() const {
    return parent_;
}

const std::string& TGObject::GetId() const {
    return id_;
}

void TGObject::SetId(const std::string& id) {
    id_ = id;
}

const std::string& TGObject::GenerateRandomId() {
    id_ = StringTools::generateRandomString(15);
    return id_;
}

void TGObject::SetObjectInWork(bool b)
{
    in_work_ = b;
}

bool TGObject::IsInWork() const
{
    return in_work_;
}

//=============================================
//===== T G M E S S A G E =====================
//=============================================

TGMessage::TGMessage(const std::string& message, TGParent* parent)
    : TGObject(parent)
    , message_(message)
{
    screen_symbols_(message_, screened_symbols_);
    parse_message_();
    get_tags_ptr_();
}

TGMessage::TGMessage(std::string&& message, TGParent* parent)
    : TGObject(parent)
    , message_(std::move(message))
{
    screen_symbols_(message_, screened_symbols_);
    parse_message_();
}

void TGMessage::SetText(const std::string& mes) {
    message_ = mes;
    screen_symbols_(message_, screened_symbols_);
    parse_message_();
    get_tags_ptr_();
}

void TGMessage::SetText(std::string&& mes) {
    message_ = std::move(mes);
    screen_symbols_(message_, screened_symbols_);
    parse_message_();
    get_tags_ptr_();
}

void TGMessage::Send(int64_t chat_id) const {
    if(!IsInWork()) return;
    TgBot::InlineKeyboardMarkup::Ptr inline_kb = nullptr;
    if(inline_buttons_.size() > 0) {
        inline_kb = TgBot::InlineKeyboardMarkup::Ptr(new TgBot::InlineKeyboardMarkup);
        for(auto& it: inline_buttons_) {
            inline_kb->inlineKeyboard.push_back({it->GetTgInlineButton()});
        }
    }
    GetTGParent()->BotSendMessage(chat_id, GetTextToSend(), inline_kb);
}

const std::string& TGMessage::GetText() const {
    return message_;
}

const std::string TGMessage::GetTextToSend() const
{
    std::string message{""};
    for(const auto& it: message_parts_) {
        if(segments_to_tag_id_.count(&it)) {
            if(id_to_data_tags_.count(segments_to_tag_id_.at(&it))) {
                message += id_to_data_tags_.at(segments_to_tag_id_.at(&it))->GetStringValue().replace(".", ",").toStdString();
            } else {
                message += "NOT FOUND";
            }
        } else {
            message += it;
        }
    }

    return message;
}

bool TGMessage::HasButtons() const
{
    return !inline_buttons_.empty();
}

bool TGMessage::HasTags() const
{
    return !id_to_data_tags_.empty();
}

void TGMessage::AddTGInlineButton(TGButtonWCallback* btn) {
    if(!btn->IsInWork()) return;
    inline_buttons_.push_back(btn);
}

void TGMessage::DeleteInlineButton(std::string &id)
{
    for(auto it = inline_buttons_.begin(); it != inline_buttons_.end(); ++it) {
        if((*it)->GetId() == id) {
            inline_buttons_.erase(it);
            break;
        }
    }
}

void TGMessage::ClearInlineButtons()
{
    inline_buttons_.clear();
}

std::vector<TGButtonWCallback*> TGMessage::GetButtons() const
{
    return inline_buttons_;
}

QJsonObject TGMessage::SaveToJson() const
{
    QJsonObject ret_obj;
    ret_obj.insert("id", QString::fromStdString(GetId()));
    ret_obj.insert("text", QString::fromStdString(GetText()));
    ret_obj.insert("in_work", in_work_);
    QJsonArray buttons;
    for(const auto& it: inline_buttons_) {
        buttons.append(QString::fromStdString(it->GetId()));
    }
    ret_obj.insert("inline_buttons", buttons);
    QJsonArray tags_ids;
    for(const auto& it: GetTagIDs()) {
        tags_ids.append(static_cast<int64_t>(it));
    }
    ret_obj.insert("opc_tags", tags_ids);
    return ret_obj;
}

void TGMessage::screen_symbols_(std::string& text, const std::string& symbols) const
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

void TGMessage::parse_message_() {
    std::string mes = message_;
    size_t beg = 0;
    size_t end = 0;
    end = mes.find("#TAG:", beg);
    message_parts_.clear();
    segments_to_tag_id_.clear();
    while(end != std::string::npos) {
        message_parts_.push_back(mes.substr(beg, end - beg));
        beg = end + 5;
        end = mes.find('#', beg);
        if(end != std::string::npos) {
            size_t tag_id = 0;
            std::string id_str = mes.substr(beg, end - beg + 1);
            auto[ptr, ec] = std::from_chars(id_str.data(), id_str.data() + id_str.size(), tag_id);
            if(ec == std::errc() && *ptr == '#') {
                id_str = "#";
                id_str += std::to_string(tag_id);
                message_parts_.push_back(id_str);
                segments_to_tag_id_[&message_parts_.back()] = tag_id;
            } else {
                message_parts_.push_back("NOT FOUND");
            }
            beg = end + 1;
            end = mes.find("#TAG:", beg);
        }
    }
    if(beg != end) {
        message_parts_.push_back(mes.substr(beg));
    }
}

void TGMessage::get_tags_ptr_() {
    id_to_data_tags_.clear();
    if(!GetTGParent()->TagManager()) {
        qCritical() << "Указатель на OPCManager не инициализирован!";
        return;
    }
    for(const auto& [str_ref, id]: segments_to_tag_id_) {
        id_to_data_tags_[id] = this->GetTGParent()->TagManager()->GetTagOfId(id);
    }
}

std::vector<size_t> TGMessage::GetTagIDs() const {
    auto tags = std::views::keys(id_to_data_tags_);
    return {tags.begin(), tags.end()};
}


//===================================================
//==== T G T R I G G E R C O M M A N D ==============
//===================================================

TGTriggerUserCommand::TGTriggerUserCommand(std::string& command, TGParent* parent)
    : TGTrigger(parent)
    , command_(command)
{}
TGTriggerUserCommand::TGTriggerUserCommand(std::string&& command, TGParent* parent)
    : TGTrigger(parent)
    , command_(std::move(command))
{}

void TGTriggerUserCommand::SetCommand(const std::string& command, const std::string& description) {
    command_ = command;
    description_ = description;
}

std::pair<const std::string&, const std::string&> TGTriggerUserCommand::GetCommandAndDescription() const {
    return {command_, description_};
}

TgBot::BotCommand::Ptr TGTriggerUserCommand::GetTGCommand() const {
    TgBot::BotCommand::Ptr ret_ptr(new TgBot::BotCommand);
    ret_ptr->command = command_;
    ret_ptr->description = description_;
    return ret_ptr;
}

QJsonObject TGTriggerUserCommand::SaveToJson() const
{
    QJsonObject ret_obj;
    ret_obj.insert("id", QString::fromStdString(GetId()));
    ret_obj.insert("command", QString::fromStdString(command_));
    ret_obj.insert("description", QString::fromStdString(description_));
    ret_obj.insert("in_work", in_work_);
    QJsonArray messages;
    for(const auto& it: GetMessages()) {
        messages.append(QString::fromStdString(it->GetId()));
    }
    ret_obj.insert("messages", messages);
    ret_obj.insert("main_menu", included_to_main_menu_);
    ret_obj.insert("auth_level", tg_user_type_to_qstring(user_type_));
    return ret_obj;
}

void TGTriggerUserCommand::RegisterTrigger() {
    if(!IsInWork()) return;
    auto CommandExec = [this](const TgBot::Message::Ptr mes){
        for(const auto& mes_ptr: GetMessages()) {
            if(mes_ptr) {
                mes_ptr->Send(mes->chat->id);
            }
        }
    };
    GetTGParent()->Bot()->getEvents().onCommand(command_, CommandExec);
}

void TGTriggerUserCommand::IncludeToMainMenu(bool b)
{
    included_to_main_menu_ = b;
}

bool TGTriggerUserCommand::IncludedToMainMenu() const
{
    return included_to_main_menu_;
}

//===================================================
//==== T G T R I G G E R T A G V A L U E=============
//===================================================

ValueVariant DATATAG::operator-(ValueVariant lhs, ValueVariant rhs);
ValueVariant DATATAG::operator+(ValueVariant lhs, ValueVariant rhs);

void TGTriggerTagValue::SetTagTrigger(size_t id, COMPARE_TYPE type, ValueVariant value, ValueVariant hysterezis) {
    tag_id_ = id;
    tag_ptr_ = GetTGParent()->TagManager()->GetTagOfId(id);
    if(!tag_ptr_) return;

    type_ = type;
    iVal_ = value;
    hysterezis_ = hysterezis;

    CheckF_ = [this]() {
        if(this->type_ == COMPARE_TYPE::EQUAL) return tag_ptr_->GetValue(false) == iVal_;
        if(this->type_ == COMPARE_TYPE::GREATER) return tag_ptr_->GetValue(false) > iVal_;
        if(this->type_ == COMPARE_TYPE::LESS) return tag_ptr_->GetValue(false) < iVal_;
        if(this->type_ == COMPARE_TYPE::NOT_EQUAL) return tag_ptr_->GetValue(false) != iVal_;
        return tag_ptr_->GetValue(false) != lastiVal_;
    };
    CheckFHyst_ = [this]() {
        if(this->type_ == COMPARE_TYPE::EQUAL) {return tag_ptr_->GetValue(false) == iVal_;}
        if(this->type_ == COMPARE_TYPE::GREATER) {return (tag_ptr_->GetValue(false) > (iVal_ - hysterezis_));}
        if(this->type_ == COMPARE_TYPE::LESS)return (tag_ptr_->GetValue(false) < (iVal_ + hysterezis_));
        if(this->type_ == COMPARE_TYPE::NOT_EQUAL) return tag_ptr_->GetValue(false) != iVal_;
        return tag_ptr_->GetValue(false) != lastiVal_;
    };
}

std::tuple<size_t, COMPARE_TYPE, ValueVariant, ValueVariant> TGTriggerTagValue::GetTagTrigger() const
{
    return {tag_id_, type_, iVal_, hysterezis_};
}

bool TGTriggerTagValue::CheckTrigger() {
    bool ps = previous_state_;

    if(first_scan_ && tag_ptr_ && tag_ptr_->TagQualityIsGood()) {
        first_scan_ = false;
        lastiVal_ = tag_ptr_->GetValue(false);
        previous_state_ = CheckF_();
        ps = previous_state_;
    }

    if(first_scan_) return false;

    if(!previous_state_ && CheckF_()) {
        previous_state_ = true;
    }

    if(previous_state_ && !CheckFHyst_()) {
        previous_state_ = false;
    }

    lastiVal_ = tag_ptr_->GetValue(false);
    return previous_state_ && !ps;
}

void TGTriggerTagValue::Execute() const {
    if(!IsInWork()) return;
    for(const auto id: this->GetTGParent()->GetChatIDs(user_type_)) {
        for(const auto& mes: GetMessages()) {
            mes->Send(id);
        }
    }
}

QJsonObject TGTriggerTagValue::SaveToJson() const
{
    QJsonObject ret_obj;
    ret_obj.insert("id", QString::fromStdString(GetId()));
    ret_obj.insert("in_work", in_work_);
    switch(iVal_.index()) {
    case 0: ret_obj.insert("int_val", std::get<int64_t>(iVal_)); break;
    case 1: ret_obj.insert("double_val", std::get<double>(iVal_)); break;
    }

    switch(hysterezis_.index()) {
    case 0: ret_obj.insert("hysterezis", std::get<int64_t>(hysterezis_)); break;
    case 1: ret_obj.insert("hysterezis", std::get<double>(hysterezis_)); break;
    }

    QString type = "NONE";
    switch(type_) {
    case COMPARE_TYPE::EQUAL: type = "EQUAL"; break;
    case COMPARE_TYPE::GREATER: type = "GREATER"; break;
    case COMPARE_TYPE::LESS: type = "LESS"; break;
    case COMPARE_TYPE::CHANGED: type = "CHANGED"; break;
    case COMPARE_TYPE::NOT_EQUAL: type = "NOT_EQUAL"; break;
    }
    ret_obj.insert("comparision_type", type);

    ret_obj.insert("auth_level", tg_user_type_to_qstring(user_type_));

    ret_obj.insert("tag_id", static_cast<int64_t>(tag_id_));
    QJsonArray messages;
    for(const auto& it: GetMessages()) {
        messages.append(QString::fromStdString(it->GetId()));
    }
    ret_obj.insert("messages", messages);
    return ret_obj;
}

//===================================================
//==== T G B U T T O N W C A L L B A C K ============
//===================================================

TGButtonWCallback::TGButtonWCallback(TGParent* parent)
    : TGTrigger(parent)
    , tgbot_inline_button_(new TgBot::InlineKeyboardButton)
{}

void TGButtonWCallback::SetButtonName(const std::string& name) {
    tgbot_inline_button_->text = name;
}

std::string TGButtonWCallback::GetButtonName() const
{
    return tgbot_inline_button_->text;
}

TgBot::InlineKeyboardButton::Ptr TGButtonWCallback::GetTgInlineButton() {
    tgbot_inline_button_->callbackData = this->GetId();
    return tgbot_inline_button_;
}

std::tuple<const std::string&, std::function<void(const TgBot::CallbackQuery::Ptr)>> TGButtonWCallback::GetCallBack() const {
    auto call_back = [this](const TgBot::CallbackQuery::Ptr cb_query) {
        for(const auto& it: GetMessages()) {
            it->Send(cb_query->message->chat->id);
        }

        if(GetTGParent()->CheckUserChatId(cb_query->message->chat->id, user_type_)) {
            for(const auto& [id, val]: id_to_tag_set_values_) {
                id_to_tag_ptr_.at(id)->SetValueToWrite(val);
            }
        } else {
            std::string mes{"У вас нет прав на запись команд"};
            GetTGParent()->BotSendMessage(cb_query->message->chat->id, mes, nullptr);
        }
    };
    return {GetId(), call_back};
}

QJsonObject TGButtonWCallback::SaveToJson() const
{
    QJsonObject ret_obj;
    ret_obj.insert("id", QString::fromStdString(GetId()));
    ret_obj.insert("name", QString::fromStdString(GetButtonName()));
    ret_obj.insert("in_work", in_work_);
    QJsonArray messages;
    for(const auto& it: GetMessages()) {
        messages.append(QString::fromStdString(it->GetId()));
    }
    ret_obj.insert("messages", messages);
    QJsonArray tags;
    for(const auto& [id, val]: id_to_tag_set_values_) {
        QJsonObject obj;
        obj.insert("tag_id", static_cast<qint64>(id));
        switch(val.index()) {
        case 0: obj.insert("int_value", std::get<int64_t>(val)); break;
        case 1: obj.insert("double_value", std::get<double>(val)); break;
        case 2: obj.insert("string_value", std::get<QString>(val)); break;
        }
        tags.append(obj);
    }
    ret_obj.insert("tag_to_set", tags);
    return ret_obj;
}

//===================================================
//==== T G T R I G G E R  ===========================
//===================================================

void TGTrigger::AddTGMessage(const TGMessage* message)
{
    if(message) {
        messages_.push_back(message);
    }
}

void TGTrigger::ClearMessages()
{
    messages_.clear();
}

void TGTrigger::DeleteMessage(const std::string &id)
{
    for(auto it = messages_.begin(); it != messages_.end(); ++it) {
        if((*it) && (*it)->GetId() == id) {
            messages_.erase(it);
            break;
        }
    }
}

const std::vector<const TGMessage *> TGTrigger::GetMessages() const
{
    return messages_;
}

bool TGTrigger::HasMessages() const
{
    return messages_.size() > 0;
}

void TGTrigger::SetAuthorizationLevel(USER_TYPE type)
{
    user_type_ = type;
}

USER_TYPE TGTrigger::GetAuthorizationLevel() const
{
    return user_type_;
}

void TGTrigger::AddOPCTagWValue(size_t tag_id, ValueVariant value)
{
    auto tag_ptr = GetTGParent()->TagManager()->GetTagOfId(tag_id);
    if(!tag_ptr || value.valueless_by_exception()) return;
    id_to_tag_ptr_[tag_id] = std::move(tag_ptr);
    id_to_tag_set_values_[tag_id] = value;
}

const std::unordered_map<size_t, ValueVariant> &TGTrigger::GetIdTagsWSetValues() const
{
    return id_to_tag_set_values_;
}

void TGTrigger::ClearTagsToWrite()
{
    id_to_tag_set_values_.clear();
    id_to_tag_ptr_.clear();
}


//===================================================
//==== P E R I O D I C A L T A S K ==================
//===================================================

void PeriodicalTask::SetTime(QTime time)
{
    hour_ = time.hour();
    minute_ = time.minute();
}

void PeriodicalTask::SetPeriod(Period period)
{
    period_ = period;
}

void PeriodicalTask::SetDate(QDate date)
{
    day_ = date.day();
    month_ = date.month();
}

bool PeriodicalTask::Check() const
{
    QDateTime curr_dt = QDateTime::currentDateTime();
    int h, m, d, mon, dow;
    h = hour_.has_value() ? hour_.value() : 0;
    m = minute_.has_value() ? minute_.value() : 0;
    d = day_.has_value() ? day_.value() : 1;
    mon = month_.has_value() ? month_.value() : 1;
    dow = day_of_week_.has_value() ? day_of_week_.value() : 1;

    switch(period_) {
        case Period::HOURLY: return m == curr_dt.time().minute();
        case Period::DAILY: return h == curr_dt.time().hour() && m == curr_dt.time().minute();
        case Period::WEEKLY: return dow == curr_dt.date().dayOfWeek() && h == curr_dt.time().hour() && m == curr_dt.time().minute();
        case Period::MONTHLY: return d == curr_dt.date().day() && h == curr_dt.time().hour() && m == curr_dt.time().minute();
        case Period::YEARLY: return d == curr_dt.date().day()
                                    && mon == curr_dt.date().month()
                                    && h == curr_dt.time().hour()
                                    && m == curr_dt.time().minute();
        case Period::NONE: return false;
    }
    return false;
}

QJsonObject PeriodicalTask::ToJson() const
{
    QJsonObject ret_obj;
    QString per_str;
    switch(period_) {
    case Period::DAILY: per_str = "daily"; break;
    case Period::HOURLY: per_str = "hourly"; break;
    case Period::MONTHLY: per_str = "monthly"; break;
    case Period::NONE: per_str = "none"; break;
    case Period::WEEKLY: per_str = "weekly"; break;
    case Period::YEARLY: per_str = "yearly"; break;
    }
    ret_obj.insert("type", per_str);

    if(minute_.has_value()) {
        ret_obj.insert("minute", minute_.value());
    }
    if(hour_.has_value()) {
        ret_obj.insert("hour", hour_.value());
    }
    if(day_of_week_.has_value()) {
        ret_obj.insert("day_of_week", day_of_week_.value());
    }
    if(day_.has_value()) {
        ret_obj.insert("day", day_.value());
    }
    if(month_.has_value()) {
        ret_obj.insert("month", month_.value());
    }

    return ret_obj;
}

std::optional<PeriodicalTask> PeriodicalTask::FromJson(const QJsonObject &json_obj)
{
    if(!(json_obj.contains("type") && json_obj.value("type").isString())
        || (json_obj.contains("minute") && !json_obj.value("minute").isDouble())
        || (json_obj.contains("hour") && !json_obj.value("hour").isDouble())
        || (json_obj.contains("day_of_week") && !json_obj.value("day_of_week").isDouble())
        || (json_obj.contains("day") && !json_obj.value("day").isDouble())
        || (json_obj.contains("month") && !json_obj.value("month").isDouble())) {
        return std::nullopt;
    }
    Period per;
    QString per_str = json_obj.value("type").toString();
    if(per_str == "daily") {per = Period::DAILY;}
    else if(per_str == "hourly") {per = Period::HOURLY;}
    else if(per_str == "monthly") {per = Period::MONTHLY;}
    else if(per_str == "none") {per = Period::NONE;}
    else if(per_str == "weekly") {per = Period::WEEKLY;}
    else if(per_str == "yearly") {per = Period::YEARLY;}
    else return std::nullopt;

    PeriodicalTask ret_val(per);
    int m, h, d, mon;
    bool has_time = false;
    bool has_date = false;

    if(json_obj.contains("minute")) {
        m = json_obj.value("minute").toInteger();
        m = m < 0 ? 0 : m;
        m = m > 59 ? 0 : m;
        has_time = true;
    } else {
        m = 0;
    }

    if(json_obj.contains("hour")) {
        h = json_obj.value("hour").toInteger();
        h = h < 0 ? 0 : h;
        h = h > 23 ? 0 : h;
        has_time = true;
    } else {
        h = 0;
    }

    if(json_obj.contains("day_of_week")) {
        int dow = json_obj.value("day_of_week").toInteger();
        dow = dow < 1 ? 1 : dow;
        dow = dow > 7 ? 1 : dow;
        ret_val.SetDayOfWeek(dow);
    };

    if(json_obj.contains("day")) {
        d = json_obj.value("day").toInteger();
        d = d < 1 ? 1 : d;
        d = d > 31 ? 1 : d;
        has_date = true;
    } else {
        d = 1;
    }

    if(json_obj.contains("month")) {
        mon = json_obj.value("month").toInteger();
        mon = mon < 1 ? 1 : mon;
        mon = mon > 12 ? 1 : mon;
        has_date = true;
    } else {
        mon = 1;
    }

    if(has_time) {
        QTime task_time(h, m);
        ret_val.SetTime(task_time);
    }

    if(has_date) {
        QDate task_date(QDateTime::currentDateTime().date().year(), mon, d);
        ret_val.SetDate(task_date);
    }

    return ret_val;
}

std::tuple<PeriodicalTask::Period, QDate, QTime, int> PeriodicalTask::GetTask() const
{
    QTime time;
    QDate date;
    int h, m, d, mon, dow;

    if(minute_.has_value()) {
        m = minute_.value();
    } else {
        m = 0;
    }

    if(hour_.has_value()) {
        h = hour_.value();
    } else {
        h = 0;
    }

    if(day_.has_value()) {
        d = day_.value();
    } else {
        d = 1;
    }

    if(month_.has_value()) {
        mon = month_.value();
    } else {
        mon = 1;
    }

    if(day_of_week_.has_value()) {
        dow = day_of_week_.value();
    } else {
        dow = 1;
    }

    time.setHMS(h, m, 0);
    date.setDate(QDateTime::currentDateTime().date().year(), mon, d);

    return {period_, date, time, dow};
}

int PeriodicalTask::SetDayOfWeek(int day_of_week)
{
    if(day_of_week > 0 && day_of_week < 8) {
        day_of_week_ = day_of_week;
    } else {
        day_of_week_ = 1;
    }

    return day_of_week_.value();
}

//===================================================
//==== T S C H E D U L E D E V E N T ================
//===================================================

TGScheduledEvent::TGScheduledEvent(TGParent *parent, PeriodicalTask::Period period)
    :TGTrigger(parent)
{
    task_.SetPeriod(period);
}

bool TGScheduledEvent::CheckTrigger()
{
    if(first_scan_) {
        task_last_check_ = task_.Check();
        first_scan_ = false;
    }
    bool lc = task_last_check_;
    task_last_check_ = task_.Check();

    return !lc && task_last_check_;
}

void TGScheduledEvent::Execute() const
{
    if(!IsInWork()) return;
    for(const auto id: this->GetTGParent()->GetChatIDs(user_type_)) {
        for(const auto& mes: GetMessages()) {
            mes->Send(id);
        }
    }
}

void TGScheduledEvent::SetTask(const PeriodicalTask &task)
{
    task_ = task;
}

const PeriodicalTask &TGScheduledEvent::GetTask() const
{
    return task_;
}

QJsonObject TGScheduledEvent::SaveToJson() const
{
    QJsonObject ret_obj;
    ret_obj.insert("id", QString::fromStdString(GetId()));
    ret_obj.insert("in_work", in_work_);
    QJsonArray messages;
    for(const auto& it: GetMessages()) {
        messages.append(QString::fromStdString(it->GetId()));
    }
    ret_obj.insert("messages", messages);
    ret_obj.insert("scheduled_task", task_.ToJson());
    ret_obj.insert("auth_level", tg_user_type_to_qstring(user_type_));
    return ret_obj;
}
