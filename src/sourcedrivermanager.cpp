#include "sourcedrivermanager.h"

#include "sourcedrivers.h"
#include "datatagregistry.h"
#include "datatag.h"

SourceDriverManager::SourceDriverManager(DataTagRegistry *registry_ptr, QObject *parent)
    : QObject(parent)
    , tag_registry_(registry_ptr)
{
    if(!registry_ptr) throw std::invalid_argument("DataTagRegistry pointer is null");
}

void SourceDriverManager::SetPeriodReading(int period)
{
    period_reading_ = period > 0 ? period : period_reading_;
}

int SourceDriverManager::GetPeriodReading() const
{
    return period_reading_;
}

void SourceDriverManager::StartPeriodReading(int period)
{
    SetPeriodReading(period);
    StartPeriodReading();
}

void SourceDriverManager::StopPeriodReading()
{
    if(opcda_driver_) {
        opcda_driver_->StopPeriodReading();
    }
    if(opcua_driver_) {
        opcua_driver_->StopPeriodReading();
    }
}

void SourceDriverManager::ReadTagsOnce(std::vector<std::shared_ptr<DataTag> > &tags)
{
    OpcDaDriver()->ReadTagsOnce(tags);
    OpcUaDriver()->ReadTagsOnce(tags);
}

bool SourceDriverManager::InWork() const
{
    bool res = false;
    if(opcda_driver_) {
        res = !opcda_driver_->HasTagsToRead() || (opcda_driver_->HasTagsToRead() && opcda_driver_->PeriodicReadingOn());
    }
    if(opcua_driver_) {
        res = res && ((!opcua_driver_->HasTagsToRead()) || (opcua_driver_->HasTagsToRead() && opcua_driver_->PeriodicReadingOn()));
    }
    return res;
}

OPCDADriver *SourceDriverManager::OpcDaDriver()
{
    if(!opcda_driver_) {
        opcda_driver_.reset(new OPCDADriver());
        QObject::connect(opcda_driver_.get(), &OPCDADriver::sg_periodic_reading_changed, this, &SourceDriverManager::sl_opc_da_status_changed);
        QObject::connect(opcda_driver_.get(), &OPCDADriver::sg_reading_periodic_complete, this, &SourceDriverManager::sg_periodic_finished);
        QObject::connect(opcda_driver_.get(), &OPCDADriver::sg_reading_request_complete, this, &SourceDriverManager::sg_reading_request_complete);
        QObject::connect(opcda_driver_.get(), &OPCDADriver::sg_send_message_to_console, this, &SourceDriverManager::sg_send_message_to_console);
    }
    return opcda_driver_.get();
}

OPCUADriver *SourceDriverManager::OpcUaDriver()
{
    if(!opcua_driver_) {
        opcua_driver_.reset(new OPCUADriver());
        QObject::connect(opcua_driver_.get(), &OPCUADriver::sg_reading_periodic_complete, this, &SourceDriverManager::sg_periodic_finished);
        QObject::connect(opcua_driver_.get(), &OPCUADriver::sg_reading_request_complete, this, &SourceDriverManager::sg_reading_request_complete);
        QObject::connect(opcua_driver_.get(), &OPCUADriver::sg_send_message_to_console, this, &SourceDriverManager::sg_send_message_to_console);
    }
    return opcua_driver_.get();
}

DataTagRegistry *SourceDriverManager::TagRegistry() const
{
    return tag_registry_;
}

void SourceDriverManager::sl_opc_da_status_changed(bool connected)
{
    emit sg_data_driver_status_changed(DataTag::DataSource::OPCDA, connected);
    if(InWork()) {
        emit sg_periodic_started();
    } else {
        emit sg_periodic_finished();
    }
}

void SourceDriverManager::sl_opc_ua_status_changed(bool connected)
{
    emit sg_data_driver_status_changed(DataTag::DataSource::OPCUA, connected);
    if(InWork()) {
        emit sg_periodic_started();
    } else {
        emit sg_periodic_finished();
    }
}

void SourceDriverManager::StartPeriodReading()
{
    auto tags_da = tag_registry_->GetTagsOfType(DataTag::DataSource::OPCDA);
    if(tags_da.size() > 0) {
        OpcDaDriver()->SetTagsList(std::move(tags_da));
        opcda_driver_->SetPeriodReading(period_reading_);
        opcda_driver_->StartPeriodReading();
    }
    auto tags_ua = tag_registry_->GetTagsOfType(DataTag::DataSource::OPCUA);
    if(tags_ua.size() > 0) {
        OpcUaDriver()->SetTagsList(std::move(tags_da));
        opcua_driver_->StartPeriodReading(period_reading_);
    }
}
