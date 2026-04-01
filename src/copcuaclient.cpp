#include "copcuaclient.h"

#include <QOpcUaProvider>
#include <QTimer>
#include <QThread>

COPCUAClient::COPCUAClient()
    : QObject()
{
    qInfo() << QString("Новый экземпляр ОРС UA клиента, поток [%1]").arg(QThread::currentThread()->objectName());

}

COPCUAClient::~COPCUAClient()
{
    emit sg_send_message_to_console(QString("ОРС UA клиент поток [%1]: завершен.").arg(QThread::currentThread()->objectName()));
    qInfo() << QString("Завершен ОРС UA клиент, поток %1").arg(QThread::currentThread()->objectName());
}

const std::set<QString>& COPCUAClient::GetEndpoints(const QString &hostname)
{
    if(!init_client_()) {
        return {};
    }

    if(hostname_ == hostname && endpoints_.size() > 0) {
        return endpoints_texts_;
    }

    endpoints_requested_ = true;
    QTimer::singleShot(5000, this, [this] {endpoints_requested_ = false;});

    if(!ua_client_->requestEndpoints(hostname)) {
        QString mes = QString("ОРС UA клиент поток [%1]: Не удалось запросит точки подключения сервера.").arg(QThread::currentThread()->objectName());
        qCritical() << mes;
        emit sg_send_message_to_console(mes);
        return {};
    }

    while(endpoints_requested_) {
        QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
    }

    hostname_ = hostname;
    for(qsizetype i = 0; i < endpoints_.size(); ++i) {
        auto it = endpoints_texts_.insert(endpoint_to_text_(endpoints_.at(i))).first;
        ep_text_to_index_[i] = &(*it);
    }
    return endpoints_texts_;
}

void COPCUAClient::RefreshEndpointsList()
{
    clear_internal_data_();
    GetEndpoints(hostname_);
}

size_t COPCUAClient::AddTags(std::vector<std::shared_ptr<DataTagOpcUA> > &tags)
{
    if(tags.empty()) return 0;

    size_t ret_val = 0;
    if(tags.front()->GetHostName() != hostname_ || !endpoints_texts_.contains(tags.front()->GetEndpointName())) {
        hostname_ = tags.front()->GetHostName();
        RefreshEndpointsList();
    }

    for(const auto& it: tags) {
        if(it->GetHostName() == hostname_
            && endpoints_texts_.contains(it->GetEndpointName())) {

            opc_tags_.push_back(it);
            ++ret_val;
        }
    }
    return ret_val;
}

size_t COPCUAClient::ReadTags()
{

}

void COPCUAClient::ClearTags()
{
    if(ua_client_ && ua_client_->connected()) {
        ua_client_->disconnect();
    }
    opc_tags_.clear();
}

void COPCUAClient::sl_endpoint_request_finished(QList<QOpcUaEndpointDescription> endpoints)
{
    QString mes = QString("ОРС UA клиент поток [%1]: Получено %2 точки подключения сервера.").arg(QThread::currentThread()->objectName()).arg(endpoints.count());
    qInfo() << mes;
    emit sg_send_message_to_console(mes);
    clear_internal_data_();
    endpoints_ = std::move(endpoints);
    endpoints_requested_ = false;
}

void COPCUAClient::sl_connection_state_changed(QOpcUaClient::ClientState state)
{

}

QString COPCUAClient::endpoint_to_text_(const QOpcUaEndpointDescription &ep_description)
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

void COPCUAClient::clear_internal_data_()
{
    endpoints_texts_.clear();
    ep_text_to_index_.clear();
    endpoints_.clear();
}


bool COPCUAClient::init_client_()
{
    if(ua_client_) return true;

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

    QObject::connect(ua_client_.get(), &QOpcUaClient::endpointsRequestFinished, this, &COPCUAClient::sl_endpoint_request_finished);
    QObject::connect(ua_client_.get(), &QOpcUaClient::stateChanged, this, &COPCUAClient::sl_connection_state_changed);

    return true;
}

