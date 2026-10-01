#ifndef DATATAG_OPCDA_H
#define DATATAG_OPCDA_H

#include "datatag.h"
#include "opcda.h"

//=========================================================================
//================== DataTagOpcDA =========================================
//=========================================================================

class DataTagOpcDA: public DataTag
{
public:
    explicit DataTagOpcDA(const QString& host, const QString& endpoint, const QString& tagname);
    tagOPCITEMDEF GetItemDefStruct();
    void SetOPCItemState(tagOPCITEMSTATE* item_state);
    WORD GetOpcDaQuality();
    QString GetOpcDaQualityAsString();
    std::optional<VARIANT> GetOPCVariantToWrite();

private:
    std::wstring tag_id_wstring_;
    std::wstring buffer_string_;
    VARENUM opc_legacy_type_ = VT_EMPTY;
    tagOPCITEMSTATE last_opc_value_ = {};

    DataType get_type_from_opc_legacy_type_(const unsigned short usType) const;
    QVariant extract_value_from_opc_struct_() const;
};

#endif // DATATAG_OPCDA_H
