#ifndef OPCCLIENTWORKER_H
#define OPCCLIENTWORKER_H

#include <QObject>
#include <QFuture>
#include <set>

#include "opcda.h"

//#include "opctag.h"
#include "copcclient.h"
#include "datatag.h"

class QMutex;
class QTimer;

namespace OPC_HELPER {

class OPCDATagBrowser: public QObject
{
    Q_OBJECT
public:
    OPCDATagBrowser(const QString& hostname, const QString& server_name, std::vector<QString>& tags_ret_vec, QObject *parent = nullptr)
        : QObject(parent)
        , hostname_(hostname)
        , server_name_(server_name)
        , tags_list_(tags_ret_vec)
    {}

signals:
    void sg_browse_complete(size_t n_tags);
    void sg_get_part_tag_names_from_server(const QString& host, const QString& server, size_t n_tags);
    void sg_get_all_tag_names_from_server(const QString& host, const QString& server, size_t n_tags);
    void sg_finished();
    void sg_send_message_to_console(QString mes);
    void sg_opcclient_got_exception(QString what);

public slots:
    void sl_process();
    void sl_stop_browsing();

private:
    QString hostname_;
    QString server_name_;
    std::vector<QString>& tags_list_;
    bool request_interrupt_ = false;
};

class OPCDAWorker : public QObject
{
    Q_OBJECT
public:
    explicit OPCDAWorker(std::vector<std::shared_ptr<DataTag>>& tags, QObject *parent = nullptr);
    OPCDAWorker(QObject *parent = nullptr);    
    virtual ~OPCDAWorker();
    void SetTagsList(const std::vector<std::shared_ptr<DataTag>>& tags);
    void SetTagsList(const std::vector<std::shared_ptr<DataTag>>&& tags);
    bool SetPeriodicReading(int period);

signals:
    void sg_reading_complete(size_t n_tags);
    void sg_reading_error(size_t n_tags);
    void sg_send_message_to_console(QString mes);
    void sg_server_error(QString host, QString server, size_t server_state);
    void sg_opcclient_got_exception(QString what);
    void sg_writing_tags(size_t n_tags);
    void sg_finished();

public slots:
    void sl_process();
    void sl_stop_reading();

private slots:
    void sl_read_tags();

private:
    std::vector<std::shared_ptr<DataTagOpcDA>> tags_;
    std::set<QString> hostnames_;
    int period_reading_ = 0;
    std::unordered_map<const QString*, std::set<QString>> hostname_to_server_names_;
    std::unique_ptr<OPC_HELPER::COPCClient> opc_client_ = nullptr;
    QTimer* periodic_timer_ = nullptr;

    void process_tags_();
};

} //namespace

#endif // OPCCLIENTWORKER_H
