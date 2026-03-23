#include "copcclient.h"
#include <QDebug>
#include <QEventLoop>
#include <QThread>
#include <QAbstractEventDispatcher>

#include "opccomn.h"
#include "opcerror.h"

#include "datatag.h"

//IOPCServerList uuid(13486D50-4821-11D2-A494-3CB306C10000)
__CRT_UUID_DECL(IOPCServerList, 0x13486D50, 0x4821, 0x11d2, 0xa4, 0x94, 0x3c, 0xb3, 0x06, 0xc1, 0x00, 0x00)

// IOPCServer uuid(39c13a4d-011e-11d0-9675-0020afd8adb3)
__CRT_UUID_DECL(IOPCServer, 0x39c13a4d, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3)

// IOPCBrowseServerAddressSpace uuid(39c13a4f-011e-11d0-9675-0020afd8adb3)
__CRT_UUID_DECL(IOPCBrowseServerAddressSpace, 0x39c13a4f, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3)

// IOPCItemMgt uuid(39c13a54-011e-11d0-9675-0020afd8adb3)
__CRT_UUID_DECL(IOPCItemMgt, 0x39c13a54, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3)

// IOPCSyncIO uuid(39c13a52-011e-11d0-9675-0020afd8adb3)
__CRT_UUID_DECL(IOPCSyncIO, 0x39c13a52, 0x011e, 0x11d0, 0x96, 0x75, 0x00, 0x20, 0xaf, 0xd8, 0xad, 0xb3)


using namespace OPC_HELPER;
using namespace Qt::StringLiterals;

COPCClient::COPCClient(): QObject()
{
    qInfo() << QString("Новый экземпляр ОРС клиента, поток [%1]").arg(QThread::currentThread()->objectName());
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);//COINIT_APARTMENTTHREADED);
}

COPCClient::~COPCClient() {
    clear_internal_data_();
    CoUninitialize();
    emit sg_send_message_to_console(QString("ОРС-клиент поток [%1]: завершен.").arg(QThread::currentThread()->objectName()));
    qInfo() << QString("Завершен ОРС клиент, поток %1").arg(QThread::currentThread()->objectName());
}

void COPCClient::clear_internal_data_() {
    for(auto& guid_ptr: guid_servers_) {
        disconnect_server_(&guid_ptr);
    }

    host_to_opc_names_.clear();
    opc_server_to_guid_.clear();
    guid_servers_.clear();
    opc_server_guid_to_data_.clear();
    opc_server_guid_to_tag_names_.clear();
    opc_server_guid_to_IOPCSErver_.clear();
    opc_server_guid_to_group_.clear();
}

void COPCClient::RefreshOPCServersList() {
    clear_internal_data_();
    for(const auto& host: hostnames_) {
        get_registered_servers_(host);
    }
}

