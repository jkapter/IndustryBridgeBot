#include "opcdatamanager.h"

#include <QRegularExpression>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>

#include "opcclientworker.h"
#include "opctag.h"

using namespace OPC_HELPER;
using namespace Qt::StringLiterals;

OPCDataManager::OPCDataManager()
    : QObject()
{
    tag_name_check_re_.setPattern("^([^@]*)@([^@#].*)#(.*)$");
// ^\[([^\[].*[^\]])\]\[([^\[].*[^\]])\]\[([^\[].*[^\]])\]$ [host][server][tag]
    RestoreDataFromFile();
}

OPCDataManager::~OPCDataManager() {
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
        qInfo() << QString("Автосохранение кофигурационных файлов OPCDataManager в папке %1").arg(path);
        SaveDataToFile(path);
    } else {
        qWarning() << "Автосохранение кофигурационных файлов OPCDataManager не удалось.";
    }

    if(opc_threads_on_request_count_ > 0 || period_reading_on_) {
        emit sg_stop_reading();
        QTimer::singleShot(TIME_WAITING_THREAD_, this,
                           [this](){
                               opc_threads_on_request_count_ = 0;
                               period_reading_on_ = false;
                            });
        while(opc_threads_on_request_count_ > 0 || period_reading_on_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
        }
    }

    qInfo() << "OPCDataManager деструктор завершен.";
}

TAG_STATUS OPCDataManager::CheckTagReadState(const QString& full_tag_name) const {
    if(tag_names_.count(full_tag_name) == 0) {
        return TAG_STATUS::NOT_FOUND;
    }

    const QString* tag_ref = &(*tag_names_.find(full_tag_name));

    size_t id = tag_name_to_id_.at(tag_ref);

    bool rp;

    rp = opc_tags_id_value_controlled_.count(id) > 0;

    if(rp) {
        return TAG_STATUS::PERIODIC_READ;
    } else {
        return TAG_STATUS::NOT_FOUND;
    }
}

size_t OPCDataManager::DeleteTagFromPeriodicRead(const QString& full_tag_name) {
    if(tag_names_.count(full_tag_name) == 0) {
        return 0;
    }

    const QString* tag_ref = &(*tag_names_.find(full_tag_name));
    size_t id = tag_name_to_id_.at(tag_ref);

    if(opc_tags_id_value_controlled_.count(id) == 0) {
        return 0;
    }

    opc_tags_id_value_controlled_.erase(id);
    emit sg_periodic_list_changed();
    return id;
}

size_t OPCDataManager::GetTagId(const QString& full_tag_name) const
{
    if(tag_names_.count(full_tag_name)) {
        auto it = tag_names_.find(full_tag_name);
        return tag_name_to_id_.at(&(*it));
    }
    return std::numeric_limits<size_t>::max();
}

size_t OPCDataManager::GetTagId(const QString& hostname, const QString& server_name, const QString& tag_name) const {
    QString full_tag_name = QString("%1@%2#%3").arg(hostname, server_name, tag_name);
    return GetTagId(full_tag_name);
}

size_t OPCDataManager::check_or_add_tag_(const QString& full_tag_name) {

    QRegularExpressionMatch match = tag_name_check_re_.match(full_tag_name);
    if(match.hasMatch()) {
        return check_or_add_tag_(match.captured(1), match.captured(2), match.captured(3));
    } else {
        QString error_matching(u"error"_s);
        return check_or_add_tag_(error_matching, error_matching, error_matching);
    }
}

size_t OPCDataManager::check_or_add_tag_(const QString& hostname, const QString& server_name, const QString& tag_name) {
    const QString* server_name_ptr;
    const QString* hostname_ptr;

    if(hostnames_.count(hostname) == 0) {
        hostname_ptr = &(*hostnames_.insert(hostname).first);
        host_to_opc_servers_[hostname_ptr] = {};
    } else {
        hostname_ptr = &(*hostnames_.find(hostname));
    }

    if(host_to_opc_servers_.at(hostname_ptr).count(server_name) == 0) {
        server_name_ptr = &(*host_to_opc_servers_.at(hostname_ptr).insert(server_name).first);
        opc_server_to_host_[server_name_ptr] = hostname_ptr;
    } else {
        server_name_ptr = &(*host_to_opc_servers_.at(hostname_ptr).find(server_name));
    }

    QString tag_full_name = QString("%1@%2#%3").arg(hostname, server_name, tag_name);
    if(tag_names_.count(tag_full_name) == 0) {
        auto [tag_ref, _] = tag_names_.insert(tag_full_name);
        tag_name_to_id_[&(*tag_ref)] = ++last_id_;
        opc_server_name_to_id_tags_set_[server_name_ptr].insert(last_id_);
        opc_tag_to_server_name_[last_id_] = server_name_ptr;
        id_tag_to_OPCTag_pointer_[last_id_] = std::make_shared<OPCTag>(hostname, server_name, tag_name);
        return last_id_;
    } else {
        const QString* tag_ref = &(*tag_names_.find(tag_name));
        size_t id = tag_name_to_id_.at(tag_ref);
        return id;
    }
}


