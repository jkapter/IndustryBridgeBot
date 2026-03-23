#include "sourcedrivers.h"

#include <QThread>
#include <QTimer>

#include "opcclientworker.h"
#include "copcclient.h"

OPCDADriver::OPCDADriver(QObject *parent)
{}

OPCDADriver::~OPCDADriver()
{
    emit sg_stop_browsing_tags();
    emit sg_stop_reading();
    if(opc_threads_on_request_count_ > 0 || period_reading_on_) {
        QTimer::singleShot(TIME_WAITING_THREAD_, this,
                           [this](){
                               opc_threads_on_request_count_ = 0;
                               period_reading_on_ = false;
                           });
        while(opc_threads_on_request_count_ > 0 || period_reading_on_) {
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::ExcludeUserInputEvents | QEventLoop::ExcludeSocketNotifiers);
        }
    }

    qInfo() << "OPCDADriver деструктор завершен.";
}

std::set<QString> OPCDADriver::GetServerNames(const QString &host)
{
    if(host_to_servers_.contains(host)) return host_to_servers_.at(host);
    OPC_HELPER::COPCClient client;
    auto servers = client.GetOPCServerNames(host);
    host_to_servers_[host] = {servers.begin(), servers.end()};
    return host_to_servers_.at(host);
}

void OPCDADriver::StartBrowsingTagsNames(const QString &hostname, const QString &server_name)
{
    if(!host_to_servers_.contains(hostname)) {
        GetServerNames(hostname);
    }

    if(!host_to_servers_.contains(hostname)) return;

    auto server_it = host_to_servers_.at(hostname).find(server_name);
    if(server_it == host_to_servers_.at(hostname).end()) return;

    if(!server_to_tag_names_.contains(&(*server_it))
        || !server_tags_is_browsing_.contains(&(*server_it))) {

        server_to_tag_names_[&(*server_it)] = {};
        server_tags_is_browsing_[&(*server_it)] = true;

        OPC_HELPER::OPCDATagBrowser* browser = new OPC_HELPER::OPCDATagBrowser(hostname, server_name, server_to_tag_names_.at(&(*server_it)));
        QThread* opc_thread = new QThread(this);
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
        QObject::connect(this, &OPCDADriver::sg_stop_browsing_tags, browser, &OPC_HELPER::OPCDATagBrowser::sl_stop_browsing);
        QObject::connect(opc_thread, &QThread::started, browser, &OPC_HELPER::OPCDATagBrowser::sl_process);
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_finished, opc_thread, &QThread::quit);
        QObject::connect(opc_thread, &QThread::finished, opc_thread, &QObject::deleteLater);
        QObject::connect(opc_thread, &QThread::finished, this, [this](){--opc_threads_on_request_count_;});
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_get_part_tag_names_from_server, this, &OPCDADriver::sg_get_part_tag_names_from_server);
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_get_all_tag_names_from_server, this, &OPCDADriver::sl_get_all_tag_names_from_server);
        QObject::connect(browser, &OPC_HELPER::OPCDATagBrowser::sg_get_all_tag_names_from_server, this, &OPCDADriver::sg_get_all_tag_names_from_server);
        QObject::connect(opc_thread, &QThread::finished, [browser] {delete browser;});

        browser->moveToThread(opc_thread);
        opc_thread->start();
        ++opc_threads_on_request_count_;
        emit sg_send_message_to_console(QString("OPCDADriver: Запрос списка тэгов сервера %1 на хосте %2.").arg(server_name, hostname));
        return;
    }
}

std::optional<const std::vector<QString>> OPCDADriver::GetTagNames(const QString &hostname, const QString &server_name)
{
    if(!host_to_servers_.contains(hostname)) return std::nullopt;

    auto server_it = host_to_servers_.at(hostname).find(server_name);
    if(server_it == host_to_servers_.at(hostname).end()) return std::nullopt;;

    if(!server_to_tag_names_.contains(&(*server_it)) || !server_tags_is_browsing_.contains(&(*server_it)) || server_tags_is_browsing_.at(&(*server_it))) return std::nullopt;

    return server_to_tag_names_.at(&(*server_it));
}