int COPCClient::get_registered_servers_(const QString& hostname)
{
    auto host_it = hostnames_.insert(hostname).first;
    if(host_to_opc_names_.contains(&(*host_it))) {
        host_to_opc_names_.at(&(*host_it)).clear();
    } else {
        host_to_opc_names_[&(*host_it)] = {};
    }

    CLSID clsid;
    CLSID clsidcat;
    HRESULT hRes;

    CLSIDFromString(L"{63D5F432-CFE4-11D1-B2C8-0060083BA1FB}", &clsidcat); //OPC DA 2.0 const class ID
    CLSIDFromProgID(L"OPC.ServerList", &clsid);

    IID IID_IOPCServerList = __uuidof(IOPCServerList);
    IOPCServerList2* pServerList = NULL;

    if(hostname == u"localhost"_s) {
        hRes = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER, IID_IOPCServerList, (void**)&pServerList);
    } else {
        COSERVERINFO ServerInfo = {0};

        std::wstring wstr_hostname = hostname.toStdWString();
        ServerInfo.pwszName = wstr_hostname.data();
        ServerInfo.pAuthInfo = NULL;

        MULTI_QI MultiQI [2] = {0};
        MultiQI [0].pIID = &IID_IOPCServerList;
        MultiQI [0].pItf = NULL;
        MultiQI [0].hr = S_OK;

        hRes = CoCreateInstanceEx(clsid, NULL, CLSCTX_REMOTE_SERVER, &ServerInfo, 1, MultiQI);
        pServerList = static_cast<IOPCServerList2*>(MultiQI[0].pItf);
    }

    if(hRes != S_OK) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка запроса списка серверов. hRes = %2 : %3")
                                  .arg(QThread::currentThread()->objectName())
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(pServerList) {
            pServerList->Release();
        }
        return -1;
    }

    if(!pServerList) {
        QString log_message = QString("ОРС-клиент поток [%1]: список серверов пуст.").arg(QThread::currentThread()->objectName());
        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;
        return -1;
    }

    IOPCEnumGUID* pIEnumGuid = NULL;
    hRes = pServerList->EnumClassesOfCategories(1, &clsidcat, 0, NULL, &pIEnumGuid);

    if(hRes != S_OK) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка запроса списка классов. hRes = %2 : %3")
                                  .arg(QThread::currentThread()->objectName())
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;
    }

    if(!pIEnumGuid || hRes != S_OK) {
        QString log_message = QString("ОРС-клиент поток [%1]: список GUID серверов пуст.").arg(QThread::currentThread()->objectName());
        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(pServerList) {
            pServerList->Release();
        }
        if(pIEnumGuid) {
            pIEnumGuid->Release();
        }
        return -1;
    }

    GUID guid;
    int nServerCnt = 0;
    unsigned long iRetSvr;
    pIEnumGuid->Next(1, &guid, &iRetSvr);

    while (iRetSvr != 0)
    {
        wchar_t* pszProgID = NULL;
        wchar_t* pszUserType = NULL;
        wchar_t* pszVerIndProgID = NULL;
        nServerCnt++;
        pServerList->GetClassDetails(guid, &pszProgID, &pszUserType,  &pszVerIndProgID);

        auto host_it = hostnames_.find(hostname);
        auto [server_it, b]  = host_to_opc_names_.at(&(*host_it)).insert(QString::fromWCharArray(pszProgID));
        if(!b) continue;

        guid_servers_.push_back(guid);
        opc_server_to_guid_[&(*server_it)] = &guid_servers_.back();
        opc_server_guid_to_data_[&guid_servers_.back()] = {QString::fromWCharArray(pszProgID), QString::fromWCharArray(pszUserType), hostname};
        opc_server_guid_to_IOPCSErver_[&guid_servers_.back()] = NULL;
        opc_server_guid_to_group_[&guid_servers_.back()] = {};
        opc_server_guid_to_tag_names_[&guid_servers_.back()] = {};
        pIEnumGuid->Next(1, &guid, &iRetSvr);
        CoTaskMemFree(pszProgID);
        CoTaskMemFree(pszUserType);
        CoTaskMemFree(pszVerIndProgID);
    }

    pServerList->Release();
    pIEnumGuid->Release();
    return nServerCnt;
}

bool COPCClient::browse_server_address_space_(const GUID* guid_ptr, int notify_of_portion) {
    if(!guid_ptr || !opc_server_guid_to_IOPCSErver_.at(guid_ptr)) return false;

    //Подключение установлено
    IID IID_IOPCBrowseServerAddressSpace = __uuidof(IOPCBrowseServerAddressSpace);
    IOPCBrowseServerAddressSpace* pBrowse = NULL;

    HRESULT hRes = opc_server_guid_to_IOPCSErver_.at(guid_ptr)->QueryInterface(IID_IOPCBrowseServerAddressSpace, (void**)&pBrowse);

    if (hRes != S_OK) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка подключения к серверу %2@%3. hRes = %4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).ProgID, opc_server_guid_to_data_.at(guid_ptr).Hostname)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(pBrowse != NULL) {
            pBrowse->Release();
        }
        return false;
    }

    opc_server_guid_to_tag_names_.at(guid_ptr).clear();
    // отображаем содержимое сервера, начиная с корневого узла
    export_server_address_space_(pBrowse, guid_ptr, notify_of_portion);
    pBrowse->Release();

    disconnect_server_(guid_ptr);

    return true;
}

