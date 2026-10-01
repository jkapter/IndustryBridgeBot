#include "copcuaclient.h"

#include <QOpcUaProvider>
#include <QTimer>
#include <QThread>

#include <algorithm>

using namespace Qt::StringLiterals;

//=====================================================================
//========================== COPCUAClient =============================
//=====================================================================

COPCUAClient::COPCUAClient(const QString& hostname)
    : QObject()
    , hostname_(hostname)
{
    qInfo() << QString("Новый экземпляр ОРС UA клиента, поток [%1]. Url: %2").arg(QThread::currentThread()->objectName(), hostname_);
    init_client_();
    GetEndpoints(hostname);

    write_flush_timer_ = new QTimer(this);
    QObject::connect(write_flush_timer_, &QTimer::timeout, this, &COPCUAClient::sl_write_flush_timeout_);
}

bool COPCUAClient::init_client_()
{
    if(ua_client_) return true;
    root_data_browse_item_.reset();

    QOpcUaProvider provider;
    if (provider.availableBackends().isEmpty()) {
        QString mes = QString("ОРС UA клиент поток [%1]: No available backends!").arg(QThread::currentThread()->objectName());
        qCritical() << mes;
        emit sg_send_message_to_console(mes);
        return false;
    }

    ua_client_.reset(provider.createClient(provider.availableBackends().at(0)));
    if(!ua_client_) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не удалось инициализировать клиента").arg(QThread::currentThread()->objectName());
        qCritical() << mes;
        emit sg_send_message_to_console(mes);
        return false;
    }

    QObject::connect(ua_client_.get(), &QOpcUaClient::endpointsRequestFinished, this, &COPCUAClient::sl_endpoint_request_finished_);
    QObject::connect(ua_client_.get(), &QOpcUaClient::stateChanged, this, &COPCUAClient::sl_connection_state_changed_);

    return true;
}

void COPCUAClient::add_tags_()
{
    size_t cnt = 0;
    for(const auto& it: opc_tags_temp_) {
        if(tag_ptr_to_opc_node_ptr_.contains(it.get())) continue;

        if(it->GetHostName() == hostname_
            && endpoints_texts_.contains(it->GetEndpointName())) {

            auto ua_node_ptr = ua_client_->node(it->GetTagId());
            if(!ua_node_ptr) continue;

            opc_tags_.push_back(it);
            tag_ptr_to_opc_node_ptr_[it.get()] = std::unique_ptr<QOpcUaNode>(ua_node_ptr);
            it->SetUaNodePtr(tag_ptr_to_opc_node_ptr_.at(it.get()).get());

            QObject::connect(tag_ptr_to_opc_node_ptr_.at(it.get()).get(), &QOpcUaNode::attributeRead, this, &COPCUAClient::sl_ua_node_attribute_readed_);
            QObject::connect(tag_ptr_to_opc_node_ptr_.at(it.get()).get(), &QOpcUaNode::attributeUpdated, this, &COPCUAClient::sl_ua_node_value_updated_);
            QObject::connect(tag_ptr_to_opc_node_ptr_.at(it.get()).get(), &QOpcUaNode::enableMonitoringFinished, this, &COPCUAClient::sl_ua_node_monitoring_enabled_);
            QObject::connect(tag_ptr_to_opc_node_ptr_.at(it.get()).get(), &QOpcUaNode::disableMonitoringFinished, this, &COPCUAClient::sl_ua_node_monitoring_disabled_);

            tag_ptr_to_opc_node_ptr_.at(it.get())->readAttributes(QOpcUa::NodeAttribute::Value
                                                                  | QOpcUa::NodeAttribute::NodeClass
                                                                  | QOpcUa::NodeAttribute::Description
                                                                  | QOpcUa::NodeAttribute::DataType
                                                                  | QOpcUa::NodeAttribute::BrowseName
                                                                  | QOpcUa::NodeAttribute::DisplayName
                                                                  );

            opc_node_ptr_to_tag_ptr_[tag_ptr_to_opc_node_ptr_.at(it.get()).get()] = it.get();
            ++cnt;
        }
    }

    if(cnt > 0 && value_monitoring_enabled_) {
        SubscribeToChangeValue(true, period_reading_);
    }

    emit sg_tags_added(cnt);
}

