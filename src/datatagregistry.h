#ifndef DATATAGREGISTRY_H
#define DATATAGREGISTRY_H

#include <QRegularExpression>

#include "datatag.h"

struct DataSourceInfo {
    QString EndpointName;
    std::unordered_set<QStringView> Tags;
};

class QRegularExpression;

class DataTagRegistry: public QObject
{
    Q_OBJECT
public:
    DataTagRegistry(const QString& filename);
    ~DataTagRegistry();
    size_t AddDataTag(std::shared_ptr<DataTag> tag);
    size_t AddDataTag(DataTag::DataSource src, const QString& fullname);
    void DeleteTag(size_t id);
    bool SaveDataToFile();
    bool SaveDataToFile(const QString& folderpath);
    bool RestoreDataFromFile();
    size_t CheckTagExist(const QString& fullname);
    const std::unordered_set<QString>& GetHostNames() const;
    std::vector<std::shared_ptr<DataTag>> GetTagsOfType(DataTag::DataSource type) const;
    std::vector<std::shared_ptr<DataTag>> GetAllTags() const;
    const std::unordered_map<size_t, std::shared_ptr<DataTag>>& GetIdToTagsMap() const;
    std::shared_ptr<DataTag> GetTagOfId(size_t id) const;

signals:
    void sg_periodic_list_changed();

private:
    QString filename_ = QString("opstags.json");
    QRegularExpression fullname_regexp_{QString("^\\[([^\\[].*[^\\]])\\]\\[([^\\[].*[^\\]])\\]\\[([^\\[].*[^\\]])\\]$")}; //[host][server][tag]
    std::set<size_t> all_owned_ids_;
    std::unordered_map<size_t, std::shared_ptr<DataTag>> id_tag_to_opc_da_tag_pointer_;

    std::unordered_set<QString> hostnames_;
    std::unordered_map<const QString*, std::unordered_set<QString>> hostname_to_servers_;
    std::unordered_map<const QString*, std::unordered_set<QString>> servers_to_tags_;
    std::unordered_map<const QString*, size_t> tag_name_to_id_;

    std::optional<size_t> check_tag_(std::shared_ptr<DataTag> tag);
    void clear_data_();
    bool restore_data_tag_(size_t id, std::shared_ptr<DataTag> tag);
};

#endif // DATATAGREGISTRY_H
