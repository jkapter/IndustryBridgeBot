#include "datatag_opcua.h"

DataTagOpcUA::DataTagOpcUA(const QString &host, const QString &endpoint, const QString &tag_id)
    : DataTag(DataSource::OPCUA, host, endpoint, tag_id)
{}

const QString &DataTagOpcUA::GetTagName() const
{
    return ua_browse_name_.isEmpty() ? GetTagId() : ua_browse_name_;
}

void DataTagOpcUA::SetUaNodePtr(QOpcUaNode* ua_node_ptr)
{
    QMutexLocker locker(&mtx_);
    if(!ua_node_ptr) return;
    ua_node_ptr_ = ua_node_ptr;
    QObject::connect(ua_node_ptr_, &QOpcUaNode::attributeRead, [this](QOpcUa::NodeAttributes attributes){sl_ua_node_attribute_read_(attributes);});
    QObject::connect(ua_node_ptr_, &QOpcUaNode::attributeUpdated, [this](QOpcUa::NodeAttribute attr, QVariant value){sl_ua_node_attribute_updated_(attr, value);});
    QObject::connect(ua_node_ptr_, &QOpcUaNode::attributeWritten, [this](QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode){sl_ua_node_value_attribute_written_(attribute, statusCode);});
}

QOpcUa::Types DataTagOpcUA::GetOpcUaType() const
{
    return ua_data_type_;
}

QVariant DataTagOpcUA::GetOPCVariantToWrite()
{
    QMutexLocker locker(&mtx_);
    // Ответ на уже отправленную запись ещё не пришёл -- не дублируем запрос.
    if(write_pending_confirmation_) return QVariant();

    if(value_to_write_.isValid()) {
        QVariant ret_val = value_to_write_;
        QMetaType ua_meta_type = ua_type_to_meta_type_(ua_data_type_);
        if(!ua_meta_type.isValid()
            || !ret_val.canConvert(ua_meta_type)
            || !ret_val.convert(ua_meta_type))
            return QVariant();

        return ret_val;
    }
    return QVariant();
}

void DataTagOpcUA::MarkWriteSent()
{
    QMutexLocker locker(&mtx_);
    write_pending_confirmation_ = true;
    ++write_retry_count_;
}

QOpcUa::Types DataTagOpcUA::node_id_type_to_qopcuatype_(const QString& nodeIdString) {
    quint16 ns_index = 0;
    QString identifier;
    char identifier_type = '\0';

    if(QOpcUa::nodeIdStringSplit(nodeIdString, &ns_index, &identifier, &identifier_type)
        && ns_index == 0 && identifier_type == 'i') {

        bool ok = false;
        int id = identifier.toInt(&ok);
        if(ok) switch (id) {
        case 1:   return QOpcUa::Types::Boolean;
        case 2:   return QOpcUa::Types::SByte;
        case 3:   return QOpcUa::Types::Byte;
        case 4:   return QOpcUa::Types::Int16;
        case 5:   return QOpcUa::Types::UInt16;
        case 6:   return QOpcUa::Types::Int32;
        case 7:   return QOpcUa::Types::UInt32;
        case 8:   return QOpcUa::Types::Int64;
        case 9:   return QOpcUa::Types::UInt64;
        case 10:  return QOpcUa::Types::Float;
        case 11:  return QOpcUa::Types::Double;
        case 12:  return QOpcUa::Types::String;
        case 13:  return QOpcUa::Types::DateTime;
        case 14:  return QOpcUa::Types::Guid;
        case 15:  return QOpcUa::Types::ByteString;
        case 16:  return QOpcUa::Types::XmlElement;
        case 17:  return QOpcUa::Types::NodeId;
        case 18:  return QOpcUa::Types::ExpandedNodeId;
        case 19:  return QOpcUa::Types::StatusCode;
        case 20:  return QOpcUa::Types::QualifiedName;
        case 21:  return QOpcUa::Types::LocalizedText;
        default:  return QOpcUa::Types::Undefined;
        }
    }
    return QOpcUa::Types::Undefined;
}

