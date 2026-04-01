#include "datatag.h"

#include <QJsonObject>
#include <QJsonArray>

using namespace Qt::StringLiterals;

namespace DATATAG {

ValueVariant operator-(ValueVariant lhs, ValueVariant rhs) {
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) - toLongLong(rhs);
    case 1: return std::get<double>(lhs) - toDouble(rhs);
    case 2: return std::get<QString>(lhs);
    }
    return 0;
}

ValueVariant operator+(ValueVariant lhs, ValueVariant rhs) {
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) + toLongLong(rhs);
    case 1: return std::get<double>(lhs) + toDouble(rhs);
    case 2: return QString("%1%2").arg(toString(lhs), toString(rhs));
    }
    return 0;
}

bool operator<(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) < toLongLong(rhs);
    case 1: return std::get<double>(lhs) < toDouble(rhs);
    case 2: return std::get<QString>(lhs) < toString(rhs);
    }
    return false;
}

bool operator==(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) == toLongLong(rhs);
    case 1: return std::get<double>(lhs) == toDouble(rhs);
    case 2: return std::get<QString>(lhs) == toString(rhs);
    }
    return false;
}

bool operator>(ValueVariant lhs, ValueVariant rhs)
{
    switch(lhs.index()) {
    case 0: return std::get<int64_t>(lhs) > toLongLong(rhs);
    case 1: return std::get<double>(lhs) > toDouble(rhs);
    case 2: return std::get<QString>(lhs) > toString(rhs);
    }
    return false;
}

QString toString(ValueVariant val)
{
    if(val.valueless_by_exception()) return "";
    switch(val.index()) {
    case 0: return QString("%1").arg(std::get<0>(val));
    case 1: return QString("%1").arg(std::get<1>(val));
    case 2: return QString("%1").arg(std::get<2>(val));
    }
    return "";
}

double toDouble(ValueVariant val) {
    if(val.index() == 0) return static_cast<double>(std::get<int64_t>(val));
    if(val.index() == 1) return std::get<double>(val);

    QString str = std::get<QString>(val);
    bool b = false;
    double ret_val = str.toDouble(&b);
    if(b) {
        return ret_val;
    } else {
        return 0;
    }
}

int64_t toLongLong(ValueVariant val) {
    if(val.index() == 0) return std::get<int64_t>(val);
    if(val.index() == 1) return std::numeric_limits<int64_t>::max() > std::get<double>(val) ? static_cast<int64_t>(std::get<double>(val)) : std::numeric_limits<int64_t>::max();

    QString str = std::get<QString>(val);
    bool b = false;
    double ret_val = str.toLongLong(&b);
    if(b) {
        return ret_val;
    } else {
        return 0;
    }
}
} //namespace

DataTag::DataTag(DataSource src, const QString& host, const QString& endpoint, const QString& tagname)
    : tag_source_(src)
    , data_type_(DataTag::DataType::UNKNOWN)
    , comment_(QString("[%1][%2]").arg(u"localhost"_s, endpoint))
    , hostname_(host)
    , endpoint_name_(endpoint)
    , tag_name_(tagname)
    , tag_quality_(DataTag::DataQuality::BAD)
{}

DataTag::DataSource DataTag::GetDataSource() const
{
    return tag_source_;
}

const QString &DataTag::GetHostName() const
{
    return hostname_;
}

const QString &DataTag::GetTagName() const
{
    return tag_name_;
}

QString DataTag::GetFullTagDescription() const
{
    return QString("[%1][%2][%3]").arg(hostname_, endpoint_name_, tag_name_);
}

QString DataTag::GetStringType()
{
    QMutexLocker locker(&mtx_);
    QString ret_str;
    switch(data_type_) {
    case DataType::BOOLEAN      : ret_str = u"BOOLEAN"_s; break;
    case DataType::FLOAT4       : ret_str = u"FLOAT4"_s; break;
    case DataType::INT1         : ret_str = u"INT1"_s; break;
    case DataType::INT2         : ret_str = u"INT2"_s; break;
    case DataType::INT4         : ret_str = u"INT4"_s; break;
    case DataType::INT8         : ret_str = u"INT8"_s; break;
    case DataType::REAL8        : ret_str = u"REAL8"_s; break;
    case DataType::UINT1        : ret_str = u"UINT1"_s; break;
    case DataType::UINT2        : ret_str = u"UINT2"_s; break;
    case DataType::UINT4        : ret_str = u"UINT4"_s; break;
    case DataType::UINT8        : ret_str = u"UINT8"_s; break;
    case DataType::UNKNOWN      : ret_str = u"UNKNOWN"_s; break;
    case DataType::WSTRING      : ret_str = u"WSTRING"_s; break;
    case DataType::UNSUPPORTED  : ret_str = u"UNSUPPORTED"_s; break;
    default                     : ret_str = u"UNSUPPORTED"_s; break;
    }
    return ret_str;
}