size_t OPCDataManager::AddTagToPeriodicReadList(const QString& hostname, const QString& server_name, const QString& tag_name) {
    size_t id = check_or_add_tag_(hostname, server_name, tag_name);
    opc_tags_id_value_controlled_.insert(id);
    emit sg_periodic_list_changed();
    return id;
}

size_t OPCDataManager::AddTagToPeriodicReadList(const QString& full_tag_name) {
    QRegularExpressionMatch match = tag_name_check_re_.match(full_tag_name);
    size_t id;
    if(match.hasMatch()) {
        id = check_or_add_tag_(match.captured(1), match.captured(2), match.captured(3));
    } else {
        id = std::numeric_limits<size_t>::max();
    }
    opc_tags_id_value_controlled_.insert(id);
    emit sg_periodic_list_changed();
    return id;
}

void OPCDataManager::ClearMonitoringTags() {
    opc_tags_id_value_controlled_.clear();
    emit sg_periodic_list_changed();
}

std::vector<std::shared_ptr<OPCTag>> OPCDataManager::GetPeriodicTags() const {
    std::vector<std::shared_ptr<OPCTag>> ret_vec;
    for(const auto& it: opc_tags_id_value_controlled_) {
        ret_vec.push_back(id_tag_to_OPCTag_pointer_.at(it));
    }
    return ret_vec;
}

std::vector<std::shared_ptr<OPCTag>> OPCDataManager::GetPeriodicTags(const QString& hostname) const {
    std::vector<std::shared_ptr<OPCTag>> ret_vec;
    for(const auto& it: opc_tags_id_value_controlled_) {
        if(id_tag_to_OPCTag_pointer_.at(it)->GetHostname() == hostname) {
            ret_vec.push_back(id_tag_to_OPCTag_pointer_.at(it));
        }
    }
    return ret_vec;
}

std::map<size_t, std::shared_ptr<OPCTag>> OPCDataManager::GetIdToTagPeriodicTags() const {
    std::map<size_t, std::shared_ptr<OPCTag>> ret_map;
    for(const auto& id: opc_tags_id_value_controlled_) {
        ret_map[id] = id_tag_to_OPCTag_pointer_.at(id);
    }
    return ret_map;
}

std::shared_ptr<OPCTag> OPCDataManager::GetOPCTag(size_t id) const {
    if(id_tag_to_OPCTag_pointer_.count(id) > 0) {
        return id_tag_to_OPCTag_pointer_.at(id);
    }
    return nullptr;
}

const std::set<QString> &OPCDataManager::GetHostNames() const
{
    return hostnames_;
}

bool OPCDataManager::SaveDataToFile() {
    QString path = qApp->applicationDirPath();
    return SaveDataToFile(path);
}

bool OPCDataManager::SaveDataToFile(const QString& foldername) {
    QString filename = QString("%1/opctags.json").arg(foldername);
    QFile file(filename);
    if(file.open(QIODeviceBase::WriteOnly)) {
        QJsonArray json_ar;
        for(const auto& [id, tag_ptr]: id_tag_to_OPCTag_pointer_) {
            QJsonObject t_obj;
            t_obj.insert("tag_name", tag_ptr->GetTagName());
            t_obj.insert("tag_comment", tag_ptr->GetCommentString());
            t_obj.insert("id", static_cast<int64_t>(id));
            t_obj.insert("server", tag_ptr->GetServerName());
            t_obj.insert("hostname", tag_ptr->GetHostname());
            t_obj.insert("periodic", opc_tags_id_value_controlled_.count(id) != 0);
            if(tag_ptr->GetGainOption().has_value()) {
                t_obj.insert("gain", tag_ptr->GetGainOption().value());
            }
            auto subst_map = tag_ptr->GetEnumStringValues();
            if(subst_map.size() > 0) {
                QJsonArray subst_array;
                for(const auto& [val, substitute_val]: subst_map) {
                    subst_array.append(QJsonObject{{val, substitute_val}});
                }
                t_obj.insert("substitute_values", std::move(subst_array));
            }
            json_ar.append(std::move(t_obj));
        }
        QJsonDocument json_doc(json_ar);
        bool res;
        res = file.write(json_doc.toJson()) != (-1);
        file.close();
        qInfo() << QString("OPCDataManager: сохранение данных в файл %1.").arg(filename);
        return res;
    }
    qWarning() << QString("OPCDataManager: не удалось сохраненить данные в файл %1.").arg(filename);
    return false;
}