void COPCClient::export_server_address_space_(IOPCBrowseServerAddressSpace* pParent, const GUID* guid_ptr, int notify_of_portion)
{
    if(!pParent || !guid_ptr) return;

    IEnumString* pEnum = NULL;
    wchar_t* strName = NULL;
    wchar_t* lpItemID = NULL;

    unsigned long cnt;

    HRESULT hRes = pParent->BrowseOPCItemIDs(OPC_LEAF, L"", VT_EMPTY, 0, &pEnum);

    if(!pEnum) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка экспорта адресного пространства сервера %2@%3 OPC_LEAF. hRes = %4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).ProgID, opc_server_guid_to_data_.at(guid_ptr).Hostname)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        return;
    }

    pEnum->Next(1, &strName, &cnt);

    while (cnt!=0) {
        pParent->GetItemID(strName, &lpItemID);//получает полный идентификатор тега
        opc_server_guid_to_tag_names_.at(guid_ptr).push_back(QString::fromWCharArray(lpItemID));
        pEnum->Next(1, &strName, &cnt);
        if(notify_of_portion > 0 && (opc_server_guid_to_tag_names_.at(guid_ptr).size() % notify_of_portion) == 0) {
            emit sg_get_part_tag_names_from_server(opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID, opc_server_guid_to_tag_names_.at(guid_ptr).size());
            QThread::currentThread()->eventDispatcher()->processEvents(QEventLoop::AllEvents);
        }
        if(QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
    }
    pEnum->Release();
    pEnum = NULL;

    pParent->BrowseOPCItemIDs(OPC_BRANCH, L"", VT_EMPTY, 0, &pEnum);

    if(!pEnum) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка экспорта адресного пространства сервера %2@%3 OPC_BRANCH. hRes = %4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).ProgID, opc_server_guid_to_data_.at(guid_ptr).Hostname)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        return;
    }

    pEnum->Next(1, &strName, &cnt);
    while (cnt != 0) {
        hRes = pParent->ChangeBrowsePosition(OPC_BROWSE_DOWN,strName);
        if (S_OK == hRes) {
            export_server_address_space_(pParent, guid_ptr, notify_of_portion);
            pParent->ChangeBrowsePosition(OPC_BROWSE_UP, strName);
            pEnum->Next(1, &strName, &cnt);
        } else {
            cnt = 0;
        }
        if(QThread::currentThread()->isInterruptionRequested()) {
            break;
        }
    }
    pEnum->Release();
}

bool COPCClient::connect_server_(const GUID* guid_ptr) {

    if (opc_server_guid_to_IOPCSErver_.at(guid_ptr) != NULL) {
        return true;
    }

    HRESULT hRes;
    IID IID_IOPCSERVER = __uuidof(IOPCServer);

    if(opc_server_guid_to_data_.at(guid_ptr).Hostname == u"localhost"_s) {
        hRes = CoCreateInstance(*guid_ptr, NULL, CLSCTX_LOCAL_SERVER, IID_IOPCSERVER, (void**)&opc_server_guid_to_IOPCSErver_.at(guid_ptr));
    } else {
        COSERVERINFO ServerInfo = {0};
        std::wstring wstr_hostname = opc_server_guid_to_data_.at(guid_ptr).Hostname.toStdWString();
        ServerInfo.pwszName = wstr_hostname.data();
        ServerInfo.pAuthInfo = NULL;

        MULTI_QI MultiQI [2] = {0};

        MultiQI [0].pIID = &IID_IOPCSERVER;
        MultiQI [0].pItf = NULL;
        MultiQI [0].hr = S_OK;

        hRes = CoCreateInstanceEx(*guid_ptr, NULL, CLSCTX_REMOTE_SERVER, &ServerInfo, 1, MultiQI);
        opc_server_guid_to_IOPCSErver_.at(guid_ptr) = static_cast<IOPCServer*>(MultiQI[0].pItf);
    }

    if (hRes != S_OK) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка подключения к серверу %2@%3. hRes = 0x%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(opc_server_guid_to_IOPCSErver_.at(guid_ptr) != NULL) {
            opc_server_guid_to_IOPCSErver_.at(guid_ptr)->Release();
        }
        opc_server_guid_to_IOPCSErver_.at(guid_ptr) = NULL;
        return false;
    }

    QString log_message = QString("ОРС-клиент поток [%1]: подключениe к серверу %2@%3 успешно")
                              .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID);

    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;

    return true;
}

