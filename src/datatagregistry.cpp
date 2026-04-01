#include "datatagregistry.h"

#include <QFile>
#include <QJsonObject>
#include <QJsonArray>
#include <QApplication>
#include <QJsonParseError>

using namespace Qt::StringLiterals;

DataTagRegistry::DataTagRegistry(const QString &filename)
    : QObject(nullptr)
    , filename_(filename)
{}

DataTagRegistry::~DataTagRegistry()
{
    clear_data_();
}

std::optional<size_t> DataTagRegistry::check_tag_(std::shared_ptr<DataTag> tag)
{
    QString host = tag->GetHostName();
    QString server = tag->GetEndpointName();
    QString tag_name = tag->GetTagName();

    if(!hostnames_.contains(host)) return std::nullopt;
    const QString* host_it = &(*hostnames_.find(host));

    if(!hostname_to_servers_.at(host_it).contains(server)) return std::nullopt;
    const QString* server_it = &(*hostname_to_servers_.at(host_it).find(server));

    if(!servers_to_tags_.at(server_it).contains(tag_name)) return std::nullopt;

    const QString* tag_name_it = &(*servers_to_tags_.at(server_it).find(tag_name));
    return tag_name_to_id_.at(tag_name_it);
}

void DataTagRegistry::clear_data_()
{
    all_owned_ids_.clear();
    id_tag_to_opc_da_tag_pointer_.clear();
    hostnames_.clear();
    hostname_to_servers_.clear();
    servers_to_tags_.clear();
    tag_name_to_id_.clear();
}

bool DataTagRegistry::restore_data_tag_(size_t id, std::shared_ptr<DataTag> tag)
{
    if(check_tag_(tag).has_value()) return false;

    QString host = tag->GetHostName();
    QString server = tag->GetEndpointName();
    QString tag_name = tag->GetTagName();

    all_owned_ids_.insert(id);
    id_tag_to_opc_da_tag_pointer_[id] = std::move(tag);

    auto host_it = hostnames_.insert(host).first;
    auto server_it = hostname_to_servers_[&(*host_it)].insert(server).first;
    auto tag_it = servers_to_tags_[&(*server_it)].insert(tag_name).first;

    tag_name_to_id_[&(*tag_it)] = id;
    return true;
}

size_t DataTagRegistry::AddDataTag(std::shared_ptr<DataTag> tag)
{
    size_t last_id = all_owned_ids_.size() > 0 ? *all_owned_ids_.rbegin() : 1;
    QString host = tag->GetHostName();
    QString server = tag->GetEndpointName();
    QString tag_name = tag->GetTagName();

    auto checked_id = check_tag_(tag);
    if(checked_id.has_value()) return checked_id.value();

    auto host_it = hostnames_.insert(host).first;
    auto server_it = hostname_to_servers_[&(*host_it)].insert(server).first;
    auto tag_it = servers_to_tags_[&(*server_it)].insert(tag_name).first;

    tag_name_to_id_[&(*tag_it)] = ++last_id;
    all_owned_ids_.insert(last_id);
    id_tag_to_opc_da_tag_pointer_[last_id] = std::move(tag);

    emit sg_periodic_list_changed();

    return last_id;
}

size_t DataTagRegistry::AddDataTag(DataTag::DataSource src, const QString &fullname)
{
    QRegularExpressionMatch match = fullname_regexp_.match(fullname);
    if(!match.hasMatch()) return 0;

    std::shared_ptr<DataTag> new_tag;
    switch(src) {
    case DataTag::DataSource::OPCDA:    new_tag = std::make_shared<DataTagOpcDA>(match.captured(1), match.captured(2), match.captured(3)); break;
    case DataTag::DataSource::OPCUA:    new_tag = std::make_shared<DataTagOpcUA>(match.captured(1), match.captured(2), match.captured(3)); break;
    default:                            new_tag = nullptr;
    }

    return AddDataTag(std::move(new_tag));
}

