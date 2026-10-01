#include "sourcedrivermanager.h"

#include "sourcedrivers.h"
#include "datatagregistry.h"
#include "datatag.h"

using namespace Qt::StringLiterals;

SourceDriverManager::SourceDriverManager(DataTagRegistry *registry_ptr, QObject *parent)
    : QObject(parent)
    , tag_registry_(registry_ptr)
{
    if(!registry_ptr) throw std::invalid_argument("DataTagRegistry pointer is null");
}

const std::vector<DataTag::DataSource>& SourceDriverManager::known_sources_()
{
    static const std::vector<DataTag::DataSource> sources = {
#ifdef _WIN32
        DataTag::DataSource::OPCDA,
#endif
        DataTag::DataSource::OPCUA,
    };
    return sources;
}

std::unique_ptr<DriverInterface> SourceDriverManager::make_driver_(DataTag::DataSource source)
{
    switch(source) {
        using enum DataTag::DataSource;
#ifdef _WIN32
    case OPCDA: return std::make_unique<OPCDADriver>();
#endif
    case OPCUA: return std::make_unique<OPCUADriver>();
    default: return nullptr;
    }
}

DriverInterface *SourceDriverManager::driver_(DataTag::DataSource source)
{
    auto it = drivers_.find(source);
    if(it != drivers_.end()) return it->second.get();

    auto new_driver = make_driver_(source);
    if(!new_driver) return nullptr;

    DriverInterface* raw_ptr = new_driver.get();
    QObject::connect(raw_ptr, &DriverInterface::sg_periodic_reading_changed, this, [this, source](bool connected) {
        emit sg_data_driver_status_changed(source, connected);
        if(InWork()) {
            emit sg_periodic_started();
        } else {
            emit sg_periodic_finished();
        }
    });
    QObject::connect(raw_ptr, &DriverInterface::sg_reading_periodic_complete, this, &SourceDriverManager::sg_reading_periodic_complete);
    QObject::connect(raw_ptr, &DriverInterface::sg_reading_request_complete, this, &SourceDriverManager::sg_reading_request_complete);
    QObject::connect(raw_ptr, &DriverInterface::sg_send_message_to_console, this, &SourceDriverManager::sg_send_message_to_console);
    QObject::connect(raw_ptr, &DriverInterface::sg_get_part_tag_names_from_server, this, &SourceDriverManager::sg_get_part_tag_names_from_server);
    QObject::connect(raw_ptr, &DriverInterface::sg_get_all_tag_names_from_server, this, &SourceDriverManager::sg_get_all_tag_names_from_server);

    drivers_[source] = std::move(new_driver);
    return raw_ptr;
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
    for(auto& [source, driver_ptr]: drivers_) {
        driver_ptr->StopPeriodReading();
    }
}

void SourceDriverManager::ReadTagsOnce(std::vector<std::shared_ptr<DataTag> > &tags)
{
    for(auto& [source, driver_ptr]: drivers_) {
        driver_ptr->ReadTagsOnce(tags);
    }
}

void SourceDriverManager::WriteTagNow(const std::shared_ptr<DataTag>& tag)
{
    if(!tag) return;
    DriverInterface* driver = GetDriverPtr(tag->GetDataSource());
    if(driver) {
        driver->WriteTagNow(tag);
    }
}

void SourceDriverManager::DeleteTag(size_t id)
{
    auto tag_ptr = tag_registry_->GetTagOfId(id);
    if(tag_ptr) {
        DriverInterface* driver = GetDriverPtr(tag_ptr->GetDataSource());
        if(driver) {
            driver->RemoveTag(tag_ptr);
        }
    }
    tag_registry_->DeleteTag(id);
}

bool SourceDriverManager::InWork() const
{
    for(const auto& [source, driver_ptr]: drivers_) {
        if(driver_ptr->HasTagsToRead() && driver_ptr->PeriodicReadingOn()) return true;
    }
    return false;
}

DriverInterface *SourceDriverManager::GetDriverPtr(DataTag::DataSource source)
{
    DriverInterface* d = driver_(source);
    if(!d) {
        qCritical() << u"SourceDriverManager: Запрос несуществующего драйвера."_s;
        throw std::logic_error("wrong driver source");
    }
    return d;
}

DataTagRegistry *SourceDriverManager::TagRegistry() const
{
    return tag_registry_;
}

void SourceDriverManager::StartPeriodReading()
{
    for(const auto source: known_sources_()) {
        auto tags = tag_registry_->GetTagsOfType(source);
        if(tags.empty()) continue;

        auto* d = driver_(source);
        if(!d) continue;

        d->SetTagsList(tags);
        d->StartPeriodReading(period_reading_);
    }
}
