#include "sourcedrivers.h"

#include <QThread>
#include <QTimer>

#include "opcclientworker.h"
#include "copcclient.h"

using namespace Qt::StringLiterals;

OPCDADriver::OPCDADriver(QObject *parent): DriverInterface(parent)
{}

OPCDADriver::~OPCDADriver()
{
    request_stop_periodic_reading_ = true;
    emit sg_stop_browsing_tags();
    emit sg_stop_reading();

    for(auto& it: thread_pointers_) {
        if(!it.isNull() && it->isRunning()) {
            it->quit();
            it->wait(TIME_WAITING_THREAD_);
        }
    }

    qInfo() << u"OPCDADriver деструктор завершен."_s;
}

std::set<QString> OPCDADriver::GetEndpointNames(const QString &host)
{
    if(host_to_servers_.contains(host)) return host_to_servers_.at(host);
    COPCClient client;
    auto servers = client.GetOPCServerNames(host);
    host_to_servers_[host] = {servers.begin(), servers.end()};
    emit sg_get_endpoints_names(host);
    return host_to_servers_.at(host);
}

void OPCDADriver::start_browsing_tags_names_(const QString &hostname, const QString &server_name)
{
    if(!host_to_servers_.contains(hostname)) {
        GetEndpointNames(hostname);
    }

    if(!host_to_servers_.contains(hostname)) return;

    auto server_it = host_to_servers_.at(hostname).find(server_name);
    if(server_it == host_to_servers_.at(hostname).end()) return;

    if(!server_to_tag_names_.contains(&(*server_it))
        || !server_tags_is_browsing_.contains(&(*server_it))) {

        server_to_tag_names_[&(*server_it)] = {};
        server_tags_is_browsing_[&(*server_it)] = true;

        OPCDATagBrowser* browser = new OPCDATagBrowser(hostname, server_name, server_to_tag_names_.at(&(*server_it)));
        QThread* opc_thread = new QThread(this);
        thread_pointers_.push_back(opc_thread);

        QObject::connect(browser, &OPCDATagBrowser::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
        QObject::connect(browser, &OPCDATagBrowser::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
        QObject::connect(this, &OPCDADriver::sg_stop_browsing_tags, browser, &OPCDATagBrowser::sl_stop_browsing);
        QObject::connect(opc_thread, &QThread::started, browser, &OPCDATagBrowser::sl_process);
        QObject::connect(browser, &OPCDATagBrowser::sg_finished, opc_thread, &QThread::quit);
        QObject::connect(opc_thread, &QThread::finished, opc_thread, &QObject::deleteLater);
        QObject::connect(opc_thread, &QThread::finished, this, [this](){--opc_threads_on_request_count_;});
        QObject::connect(browser, &OPCDATagBrowser::sg_get_part_tag_names_from_server, this, &OPCDADriver::sg_get_part_tag_names_from_server);
        QObject::connect(browser, &OPCDATagBrowser::sg_get_all_tag_names_from_server, this, &OPCDADriver::sl_get_all_tag_names_from_server);
        QObject::connect(browser, &OPCDATagBrowser::sg_get_all_tag_names_from_server, this, &OPCDADriver::sg_get_all_tag_names_from_server);
        QObject::connect(opc_thread, &QThread::finished, [browser] {delete browser;});

        browser->moveToThread(opc_thread);
        opc_thread->start();
        ++opc_threads_on_request_count_;
        emit sg_send_message_to_console(QString("OPCDADriver: Запрос списка тэгов сервера %1 на хосте %2.").arg(server_name, hostname));
    }
}

std::optional<const std::vector<QString>> OPCDADriver::GetTagNames(const QString &hostname, const QString &server_name)
{
    if(!host_to_servers_.contains(hostname)) return std::nullopt;

    auto server_it = host_to_servers_.at(hostname).find(server_name);
    if(server_it == host_to_servers_.at(hostname).end()) return std::nullopt;

    if(!server_to_tag_names_.contains(&(*server_it))) {
        start_browsing_tags_names_(hostname, server_name);
    }

    if(!server_to_tag_names_.contains(&(*server_it)) || !server_tags_is_browsing_.contains(&(*server_it)) || server_tags_is_browsing_.at(&(*server_it))) {
        return std::nullopt;
    }

    return server_to_tag_names_.at(&(*server_it));
}

DataBrowseItem *OPCDADriver::GetVariablesNode(const QString &hostname, const QString &server_name)
{
    if(!host_to_servers_.contains(hostname)) {
        GetEndpointNames(hostname);
    }

    auto server_it = host_to_servers_.at(hostname).find(server_name);
    if(server_it == host_to_servers_.at(hostname).end()) return nullptr;

    if(!server_to_tag_names_.contains(&(*server_it))) {
        start_browsing_tags_names_(hostname, server_name);
    }

    if(!server_to_tag_names_.contains(&(*server_it)) || !server_tags_is_browsing_.contains(&(*server_it)) || server_tags_is_browsing_.at(&(*server_it))) {
        return nullptr;
    }

    DataBrowseItem* ret_ptr = new DataBrowseItem(DataBrowseItem::ItemType::ENDPOINT, DataTag::DataSource::OPCDA,
                                                 server_name, QString("[%1][%2]").arg(hostname, server_name), nullptr);

    for(const auto& it: server_to_tag_names_.at(&(*server_it))) {
        ret_ptr->AppendChild(DataBrowseItem::ItemType::VARIABLE, it, it);
    }
    return ret_ptr;
}

size_t OPCDADriver::SetTagsList(std::vector<std::shared_ptr<DataTag>>& tags)
{
    tags_to_read_.clear();
    for(const auto& it: tags) {
        if(it->GetDataSource() == DataTag::DataSource::OPCDA) tags_to_read_.push_back(it);
    }
    return tags_to_read_.size();
}

void OPCDADriver::WriteTagNow(const std::shared_ptr<DataTag>& tag)
{
    if(!tag || tag->GetDataSource() != DataTag::DataSource::OPCDA) return;

    std::vector<std::shared_ptr<DataTag>> tags{tag};
    ReadTagsOnce(tags);
}

void OPCDADriver::ReadTagsOnce(std::vector<std::shared_ptr<DataTag> > &tags)
{
    QThread* thread_req = new QThread();
    thread_pointers_.push_back(thread_req);

    OPCDAWorker* worker = new OPCDAWorker();

    worker->SetTagsList(tags);
    worker->moveToThread(thread_req);

    QObject::connect(thread_req, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread_req, &QThread::finished, thread_req, &QObject::deleteLater);
    QObject::connect(thread_req, &QThread::finished, this, &OPCDADriver::sl_on_request_thread_finished);
    QObject::connect(thread_req, &QThread::started, worker, &OPCDAWorker::sl_process);
    QObject::connect(worker, &OPCDAWorker::sg_finished, thread_req, &QThread::quit);
    QObject::connect(this, &OPCDADriver::sg_stop_reading, worker, &OPCDAWorker::sl_stop_reading);
    QObject::connect(worker, &OPCDAWorker::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sl_thread_send_exception);
    QObject::connect(worker, &OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sg_reading_request_complete);
    QObject::connect(worker, &OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sl_on_request_reading_tags_complete);
    QObject::connect(worker, &OPCDAWorker::sg_server_error, this, &OPCDADriver::sl_thread_send_opc_status);

    ++opc_threads_on_request_count_;

    thread_req->start();

    QString log_message = QString("OPCDADriver: опрос %1 тэгов по запросу.").arg(tags.size());
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
}

bool OPCDADriver::PeriodicReadingOn() const
{
    return period_reading_on_;
}

void OPCDADriver::StartPeriodReading()
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
    thread_pointers_.push_back(thread_per);

    OPCDAWorker* worker = new OPCDAWorker();

    size_t n_nags = tags_to_read_.size();
    worker->SetTagsList(tags_to_read_);
    worker->moveToThread(thread_per);
    worker->SetPeriodicReading(opc_period_reading_);

    QObject::connect(thread_per, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread_per, &QThread::finished, thread_per, &QObject::deleteLater);
    QObject::connect(thread_per, &QThread::finished, this, &OPCDADriver::sl_periodic_thread_finished);
    QObject::connect(thread_per, &QThread::started, worker, &OPCDAWorker::sl_process);
    QObject::connect(worker, &OPCDAWorker::sg_finished, thread_per, &QThread::quit);
    QObject::connect(this, &OPCDADriver::sg_stop_reading, worker, &OPCDAWorker::sl_stop_reading);
    QObject::connect(worker, &OPCDAWorker::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sl_thread_send_exception);
    QObject::connect(worker, &OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sg_reading_periodic_complete);
    QObject::connect(worker, &OPCDAWorker::sg_server_error, this, &OPCDADriver::sl_thread_send_opc_status);

    errors_periodic_opc_server_count_ = 0;
    errors_server_status_periodic_count_ = 0;

    thread_per->start();

    QString log_message = QString("OPCDADriver: запуск периодического опроса. Количество тэгов %1").arg(n_nags);
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
    request_stop_periodic_reading_ = false;
    period_reading_on_ = true;
    emit sg_periodic_reading_changed(true);
}

void OPCDADriver::StartPeriodReading(int period)
{
    SetPeriodReading(period);
    StartPeriodReading();
}

void OPCDADriver::StopPeriodReading()
{
    request_stop_periodic_reading_ = true;
    emit sg_stop_reading();
    emit sg_periodic_reading_changed(false);
}

bool OPCDADriver::HasTagsToRead() const
{
    return !tags_to_read_.empty();
}

void OPCDADriver::sl_on_request_thread_finished()
{
    --opc_threads_on_request_count_;
}

void OPCDADriver::sl_on_request_reading_tags_complete(size_t ntags)
{
    QString log_message = QString("OPCDADriver: прочитано %1 тэгов по запросу.").arg(ntags);
    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;
}

void OPCDADriver::sl_thread_send_opc_status(QString host, QString server, size_t server_state)
{
    QString ser_state;
    switch(server_state) {
    case OPC_STATUS_RUNNING: ser_state = u"OPC_STATUS_RUNNING"_s; break;
    case OPC_STATUS_FAILED: ser_state = u"OPC_STATUS_FAILED"_s; break;
    case OPC_STATUS_NOCONFIG: ser_state = u"OPC_STATUS_NOCONFIG"_s; break;
    case OPC_STATUS_SUSPENDED: ser_state = u"OPC_STATUS_SUSPENDED"_s; break;
    case OPC_STATUS_TEST: ser_state = u"OPC_STATUS_TEST"_s; break;
    case OPC_STATUS_COMM_FAULT:	ser_state = u"OPC_STATUS_COMM_FAULT"_s; break;
    default: ser_state = u"UNKNOWN"_s; break;
    }

    QString log_message = QString("OPCDADriver: сервер %1@%2 получено состояние %3").arg(host, server, ser_state);
    emit sg_send_message_to_console(log_message);
    qWarning() << log_message;

    if(server_state != OPC_STATUS_RUNNING && period_reading_on_) ++errors_server_status_periodic_count_;
    if(errors_server_status_periodic_count_ > MAX_PERIODIC_ERRORS_COUNT && !request_stop_periodic_reading_) {
        log_message = QString("OPCDADriver: перезапуск клиента по максимальному количеству ошибок чтения.");
        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;
        emit sg_stop_reading();
        request_stop_periodic_reading_ = false;
    }
}

void OPCDADriver::sl_periodic_thread_finished()
{
    period_reading_on_ = false;
    emit sg_periodic_reading_changed(false);
    if(!request_stop_periodic_reading_) {
        QTimer::singleShot(opc_period_reading_*2000, this, [this]() {this->StartPeriodReading();});
        QString log_message = QString("OPCDADriver: неожиданное завершение потока клиента, перезапуск.");
        emit sg_send_message_to_console(log_message);
        qCritical() << log_message;
    }
}

void OPCDADriver::sl_thread_send_exception(QString text)
{
    QString log_message = QString("OPCDADriver: получено исключение в ОРС-клиенте: %1").arg(text);
    emit sg_send_message_to_console(log_message);
    qCritical() << log_message;
    emit sg_stop_reading();
    emit sg_send_message_to_console(QString("OPCDADriver: таймер на рестарт клиента периодического опроса."));
    emit sg_periodic_reading_changed(false);
}

void OPCDADriver::sl_get_all_tag_names_from_server(const QString& hostname, const QString& server, size_t n_tags)
{
    if(!host_to_servers_.contains(hostname)) return;

    auto server_it = host_to_servers_.at(hostname).find(server);
    if(server_it == host_to_servers_.at(hostname).end()) return;

    if(server_tags_is_browsing_.contains(&(*server_it))) server_tags_is_browsing_.at(&(*server_it)) = false;
}
