#include "sourcedrivers.h"

#include <QOpcUaProvider>
#include <QThread>
#include <QTimer>

#include "datatag_opcua.h"

using namespace Qt::StringLiterals;

//===================================================================
//====================== OPCUADriver ================================
//===================================================================

OPCUADriver::~OPCUADriver()
{
    if(periodic_reading_on_) {
        for(auto& [host_ptr, ua_client]: host_to_ua_client_) {
            ua_client->SubscribeToChangeValue(false, tags_period_reading_);
        }
    }

    emit sg_stop_browsing_tags();
    emit sg_stop_reading();

    qInfo() << u"OPCDADriver деструктор завершен."_s;
}

std::set<QString> OPCUADriver::GetEndpointNames(const QString &host)
{
    auto host_it = hosts_.find(host);
    if(host_it != hosts_.end()) {
        std::set<QString> ret_set = {host_to_endpoints_.at(&(*host_it)).begin(), host_to_endpoints_.at(&(*host_it)).end()};
        ret_set.erase(u"defaultEp"_s);
        return ret_set;
    }

    auto client = take_or_init_client_(host, u"defaultEp"_s);

    auto endpoints = client->GetEndpoints(host);

    if(endpoints.has_value()) {
        host_it = hosts_.find(host);
        host_to_endpoints_.at(&(*host_it)).insert(endpoints.value().begin(), endpoints.value().end());
        for(const auto& it: host_to_endpoints_.at(&(*host_it))) {
            endpoint_to_host_[&it] = &(*host_it);
        }
        return GetEndpointNames(host);
    }
    return {};
}

void OPCUADriver::sl_get_endpoints(QString host)
{
    if(++host_retry_count_[host] > MAX_ENDPOINT_RETRIES) {
        QString mes = QString("OPCUADriver: не удалось получить эндпоинты хоста %1 после %2 попыток")
                          .arg(host).arg(MAX_ENDPOINT_RETRIES);
        emit sg_send_message_to_console(mes);
        qWarning() << mes;
        host_retry_count_.erase(host);
        return;
    }

    auto client = take_or_init_client_(host, u"defaultEp"_s);
    auto ep_opt = client->GetEndpoints(host);

    if(!ep_opt.has_value()) return;

    auto host_it = hosts_.insert(host).first;

    host_to_endpoints_[&(*host_it)] = {ep_opt.value().begin(), ep_opt.value().end()};
    emit sg_get_endpoints_names(host);
}

std::optional<const std::vector<QString> > OPCUADriver::GetTagNames(const QString &hostname, const QString &server_name)
{
    return std::nullopt;
}

DataBrowseItem* OPCUADriver::GetVariablesNode(const QString &hostname, const QString &server_name)
{
    auto host_it = hosts_.find(hostname);
    if(host_it == hosts_.end()) {
        GetEndpointNames(hostname);
    }

    host_it = hosts_.find(hostname);
    if(host_it == hosts_.end() || !host_to_endpoints_.contains(&(*host_it))) return nullptr;

    auto ep_it = host_to_endpoints_.at(&(*host_it)).find(server_name);
    if(ep_it == host_to_endpoints_.at(&(*host_it)).end()) return nullptr;

    if(endpoint_to_data_browse_item_.contains(&(*ep_it))) {
        auto ret_ptr = endpoint_to_data_browse_item_.at(&(*ep_it)).release();
        endpoint_to_data_browse_item_.erase(&(*ep_it));
        return ret_ptr;
    }

    auto client = take_or_init_client_(hostname, server_name);

    auto node = client->GetVariablesNode(hostname, server_name, 50);

    if(node) {
        return node.release();
    }

    return nullptr;
}

size_t OPCUADriver::SetTagsList(std::vector<std::shared_ptr<DataTag>> &tags)
{
    std::unordered_map<QString, QString> hosts_to_ep;
    std::unordered_map<QString*, std::vector<std::shared_ptr<DataTagOpcUA>>> ep_to_tag_vec;
    size_t ret_val = 0;

    for(auto & it: tags) {
        if(it->GetDataSource() != DataTag::DataSource::OPCUA) continue;
        hosts_to_ep[it->GetHostName()] = it->GetEndpointName();
        ep_to_tag_vec[&hosts_to_ep.at(it->GetHostName())].push_back(std::static_pointer_cast<DataTagOpcUA>(it));
    }

    for(auto& [host, ep]: hosts_to_ep) {
        auto client_ptr = take_or_init_client_(host, ep);
        client_ptr->AddTags(ep_to_tag_vec.at(&ep));
        ret_val += ep_to_tag_vec.at(&ep).size();
    }
    has_tags_to_read_ = tags.size() > 0;
    return ret_val;
}

void OPCUADriver::RemoveTag(const std::shared_ptr<DataTag>& tag)
{
    if(!tag || tag->GetDataSource() != DataTag::DataSource::OPCUA) return;

    auto host_it = hosts_.find(tag->GetHostName());
    if(host_it == hosts_.end()) return;

    auto client_it = host_to_ua_client_.find(&(*host_it));
    if(client_it == host_to_ua_client_.end()) return;

    client_it->second->RemoveTag(std::static_pointer_cast<DataTagOpcUA>(tag));
}

