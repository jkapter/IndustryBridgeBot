#ifndef COPCUACLIENT_H
#define COPCUACLIENT_H

#include <QObject>
#include <QOpcUaClient>

#include <unordered_set>

#include "datatag_opcua.h"

class QTimer;

class COPCUAClient : public QObject
{
    Q_OBJECT
public:
    COPCUAClient() = delete;
    explicit COPCUAClient(const QString& hostname);
    ~COPCUAClient();
    std::optional<std::set<QString>> GetEndpoints(const QString& hostname);

    void RefreshEndpointsList();
    void AddTags(std::vector<std::shared_ptr<DataTagOpcUA>>& tags);
    void RemoveTag(const std::shared_ptr<DataTagOpcUA>& tag);
    void ReadTags();
    size_t SubscribeToChangeValue(bool set, double period = 1.0);
    size_t WriteTags();
    void ClearTags();
    std::unique_ptr<DataBrowseItem> GetVariablesNode(const QString& hostname, const QString& server_name, int notify_of_portion = 50);

signals:
    void sg_send_message_to_console(QString mes);
    void sg_get_part_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);
    void sg_get_all_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);
    void sg_all_tags_readed(size_t tags_n);
    void sg_got_endpoints(const QString& hostname);
    void sg_tags_added(size_t cnt);

private slots:
    void sl_endpoint_request_finished_(QList<QOpcUaEndpointDescription> endpoints);
    void sl_connection_state_changed_(QOpcUaClient::ClientState state);
    void sl_ua_node_attribute_readed_(QOpcUa::NodeAttributes attributes);
    void sl_ua_node_value_updated_(QOpcUa::NodeAttribute attribute, QVariant value);
    void sl_ua_node_monitoring_enabled_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode);
    void sl_ua_node_monitoring_disabled_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode);
    void sl_write_flush_timeout_();

private:
    const int CONNECTION_TIMEOUT = 5000;

    std::unique_ptr<QOpcUaClient> ua_client_ = nullptr;
    QString hostname_;
    QString endpoint_connected_;
    QList<QOpcUaEndpointDescription> endpoints_;
    std::unordered_map<const QString*, qsizetype> ep_text_to_index_;
    std::set<QString> endpoints_texts_;
    double period_reading_ = 1.;
    bool value_monitoring_enabled_ = false;

    std::vector<std::shared_ptr<DataTagOpcUA>> opc_tags_;
    std::vector<std::shared_ptr<DataTagOpcUA>> opc_tags_temp_;
    std::unordered_map<DataTagOpcUA*, std::unique_ptr<QOpcUaNode>> tag_ptr_to_opc_node_ptr_;
    std::unordered_map<QOpcUaNode*, DataTagOpcUA*> opc_node_ptr_to_tag_ptr_;
    std::unordered_set<QOpcUaNode*> monitored_nodes_;
    std::unordered_set<QOpcUaNode*> monitoring_pending_;
    std::unordered_map<DataBrowseItem*, std::unique_ptr<QOpcUaNode>> browse_item_to_ua_node_;
    std::unique_ptr<DataBrowseItem> root_data_browse_item_;
    QTimer* write_flush_timer_ = nullptr; // периодически сбрасывает отложенные записи, пока идёт мониторинг

    bool client_add_tags_requested_ = false;
    bool endpoints_list_requested_  = false;
    bool read_tags_requested_ = false;
    bool tags_tree_is_browsing_ = false;
    bool client_disconnect_requested_ = false;
    bool browse_requested_ = false;
    bool connect_in_progress_ = false;

    int tags_to_read_;
    int browsed_tags_to_notify_;
    int notify_of_portion_;

    QString endpoint_to_text_(const QOpcUaEndpointDescription &ep_description);
    void request_connect_(const QOpcUaEndpointDescription &ep_description);
    void clear_internal_data_();
    bool init_client_();
    void add_tags_();
    
    void browse_childs_making_tree_(DataBrowseItem* parent, int notify_of_tags_browsed);
    bool check_all_childs_browsed_(DataBrowseItem* parent);
};



#endif // COPCUACLIENT_H
