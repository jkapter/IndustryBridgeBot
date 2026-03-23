#ifndef SOURCEDRIVERS_H
#define SOURCEDRIVERS_H

#include <QObject>

#include "datatag.h"

class OPCDADriver : public QObject
{
    Q_OBJECT
public:
    explicit OPCDADriver(QObject *parent = nullptr);
    ~OPCDADriver();
    std::set<QString> GetServerNames(const QString& host);
    void StartBrowsingTagsNames(const QString& hostname, const QString& server_name);
    std::optional<const std::vector<QString>> GetTagNames(const QString &hostname, const QString &server_name);
    size_t SetTagsList(std::vector<std::shared_ptr<DataTag>>&& tags);
    void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags);
    bool PeriodicReadingOn() const;
    int GetPeriodReading() const;
    void SetPeriodReading(int period);
    void StartPeriodReading();
    void StartPeriodReading(int period);
    void StopPeriodReading();
    bool HasTagsToRead() const;

signals:
    void sg_stop_reading();
    void sg_send_message_to_console(QString);
    void sg_reading_request_complete(size_t);
    void sg_reading_periodic_complete(size_t);
    void sg_periodic_reading_changed(bool is_started);
    void sg_stop_browsing_tags();
    void sg_get_part_tag_names_from_server(const QString&,const QString&,size_t);
    void sg_get_all_tag_names_from_server(const QString&,const QString&,size_t);

private slots:
    void sl_on_request_thread_finished();
    void sl_on_request_reading_tags_complete(size_t ntags);
    void sl_thread_send_opc_status(QString host, QString server, size_t server_state);
    void sl_periodic_thread_finished();
    void sl_thread_send_exception(QString text);
    void sl_get_all_tag_names_from_server(const QString& host,const QString& server,size_t n_tags);

private:
    const int TIME_WAITING_THREAD_ = 30000; //30sec
    const int MAX_PERIODIC_ERRORS_COUNT = 10;
    bool period_reading_on_ = false;
    bool request_stop_periodic_reading_ = false;
    int opc_threads_on_request_count_ = 0;
    int errors_periodic_opc_server_count_ = 0;
    int errors_server_status_periodic_count_ = 0;
    int opc_period_reading_ = 2;

    std::vector<std::shared_ptr<DataTag>> tags_to_read_;
    std::unordered_map<QString, std::set<QString>> host_to_servers_;
    std::unordered_map<const QString*, std::vector<QString>> server_to_tag_names_;
    std::unordered_map<const QString*, bool> server_tags_is_browsing_;
};

class OPCUADriver : public QObject
{
    Q_OBJECT
public:
    OPCUADriver() = default;
    bool PeriodicReadingOn() const {return false;}
    bool HasTagsToRead() const {return false;}
    void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags) {;}
    void StartPeriodReading() {;}
    void StartPeriodReading(int period) {;}
    void StopPeriodReading(){;}
    size_t SetTagsList(std::vector<std::shared_ptr<DataTag>>&& tags){return 0;}

signals:
    void sg_reading_request_complete(size_t);
    void sg_reading_periodic_complete(size_t);
    void sg_send_message_to_console(QString);

};
#endif // SOURCEDRIVERS_H