bool COPCClient::register_group_(const GUID* guid_ptr)
{
    long bActive = 1;
    if(!opc_server_guid_to_IOPCSErver_.contains(guid_ptr) || !opc_server_guid_to_group_.contains(guid_ptr)) return false;

    IOPCServer* server = opc_server_guid_to_IOPCSErver_.at(guid_ptr);
    OPCGroupHandler& hgroup = opc_server_guid_to_group_.at(guid_ptr);

    if(hgroup.opc_handle_group != 0 && hgroup.pItemMgt != NULL && hgroup.pSyncIO != NULL) return true;

    if(hgroup.opc_handle_group != 0 && (hgroup.pItemMgt || hgroup.pSyncIO)) {
        remove_group_(guid_ptr);
    }

    HRESULT hRes = server->AddGroup(OLESTR("OPCDATG_GROUP"), bActive, hgroup.update_rate, 1, NULL, NULL, 0, &hgroup.opc_handle_group, &hgroup.update_rate, __uuidof(IOPCItemMgt), (IUnknown**)&hgroup.pItemMgt);

    if (FAILED(hRes)) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка добавления группы тэгов на сервер %2@%3. hRes = 0х%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(hgroup.pItemMgt != NULL) {
            hgroup.pItemMgt->Release();
        }
        hgroup.pItemMgt = NULL;
        hgroup.opc_handle_group = 0;

        return false;
    }

    IID IID_IOPCSYNCIO = __uuidof(IOPCSyncIO);
    hRes = hgroup.pItemMgt->QueryInterface(IID_IOPCSYNCIO, (void**)& hgroup.pSyncIO);

    if(FAILED(hRes)) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка запроса интерфейса сервера %2@%3. hRes = 0х%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        if(hgroup.pSyncIO != NULL) {
            hgroup.pSyncIO->Release();
        }
        return false;
    }
    return true;
}

void COPCClient::remove_group_(const GUID* guid_ptr)
{
    if(!guid_ptr || !opc_server_guid_to_IOPCSErver_.contains(guid_ptr) || opc_server_guid_to_IOPCSErver_.at(guid_ptr) == NULL) return;

    OPCGroupHandler& group_handler = opc_server_guid_to_group_.at(guid_ptr);

    if(group_handler.pItemMgt != NULL) {
        if(group_handler.opc_handle_group != 0) {
            HRESULT* hErr = NULL;
            HRESULT hRes;
            if(group_handler.dwCount() > 0) {
                OPCHANDLE* item_handlers = group_handler.GetTagHandlesArrayToRead();
                hRes = group_handler.pItemMgt->RemoveItems(group_handler.dwCount(), item_handlers, &hErr);

                if(FAILED(hRes)) {
                    QString log_message = QString("ОРС-клиент поток [%1]: ошибка удаления элементов группы тэгов из сервера %2@%3. hRes = 0х%4 : %5")
                                              .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                              .arg(static_cast<unsigned long>(hRes), 10, 16)
                                              .arg(GetErrorStringFromHRESULT(hRes));

                    emit sg_send_message_to_console(log_message);
                    qWarning() << log_message;
                }

                if(hErr != NULL) {
                    CoTaskMemFree(hErr);
                }
                delete[] item_handlers;
                group_handler.ClearTags();
            }
            if(group_handler.opc_handle_group != 0) {
                hRes = opc_server_guid_to_IOPCSErver_.at(guid_ptr)->RemoveGroup(group_handler.opc_handle_group, true);
                if(FAILED(hRes)) {
                    QString log_message = QString("ОРС-клиент поток [%1]: ошибка удаления группы тэгов из сервера %2. hRes = 0х%3 : %4")
                                              .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                              .arg(static_cast<unsigned long>(hRes), 10, 16)
                                              .arg(GetErrorStringFromHRESULT(hRes));

                    emit sg_send_message_to_console(log_message);
                    qWarning() << log_message;

                }
                group_handler.opc_handle_group = 0;
            }
        }
        group_handler.pItemMgt->Release();
        group_handler.pItemMgt = NULL;
    }
    if(opc_server_guid_to_group_.at(guid_ptr).pSyncIO != NULL) {
        group_handler.pSyncIO->Release();
        group_handler.pSyncIO = NULL;
    }
}