COPCUAClient::~COPCUAClient()
{
    if(ua_client_ && value_monitoring_enabled_) {
        SubscribeToChangeValue(false);
    }
    if(ua_client_ && ua_client_->state() == QOpcUaClient::Connected) {
        client_disconnect_requested_ = true;
        ua_client_->disconnectFromEndpoint();
    }
    while(client_disconnect_requested_) {
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    }

    emit sg_send_message_to_console(QString("ОРС UA клиент поток [%1]: завершен.").arg(QThread::currentThread()->objectName()));
    qInfo() << QString("Завершен ОРС UA клиент, поток %1").arg(QThread::currentThread()->objectName());
}

std::optional<std::set<QString>> COPCUAClient::GetEndpoints(const QString& hostname)
{
    if(!init_client_()) {
        return std::nullopt;
    }

    if(endpoints_.size() > 0) {
        return endpoints_texts_;
    }

    if(endpoints_list_requested_) return std::nullopt;

    endpoints_list_requested_ = true;
    if(!ua_client_->requestEndpoints(hostname_)) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не удалось запросить точки подключения сервера Url:").arg(QThread::currentThread()->objectName(), hostname_);
        qCritical() << mes;
        emit sg_send_message_to_console(mes);
    }

    return std::nullopt;
}

void COPCUAClient::sl_endpoint_request_finished_(QList<QOpcUaEndpointDescription> endpoints)
{
    endpoints_list_requested_ = false;
    endpoints_texts_.clear();
    ep_text_to_index_.clear();
    endpoints_ = std::move(endpoints);

    for(qsizetype i = 0; i < endpoints_.size(); ++i) {
        auto it = endpoints_texts_.insert(endpoint_to_text_(endpoints_.at(i))).first;
        ep_text_to_index_[&(*it)] = i;
    }

    QString mes = QString("ОРС UA клиент поток [%1]: Получено %2 точки подключения сервера Url: %3.")
                      .arg(QThread::currentThread()->objectName())
                      .arg(endpoints_.count())
                      .arg(hostname_);
    qInfo() << mes;

    emit sg_send_message_to_console(mes);
    emit sg_got_endpoints(hostname_);   
}

void COPCUAClient::RefreshEndpointsList()
{
    if(endpoints_list_requested_) return;
    endpoints_texts_.clear();
    ep_text_to_index_.clear();
    endpoints_.clear();
    GetEndpoints(hostname_);
    root_data_browse_item_.reset();
}

void COPCUAClient::AddTags(std::vector<std::shared_ptr<DataTagOpcUA>>& tags)
{
    if(tags.empty() || !ua_client_) return;

    if(endpoints_texts_.empty()) {
        RefreshEndpointsList();
        while(endpoints_list_requested_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents);
        }
    }

    QString endpoint = tags.front()->GetEndpointName();

    if(!endpoints_texts_.contains(endpoint)) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не найдена точка подключения сервера %2 при добавлении тэгов.").arg(QThread::currentThread()->objectName(), endpoint);
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
        return;
    }

    opc_tags_temp_ = tags;
    endpoint_connected_ = endpoint;

    if(ua_client_->state() != QOpcUaClient::Connected) {
        read_tags_requested_ = true;
        client_add_tags_requested_ = true;
        qsizetype index = ep_text_to_index_.at(&(*endpoints_texts_.find(endpoint)));
        request_connect_(endpoints_.at(index));
    } else {
        add_tags_();
    }
}

void COPCUAClient::RemoveTag(const std::shared_ptr<DataTagOpcUA>& tag)
{
    if(!tag) return;
    auto it = tag_ptr_to_opc_node_ptr_.find(tag.get());
    if(it == tag_ptr_to_opc_node_ptr_.end()) return;

    QOpcUaNode* node = it->second.get();
    QObject::disconnect(node, nullptr, nullptr, nullptr);
    if(monitored_nodes_.contains(node)) {
        node->disableMonitoring(QOpcUa::NodeAttribute::Value);
    }
    monitored_nodes_.erase(node);
    monitoring_pending_.erase(node);

    opc_node_ptr_to_tag_ptr_.erase(node);
    tag_ptr_to_opc_node_ptr_.erase(it);
    opc_tags_.erase(std::remove(opc_tags_.begin(), opc_tags_.end(), tag), opc_tags_.end());
}

