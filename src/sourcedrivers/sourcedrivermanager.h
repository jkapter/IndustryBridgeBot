#ifndef SOURCEDRIVERMANAGER_H
#define SOURCEDRIVERMANAGER_H

#include <QObject>

#include "sourcedrivers.h"

class DataTagRegistry;
class OPCDAWorker;

class SourceDriverManager : public QObject
{
    Q_OBJECT
public:
    explicit SourceDriverManager(DataTagRegistry* registry_ptr, QObject *parent = nullptr);
    void SetPeriodReading(int period);
    int GetPeriodReading() const;
    void StartPeriodReading();
    void StartPeriodReading(int period);
    void StopPeriodReading();
    void ReadTagsOnce(std::vector<std::shared_ptr<DataTag>>& tags);
    void WriteTagNow(const std::shared_ptr<DataTag>& tag);
    void DeleteTag(size_t id);
    bool InWork() const;

    DriverInterface* GetDriverPtr(DataTag::DataSource source);
    DataTagRegistry* TagRegistry() const;

signals:
    void sg_data_driver_status_changed(DataTag::DataSource, bool connected);
    void sg_periodic_started();
    void sg_periodic_finished();
    void sg_reading_periodic_complete();
    void sg_reading_request_complete();
    void sg_send_message_to_console(QString);
    void sg_get_part_tag_names_from_server(const QString&,const QString&,size_t);
    void sg_get_all_tag_names_from_server(const QString&,const QString&,size_t);

private:
    DataTagRegistry* tag_registry_;
    std::unordered_map<DataTag::DataSource, std::unique_ptr<DriverInterface>> drivers_;

    int period_reading_ = 2;

    static const std::vector<DataTag::DataSource>& known_sources_();
    std::unique_ptr<DriverInterface> make_driver_(DataTag::DataSource source);
    DriverInterface* driver_(DataTag::DataSource source);
};

#endif // SOURCEDRIVERMANAGER_H