QString DataTag::GetStringValue(bool use_substitute_values)
{
    QMutexLocker locker(&mtx_);
    QString ret_str;
    if(data_type_ == DataType::UNKNOWN) return ret_str;
    if(data_type_ == DataType::UNSUPPORTED) return value_.canConvert<QString>() ? value_.toString() : ret_str;
    if(ValueIsBool()) {
        ret_str = value_.toBool() ? "ДА" : "НЕТ";
    } else {
        ret_str = value_.toString();
    }

    if(use_substitute_values && substitute_values_.contains(ret_str)) ret_str = substitute_values_.at(ret_str);
    return ret_str;
}

ValueVariant DataTag::GetValue(bool use_substitute_values)
{
    QMutexLocker locker(&mtx_);
    if(ValueIsBool()) return static_cast<int64_t>(value_.toBool());
    if(ValueIsInteger()) return static_cast<int64_t>(value_.toLongLong());
    if(ValueIsUnsignedInteger()) return value_.toLongLong() < std::numeric_limits<int64_t>::max() ? static_cast<int64_t>(value_.toLongLong()) : std::numeric_limits<int64_t>::max();
    if(ValueIsReal()) return value_.toDouble();
    return GetStringValue(use_substitute_values);
}

QString DataTag::QualityToString(DataQuality q)
{
    switch(q) {
        using enum DataQuality;
    case BAD:           return u"BAD"_s;
    case GOOD:          return u"GOOD"_s;
    case UNCERTAIN:     return u"UNCERTAIN"_s;
    }
    return "";
}

const QString &DataTag::GetEndpointName() const
{
    return endpoint_name_;
}

DataTag::DataQuality DataTag::GetTagQuality()
{
    QMutexLocker locker(&mtx_);
    return tag_quality_;
}

bool DataTag::TagQualityIsGood() const
{
    return tag_quality_ == DataTag::DataQuality::GOOD;
}

bool DataTag::ValueIsInteger() const
{
    return data_type_ == DataType::INT1
            || data_type_ == DataType::INT2
            || data_type_ == DataType::INT4
            || data_type_ == DataType::INT8;
}

bool DataTag::ValueIsReal() const
{
    return data_type_ == DataType::FLOAT4
           || data_type_ == DataType::REAL8;
}

bool DataTag::ValueIsUnsignedInteger() const
{
    return data_type_ == DataType::UINT1
           || data_type_ == DataType::UINT2
           || data_type_ == DataType::UINT4
           || data_type_ == DataType::UINT8;
}

bool DataTag::ValueIsString() const
{
    return data_type_ == DataType::WSTRING;
}

bool DataTag::ValueIsBool() const
{
    return data_type_ == DataType::BOOLEAN;
}

const QString &DataTag::GetCommentString() const
{
    return comment_;
}

void DataTag::SetCommentString(const QString &str)
{
    comment_ = str;
}

void DataTag::SetCommentString(QString &&str)
{
    comment_ = std::move(str);
}

std::optional<double> DataTag::GetGainOption() const
{
    return gain_value_;
}

void DataTag::SetGainOption(double gain)
{
    gain_value_ = gain;
}

void DataTag::ResetGainOption()
{
    gain_value_.reset();
}

void DataTag::SetValueToWrite(QVariant val)
{
    QMutexLocker locker(&mtx_);
    value_to_write_ = val;
}

void DataTag::SetValueToWrite(ValueVariant val)
{
    QMutexLocker locker(&mtx_);
    if(val.valueless_by_exception()) return;
    switch(val.index()) {
    case 0: value_to_write_ = std::get<0>(val); break;
    case 1: value_to_write_ = std::get<1>(val); break;
    case 2: value_to_write_ = std::get<2>(val); break;
    }
}

void DataTag::ResetValueToWrite()
{
    value_to_write_.clear();
}

void DataTag::AddSubstituteStringValue(const QString &raw_value, const QString &substitute_value)
{
    substitute_values_[raw_value] = substitute_value;
}

const std::unordered_map<QString, QString> &DataTag::GetSubstituteStringValues() const
{
    return substitute_values_;
}

void DataTag::ClearSubstituteStringValues()
{
    substitute_values_.clear();
}