void OPCUADriver::WriteTagNow(const std::shared_ptr<DataTag>& tag)
{
    if(!tag || tag->GetDataSource() != DataTag::DataSource::OPCUA) return;

    auto host_it = hosts_.find(tag->GetHostName());
    if(host_it == hosts_.end()) return;

    auto client_it = host_to_ua_client_.find(&(*host_it));
    if(client_it == host_to_ua_client_.end()) return;

    client_it->second->WriteTags();
}

void OPCUADriver::ReadTagsOnce(std::vector<std::shared_ptr<DataTag>> &tags)
{
    SetTagsList(tags);
    for(auto& [host_ptr, ua_client]: host_to_ua_client_) {
        ua_client->WriteTags();
        ua_client->ReadTags();
    }
    has_tags_to_read_ = tags.size() > 0;
}

bool OPCUADriver::PeriodicReadingOn() const
{
    return periodic_reading_on_;
}

void OPCUADriver::StartPeriodReading()
{
    for(auto& [host_ptr, ua_client]: host_to_ua_client_) {
        ua_client->SubscribeToChangeValue(true, tags_period_reading_);
    }
    periodic_reading_on_ = true;
    emit sg_periodic_reading_changed(true);
}

void OPCUADriver::StartPeriodReading(int period)
{
    SetPeriodReading(period);
    StartPeriodReading();
}

void OPCUADriver::StopPeriodReading()
{
    for(auto& [host_ptr, ua_client]: host_to_ua_client_) {
        ua_client->SubscribeToChangeValue(false, tags_period_reading_);
    }
    periodic_reading_on_ = false;
    emit sg_periodic_reading_changed(false);
}

bool OPCUADriver::HasTagsToRead() const
{
    return has_tags_to_read_;
}

void OPCUADriver::sl_get_all_tag_names_from_server(const QString &host, const QString &server, size_t tag_cnt)
{
    auto client = take_or_init_client_(host, server);
    auto node = client->GetVariablesNode(host, server, 50);

    auto host_it = hosts_.find(host);
    if(host_it == hosts_.end()) return;

    if(node) {
        auto ep_it = host_to_endpoints_.at(&(*host_it)).find(server);
        if(ep_it == host_to_endpoints_.at(&(*host_it)).end()) return;
        endpoint_to_data_browse_item_[&(*ep_it)] = std::move(node);
    }
}

QString OPCUADriver::endpoint_to_text_(const QOpcUaEndpointDescription &ep_description)
{
    QString ret_str = QString("Url: %1").arg(ep_description.endpointUrl());
    QString security_str = u" Security: "_s;
    switch (ep_description.securityLevel()) {
    case QOpcUaEndpointDescription::None: security_str.append(u"None"_s); break;
    case QOpcUaEndpointDescription::Sign: security_str.append(u"Sign"_s); break;
    case QOpcUaEndpointDescription::SignAndEncrypt: security_str.append(u"SignAndEncrypt"_s); break;
    default: security_str.append(u"Invalid"_s); break;
    }
    return ret_str.append(security_str);
}

COPCUAClient* OPCUADriver::take_or_init_client_(const QString &host, const QString& endpoint)
{
    auto host_it = hosts_.insert(host).first;

    QString ep_key = endpoint.isEmpty() ? u"defaultEp"_s : endpoint;
    auto ep_it = host_to_endpoints_[&(*host_it)].insert(ep_key).first;
    endpoint_to_host_[&(*ep_it)] = &(*host_it);

    if(host_to_ua_client_.contains(&(*host_it))) {
        return host_to_ua_client_.at(&(*host_it)).get();
    }

    host_to_ua_client_[&(*host_it)] = std::move(std::unique_ptr<COPCUAClient>(new COPCUAClient(host)));

    COPCUAClient* new_client = host_to_ua_client_.at(&(*host_it)).get();
    QObject::connect(new_client, &COPCUAClient::sg_got_endpoints, this, &OPCUADriver::sl_get_endpoints);
    QObject::connect(new_client, &COPCUAClient::sg_got_endpoints, this, &OPCUADriver::sg_get_endpoints_names);
    QObject::connect(new_client, &COPCUAClient::sg_send_message_to_console, this, &OPCUADriver::sg_send_message_to_console);
    QObject::connect(new_client, &COPCUAClient::sg_get_part_tag_names_from_server, this, &OPCUADriver::sg_get_part_tag_names_from_server);
    QObject::connect(new_client, &COPCUAClient::sg_get_all_tag_names_from_server, this, &OPCUADriver::sl_get_all_tag_names_from_server);
    QObject::connect(new_client, &COPCUAClient::sg_get_all_tag_names_from_server, this, &OPCUADriver::sg_get_all_tag_names_from_server);
    QObject::connect(new_client, &COPCUAClient::sg_all_tags_readed, this, &OPCUADriver::sg_reading_periodic_complete);
    QObject::connect(new_client, &COPCUAClient::sg_all_tags_readed, this, &OPCUADriver::sg_reading_request_complete);

    return new_client;
}