void COPCUAClient::ReadTags()
{
    if(opc_tags_.empty() || !ua_client_) return;

    tags_to_read_ = static_cast<int>(opc_tags_.size());

    if(ua_client_->state() != QOpcUaClient::Connected) {
        qsizetype index = ep_text_to_index_.at(&(*endpoints_texts_.find(endpoint_connected_)));
        read_tags_requested_ = true;
        request_connect_(endpoints_.at(index));
    } else {
        for(auto& it: opc_tags_) {
            tag_ptr_to_opc_node_ptr_.at(it.get())->readAttributes(QOpcUa::NodeAttribute::Value);
        }
    }
}

size_t COPCUAClient::SubscribeToChangeValue(bool set, double period)
{
    if(!ua_client_) return 0;
    if(period > 1.0) period_reading_ = period;
    value_monitoring_enabled_ = set;

    if(!set) {
        write_flush_timer_->stop();
    }

    if(tag_ptr_to_opc_node_ptr_.empty()) return 0;

    if(set) {
        // Периодически сбрасываем отложенные записи (TGMessageWaitAnswer, фиксированные
        // записи кнопок/событий и т.п.) -- в UA-ветке чтение идёт по подписке, своего
        // "тика", на который можно было бы опереться для записи, как у OPC DA, нет.
        write_flush_timer_->start(static_cast<int>(period_reading_ * 1000.0));
    }

    size_t ret_val = 0;

    if(set && ua_client_->state() != QOpcUaClient::Connected) {
        qsizetype index = ep_text_to_index_.at(&(*endpoints_texts_.find(endpoint_connected_)));
        read_tags_requested_ = true;
        request_connect_(endpoints_.at(index));
    }

    for(auto& [_, ua_node_ptr]: tag_ptr_to_opc_node_ptr_) {
        QOpcUaNode* node = ua_node_ptr.get();
        bool already_monitored = monitored_nodes_.contains(node);
        bool request_pending = monitoring_pending_.contains(node);
        if(set && !already_monitored && !request_pending) {
            QOpcUaMonitoringParameters params(period_reading_ * 1000.0);
            if(node->enableMonitoring(QOpcUa::NodeAttribute::Value, params)) {
                monitoring_pending_.insert(node);
                ++ret_val;
            }
        } else if(!set && already_monitored && !request_pending) {
            if(node->disableMonitoring(QOpcUa::NodeAttribute::Value)) {
                monitoring_pending_.insert(node);
                ++ret_val;
            }
        }
    }
    return ret_val;
}

size_t COPCUAClient::WriteTags()
{
    if(!ua_client_) return 0;
    size_t ret_val = 0;
    for(auto& it: opc_tags_) {
        QVariant val_to_write = it->GetOPCVariantToWrite();
        if(val_to_write.isValid()) {
            tag_ptr_to_opc_node_ptr_.at(it.get())->writeValueAttribute(val_to_write, it->GetOpcUaType());
            it->MarkWriteSent();
            ++ret_val;
        }
    }
    return ret_val;
}

void COPCUAClient::ClearTags()
{
    if(ua_client_ && ua_client_->state() == QOpcUaClient::Connected) {
        ua_client_->disconnect();
        client_disconnect_requested_ = true;
        return;
    }
    opc_tags_.clear();
    tag_ptr_to_opc_node_ptr_.clear();
    opc_node_ptr_to_tag_ptr_.clear();
    monitored_nodes_.clear();
    monitoring_pending_.clear();
}

std::unique_ptr<DataBrowseItem> COPCUAClient::GetVariablesNode(const QString& hostname, const QString& server_name, int notify_of_portion)
{
    if(root_data_browse_item_ && !tags_tree_is_browsing_) {
        browse_item_to_ua_node_.clear();
        return std::move(root_data_browse_item_);
    }
    if(tags_tree_is_browsing_) return nullptr;

    if(endpoints_texts_.empty()) {
        RefreshEndpointsList();
        while(endpoints_list_requested_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::WaitForMoreEvents);
        }
    }

    if(!endpoints_texts_.contains(server_name)) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не найдена точка подключения сервера %2 при добавлении тэгов.").arg(QThread::currentThread()->objectName(), server_name);
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
        return nullptr;
    }

    browsed_tags_to_notify_ = 0;
    notify_of_portion_ = notify_of_portion;

    read_tags_requested_ = true;
    qsizetype index = ep_text_to_index_.at(&(*endpoints_texts_.find(server_name)));
    endpoint_connected_ = server_name;

    if(ua_client_->state() != QOpcUaClient::Connected) {
        browse_requested_ = true;
        request_connect_(endpoints_.at(index));
        return nullptr;
    }

    browse_item_to_ua_node_.clear();
    root_data_browse_item_.reset(new DataBrowseItem(DataBrowseItem::ItemType::ENDPOINT, DataTag::DataSource::OPCUA,
                                                    endpoint_connected_, u"ns=0;i=84"_s, nullptr));

    browse_item_to_ua_node_[root_data_browse_item_.get()] = std::move(std::unique_ptr<QOpcUaNode>(ua_client_->node(u"ns=0;i=84"_s)));

    browse_childs_making_tree_(root_data_browse_item_.get(), notify_of_portion);

    return nullptr;
}