bool OPCDataManager::RestoreDataFromFile() {

    emit sg_send_message_to_console("OPCDataManager: чтение данных из файла opctags.json.");
    return restore_data_from_json_();
}

bool OPCDataManager::PeriodicReadingOn() const
{
    return period_reading_on_;
}

int OPCDataManager::GetPeriodReading() const
{
    return opc_period_reading_;
}

void OPCDataManager::SetPeriodReading(int period)
{
    opc_period_reading_ = period > 0 ? period : opc_period_reading_;
    qInfo() << QString("OPCDataManager изменен период чтения, новое значение %1.").arg(period);
}

void OPCDataManager::ReadTagsOnce(std::vector<std::shared_ptr<OPCTag>>& tags)
{
    QThread* thread_req = new QThread();
    OPC_HELPER::OPCDAWorker* worker = new OPC_HELPER::OPCDAWorker();

    worker->SetTagsList(tags);
    worker->moveToThread(thread_req);

    QObject::connect(thread_req, SIGNAL(finished()), worker, SLOT(deleteLater()));
    QObject::connect(thread_req, SIGNAL(finished()), thread_req, SLOT(deleteLater()));
    QObject::connect(thread_req, SIGNAL(finished()), this, SLOT(sl_on_request_thread_finished()));
    QObject::connect(thread_req, SIGNAL(started()), worker, SLOT(sl_process()));
    QObject::connect(worker, SIGNAL(sg_finished()), thread_req, SLOT(quit()));
    QObject::connect(this, SIGNAL(sg_stop_reading()), worker, SLOT(sl_stop_reading()));
    QObject::connect(worker, SIGNAL(sg_send_message_to_console(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
    QObject::connect(worker, SIGNAL(sg_opcclient_got_exception(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
    QObject::connect(worker, SIGNAL(sg_opcclient_got_exception(QString)), this, SLOT(sl_thread_send_exception(QString)));
    QObject::connect(worker, SIGNAL(sg_reading_complete(size_t)), this, SIGNAL(sg_reading_request_complete()));
    QObject::connect(worker, SIGNAL(sg_reading_complete(size_t)), this, SLOT(sl_on_request_reading_tags_complete(size_t)));
    QObject::connect(worker, SIGNAL(sg_server_error(QString,QString,size_t)), this, SLOT(sl_thread_send_opc_status(QString,QString,size_t)));

    ++opc_threads_on_request_count_;

    thread_req->start();

    QString log_message = QString("OPCDataManager: опрос тэгов по запросу.");
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
}

void OPCDataManager::StartPeriodReading()
{
    if(period_reading_on_) {
        emit sg_stop_reading();
        QTimer::singleShot(TIME_WAITING_THREAD_, this,
                           [this]() {
                               period_reading_on_ = false;
                               opc_threads_on_request_count_ = 0;

                                    }
                            );

        while(period_reading_on_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
        }
    }

    QThread* thread_per = new QThread();
    OPC_HELPER::OPCDAWorker* worker = new OPC_HELPER::OPCDAWorker();

    std::vector<std::shared_ptr<OPCTag>> tags_vec = GetPeriodicTags();
    size_t n_nags = tags_vec.size();
    worker->SetTagsList(std::move(tags_vec));
    worker->moveToThread(thread_per);
    worker->SetPeriodicReading(opc_period_reading_);

    QObject::connect(thread_per, SIGNAL(finished()), worker, SLOT(deleteLater()));
    QObject::connect(thread_per, SIGNAL(finished()), thread_per, SLOT(deleteLater()));
    QObject::connect(thread_per, SIGNAL(finished()), this, SLOT(sl_periodic_thread_finished()));
    QObject::connect(thread_per, SIGNAL(started()), worker, SLOT(sl_process()));
    QObject::connect(worker, SIGNAL(sg_finished()), thread_per, SLOT(quit()));
    QObject::connect(this, SIGNAL(sg_stop_reading()), worker, SLOT(sl_stop_reading()));
    QObject::connect(worker, SIGNAL(sg_send_message_to_console(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
    QObject::connect(worker, SIGNAL(sg_opcclient_got_exception(QString)), this, SIGNAL(sg_send_message_to_console(QString)));
    QObject::connect(worker, SIGNAL(sg_opcclient_got_exception(QString)), this, SLOT(sl_thread_send_exception(QString)));
    QObject::connect(worker, SIGNAL(sg_reading_complete(size_t)), this, SIGNAL(sg_reading_periodic_complete()));
    QObject::connect(worker, SIGNAL(sg_server_error(QString,QString,size_t)), this, SLOT(sl_thread_send_opc_status(QString,QString,size_t)));

    errors_periodic_opc_server_count_ = 0;
    errors_server_status_periodic_count_ = 0;

    thread_per->start();

    QString log_message = QString("OPCDataManager: запуск периодического опроса. Количество тэгов %1").arg(n_nags);
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
    request_stop_periodic_reading_ = false;
    period_reading_on_ = true;
    emit sg_periodic_started();
    emit sg_set_text_state("OPC клиент в работе");
}

void OPCDataManager::StartPeriodReading(int period)
{
    opc_period_reading_ = period;
    StartPeriodReading();
}

void OPCDataManager::StopPeriodReading()
{
    emit sg_set_text_state("OPC клиент: останов периодического чтения.");
    emit sg_stop_reading();
    request_stop_periodic_reading_ = true;
}

bool OPCDataManager::OPCDataManager::restore_data_from_json_() {
    tag_names_.clear();
    hostnames_.clear();
    host_to_opc_servers_.clear();
    opc_server_to_host_.clear();
    tag_name_to_id_.clear();
    id_tag_to_name_tag_.clear();
    opc_server_name_to_id_tags_set_.clear();
    opc_tag_to_server_name_.clear();
    id_tag_to_OPCTag_pointer_.clear();
    opc_tags_id_value_controlled_.clear();

    QFile file("opctags.json");
    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);

        if(json_error.error != QJsonParseError::NoError) {
            QString log_message = QString("OPCDataManager: файл opctags.json поврежден.");
            emit sg_send_message_to_console(log_message);
            qInfo() << log_message;
            return false;
        }

        if(input_doc.isArray()) {
            for(int i = 0; i < input_doc.array().size(); ++i) {
                QJsonObject item_obj = input_doc.array().at(i).toObject();
                if((item_obj.contains("tag_name") && item_obj.value("tag_name").isString())
                    && (item_obj.contains("tag_comment") && item_obj.value("tag_comment").isString())
                    && (item_obj.contains("server") && item_obj.value("server").isString())
                    && (item_obj.contains("periodic") && item_obj.value("periodic").isBool())
                    && (item_obj.contains("id") && item_obj.value("id").isDouble())) {

                    QString hostname = item_obj.contains("hostname") ? item_obj.value("hostname").toString() : u"localhost"_s;
                    auto host_ref = hostnames_.insert(hostname).first;
                    auto server_ref = host_to_opc_servers_[&(*host_ref)].insert(item_obj.value("server").toString()).first;
                    opc_server_to_host_[&(*server_ref)] = &(*host_ref);

                    QString full_tag_name = QString("%1@%2#%3").arg(*host_ref, *server_ref, item_obj.value("tag_name").toString());
                    auto tag_ref = tag_names_.insert(full_tag_name).first;

                    size_t id = static_cast<size_t>(item_obj.value("id").toInteger());
                    tag_name_to_id_[&(*tag_ref)] = id;
                    id_tag_to_name_tag_[id] = &(*tag_ref);
                    opc_server_name_to_id_tags_set_[&(*server_ref)].insert(id);
                    opc_tag_to_server_name_[id] = &(*server_ref);
                    id_tag_to_OPCTag_pointer_[id] = std::make_shared<OPCTag>(full_tag_name);

                    QString comment = item_obj.value("tag_comment").toString().length() > 0 ? item_obj.value("tag_comment").toString() : QString("%1@%2").arg(*host_ref, *server_ref);
                    id_tag_to_OPCTag_pointer_.at(id)->SetCommentString(comment);

                    if(item_obj.value("periodic").toBool()) {
                        opc_tags_id_value_controlled_.insert(id);
                    }

                    if(item_obj.contains("gain") && item_obj.value("gain").isDouble()) {
                        id_tag_to_OPCTag_pointer_.at(id)->SetGainOption(item_obj.value("gain").toDouble());
                    }
                    if(item_obj.contains("substitute_values") && item_obj.value("substitute_values").isArray()) {
                        QJsonArray subst_array = item_obj.value("substitute_values").toArray();
                        for(auto it = subst_array.begin(); it < subst_array.end(); ++it) {
                            for(const auto & key: (*it).toObject().keys()) {
                                id_tag_to_OPCTag_pointer_.at(id)->AddEnumStringValues(key, (*it).toObject().value(key).toString());
                            }
                        }
                    }

                    last_id_ = id > last_id_ ? id : last_id_;

                } else {
                    tag_names_.clear();
                    hostnames_.clear();
                    host_to_opc_servers_.clear();
                    opc_server_to_host_.clear();
                    tag_name_to_id_.clear();
                    id_tag_to_name_tag_.clear();
                    opc_server_name_to_id_tags_set_.clear();
                    opc_tag_to_server_name_.clear();
                    id_tag_to_OPCTag_pointer_.clear();
                    opc_tags_id_value_controlled_.clear();
                    return false;
                }
            }
        }
        return true;
    }
    return false;
}

void OPCDataManager::sl_opc_read_error(size_t n_tags)
{
    ++errors_periodic_opc_server_count_;
    QString message = QString("OPCDataManager: ошибка чтения тэгов прочитано %1 из %2. Ошибка подряд: %3")
                          .arg(n_tags)
                          .arg(GetPeriodicTags().size())
                          .arg(errors_periodic_opc_server_count_);
    emit sg_send_message_to_console(message);
    qWarning() << message;
    if(errors_periodic_opc_server_count_ > MAX_PERIODIC_ERRORS_COUNT && !request_stop_periodic_reading_) {
        emit sg_stop_reading();
        QTimer::singleShot(opc_period_reading_*2000, this, [this]() {this->StartPeriodReading();});
        emit sg_send_message_to_console(QString("OPCDataManager: таймер на рестарт клиента периодического опроса."));
        emit sg_set_text_state("OPC клиент: перезапуск по ошибке");
        qWarning() << QString("OPCDataManager: таймер на рестарт клиента периодического опроса.");
    }
}

void OPCDataManager::sl_thread_send_exception(QString text)
{
    QString log_message = QString("OPCDataManager: получено исключение в ОРС-клиенте: %1").arg(text);
    emit sg_send_message_to_console(log_message);
    qCritical() << log_message;
    emit sg_stop_reading();
    emit sg_send_message_to_console(QString("OPCDataManager: таймер на рестарт клиента периодического опроса."));
    emit sg_set_text_state("OPC клиент перезапуск");
}

void OPCDataManager::sl_thread_send_opc_status(QString host, QString server, size_t state)
{
    QString ser_state;
    switch(state) {
    case OPC_STATUS_RUNNING: ser_state = "OPC_STATUS_RUNNING"; break;
    case OPC_STATUS_FAILED: ser_state = "OPC_STATUS_FAILED"; break;
    case OPC_STATUS_NOCONFIG: ser_state = "OPC_STATUS_NOCONFIG"; break;
    case OPC_STATUS_SUSPENDED: ser_state = "OPC_STATUS_SUSPENDED"; break;
    case OPC_STATUS_TEST: ser_state = "OPC_STATUS_TEST"; break;
    case OPC_STATUS_COMM_FAULT:	ser_state = "OPC_STATUS_COMM_FAULT"; break;
    default: ser_state = "UNKNOWN"; break;
    }

    QString log_message = QString("OPCDataManager: сервер %1@%2 получено состояние %3").arg(host, server, ser_state);
    emit sg_send_message_to_console(log_message);
    qWarning() << log_message;

    if(state != OPC_STATUS_RUNNING && period_reading_on_) ++errors_server_status_periodic_count_;
    if(errors_server_status_periodic_count_ > MAX_PERIODIC_ERRORS_COUNT && !request_stop_periodic_reading_) {
        log_message = QString("OPCDataManager: перезапуск по максимальному количеству ошибок чтения.");
        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;
        emit sg_stop_reading();
        request_stop_periodic_reading_ = false;
    }
}

void OPCDataManager::sl_periodic_thread_finished()
{
    period_reading_on_ = false;
    emit sg_periodic_finished();
    emit sg_set_text_state("OPC клиент остановлен");
    if(!request_stop_periodic_reading_) {
        QTimer::singleShot(opc_period_reading_*2000, this, [this]() {this->StartPeriodReading();});
        QString log_message = QString("OPCDataManager: неожиданное завершение потока клиента, перезапуск.");
        emit sg_send_message_to_console(log_message);
        qCritical() << log_message;
        emit sg_set_text_state("OPC клиент перезапуск");
    }
}

void OPCDataManager::sl_on_request_thread_finished()
{
    --opc_threads_on_request_count_;

}

void OPCDataManager::sl_on_request_reading_tags_complete(size_t ntags)
{
    QString log_message = QString("OPCDataManager: прочитано %1 тэгов по запросу.").arg(ntags);
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
}
