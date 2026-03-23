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
    bool InWork() const;
    OPCDADriver* OpcDaDriver();
    OPCUADriver* OpcUaDriver();
    DataTagRegistry* TagRegistry() const;

signals:
    void sg_data_driver_status_changed(DataTag::DataSource, bool connected);
    void sg_periodic_started();
    void sg_periodic_finished();
    void sg_reading_periodic_complete();
    void sg_reading_request_complete();
    void sg_send_message_to_console(QString);

private slots:
    void sl_opc_da_status_changed(bool connected);
    void sl_opc_ua_status_changed(bool connected);

private:
    DataTagRegistry* tag_registry_;
    std::unique_ptr<OPCDADriver> opcda_driver_;
    std::unique_ptr<OPCUADriver> opcua_driver_;

    int period_reading_ = 2;
};

#endif // SOURCEDRIVERMANAGER_H