void DataTagRegistry::DeleteTag(size_t id)
{
    if(!all_owned_ids_.contains(id)) return;

    auto tag_ptr = id_tag_to_opc_da_tag_pointer_.at(id);
    id_tag_to_opc_da_tag_pointer_.erase(id);

    QString host = tag_ptr->GetHostName();
    QString server = tag_ptr->GetEndpointName();
    QString tag_name = tag_ptr->GetTagName();

    auto host_it = hostnames_.find(host);
    auto server_it = hostname_to_servers_.at(&(*host_it)).find(server);
    auto tag_it = servers_to_tags_.at(&(*server_it)).find(tag_name);
    tag_name_to_id_.erase(&(*tag_it));
    servers_to_tags_.at(&(*server_it)).erase(tag_it);

    emit sg_periodic_list_changed();
}

bool DataTagRegistry::SaveDataToFile()
{
    QString path = qApp->applicationDirPath();
    return SaveDataToFile(path);
}

bool DataTagRegistry::SaveDataToFile(const QString &folderpath)
{
    QString filename = QString("%1/%2").arg(folderpath, filename_);
    QFile file(filename);
    if(file.open(QIODeviceBase::WriteOnly)) {
        QJsonArray json_ar;
        for(const auto& [host_ptr, server_set]: hostname_to_servers_) {
            for(const auto& server_it: server_set) {
                QJsonObject server_obj;
                QJsonArray tags_array;
                server_obj.insert("hostname", *host_ptr);
                server_obj.insert("server", server_it);

                for(const auto& tag_it: servers_to_tags_.at(&(server_it))) {
                    auto tag_ptr = id_tag_to_opc_da_tag_pointer_.at(tag_name_to_id_.at(&(tag_it)));
                    auto tag_json = tag_ptr->TagToJson(false);
                    tag_json.insert("id", static_cast<qint64>(tag_name_to_id_.at(&(tag_it))));
                    tags_array.push_back(std::move(tag_json));
                }
                server_obj.insert("tags", tags_array);
                json_ar.append(std::move(server_obj));
            }
        }

        QJsonDocument json_doc(json_ar);
        bool res;
        res = file.write(json_doc.toJson()) != (-1);
        file.close();
        qInfo() << QString("DataRegistry: сохранение данных в файл %1.").arg(filename);
        return res;
    }
    qWarning() << QString("DataRegistry: не удалось сохраненить данные в файл %1.").arg(filename);
    return false;
}

