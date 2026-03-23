#ifndef COPCCLIENT_H
#define COPCCLIENT_H

#include <QObject>
#include <QString>

#include <set>
#include <unordered_map>
#include <vector>
#include <optional>

#include "opcda.h"

class DataTagOpcDA;
class DataTag;


namespace OPC_HELPER {

QString GetErrorStringFromHRESULT(HRESULT hr);
QString GetServerStatus(const OPCSERVERSTATUS& status_struct);

struct OPCServerData {
    QString ProgID;
    QString UserType;
    QString Hostname;
};

class OPCGroupHandler {
public:
    OPCGroupHandler() = default;
    IOPCItemMgt *pItemMgt = NULL;
    IOPCSyncIO * pSyncIO = NULL;
    OPCHANDLE opc_handle_group = 0;
    DWORD update_rate = 1000;
    DWORD dwCount() const;
    [[nodiscard]] OPCHANDLE* GetTagHandlesArrayToRead() const;
    [[nodiscard]] std::tuple<OPCHANDLE*, VARIANT*, DWORD> GetTagHandlesArrayToWrite() const;
    bool CheckTagExist(const std::shared_ptr<DataTagOpcDA>& tag) const;
    bool AddTagWithHandle(std::shared_ptr<DataTagOpcDA>& tag, OPCHANDLE hnd);
    void ClearTags();
    std::shared_ptr<DataTagOpcDA> GetTagPtr(OPCHANDLE hnd) const;
    OPCHANDLE GetTagHandle(std::shared_ptr<DataTagOpcDA>& tag) const;
    size_t GetTagsCount() const;
private:
    std::unordered_map<std::shared_ptr<DataTagOpcDA>, OPCHANDLE> tag_to_opchandle_;
    std::unordered_map<OPCHANDLE, std::shared_ptr<DataTagOpcDA>> opchandle_to_tag_;

};

class COPCClient : public QObject {
    Q_OBJECT
public:
    COPCClient();
    ~COPCClient();

    const std::set<QString>& GetOPCServerNames(const QString& hostname);
    const std::vector<QString>& GetOPCTagsNames(const QString& hostname, const QString& server_name, int notify_of_portion = 50);

    void RefreshOPCServersList();
    size_t AddTags(std::vector<std::shared_ptr<DataTagOpcDA>>& tags);
    size_t ReadTags();
    size_t WriteTags();
    void ClearTags();
    std::optional<OPCSERVERSTATUS> GetServerStatus(const QString& hostname, const QString& server_name);

signals:
    void sg_send_message_to_console(QString mes);
    void sg_get_part_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);
    void sg_get_all_tag_names_from_server(const QString& host, const QString& server, size_t tag_cnt);

private:
    std::set<QString> hostnames_;
    std::unordered_map<const QString*, std::set<QString>> host_to_opc_names_;
    std::unordered_map<const QString*, const GUID*> opc_server_to_guid_;
    std::list<GUID> guid_servers_;
    std::unordered_map<const GUID*, OPCServerData> opc_server_guid_to_data_;
    std::unordered_map<const GUID*, IOPCServer*> opc_server_guid_to_IOPCSErver_;
    std::unordered_map<const GUID*, OPCGroupHandler> opc_server_guid_to_group_;
    std::unordered_map<const GUID*, std::vector<QString>> opc_server_guid_to_tag_names_;

    const std::vector<QString> empty_vec_ = {};

    int get_registered_servers_(const QString& hostname);
    bool browse_server_address_space_(const GUID* guid_ptr, int notify_of_portion);
    void export_server_address_space_(IOPCBrowseServerAddressSpace* pParent, const GUID* guid_ptr, int notify_of_portion);
    void clear_internal_data_();
    bool connect_server_(const GUID* guid_ptr);
    void disconnect_server_(const GUID* guid_ptr);
    bool register_group_(const GUID* guid_ptr);
    void remove_group_(const GUID* guid_ptr);
    size_t add_tags_to_group_(const GUID* guid_ptr, std::vector<std::shared_ptr<DataTagOpcDA>>& tags);
    size_t read_server_tags_(const GUID* guid_ptr);
    size_t write_server_tags_(const GUID* guid_ptr);
};

}

#endif // COPCCLIENT_H