void COPCClient::disconnect_server_(const GUID* guid_ptr)
{
    if(!guid_ptr) return;

    remove_group_(guid_ptr);

    if (opc_server_guid_to_IOPCSErver_.at(guid_ptr) != NULL) {
        opc_server_guid_to_IOPCSErver_.at(guid_ptr)->Release();
        opc_server_guid_to_IOPCSErver_.at(guid_ptr) = NULL;

        qInfo() << QString("OPC- клиент поток [%1]. Отключен от сервера %2@%3")
                       .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID);
    }
}

size_t COPCClient::read_server_tags_(const GUID* guid_ptr)
{
    if(!guid_ptr || opc_server_guid_to_group_.count(guid_ptr) == 0) {
        return 0;
    }

    const OPCGroupHandler& group_hnd = opc_server_guid_to_group_.at(guid_ptr);

    if(group_hnd.dwCount() == 0) return 0;
    if(!connect_server_(guid_ptr)) return 0;
    if(!register_group_(guid_ptr)) return 0;

    OPCHANDLE* pHandles = group_hnd.GetTagHandlesArrayToRead();
    tagOPCITEMSTATE *pItemValue = NULL;
    HRESULT* pErrors = NULL;

    //Считываем элементы
    HRESULT hRes = group_hnd.pSyncIO->Read(OPC_DS_DEVICE, group_hnd.dwCount(), pHandles, &pItemValue, &pErrors);

    if(FAILED(hRes)) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка чтения тэгов из сервера %2@%3. hRes = 0х%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

    } else {
        for(size_t i = 0; i < group_hnd.dwCount(); ++i) {
            if(group_hnd.GetTagPtr(pHandles[i])) {
                group_hnd.GetTagPtr(pHandles[i])->SetOPCItemState(&pItemValue[i]);
            }
        }
    }

    CoTaskMemFree(pItemValue);
    CoTaskMemFree(pErrors);
    if(pHandles) delete[] pHandles;

    if(FAILED(hRes)) {
        return 0;
    } else {
        return group_hnd.dwCount();
    }
}

size_t COPCClient::write_server_tags_(const GUID* guid_ptr)
{
    if(!guid_ptr || opc_server_guid_to_group_.count(guid_ptr) == 0) return 0;

    const OPCGroupHandler& group_hnd = opc_server_guid_to_group_.at(guid_ptr);

    if(group_hnd.dwCount() == 0) return 0;
    if(!connect_server_(guid_ptr)) return 0;
    if(!register_group_(guid_ptr)) return 0;

    auto [pHandles, pItemValues, dwCount] = group_hnd.GetTagHandlesArrayToWrite();

    if(dwCount == 0) {
        if(pHandles) delete[] pHandles;
        if(pItemValues) delete[] pItemValues;
        return 0;
    }

    HRESULT* pErrors = NULL;

    //Считываем элементы
    HRESULT hRes = group_hnd.pSyncIO->Write(dwCount, pHandles, pItemValues, &pErrors);

    if(FAILED(hRes)) {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка записи тэгов из сервера %2@%3. hRes = 0х%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

    } else {
        for(size_t i = 0; i < dwCount; ++i) {
            if(group_hnd.GetTagPtr(pHandles[i])) {
                group_hnd.GetTagPtr(pHandles[i])->ResetValueToWrite();
            }
        }
    }

    CoTaskMemFree(pErrors);

    if(pHandles) delete[] pHandles;
    if(pItemValues) delete[] pItemValues;

    if(FAILED(hRes)) {
        return 0;
    } else {
        return dwCount;
    }
}