bool DataTagRegistry::RestoreDataFromFile()
{
    clear_data_();

    QFile file(filename_);
    if(file.open(QIODeviceBase::ReadOnly)) {
        QJsonParseError json_error;
        QJsonDocument input_doc = QJsonDocument::fromJson(file.readAll(), &json_error);

        if(json_error.error != QJsonParseError::NoError) {
            QString log_message = QString("DataTagRegistry: файл %1 поврежден.").arg(filename_);
            qWarning() << log_message;
            return false;
        }

        if(input_doc.isArray()) {
            for(int i = 0; i < input_doc.array().size(); ++i) {
                QJsonObject item_obj = input_doc.array().at(i).toObject();

                if((item_obj.contains("hostname") && item_obj.value("hostname").isString())
                    && (item_obj.contains("server") && item_obj.value("server").isString())
                    && (item_obj.contains("tags") && item_obj.value("tags").isArray())) {

                    QString hostname = item_obj.value("hostname").toString();
                    QString server_name = item_obj.value("server").toString();

                    QJsonArray tags_ar = item_obj.value("tags").toArray();

                    for(int j = 0; j < tags_ar.size(); j++) {
                        QJsonObject tag_obj = tags_ar.at(j).toObject();
                        if((tag_obj.contains("id") && tag_obj.value("id").isDouble())
                            && (tag_obj.contains("tag_name") && tag_obj.value("tag_name").isString())
                            && (tag_obj.contains("tag_comment") && tag_obj.value("tag_comment").isString())
                            && (tag_obj.contains("source") && tag_obj.value("source").isString())) {

                            size_t id = static_cast<size_t>(tag_obj.value("id").toInteger());
                            QString tag_name = tag_obj.value("tag_name").toString();
                            QString tag_comment = tag_obj.value("tag_comment").toString();
                            QString src_text = tag_obj.value("source").toString();
                            std::shared_ptr<DataTag> tag_ptr;
                            if(src_text == u"OPCDA"_s) {
                                tag_ptr = std::make_shared<DataTagOpcDA>(hostname, server_name, tag_name);
                            } else if(src_text == u"OPCUA"_s) {
                                tag_ptr = std::make_shared<DataTagOpcUA>(hostname, server_name, tag_name);
                            } else {
                                qWarning() << QString("DataTagRegistry: неверный тип источника тэга %1@%2#%3").arg(hostname, server_name, tag_name);
                                continue;
                            }

                            tag_ptr->SetCommentString(std::move(tag_comment));

                            if(tag_obj.contains("gain") && tag_obj.value("gain").isDouble()) {
                                tag_ptr->SetGainOption(tag_obj.value("gain").toDouble());
                            }

                            if(tag_obj.contains("substitute_values") && item_obj.value("substitute_values").isArray()) {
                                QJsonArray subst_array = tag_obj.value("substitute_values").toArray();
                                for(auto it = subst_array.begin(); it < subst_array.end(); ++it) {
                                    for(const auto & key: (*it).toObject().keys()) {
                                        tag_ptr->AddSubstituteStringValue(key, (*it).toObject().value(key).toString());
                                    }
                                }
                            }

                            restore_data_tag_(id, tag_ptr);
                        } else {
                            qWarning() << QString("DataTagRegistry: не удалось восстановить тэг, сервер %1@%2").arg(hostname, server_name);
                        }
                    }
                } else {
                    qWarning() << u"DataTagRegistry: не удалось восстановить данные сервера тэгов."_s;
                }
            }
            qInfo() << u"DataTagRegistry: данные восстановлены из файла %1"_s.arg(filename_);
            return true;
        }
        qWarning() << u"DataTagRegistry: файл %1 неверный формат"_s.arg(filename_);
        return false;
    }
    qWarning() << u"DataTagRegistry: невозможно открыть файл %1 для чтения"_s.arg(filename_);
    return false;
}

size_t DataTagRegistry::CheckTagExist(const QString &fullname)
{
    QRegularExpressionMatch match = fullname_regexp_.match(fullname);
    if(!match.hasMatch()) return 0;

    QString host = match.captured(1);
    QString server = match.captured(2);
    QString tagname = match.captured(3);

    auto host_it = hostnames_.find(host);
    if(host_it == hostnames_.end()) return 0;

    auto server_it = hostname_to_servers_.at(&(*host_it)).find(server);
    if(server_it == hostname_to_servers_.at(&(*host_it)).end()) return 0;

    if(servers_to_tags_.at(&(*server_it)).contains(tagname)) {
        auto tag_it = servers_to_tags_.at(&(*server_it)).find(tagname);
        if(tag_it == servers_to_tags_.at(&(*server_it)).end()) return 0;
        return tag_name_to_id_.at(&(*tag_it));
    }

    return 0;
}

const std::unordered_set<QString> &DataTagRegistry::GetHostNames() const
{
    return hostnames_;
}

std::vector<std::shared_ptr<DataTag>> DataTagRegistry::GetTagsOfType(DataTag::DataSource type) const
{
    std::vector<std::shared_ptr<DataTag>> ret_vec;
    for(const auto& [_, tag_ptr]: id_tag_to_opc_da_tag_pointer_) {
        if(tag_ptr->GetDataSource() == type) {
            ret_vec.push_back(tag_ptr);
        }
    }
    return ret_vec;
}

std::vector<std::shared_ptr<DataTag> > DataTagRegistry::GetAllTags() const
{
    auto values_view = id_tag_to_opc_da_tag_pointer_ | std::views::values;
    return {values_view.begin(), values_view.end()};
}

const std::unordered_map<size_t, std::shared_ptr<DataTag>>& DataTagRegistry::GetIdToTagsMap() const
{
    return id_tag_to_opc_da_tag_pointer_;
}

std::shared_ptr<DataTag> DataTagRegistry::GetTagOfId(size_t id) const
{
    if(!all_owned_ids_.contains(id)) return nullptr;
    return id_tag_to_opc_da_tag_pointer_.at(id);
}