std::optional<std::pair<DataTag::DataType, QOpcUa::Types>> DataTagOpcUA::data_type_from_iec61131_node_id_(const QString& nodeIdString)
{
    quint16 ns_index = 0;
    QString identifier;
    char identifier_type = '\0';

    if(!QOpcUa::nodeIdStringSplit(nodeIdString, &ns_index, &identifier, &identifier_type)
        || ns_index != 3 || identifier_type != 'i') {
        return std::nullopt;
    }

    bool ok = false;
    int id = identifier.toInt(&ok);
    if(!ok) return std::nullopt;

    using enum QOpcUa::Types;
    switch(id) {
    case 3001: return std::make_pair(DataTag::DataType::UINT1, Byte);     // BYTE
    case 3002: return std::make_pair(DataTag::DataType::UINT2, UInt16);   // WORD
    case 3003: return std::make_pair(DataTag::DataType::UINT4, UInt32);   // DWORD
    case 3005: return std::make_pair(DataTag::DataType::INT8, Int64);     // TIME (длительность в мс)
    case 3013: return std::make_pair(DataTag::DataType::WSTRING, String); // STRING
    default:   return std::nullopt;
    }
}

DataTag::DataType DataTagOpcUA::data_type_from_ua_type_(QOpcUa::Types ua_type)
{
    switch(ua_type) {
        using enum QOpcUa::Types;
    case Boolean:       return DataTag::DataType::BOOLEAN;
    case SByte:         return DataTag::DataType::INT1;
    case Byte:          return DataTag::DataType::UINT1;
    case Int16:         return DataTag::DataType::INT2;
    case UInt16:        return DataTag::DataType::UINT2;
    case Int32:         return DataTag::DataType::INT4;
    case UInt32:        return DataTag::DataType::UINT4;
    case Int64:         return DataTag::DataType::INT8;
    case UInt64:        return DataTag::DataType::UINT8;
    case Float:         return DataTag::DataType::FLOAT4;
    case Double:        return DataTag::DataType::REAL8;
    case String:        return DataTag::DataType::WSTRING;
    case ByteString:    return DataTag::DataType::WSTRING;
    default:            return DataTag::DataType::UNSUPPORTED;
    }
}

void DataTagOpcUA::sl_ua_node_attribute_read_(QOpcUa::NodeAttributes attributes)
{
    QMutexLocker locker(&mtx_);
    if (attributes & QOpcUa::NodeAttribute::NodeClass)
        ua_node_class_ = ua_node_ptr_->attribute(QOpcUa::NodeAttribute::NodeClass).value<QOpcUa::NodeClass>();
    if (attributes & QOpcUa::NodeAttribute::BrowseName)
        ua_browse_name_ = ua_node_ptr_->attribute(QOpcUa::NodeAttribute::BrowseName).value<QOpcUaQualifiedName>().name();
    if (attributes & QOpcUa::NodeAttribute::DisplayName)
        ua_display_name_ = ua_node_ptr_->attribute(QOpcUa::NodeAttribute::DisplayName).value<QOpcUaLocalizedText>().text();
    if (attributes & QOpcUa::NodeAttribute::DataType) {
        QString data_type_node_id = ua_node_ptr_->attribute(QOpcUa::NodeAttribute::DataType).toString();
        ua_data_type_ = node_id_type_to_qopcuatype_(data_type_node_id);
        if(ua_data_type_ != QOpcUa::Types::Undefined) {
            data_type_ = data_type_from_ua_type_(ua_data_type_);
        } else if(auto iec_type = data_type_from_iec61131_node_id_(data_type_node_id)) {
            data_type_ = iec_type->first;
            ua_data_type_ = iec_type->second;
        } else {
            data_type_ = DataType::UNSUPPORTED;
            qWarning() << QString("ОРС UA тэг [%1]: не распознан тип DataType, NodeId = \"%2\".").arg(ua_node_ptr_->nodeId(), data_type_node_id);
        }
    }
    if(attributes & QOpcUa::NodeAttribute::Value) {
        value_ = ua_node_ptr_->attribute(QOpcUa::NodeAttribute::Value);
        ua_status_code_ = ua_node_ptr_->attributeError(QOpcUa::NodeAttribute::Value);
        tag_quality_ = quality_from_ua_status_(ua_status_code_);
    }
}

