#include "opcclientworker.h"

#include <QTimer>

#include "datatag_opcda.h"

using namespace Qt::StringLiterals;

OPCDAWorker::OPCDAWorker(std::vector<std::shared_ptr<DataTag>>& tags, QObject *parent)
    : OPCDAWorker(parent)
{
    SetTagsList(tags);
}

OPCDAWorker::OPCDAWorker(QObject *parent)
    : QObject(parent)
    , periodic_timer_(new QTimer(this))
{}

OPCDAWorker::~OPCDAWorker()
{
    sl_stop_reading();
}

void OPCDAWorker::SetTagsList(const std::vector<std::shared_ptr<DataTag>>& tags) {
    tags_.clear();
    tags_.reserve(tags.size());
    hostnames_.clear();
    hostname_to_server_names_.clear();

    for(const auto& tag: tags) {
        if(tag->GetDataSource() != ::DataTag::DataSource::OPCDA) continue;
        auto casted_tag = std::static_pointer_cast<DataTagOpcDA>(tag);
        tags_.push_back(casted_tag);
        auto host_it = hostnames_.insert(casted_tag->GetHostName()).first;
        hostname_to_server_names_[&(*host_it)].insert(casted_tag->GetEndpointName());
    }

    qInfo() << QString("ОРС DA клиент поток [%1]: добавлено %2 тэгов для чтения.").arg(QThread::currentThread()->objectName()).arg(tags.size());
}

void OPCDAWorker::SetTagsList(const std::vector<std::shared_ptr<DataTag>> &&tags)
{
    auto vec = std::move(tags);
    SetTagsList(vec);
}

bool OPCDAWorker::SetPeriodicReading(int period)
{
    period_reading_ = period >= 0 ? period : period_reading_;
    return period == period_reading_;
}

void OPCDAWorker::sl_read_tags()
{
    try {
        size_t res = opc_client_->WriteTags();
        if(res > 0) {
            QString log_message = QString("ОРС DA клиент: записано %1 тэгов.").arg(res);
            emit sg_send_message_to_console(log_message);
            qInfo() << log_message;
            emit sg_writing_tags(res);
        }

        res = opc_client_->ReadTags();
        if(res == tags_.size()) {
            emit sg_reading_complete(res);
        } else {
            emit sg_reading_error(res);
            qWarning() << QString("ОРС клиент: ошибка чтения, прочитано %1 из %2 тэгов.")
                              .arg(res)
                              .arg(static_cast<quint64>(tags_.size()));
        }

        for(const auto& [host, server_set]: hostname_to_server_names_) {
            for(const auto& server_name: server_set) {
                auto s_status = opc_client_->GetServerStatus(*host, server_name);
                if(!s_status.has_value() || s_status.value() != OPC_STATUS_RUNNING) {
                    emit sg_server_error(*host, server_name, s_status.has_value() ? static_cast<size_t>(s_status.value()) : static_cast<size_t>(OPC_STATUS_COMM_FAULT));
                    QString log_message = QString("ОРС-клиент: ошибка сервера %1@%2 : %3")
                                              .arg(*host, server_name)
                                              .arg(s_status.has_value() ? static_cast<int>(s_status.value()) : OPC_STATUS_COMM_FAULT);
                    emit sg_send_message_to_console(log_message);
                    qWarning() << log_message;
                }
            }
        }
    } catch (std::exception& e) {
        emit sg_opcclient_got_exception(QString::fromStdString(e.what()));
    }
}


void OPCDAWorker::sl_process()
{
    opc_client_.reset(new COPCClient());
    opc_client_->AddTags(tags_);

    if(period_reading_ > 0) {
        periodic_timer_->setInterval(period_reading_ * 1000);
        QObject::connect(periodic_timer_, &QTimer::timeout, this, &OPCDAWorker::sl_read_tags);
        periodic_timer_->start();
    }
    sl_read_tags();
    if(period_reading_ == 0) {
        emit sg_finished();
    }
}

void OPCDAWorker::sl_stop_reading()
{
    if(periodic_timer_) {
        periodic_timer_->stop();
    }
    emit sg_finished();
}

//=========================================================
//======= T A G B R O W S E R =============================
//=========================================================
void OPCDATagBrowser::sl_process()
{
    std::unique_ptr<COPCClient> opc_client_ = std::make_unique<COPCClient>();
    QObject::connect(opc_client_.get(), &COPCClient::sg_send_message_to_console, this, &OPCDATagBrowser::sg_send_message_to_console);
    QObject::connect(opc_client_.get(), &COPCClient::sg_get_part_tag_names_from_server,
                     this, &OPCDATagBrowser::sg_get_part_tag_names_from_server);
    QObject::connect(opc_client_.get(), &COPCClient::sg_get_all_tag_names_from_server,
                     this, &OPCDATagBrowser::sg_get_all_tag_names_from_server);

    try {
        tags_list_.clear();
        tags_list_.reserve(1000);
        auto res_vec = opc_client_->GetOPCTagsNames(hostname_, server_name_);
        tags_list_ = {res_vec.begin(), res_vec.end()};
    } catch (std::exception& e) {
        emit sg_opcclient_got_exception(QString::fromStdString(e.what()));
    }

    emit sg_browse_complete(tags_list_.size());
    emit sg_finished();
}

void OPCDATagBrowser::sl_stop_browsing()
{
    QThread::currentThread()->requestInterruption();
}