QJsonObject DataTag::TagToJson(bool full_info) const
{
    QJsonObject ret_obj;
    ret_obj.insert("tag_name", tag_name_);
    ret_obj.insert("tag_comment", comment_);
    if(full_info) {
        ret_obj.insert("server", endpoint_name_);
        ret_obj.insert("hostname", hostname_);
    }

    QString source_str;
    switch(tag_source_) {
    case DataSource::OPCDA  : source_str = "OPCDA"; break;
    case DataSource::OPCUA  : source_str = "OPCUA"; break;
    default                 : source_str = "NONVALID"; break;
    }
    ret_obj.insert("source", source_str);
    if(gain_value_.has_value()) {
        ret_obj.insert("gain", gain_value_.value());
    }
    if(substitute_values_.size() > 0) {
        QJsonArray subst_array;
        for(const auto& [val, substitute_val]: substitute_values_) {
            subst_array.append(QJsonObject{{val, substitute_val}});
        }
        ret_obj.insert("substitute_values", std::move(subst_array));
    }
    return ret_obj;
}

//=========================================================================
//================== DataTagOpcDA =========================================
//=========================================================================


DataTagOpcDA::DataTagOpcDA(const QString &host, const QString &endpoint, const QString &tagname)
    : DataTag(DataTag::DataSource::OPCDA, host, endpoint, tagname)
    , tag_name_wstring_(tagname.toStdWString())
{
}

tagOPCITEMDEF DataTagOpcDA::GetItemDefStruct()
{
    QMutexLocker locker(&mtx_);
    tagOPCITEMDEF ret_def;
    ret_def.szItemID = tag_name_wstring_.data();
    ret_def.szAccessPath = NULL;
    ret_def.bActive = TRUE;
    ret_def.hClient = 0;
    ret_def.vtRequestedDataType = opc_legacy_type_;
    ret_def.dwBlobSize = 0;
    ret_def.pBlob = NULL;
    return ret_def;
}

void DataTagOpcDA::SetOPCItemState(tagOPCITEMSTATE *item_state)
{
    QMutexLocker locker(&mtx_);
    if(!item_state) return;
    last_opc_value_ = *item_state;
    opc_legacy_type_ = VARENUM(last_opc_value_.vDataValue.vt);
    data_type_ = get_type_from_opc_legacy_type_(opc_legacy_type_);
    value_ = extract_value_from_opc_struct_();
    switch(last_opc_value_.wQuality) {
    case 0xC0:  tag_quality_ = DataQuality::GOOD; break;
    case 0x00:
    case 0x04:
    case 0x08:
    case 0x0c:
    case 0x10:
    case 0x1c:
    case 0x18:  tag_quality_ = DataQuality::BAD; break;
    default:    tag_quality_ = DataQuality::UNCERTAIN; break;
    }
}

WORD DataTagOpcDA::GetOpcDaQuality() const
{
    return last_opc_value_.wQuality;
}

QString DataTagOpcDA::GetOpcDaQualityAsString() const
{
    switch (last_opc_value_.wQuality) {
    case 0x00: return u"Bad"_s;
    case 0x04: return u"Config Error"_s;
    case 0x08: return u"Not Connected"_s;
    case 0x0C: return u"Device Failure"_s;
    case 0x10: return u"Sensor Failure"_s;
    case 0x14: return u"Last Known"_s;
    case 0x18: return u"Comm Failure"_s;
    case 0x1C: return u"Out of Service"_s;
    case 0x20: return u"Initializing"_s;
    case 0x40: return u"Uncertain"_s;
    case 0x44: return u"Last Usable"_s;
    case 0x50: return u"Sensor Calibration"_s;
    case 0x54: return u"EGU Exceeded"_s;
    case 0x58: return u"Sub Normal"_s;
    case 0xC0: return u"Good"_s;
    case 0xD8: return u"Local Override"_s;
    default: return u"Unknown"_s;
    }
}