void DataTagOpcUA::sl_ua_node_attribute_updated_(QOpcUa::NodeAttribute attr, QVariant attr_value)
{
    QMutexLocker locker(&mtx_);
    if (attr & QOpcUa::NodeAttribute::NodeClass)
        ua_node_class_ = attr_value.value<QOpcUa::NodeClass>();
    if (attr & QOpcUa::NodeAttribute::BrowseName)
        ua_browse_name_ = attr_value.value<QOpcUaQualifiedName>().name();
    if (attr & QOpcUa::NodeAttribute::DisplayName)
        ua_display_name_ = attr_value.value<QOpcUaLocalizedText>().text();
    if (attr & QOpcUa::NodeAttribute::DataType) {
        QString data_type_node_id = attr_value.toString();
        ua_data_type_ = node_id_type_to_qopcuatype_(data_type_node_id);
        if(ua_data_type_ != QOpcUa::Types::Undefined) {
            data_type_ = data_type_from_ua_type_(ua_data_type_);
        } else if(auto iec_type = data_type_from_iec61131_node_id_(data_type_node_id)) {
            data_type_ = iec_type->first;
            ua_data_type_ = iec_type->second;
        } else {
            data_type_ = DataType::UNSUPPORTED;
            qWarning() << QString("ОРС UA тэг [%1]: не распознан тип DataType, NodeId = \"%2\".").arg(tag_id_, data_type_node_id);
        }
    }
    if(attr & QOpcUa::NodeAttribute::Value) {
        value_ = attr_value;
        ua_status_code_ = ua_node_ptr_ ? ua_node_ptr_->attributeError(QOpcUa::NodeAttribute::Value) : QOpcUa::UaStatusCode::Good;
        tag_quality_ = quality_from_ua_status_(ua_status_code_);
    }
}

DataTag::DataQuality DataTagOpcUA::quality_from_ua_status_(QOpcUa::UaStatusCode status)
{
    quint32 severity = static_cast<quint32>(status) & 0xC0000000u;
    if(severity == 0x80000000u) return DataQuality::BAD;
    if(severity == 0x40000000u) return DataQuality::UNCERTAIN;
    return DataQuality::GOOD;
}

void DataTagOpcUA::sl_ua_node_value_attribute_written_(QOpcUa::NodeAttribute attribute, QOpcUa::UaStatusCode statusCode)
{
    if(attribute != QOpcUa::NodeAttribute::Value) return;

    // ResetValueToWrite() сама берёт mtx_, поэтому формируем решение в отдельном блоке
    // и снимаем запись уже после того, как локальный QMutexLocker освободит мьютекс.
    bool clear_value = false;
    {
        QMutexLocker locker(&mtx_);
        write_pending_confirmation_ = false;

        if(statusCode == QOpcUa::UaStatusCode::Good) {
            write_retry_count_ = 0;
            clear_value = true;
        } else if(write_retry_count_ >= MAX_WRITE_RETRIES_) {
            qCritical() << QString("ОРС UA тэг [%1]: запись значения отклонена сервером %2 раз подряд (статус 0x%3), попытки прекращены.")
                              .arg(tag_id_).arg(write_retry_count_).arg(static_cast<quint32>(statusCode), 8, 16, QChar('0'));
            write_retry_count_ = 0;
            clear_value = true;
        } else {
            qWarning() << QString("ОРС UA тэг [%1]: запись значения отклонена сервером, статус 0x%2, попытка %3 из %4.")
                              .arg(tag_id_).arg(static_cast<quint32>(statusCode), 8, 16, QChar('0')).arg(write_retry_count_).arg(MAX_WRITE_RETRIES_);
        }
    }

    if(clear_value) {
        ResetValueToWrite();
    }
}

QMetaType DataTagOpcUA::ua_type_to_meta_type_(QOpcUa::Types ua_type)
{
    switch(ua_type) {
    case QOpcUa::Boolean:    return QMetaType(QMetaType::Bool);
    case QOpcUa::SByte:      return QMetaType(QMetaType::SChar);
    case QOpcUa::Byte:       return QMetaType(QMetaType::UChar);
    case QOpcUa::Int16:      return QMetaType(QMetaType::Short);
    case QOpcUa::UInt16:     return QMetaType(QMetaType::UShort);
    case QOpcUa::Int32:      return QMetaType(QMetaType::Int);
    case QOpcUa::UInt32:     return QMetaType(QMetaType::UInt);
    case QOpcUa::Int64:      return QMetaType(QMetaType::LongLong);
    case QOpcUa::UInt64:     return QMetaType(QMetaType::ULongLong);
    case QOpcUa::Float:      return QMetaType(QMetaType::Float);
    case QOpcUa::Double:     return QMetaType(QMetaType::Double);
    case QOpcUa::String:     return QMetaType(QMetaType::QString);
    case QOpcUa::DateTime:   return QMetaType(QMetaType::QDateTime);
    case QOpcUa::ByteString: return QMetaType(QMetaType::QByteArray);
    default:                 return QMetaType();
    }
}
