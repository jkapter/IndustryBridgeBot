#ifndef DATATAG_OPCUA_H
#define DATATAG_OPCUA_H

#include <QtOpcUa>

#include "datatag.h"

//=========================================================================
//================== DataTagOpcUA =========================================
//=========================================================================

class DataTagOpcUA: public DataTag
{
public:
    explicit DataTagOpcUA(const QString& host, const QString& endpoint, const QString& tagname);
    virtual const QString& GetTagName() const override;
    void SetUaNodePtr(QOpcUaNode* ua_node_ptr);
    QVariant GetOPCVariantToWrite();
    void MarkWriteSent();
    QOpcUa::Types GetOpcUaType() const;

private slots:
    void sl_ua_node_attribute_read_(QOpcUa::NodeAttributes attributes);
    void sl_ua_node_attribute_updated_(QOpcUa::NodeAttribute attr, QVariant value);
    void sl_ua_node_value_attribute_written_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode);

private:
    QString ua_browse_name_;
    QString ua_display_name_;
    QOpcUa::NodeClass ua_node_class_ = QOpcUa::NodeClass::Undefined;
    QOpcUa::Types ua_data_type_ = QOpcUa::Types::Undefined;
    QOpcUa::UaStatusCode ua_status_code_ = QOpcUa::UaStatusCode::Good;
    QDateTime ua_source_timestamp_;

    QOpcUaNode* ua_node_ptr_ = nullptr;

    // Защита от "затопления" сервера повторяющейся записью: не шлём новый запрос, пока
    // не пришёл ответ на предыдущий, и сдаёмся после нескольких отклонений подряд
    // (вместо того чтобы повторять одну и ту же отклонённую запись бесконечно).
    bool write_pending_confirmation_ = false;
    int write_retry_count_ = 0;
    static constexpr int MAX_WRITE_RETRIES_ = 3;

    QOpcUa::Types node_id_type_to_qopcuatype_(const QString& nodeIdString);
    DataType data_type_from_ua_type_(QOpcUa::Types ua_type);
    QMetaType ua_type_to_meta_type_(QOpcUa::Types ua_type);
    static DataQuality quality_from_ua_status_(QOpcUa::UaStatusCode status);
    static std::optional<std::pair<DataType, QOpcUa::Types>> data_type_from_iec61131_node_id_(const QString& nodeIdString);
};

#endif // DATATAG_OPCUA_H