std::optional<VARIANT> DataTagOpcDA::GetOPCVariantToWrite()
{
    QMutexLocker locker(&mtx_);
    if(!value_to_write_.isValid()) return std::nullopt;
    VARIANT ret_var;
    ret_var.vt = opc_legacy_type_;
    double gain = gain_value_.has_value() ? gain_value_.value() : 1.0;
    switch(ret_var.vt) {
    case VT_I2: ret_var.iVal = static_cast<SHORT>(value_.toInt()); break;
    case VT_I4: ret_var.lVal = static_cast<LONG>(value_.toInt()); break;
    case VT_I1: ret_var.bVal = static_cast<SHORT>(value_.toInt()); break;
    case VT_I8: ret_var.llVal = static_cast<LONGLONG>(value_.toInt()); break;
    case VT_INT: ret_var.intVal = static_cast<INT>(value_.toInt()); break;
    case VT_R4: ret_var.fltVal = static_cast<FLOAT>(value_.toDouble() / gain); break;
    case VT_R8: ret_var.dblVal = static_cast<DOUBLE>(value_.toDouble() / gain); break;
    case VT_UI2: ret_var.uiVal = static_cast<USHORT>(value_.toUInt()); break;
    case VT_UI4: ret_var.ulVal = static_cast<ULONG>(value_.toUInt()); break;
    case VT_UI1: ret_var.uiVal = static_cast<USHORT>(value_.toUInt()); break;
    case VT_UI8: ret_var.ullVal = static_cast<ULONGLONG>(value_.toUInt()); break;
    case VT_UINT: ret_var.uintVal = static_cast<UINT>(value_.toUInt()); break;
    case VT_BSTR: {
        buffer_string_ = value_.toString().toStdWString();
        ret_var.bstrVal = const_cast<wchar_t*>(buffer_string_.c_str());
        break;
    }
    case VT_BOOL: ret_var.boolVal = value_.toBool() ? VARIANT_TRUE : VARIANT_FALSE; break;
    default : return std::nullopt;
    }
    return ret_var;
}

QVariant DataTagOpcDA::extract_value_from_opc_struct_() const
{
    QVariant ret_val;
    switch(opc_legacy_type_) {
    case VT_I1: ret_val = static_cast<int64_t>(last_opc_value_.vDataValue.bVal); break;
    case VT_I2: ret_val = static_cast<int64_t>(last_opc_value_.vDataValue.iVal); break;
    case VT_I4: ret_val = static_cast<int64_t>(last_opc_value_.vDataValue.lVal); break;
    case VT_I8: ret_val = static_cast<int64_t>(last_opc_value_.vDataValue.llVal); break;
    case VT_INT: ret_val = static_cast<int64_t>(last_opc_value_.vDataValue.intVal); break;
    case VT_R4: ret_val = static_cast<double>(last_opc_value_.vDataValue.fltVal); break;
    case VT_R8: ret_val = static_cast<double>(last_opc_value_.vDataValue.dblVal); break;
    case VT_UI1: ret_val = static_cast<size_t>(last_opc_value_.vDataValue.uiVal); break;
    case VT_UI2: ret_val = static_cast<size_t>(last_opc_value_.vDataValue.uiVal); break;
    case VT_UI4: ret_val = static_cast<size_t>(last_opc_value_.vDataValue.ulVal); break;
    case VT_UI8: ret_val = static_cast<size_t>(last_opc_value_.vDataValue.ullVal); break;
    case VT_UINT: ret_val = static_cast<size_t>(last_opc_value_.vDataValue.uintVal); break;
    case VT_BOOL: ret_val = last_opc_value_.vDataValue.boolVal == VARIANT_TRUE ? true : false; break;
    default: ;
    }
    return ret_val;
}

DataTag::DataType DataTagOpcDA::get_type_from_opc_legacy_type_(const unsigned short usType) const
{
    switch (usType)    {
        using enum VARENUM;
    case VT_R4          : return DataType::FLOAT4;
    case VT_R8          : return DataType::REAL8;
    case VT_ARRAY       : return DataType::UNSUPPORTED;
    case VT_BOOL        : return DataType::BOOLEAN;
    case VT_BSTR        : return DataType::WSTRING;
    case VT_DECIMAL     : return DataType::UNSUPPORTED;;
    case VT_I1          : return DataType::INT1;
    case VT_I2          : return DataType::INT2;
    case VT_I4          : return DataType::INT4;
    case VT_I8          : return DataType::INT8;
    case VT_INT         : return DataType::INT4;
    case VT_UI2         : return DataType::UINT2;
    case VT_UI4         : return DataType::UINT4;
    case VT_UI1         : return DataType::UINT1;
    case VT_UINT        : return DataType::UINT4;
    case VT_FILETIME    : return DataType::UNSUPPORTED;
    default             : return DataType::UNKNOWN;
    }
}

//=========================================================================
//================== DataTagOpcDA =========================================
//=========================================================================

DataTagOpcUA::DataTagOpcUA(const QString &host, const QString &endpoint, const QString &tagname)
    : DataTag(DataSource::OPCUA, host, endpoint, tagname)
{

}


