#ifndef COPCUACLIENT_H
#define COPCUACLIENT_H

#include <QObject>
#include <QOpcUaClient>

#include "datatag.h"

class COPCUAClient : public QObject
{
    Q_OBJECT
public:
    explicit COPCUAClient();
    ~COPCUAClient();
    const std::set<QString>& GetEndpoints(const QString& hostname);
    const std::vector<QString>& GetOPCTagsNames(const QString& hostname, const QString& server_name, int notify_of_portion = 50);

    void RefreshEndpointsList();
    size_t AddTags(std::vector<std::shared_ptr<DataTagOpcUA>>& tags);
    size_t ReadTags();
    size_t WriteTags();
    void ClearTags();
    std::optional<OPCSERVERSTATUS> GetServerStatus(const QString& hostname, const QString& server_name);

signals:
    void sg_send_message_to_console(QString mes);
    void sg_get_part_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);
    void sg_get_all_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);

private slots:
    void sl_endpoint_request_finished(QList<QOpcUaEndpointDescription> endpoints);
    void sl_connection_state_changed(QOpcUaClient::ClientState state);

private:
    std::unique_ptr<QOpcUaClient> ua_client_ = nullptr;
    QString hostname_;
    QList<QOpcUaEndpointDescription> endpoints_;
    std::unordered_map<QString*, qsizetype> ep_text_to_index_;
    std::set<QString> endpoints_texts_;
    std::vector<std::shared_ptr<DataTagOpcUA>> opc_tags_;

    bool endpoints_requested_;

    QString endpoint_to_text_(const QOpcUaEndpointDescription &ep_description);
    void clear_internal_data_();
    bool init_client_();
};

#endif // COPCUACLIENT_H
