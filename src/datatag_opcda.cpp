#include "datatag_opcda.h"

using namespace Qt::StringLiterals;

DataTagOpcDA::DataTagOpcDA(const QString &host, const QString &endpoint, const QString &tagname)
    : DataTag(DataTag::DataSource::OPCDA, host, endpoint, tagname)
    , tag_id_wstring_(tagname.toStdWString())
{
}

tagOPCITEMDEF DataTagOpcDA::GetItemDefStruct()
{
    QMutexLocker locker(&mtx_);
    tagOPCITEMDEF ret_def;
    ret_def.szItemID = tag_id_wstring_.data();
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

WORD DataTagOpcDA::GetOpcDaQuality()
{
    QMutexLocker locker(&mtx_);
    return last_opc_value_.wQuality;
}

QString DataTagOpcDA::GetOpcDaQualityAsString()
{
    QMutexLocker locker(&mtx_);
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