void COPCUAClient::browse_childs_making_tree_(DataBrowseItem *parent, int notify_of_tags_browsed)
{
    if(!parent
        || !browse_item_to_ua_node_.contains(parent)
        || !browse_item_to_ua_node_.at(parent))
        return;

    QObject::connect(browse_item_to_ua_node_.at(parent).get(), &QOpcUaNode::browseFinished
                     , this
                     , [this, parent, notify_of_tags_browsed](QList<QOpcUaReferenceDescription> children, QOpcUa::UaStatusCode statusCode)
                     {
                        parent->SetBrowsed(true);
                        bool all_childs_is_vars = true; //or no childs
                        for(auto& it: children) {
                            QString node_id = it.targetNodeId().nodeId();
                            QString name = it.browseName().name();
                            DataBrowseItem::ItemType type = it.nodeClass() == QOpcUa::NodeClass::Variable ? DataBrowseItem::ItemType::VARIABLE : DataBrowseItem::ItemType::NODE;
                            all_childs_is_vars = all_childs_is_vars && (type == DataBrowseItem::ItemType::VARIABLE);
                            auto child_ptr = std::unique_ptr<DataBrowseItem>(new DataBrowseItem(type, DataTag::DataSource::OPCUA, name, node_id, parent));

                            ++browsed_tags_to_notify_;
                            if(browsed_tags_to_notify_ % notify_of_tags_browsed == 0) {
                                emit sg_get_part_tag_names_from_server(hostname_, endpoint_connected_, browsed_tags_to_notify_);
                            }

                            DataBrowseItem* child_raw = child_ptr.get();
                            if(!parent->AppendChild(std::move(child_ptr))) continue;

                            browse_item_to_ua_node_[child_raw] = std::unique_ptr<QOpcUaNode>(ua_client_->node(node_id));
                            browse_childs_making_tree_(child_raw, notify_of_tags_browsed);
                        }
                        if(all_childs_is_vars && check_all_childs_browsed_(root_data_browse_item_.get())) {
                            tags_tree_is_browsing_ = false;
                            emit sg_get_all_tag_names_from_server(hostname_, endpoint_connected_, root_data_browse_item_->UpdateItemRecursievly());
                        }
                     });

    tags_tree_is_browsing_ = true;
    bool b = browse_item_to_ua_node_.at(parent).get()->browseChildren(QOpcUa::ReferenceTypeId::HierarchicalReferences, QOpcUa::NodeClass::Object | QOpcUa::NodeClass::Variable);

    if(!b) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не удалось запросить дочерние узлы для %2.").arg(QThread::currentThread()->objectName(), browse_item_to_ua_node_.at(parent).get()->nodeId());
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
    }
}

bool COPCUAClient::check_all_childs_browsed_(DataBrowseItem *parent)
{
    for(int i = 0; i < parent->ChildCount(); ++i) {
        if(!parent->Child(i)->IsBrowsed() || !check_all_childs_browsed_(parent->Child(i))) return false;
    }
    return true;
}