size_t OPCDADriver::SetTagsList(std::vector<std::shared_ptr<DataTag>>&& tags)
{
    tags_to_read_.clear();
    for(const auto& it: tags) {
        if(it->GetDataSource() == DataTag::DataSource::OPCDA) tags_to_read_.push_back(it);
    }
    return tags_to_read_.size();
}

void OPCDADriver::ReadTagsOnce(std::vector<std::shared_ptr<DataTag> > &tags)
{
    QThread* thread_req = new QThread();
    OPC_HELPER::OPCDAWorker* worker = new OPC_HELPER::OPCDAWorker();

    worker->SetTagsList(tags);
    worker->moveToThread(thread_req);

    QObject::connect(thread_req, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread_req, &QThread::finished, thread_req, &QObject::deleteLater);
    QObject::connect(thread_req, &QThread::finished, this, &OPCDADriver::sl_on_request_thread_finished);
    QObject::connect(thread_req, &QThread::started, worker, &OPC_HELPER::OPCDAWorker::sl_process);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_finished, thread_req, &QThread::quit);
    QObject::connect(this, &OPCDADriver::sg_stop_reading, worker, &OPC_HELPER::OPCDAWorker::sl_stop_reading);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sl_thread_send_exception);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sg_reading_request_complete);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sl_on_request_reading_tags_complete);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_server_error, this, &OPCDADriver::sl_thread_send_opc_status);

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

int OPCDADriver::GetPeriodReading() const
{
    return opc_period_reading_;
}

void OPCDADriver::SetPeriodReading(int period)
{
    opc_period_reading_ = period > 0 ? period : opc_period_reading_;
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
    OPC_HELPER::OPCDAWorker* worker = new OPC_HELPER::OPCDAWorker();

    size_t n_nags = tags_to_read_.size();
    worker->SetTagsList(tags_to_read_);
    worker->moveToThread(thread_per);
    worker->SetPeriodicReading(opc_period_reading_);

    QObject::connect(thread_per, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(thread_per, &QThread::finished, thread_per, &QObject::deleteLater);
    QObject::connect(thread_per, &QThread::finished, this, &OPCDADriver::sl_periodic_thread_finished);
    QObject::connect(thread_per, &QThread::started, worker, &OPC_HELPER::OPCDAWorker::sl_process);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_finished, thread_per, &QThread::quit);
    QObject::connect(this, &OPCDADriver::sg_periodic_reading_changed, worker, &OPC_HELPER::OPCDAWorker::sl_stop_reading);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_send_message_to_console, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sg_send_message_to_console);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_opcclient_got_exception, this, &OPCDADriver::sl_thread_send_exception);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_reading_complete, this, &OPCDADriver::sg_reading_periodic_complete);
    QObject::connect(worker, &OPC_HELPER::OPCDAWorker::sg_server_error, this, &OPCDADriver::sl_thread_send_opc_status);

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
    emit sg_periodic_reading_changed(false);
    request_stop_periodic_reading_ = true;
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
    case OPC_STATUS_RUNNING: ser_state = "OPC_STATUS_RUNNING"; break;
    case OPC_STATUS_FAILED: ser_state = "OPC_STATUS_FAILED"; break;
    case OPC_STATUS_NOCONFIG: ser_state = "OPC_STATUS_NOCONFIG"; break;
    case OPC_STATUS_SUSPENDED: ser_state = "OPC_STATUS_SUSPENDED"; break;
    case OPC_STATUS_TEST: ser_state = "OPC_STATUS_TEST"; break;
    case OPC_STATUS_COMM_FAULT:	ser_state = "OPC_STATUS_COMM_FAULT"; break;
    default: ser_state = "UNKNOWN"; break;
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