size_t COPCClient::add_tags_to_group_(const GUID* guid_ptr, std::vector<std::shared_ptr<DataTagOpcDA>>& tags)
{
    if(!guid_ptr) return 0;
    if(tags.size() == 0) return 0;
    if(!connect_server_(guid_ptr) || !register_group_(guid_ptr)) return 0;

    OPCGroupHandler& group_handler = opc_server_guid_to_group_.at(guid_ptr);
    std::vector<std::shared_ptr<DataTagOpcDA>> tags_checked;

    for(const auto& it: tags) {
        if(!group_handler.CheckTagExist(it)) {
            tags_checked.push_back(it);
        }
    }

    DWORD count_tags = tags_checked.size();
    if(count_tags == 0) return 0;

    tagOPCITEMDEF* pItems = new tagOPCITEMDEF[count_tags];
    tagOPCITEMRESULT* pResults = NULL;
    HRESULT* pErrors = NULL;

    for(size_t i = 0; i < count_tags; ++i) {
        pItems[i] = tags_checked.at(i)->GetItemDefStruct();
    }

    HRESULT hRes = group_handler.pItemMgt->AddItems(count_tags, pItems, &pResults, &pErrors);

    if (!FAILED(hRes)) {
        for(size_t i = 0; i < count_tags; ++i) {
            group_handler.AddTagWithHandle(tags_checked.at(i), pResults[i].hServer);
        }
    } else {
        QString log_message = QString("ОРС-клиент поток [%1]: ошибка добавления тэгов в группу сервера %2@%3. hRes = 0х%4 : %5")
                                  .arg(QThread::currentThread()->objectName(), opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                                  .arg(static_cast<unsigned long>(hRes), 10, 16)
                                  .arg(GetErrorStringFromHRESULT(hRes));

        emit sg_send_message_to_console(log_message);
        qWarning() << log_message;

        count_tags = 0;
    }
    CoTaskMemFree(pResults);
    CoTaskMemFree(pErrors);
    delete[] pItems;

    return count_tags;
}

const std::set<QString>& COPCClient::GetOPCServerNames(const QString& hostname) {
    if(!hostnames_.contains(hostname) || !host_to_opc_names_.contains(&(*hostnames_.find(hostname)))) {
        get_registered_servers_(hostname);
    }

    return host_to_opc_names_.at(&(*hostnames_.find(hostname)));
}

const std::vector<QString>& COPCClient::GetOPCTagsNames(const QString& hostname, const QString &server_name, int notify_of_portion)
{
    if(!hostnames_.contains(hostname)) {
        get_registered_servers_(hostname);
    }

    auto host_it = hostnames_.find(hostname);
    if(!host_to_opc_names_.at(&(*host_it)).contains(server_name)) {
        return empty_vec_;
    }

    auto server_it = host_to_opc_names_.at(&(*host_it)).find(server_name);

    if(opc_server_to_guid_.count(&(*server_it)) == 0) {
        return empty_vec_;
    }

    if(opc_server_guid_to_tag_names_.at(opc_server_to_guid_.at(&(*server_it))).size() > 0) {
        return opc_server_guid_to_tag_names_.at(opc_server_to_guid_.at(&(*server_it)));
    }

    const GUID* guid_ptr = opc_server_to_guid_.at(&(*server_it));

    if(!connect_server_(guid_ptr)) return empty_vec_;

    browse_server_address_space_(guid_ptr, notify_of_portion);

    QString log_message = QString("Обзор тэгов OPC сервера %1@%2: количество тэгов %3")
                            .arg(opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID)
                            .arg(opc_server_guid_to_tag_names_.at(guid_ptr).size());

    emit sg_send_message_to_console(log_message);
    qInfo() << log_message;

    emit sg_get_all_tag_names_from_server(opc_server_guid_to_data_.at(guid_ptr).Hostname, opc_server_guid_to_data_.at(guid_ptr).ProgID, opc_server_guid_to_tag_names_.at(guid_ptr).size());

    return opc_server_guid_to_tag_names_.at(guid_ptr);
}

size_t COPCClient::AddTags(std::vector<std::shared_ptr<DataTagOpcDA>> &tags)
{
    std::unordered_map<const GUID*, std::vector<std::shared_ptr<DataTagOpcDA>>> tags_map_temp;
    size_t ret_val = 0;

    for(const auto& it: tags) {
        if(!it) continue;

        if(!hostnames_.contains(it->GetHostName())) {
            get_registered_servers_(it->GetHostName());
        }
        auto host_it = hostnames_.find(it->GetHostName());

        if(!host_to_opc_names_.at(&(*host_it)).contains(it->GetEndpointName())) continue;
        auto server_it = host_to_opc_names_.at(&(*host_it)).find(it->GetEndpointName());

        auto casted_ptr = std::static_pointer_cast<DataTagOpcDA>(it);

        if(opc_server_to_guid_.contains(&(*server_it))
            && !opc_server_guid_to_group_.at(opc_server_to_guid_.at(&(*server_it))).CheckTagExist(it)) {

            tags_map_temp[opc_server_to_guid_.at(&(*server_it))].push_back(it);
        }
    }

    for(auto& [guid_ptr, opc_vec]: tags_map_temp) {
        ret_val += add_tags_to_group_(guid_ptr, opc_vec);
    }

    return ret_val;
}

size_t COPCClient::ReadTags()
{
    size_t ret_val = 0;
    for(const auto& [guid, _]: opc_server_guid_to_group_) {
        ret_val += read_server_tags_(guid);
    }
    return ret_val;
}

size_t COPCClient::WriteTags()
{
    size_t ret_val = 0;
    for(const auto& [guid, _]: opc_server_guid_to_group_) {
        ret_val += write_server_tags_(guid);
    }
    return ret_val;
}

void COPCClient::ClearTags()
{
    for(auto& [guid_ptr, opc_group]: opc_server_guid_to_group_) {
        remove_group_(guid_ptr);
        opc_group.ClearTags();
    }
}

std::optional<OPCSERVERSTATUS> COPCClient::GetServerStatus(const QString& hostname, const QString& server_name)
{
    if(!hostnames_.contains(hostname) || !host_to_opc_names_.contains(&(*hostnames_.find(hostname)))) {
        return std::nullopt;
    }

    auto host_it = hostnames_.find(hostname);
    auto server_it = host_to_opc_names_.at(&(*host_it)).find(server_name);

    if(!opc_server_to_guid_.contains(&(*server_it))) return std::nullopt;

    const GUID* guid_ptr = opc_server_to_guid_.at(&(*server_it));

    if(opc_server_guid_to_IOPCSErver_.at(guid_ptr) == NULL) {
        if(!connect_server_(guid_ptr)) return std::nullopt;
    }

    OPCSERVERSTATUS* ret_str_ptr = NULL;
    OPCSERVERSTATUS ret_str;
    HRESULT hres = opc_server_guid_to_IOPCSErver_.at(guid_ptr)->GetStatus(&ret_str_ptr);
    if(FAILED(hres)) {
        CoTaskMemFree(ret_str_ptr);
        return std::nullopt;
    }

    ret_str = *ret_str_ptr;
    CoTaskMemFree(ret_str_ptr);
    return ret_str;
}

QString OPC_HELPER::GetErrorStringFromHRESULT(HRESULT hr)
{
    QString retval;
    switch(hr)
    {
    case S_OK:					retval = "S_OK";break;
    case S_FALSE:				retval = "S_FALSE";break;
    case REGDB_E_WRITEREGDB:	retval = "REGDB_E_WRITEREGDB";break;
    case CO_E_CLASSSTRING:		retval = "CO_E_CLASSSTRING. The progID string was improperly formatted.";break;
    case E_NOINTERFACE:			retval = "E_NOINTERFACE";break;
    case E_FAIL:				retval = "E_FAIL";break;
    case E_OUTOFMEMORY:			retval = "E_OUTOFMEMORY";break;
    case E_INVALIDARG:			retval = "E_INVALIDARG";break;
    case E_NOTIMPL:				retval = "E_NOTIMPL";break;
    case OPC_E_INVALIDHANDLE:	retval = "OPC_E_INVALIDHANDLE";break;
    case OPC_E_UNKNOWNPATH:		retval = "OPC_E_UNKNOWNPATH";break;
    case OPC_E_UNKNOWNITEMID:	retval = "OPC_E_UNKNOWNITEMID";	break;
    case OPC_E_INVALIDITEMID:	retval = "OPC_E_INVALIDITEMID"; break;
    case OPC_S_UNSUPPORTEDRATE:	retval = "OPC_S_UNSUPPORTEDRATE";break;
    case OPC_E_PUBLIC:			retval = "OPC_E_PUBLIC";break;
    case OPC_E_INVALIDFILTER:	retval = "OPC_E_INVALIDFILTER";break;
    case DISP_E_TYPEMISMATCH:	retval = "DISP_E_TYPEMISMATCH";break;
    case OPC_E_BADTYPE:			retval = "OPC_E_BADTYPE";break;
    case OPC_S_CLAMP:			retval = "OPC_S_CLAMP";break;
    case OPC_E_RANGE:			retval = "OPC_E_RANGE";break;
    case OPC_E_BADRIGHTS:		retval = "OPC_E_BADRIGHTS";break;
    case DISP_E_OVERFLOW:		retval = "DISP_E_OVERFLOW";break;
    case OPC_E_DUPLICATENAME:	retval = "OPC_E_DUPLICATENAME";break;
    case static_cast<HRESULT>(0x800706ba):			retval = "RPC server unavailable";break;
    case static_cast<HRESULT>(0x80040154):			retval = "Class not registered.";break;
    case RPC_S_SERVER_UNAVAILABLE: retval = "RPC_S_SERVER_UNAVAILABLE";break;
    default:					retval = QString("Errorcode 0x%1 not defined.").arg(static_cast<unsigned long>(hr), 10, 16);
    }
    return retval;
}

//=======================================================
//===== OPCGroupHandler =================================
//=======================================================

DWORD OPCGroupHandler::dwCount() const
{
    return static_cast<unsigned long>(tag_to_opchandle_.size());
}

[[nodiscard]] std::tuple<OPCHANDLE*, VARIANT*, DWORD> OPCGroupHandler::GetTagHandlesArrayToWrite() const
{
    OPCHANDLE* ret_ptr = nullptr;
    VARIANT* ret_values = nullptr;
    DWORD ret_cnt = 0;

    if(tag_to_opchandle_.size() > 0) {
        ret_ptr = new OPCHANDLE[tag_to_opchandle_.size()];
        ret_values = new VARIANT[tag_to_opchandle_.size()];

        for(const auto& [tag_ptr, hnd]: tag_to_opchandle_) {
            if(tag_ptr->GetOPCVariantToWrite().has_value()) {
                ret_ptr[ret_cnt] = hnd;
                ret_values[ret_cnt++] = tag_ptr->GetOPCVariantToWrite().value();
            }
        }
    }
    return {ret_ptr, ret_values, ret_cnt};
}

[[nodiscard]] OPCHANDLE* OPCGroupHandler::GetTagHandlesArrayToRead() const
{
    OPCHANDLE* ret_ptr = nullptr;
    if(tag_to_opchandle_.size() > 0) {
        ret_ptr = new OPCHANDLE[tag_to_opchandle_.size()];
        size_t i = 0;
        for(const auto& [tag_ptr, hnd]: tag_to_opchandle_) {
            ret_ptr[i++] = hnd;
        }
    }
    return ret_ptr;
}

bool OPCGroupHandler::AddTagWithHandle(std::shared_ptr<DataTagOpcDA> &tag, OPCHANDLE hnd)
{
    if(!CheckTagExist(tag)) {
        tag_to_opchandle_[tag] = hnd;
        opchandle_to_tag_[hnd] = tag;
        return true;
    }
    return false;
}

void OPCGroupHandler::ClearTags()
{
    tag_to_opchandle_.clear();
    opchandle_to_tag_.clear();
}

std::shared_ptr<DataTagOpcDA> OPCGroupHandler::GetTagPtr(OPCHANDLE hnd) const
{
    if(opchandle_to_tag_.count(hnd) > 0) return opchandle_to_tag_.at(hnd);
    return nullptr;
}

OPCHANDLE OPCGroupHandler::GetTagHandle(std::shared_ptr<DataTagOpcDA> &tag) const
{
    if(tag_to_opchandle_.count(tag) > 0) return tag_to_opchandle_.at(tag);
    return 0;
}

size_t OPCGroupHandler::GetTagsCount() const
{
    return opchandle_to_tag_.size();
}

bool OPCGroupHandler::CheckTagExist(const std::shared_ptr<DataTagOpcDA>& tag) const
{
    return tag_to_opchandle_.count(tag) > 0;
}