void COPCUAClient::sl_connection_state_changed_(QOpcUaClient::ClientState state)
{
    if(state == QOpcUaClient::ClientState::Disconnected) {
        tags_tree_is_browsing_ = false;
        endpoints_list_requested_ = false;

        if(client_disconnect_requested_) {
            client_disconnect_requested_ = false;
            connect_in_progress_ = false;
            client_add_tags_requested_ = false;
            read_tags_requested_ = false;
            browse_requested_ = false;
        } else {
            QString mes = QString("ОРС UA клиент поток [%1]: Неожиданное закрытие соединения, попытка перезапуска").arg(QThread::currentThread()->objectName());
            qWarning() << mes;
            emit sg_send_message_to_console(mes);
            QTimer::singleShot(1000, this,
                               [this](){
                                   qsizetype index = ep_text_to_index_.at(&(*endpoints_texts_.find(endpoint_connected_)));
                                   connect_in_progress_ = false;
                                   request_connect_(endpoints_.at(index));
                                });
            return;
        }
     }

    if(state == QOpcUaClient::ClientState::Connected) {
        connect_in_progress_ = false;
        endpoint_connected_ = endpoint_to_text_(ua_client_->endpoint());

        if(client_add_tags_requested_) {
            add_tags_();
            client_add_tags_requested_ = false;
        }
        if(read_tags_requested_) {
            read_tags_requested_ = false;
            for(auto& it: opc_tags_) {
                tag_ptr_to_opc_node_ptr_.at(it.get())->readAttributes(QOpcUa::NodeAttribute::Value);
            }
        }
        if(browse_requested_){
            //GetVariablesNode(hostname_, endpoint_connected_, notify_of_portion_);
        }
        QString mes = QString("ОРС UA клиент поток [%1]: Клиент успешно подключен к точке [%2]").arg(QThread::currentThread()->objectName(), endpoint_connected_);
        qWarning() << mes;
        emit sg_send_message_to_console(mes);
    }
}

void COPCUAClient::sl_ua_node_attribute_readed_(QOpcUa::NodeAttributes attributes)
{
    if(read_tags_requested_ && (attributes & QOpcUa::NodeAttribute::Value)) {
        --tags_to_read_;
    }

    if(read_tags_requested_ && tags_to_read_ == 0) {
        read_tags_requested_ = false;
        emit sg_all_tags_readed(opc_tags_.size());
    }
}

void COPCUAClient::sl_ua_node_value_updated_(QOpcUa::NodeAttribute attribute, QVariant value)
{
    if(attribute == QOpcUa::NodeAttribute::Value) {
        emit sg_all_tags_readed(opc_tags_.size());
    }
}

void COPCUAClient::sl_ua_node_monitoring_enabled_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode)
{
    if(attribute != QOpcUa::NodeAttribute::Value) return;
    auto* node = qobject_cast<QOpcUaNode*>(sender());
    if(!node) return;

    monitoring_pending_.erase(node);
    if(statusCode == QOpcUa::UaStatusCode::Good) {
        monitored_nodes_.insert(node);
    } else {
        qWarning() << QString("ОРС UA клиент поток [%1]: не удалось включить мониторинг узла %2, статус %3.")
                          .arg(QThread::currentThread()->objectName(), node->nodeId()).arg(static_cast<quint32>(statusCode));
    }
}

void COPCUAClient::sl_ua_node_monitoring_disabled_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode)
{
    if(attribute != QOpcUa::NodeAttribute::Value) return;
    auto* node = qobject_cast<QOpcUaNode*>(sender());
    if(!node) return;

    monitoring_pending_.erase(node);
    monitored_nodes_.erase(node);
}

void COPCUAClient::sl_write_flush_timeout_()
{
    WriteTags();
}

QString COPCUAClient::endpoint_to_text_(const QOpcUaEndpointDescription &ep_description)
{
    QString ret_str = QString("Url: %1").arg(ep_description.endpointUrl());
    QString security_str = u" Security: "_s;
    switch (ep_description.securityMode()) {
    case QOpcUaEndpointDescription::MessageSecurityMode::None:
        security_str.append(u"None"_s); break;
    case QOpcUaEndpointDescription::MessageSecurityMode::Sign:
        security_str.append(u"Sign"_s); break;
    case QOpcUaEndpointDescription::MessageSecurityMode::SignAndEncrypt:
        security_str.append(u"SignAndEncrypt"_s); break;
    default:
        security_str.append(u"Invalid"_s); break;
    }
    return ret_str.append(security_str);
}

void COPCUAClient::request_connect_(const QOpcUaEndpointDescription &ep_description)
{
    if(connect_in_progress_) return;
    connect_in_progress_ = true;
    ua_client_->connectToEndpoint(ep_description);
}

void COPCUAClient::clear_internal_data_()
{
    if(write_flush_timer_) write_flush_timer_->stop();
    endpoints_texts_.clear();
    ep_text_to_index_.clear();
    endpoints_.clear();
    opc_tags_.clear();
    tag_ptr_to_opc_node_ptr_.clear();
    opc_node_ptr_to_tag_ptr_.clear();
    monitored_nodes_.clear();
    monitoring_pending_.clear();
    opc_tags_temp_.clear();
    root_data_browse_item_.reset(nullptr);
}


