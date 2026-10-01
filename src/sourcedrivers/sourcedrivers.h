#ifndef SOURCEDRIVERS_H
#define SOURCEDRIVERS_H

#include <QObject>
#include "datatag.h"
#include "copcuaclient.h"

class DriverInterface:  public QObject {
    Q_OBJECT
public:
    explicit DriverInterface(QObject *parent = nullptr): QObject(parent) {}
    virtual ~DriverInterface() {}
    virtual std::set<QString> GetEndpointNames(const QString& host) = 0;
    virtual std::optional<const std::vector<QString>> GetTagNames(const QString &hostname, const QString &server_name) = 0;
    virtual DataBrowseItem* GetVariablesNode(const QString& hostname, const QString& server_name) = 0;
    virtual size_t SetTagsList(std::vector<std::shared_ptr<DataTag>>& tags) = 0;
    virtual void RemoveTag(const std::shared_ptr<DataTag>& tag) {}
    virtual void WriteTagNow(const std::shared_ptr<DataTag>& tag) {}
    virtual void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags) = 0;
    virtual bool PeriodicReadingOn() const = 0;
    int GetPeriodReading() const {return tags_period_reading_;}
    void SetPeriodReading(int period) {tags_period_reading_ = period > 0 ? period : tags_period_reading_;}
    virtual void StartPeriodReading() = 0;
    virtual void StartPeriodReading(int period) = 0;
    virtual void StopPeriodReading() = 0;
    virtual bool HasTagsToRead() const = 0;

signals:
    void sg_stop_reading();
    void sg_send_message_to_console(QString);
    void sg_reading_request_complete(size_t);
    void sg_reading_periodic_complete(size_t);
    void sg_periodic_reading_changed(bool is_started);
    void sg_stop_browsing_tags();
    void sg_get_part_tag_names_from_server(const QString&,const QString&,size_t);
    void sg_get_all_tag_names_from_server(const QString&,const QString&,size_t);
    void sg_get_endpoints_names(QString host);

protected:
    int tags_period_reading_ = 2;
};


#ifdef _WIN32
class OPCDADriver : public DriverInterface
{
    Q_OBJECT
public:
    explicit OPCDADriver(QObject *parent = nullptr);
    ~OPCDADriver();
    std::set<QString> GetEndpointNames(const QString& host) override;
    std::optional<const std::vector<QString>> GetTagNames(const QString &hostname, const QString &server_name) override;
    [[nodiscard]] DataBrowseItem* GetVariablesNode(const QString& hostname, const QString& server_name) override;
    size_t SetTagsList(std::vector<std::shared_ptr<DataTag>>& tags) override;
    void WriteTagNow(const std::shared_ptr<DataTag>& tag) override;
    void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags) override;
    bool PeriodicReadingOn() const override;
    void StartPeriodReading() override;
    void StartPeriodReading(int period) override;
    void StopPeriodReading() override;
    bool HasTagsToRead() const override;

private slots:
    void sl_on_request_thread_finished();
    void sl_on_request_reading_tags_complete(size_t ntags);
    void sl_thread_send_opc_status(QString host, QString server, size_t server_state);
    void sl_periodic_thread_finished();
    void sl_thread_send_exception(QString text);
    void sl_get_all_tag_names_from_server(const QString& host,const QString& server,size_t n_tags);

private:
    const int TIME_WAITING_THREAD_ = 15000; //15sec
    const int MAX_PERIODIC_ERRORS_COUNT = 10;
    bool period_reading_on_ = false;
    bool request_stop_periodic_reading_ = false;
    int opc_threads_on_request_count_ = 0;
    int errors_periodic_opc_server_count_ = 0;
    int errors_server_status_periodic_count_ = 0;
    int opc_period_reading_ = 2;

    void start_browsing_tags_names_(const QString& hostname, const QString& server_name);

    std::vector<std::shared_ptr<DataTag>> tags_to_read_;
    std::unordered_map<QString, std::set<QString>> host_to_servers_;
    std::unordered_map<const QString*, std::vector<QString>> server_to_tag_names_;
    std::unordered_map<const QString*, bool> server_tags_is_browsing_;
    std::vector<QPointer<QThread>> thread_pointers_;
};
#endif // _WIN32

class COPCUAClient;

class OPCUADriver : public DriverInterface
{
    Q_OBJECT
public:
    explicit OPCUADriver(QObject *parent = nullptr): DriverInterface(parent) {}
    ~OPCUADriver();
    std::set<QString> GetEndpointNames(const QString& host) override;
    std::optional<const std::vector<QString>> GetTagNames(const QString &hostname, const QString &server_name) override;
    [[nodiscard]] DataBrowseItem* GetVariablesNode(const QString& hostname, const QString& server_name) override;
    size_t SetTagsList(std::vector<std::shared_ptr<DataTag>>& tags) override;
    void RemoveTag(const std::shared_ptr<DataTag>& tag) override;
    void WriteTagNow(const std::shared_ptr<DataTag>& tag) override;
    void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags) override;
    bool PeriodicReadingOn() const override;
    void StartPeriodReading() override;
    void StartPeriodReading(int period) override;
    void StopPeriodReading() override;
    bool HasTagsToRead() const override;

private slots:
    void sl_get_all_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);
    void sl_get_endpoints(QString host);
private:
    std::unordered_set<QString> hosts_;
    std::unordered_map<const QString*, std::unordered_set<QString>> host_to_endpoints_;
    std::unordered_map<const QString*, const QString*> endpoint_to_host_;
    std::unordered_map<const QString*, std::unique_ptr<COPCUAClient>> host_to_ua_client_;
    std::unordered_map<const QString*, std::unique_ptr<DataBrowseItem>> endpoint_to_data_browse_item_;

    std::unordered_map<QString, int> host_retry_count_;
    const int MAX_ENDPOINT_RETRIES = 3;

    bool periodic_reading_on_ = false;
    bool has_tags_to_read_ = false;

    QString endpoint_to_text_(const QOpcUaEndpointDescription& ep_description);

    COPCUAClient* take_or_init_client_(const QString& host, const QString& endpoint);
};
#endif // SOURCEDRIVERS_H
