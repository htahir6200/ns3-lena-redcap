/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Lluis Parcerisa <lparcerisa@cttc.cat>
 * Modified by:
 *          Danilo Abrignani <danilo.abrignani@unibo.it> (Carrier Aggregation - GSoC 2015)
 *          Biljana Bojovic <biljana.bojovic@cttc.es> (Carrier Aggregation)
 */

#ifndef RRC_HEADER_H
#define RRC_HEADER_H

#include "ns3/header.h"
#include "ns3/lte-asn1-header.h"
#include "ns3/nr-rrc-sap.h"

#include <bitset>
#include <string>

namespace ns3
{

/**
 * \ingroup lte
 */

/**
 * This class extends Asn1Header functions, adding serialization/deserialization
 * of some Information elements defined in 3GPP TS 36.331
 */
class RrcAsn1Header : public Asn1Header
{
  public:
    RrcAsn1Header();
    /**
     * Get message type
     *
     * \returns the message type
     */
    int GetMessageType() const;

  protected:
    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    // Inherited from Asn1Header
    TypeId GetInstanceTypeId() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override = 0;
    void PreSerialize() const override = 0;

    // Auxiliary functions
    /**
     * Convert from bandwidth (in RBs) to ENUMERATED value
     *
     * \param bandwidth Bandwidth in RBs: 6, 15, 25, 50, 75, 100
     * \returns ENUMERATED value: 0, 1, 2, 3, 4, 5
     */
    int BandwidthToEnum(uint16_t bandwidth) const;
    /**
     * Convert from ENUMERATED value to bandwidth (in RBs)
     *
     * \param n ENUMERATED value: 0, 1, 2, 3, 4, 5
     * \returns bandwidth Bandwidth in RBs: 6, 15, 25, 50, 75, 100
     */
    uint16_t EnumToBandwidth(int n) const;

    // Serialization functions
    /**
     * Serialize SRB to add mod list function
     *
     * \param srbToAddModList std::list<NrRrcSap::SrbToAddMod>
     */
    void SerializeSrbToAddModList(std::list<NrRrcSap::SrbToAddMod> srbToAddModList) const;
    /**
     * Serialize DRB to add mod list function
     *
     * \param drbToAddModList std::list<NrRrcSap::SrbToAddMod>
     */
    void SerializeDrbToAddModList(std::list<NrRrcSap::DrbToAddMod> drbToAddModList) const;
    /**
     * Serialize logicala channel config function
     *
     * \param logicalChannelConfig NrRrcSap::LogicalChannelConfig
     */
    void SerializeLogicalChannelConfig(NrRrcSap::LogicalChannelConfig logicalChannelConfig) const;
    /**
     * Serialize radio resource config function
     *
     * \param radioResourceConfigDedicated NrRrcSap::RadioResourceConfigDedicated
     */
    void SerializeRadioResourceConfigDedicated(
        NrRrcSap::RadioResourceConfigDedicated radioResourceConfigDedicated) const;

    void SerializeRadioBearerConfig(
      NrRrcSap::RadioBearerConfig radioBearerConfig) const;
    
    
    void SerializeSpCellConfig(
      NrRrcSap::SpCellConfig spCellConfig) const;

    void SerializeSpCellConfigDedicated(
      NrRrcSap::ServingCellConfig spCellConfigDedicated) const;
    
    void SerializeUplinkConfig(
      NrRrcSap::UplinkConfig uplinkConfig) const;

    void SerializeBwpDlCommon(NrRrcSap::Bwp_DownlinkCommon bwp_DownlinkCommon ) const;

    void SerializeBwpUlCommon(NrRrcSap::Bwp_UplinkCommon bwp_uplinkCommon ) const;

    void SerializeBWP(NrRrcSap::BWP bwp ) const;




    void SerializeBwpInactivityTimer(NrRrcSap::Bwp_InactivityTimer usedValue) const;

    void SerializeSuspendConfig(NrRrcSap::SuspendConfig scfg) const;

    void SerializeSdtConfig(NrRrcSap::SDT_Config_r17 sdtcfg) const;

    /**
     * Serialize physical config dedicated function
     *
     * \param physicalConfigDedicated NrRrcSap::PhysicalConfigDedicated
     */
    void SerializePhysicalConfigDedicated(
        NrRrcSap::PhysicalConfigDedicated physicalConfigDedicated) const;
    /**
     * Serialize physical config dedicated function
     *
     * \param pcdsc NrRrcSap::PhysicalConfigDedicatedSCell
     */
    void SerializePhysicalConfigDedicatedSCell(NrRrcSap::PhysicalConfigDedicatedSCell pcdsc) const;
    /**
     * Serialize system information block type 1 function
     *
     * \param systemInformationBlockType1 NrRrcSap::SystemInformationBlockType1
     */
    void SerializeSystemInformationBlockType1(
        NrRrcSap::SystemInformationBlockType1 systemInformationBlockType1) const;
    /**
     * Serialize system information block type 2 function
     *
     * \param systemInformationBlockType2 NrRrcSap::SystemInformationBlockType2
     */
    void SerializeSystemInformationBlockType2(
        NrRrcSap::SystemInformationBlockType2 systemInformationBlockType2) const;
    /**
     * Serialize system information block type 2 function
     *
     * \param radioResourceConfigCommon NrRrcSap::RadioResourceConfigCommon
     */
    void SerializeRadioResourceConfigCommon(
        NrRrcSap::RadioResourceConfigCommon radioResourceConfigCommon) const;
    /**
     * Serialize radio resource config common SIB function
     *
     * \param radioResourceConfigCommonSib NrRrcSap::RadioResourceConfigCommonSib
     */
    void SerializeRadioResourceConfigCommonSib(
        NrRrcSap::RadioResourceConfigCommonSib radioResourceConfigCommonSib) const;
    /**
     * Serialize measure results function
     *
     * \param measResults NrRrcSap::MeasResults
     */
    void SerializeMeasResults(NrRrcSap::MeasResults measResults) const;
    /**
     * Serialize PLMN identity function
     *
     * \param plmnId the PLMN ID
     */
    void SerializePlmnIdentity(uint32_t plmnId) const;
    /**
     * Serialize RACH config common function
     *
     * \param rachConfigCommon NrRrcSap::RachConfigCommon
     */
    void SerializeRachConfigCommon(NrRrcSap::RachConfigCommon rachConfigCommon) const;
    /**
     * Serialize measure config function
     *
     * \param measConfig NrRrcSap::MeasConfig
     */
    void SerializeMeasConfig(NrRrcSap::MeasConfig measConfig) const;
    /**
     * Serialize non critical extension config function
     *
     * \param nonCriticalExtensionConfiguration NrRrcSap::NonCriticalExtensionConfiguration
     */
    void SerializeNonCriticalExtensionConfiguration(
        NrRrcSap::NonCriticalExtensionConfiguration nonCriticalExtensionConfiguration) const;
    /**
     * Serialize radio resource config common SCell function
     *
     * \param rrccsc NrRrcSap::RadioResourceConfigCommonSCell
     */
    void SerializeRadioResourceConfigCommonSCell(
        NrRrcSap::RadioResourceConfigCommonSCell rrccsc) const;
    /**
     * Serialize radio resource dedicated SCell function
     *
     * \param rrcdsc NrRrcSap::RadioResourceConfigDedicatedSCell
     */
    void SerializeRadioResourceDedicatedSCell(
        NrRrcSap::RadioResourceConfigDedicatedSCell rrcdsc) const;
    /**
     * Serialize Q offset range function
     *
     * \param qOffsetRange q offset range
     */
    void SerializeQoffsetRange(int8_t qOffsetRange) const;
        
    
    /**
     * Serialize threshold eutra function
     *
     * \param thresholdEutra NrRrcSap::ThresholdEutra
     */
    void SerializeThresholdEutra(NrRrcSap::ThresholdEutra thresholdEutra) const;

    // Deserialization functions
    /**
     * Deserialize DRB to add mod list function
     *
     * \param drbToAddModLis std::list<NrRrcSap::DrbToAddMod> *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeDrbToAddModList(std::list<NrRrcSap::DrbToAddMod>* drbToAddModLis,
                                                Buffer::Iterator bIterator);
    /**
     * Deserialize SRB to add mod list function
     *
     * \param srbToAddModList std::list<NrRrcSap::SrbToAddMod> *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeSrbToAddModList(std::list<NrRrcSap::SrbToAddMod>* srbToAddModList,
                                                Buffer::Iterator bIterator);
    /**
     * Deserialize logical channel config function
     *
     * \param logicalChannelConfig NrRrcSap::LogicalChannelConfig *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeLogicalChannelConfig(
        NrRrcSap::LogicalChannelConfig* logicalChannelConfig,
        Buffer::Iterator bIterator);
    /**
     * Deserialize radio resource config dedicated function
     *
     * \param radioResourceConfigDedicated NrRrcSap::RadioResourceConfigDedicated *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRadioResourceConfigDedicated(
        NrRrcSap::RadioResourceConfigDedicated* radioResourceConfigDedicated,
        Buffer::Iterator bIterator);

    Buffer::Iterator DeserializeRadioBearerConfig(
      NrRrcSap::RadioBearerConfig* radioBearerConfig,
      Buffer::Iterator bIterator);    

    Buffer::Iterator DeserializeSpCellConfig(
        NrRrcSap::SpCellConfig* spCellConfig ,
        Buffer::Iterator bIterator);

    Buffer::Iterator DeserializeSpCellConfigDedicated(
      NrRrcSap::ServingCellConfig* spCellConfigDedicated,
      Buffer::Iterator bIterator);

    Buffer::Iterator DeserializeUplinkConfig(
      NrRrcSap::UplinkConfig* uplinkConfig,
      Buffer::Iterator bIterator);    

    Buffer::Iterator DeserializeBwpDlCommon(
      NrRrcSap::Bwp_DownlinkCommon* bwp_DownlinkCommon,
      Buffer::Iterator bIterator); 
    
      Buffer::Iterator DeserializeBwpUlCommon(
      NrRrcSap::Bwp_UplinkCommon* bwp_UplinkCommon,
      Buffer::Iterator bIterator); 
      
      Buffer::Iterator DeserializeBWP(
      NrRrcSap::BWP* bwp,
      Buffer::Iterator bIterator); 

      Buffer::Iterator DeserializeUlBwpToAddModList(
      std::list<NrRrcSap::BWP_Uplink>* bwp_Uplink,
      Buffer::Iterator bIterator); 

      Buffer::Iterator DeserializeDlBwpToAddModList(
      std::list<NrRrcSap::BWP_Downlink>* bwp_Downlink,
      Buffer::Iterator bIterator); 

      Buffer::Iterator DeserializeSuspendConfig(
      NrRrcSap::SuspendConfig* scfg,
      Buffer::Iterator bIterator); 
      

        
    /**
     * Deserialize physical config dedicated function
     *
     * \param physicalConfigDedicated NrRrcSap::PhysicalConfigDedicated *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializePhysicalConfigDedicated(
        NrRrcSap::PhysicalConfigDedicated* physicalConfigDedicated,
        Buffer::Iterator bIterator);
    /**
     * Deserialize system information block type 1 function
     *
     * \param systemInformationBlockType1 NrRrcSap::SystemInformationBlockType1 *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */

    Buffer::Iterator DeserializeSdtConfig(NrRrcSap::SDT_Config_r17* sdtcfg, 
      Buffer::Iterator bIterator);

    Buffer::Iterator DeserializeSystemInformationBlockType1(
        NrRrcSap::SystemInformationBlockType1* systemInformationBlockType1,
        Buffer::Iterator bIterator);
    /**   
     * Deserialize system information block type 2 function
     *
     * \param systemInformationBlockType2 NrRrcSap::SystemInformationBlockType2 *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeSystemInformationBlockType2(
        NrRrcSap::SystemInformationBlockType2* systemInformationBlockType2,
        Buffer::Iterator bIterator);
    /**
     * Deserialize radio resource config common function
     *
     * \param radioResourceConfigCommon NrRrcSap::RadioResourceConfigCommon *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRadioResourceConfigCommon(
        NrRrcSap::RadioResourceConfigCommon* radioResourceConfigCommon,
        Buffer::Iterator bIterator);
    /**
     * Deserialize radio resource config common SIB function
     *
     * \param radioResourceConfigCommonSib NrRrcSap::RadioResourceConfigCommonSib *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRadioResourceConfigCommonSib(
        NrRrcSap::RadioResourceConfigCommonSib* radioResourceConfigCommonSib,
        Buffer::Iterator bIterator);
    /**
     * Deserialize measure results function
     *
     * \param measResults NrRrcSap::MeasResults *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeMeasResults(NrRrcSap::MeasResults* measResults,
                                            Buffer::Iterator bIterator);
    /**
     * Deserialize PLMN identity function
     *
     * \param plmnId the PLMN ID
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializePlmnIdentity(uint32_t* plmnId, Buffer::Iterator bIterator);
    /**
     * Deserialize RACH config common function
     *
     * \param rachConfigCommon NrRrcSap::RachConfigCommon *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRachConfigCommon(NrRrcSap::RachConfigCommon* rachConfigCommon,
                                                 Buffer::Iterator bIterator);
    /**
     * Deserialize measure config function
     *
     * \param measConfig NrRrcSap::MeasConfig *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeMeasConfig(NrRrcSap::MeasConfig* measConfig,
                                           Buffer::Iterator bIterator);
    /**
     * Deserialize Qoffset range function
     *
     * \param qOffsetRange Qoffset range
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeQoffsetRange(int8_t* qOffsetRange, Buffer::Iterator bIterator);
    
    Buffer::Iterator DeserializeBwpInactivityTimer(NrRrcSap::Bwp_InactivityTimer* usedValue, Buffer::Iterator bIterator);
    
    
    /**
     * Deserialize threshold eutra function
     *
     * \param thresholdEutra NrRrcSap::ThresholdEutra *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeThresholdEutra(NrRrcSap::ThresholdEutra* thresholdEutra,
                                               Buffer::Iterator bIterator);
    /**
     * Deserialize non critical extension config function
     *
     * \param nonCriticalExtension NrRrcSap::NonCriticalExtensionConfiguration *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeNonCriticalExtensionConfig(
        NrRrcSap::NonCriticalExtensionConfiguration* nonCriticalExtension,
        Buffer::Iterator bIterator);
    /**
     * Deserialize cell identification function
     *
     * \param ci NrRrcSap::CellIdentification *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeCellIdentification(NrRrcSap::CellIdentification* ci,
                                                   Buffer::Iterator bIterator);
    /**
     * Deserialize radio resource config common SCell function
     *
     * \param rrccsc NrRrcSap::RadioResourceConfigCommonSCell *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRadioResourceConfigCommonSCell(
        NrRrcSap::RadioResourceConfigCommonSCell* rrccsc,
        Buffer::Iterator bIterator);
    /**
     * Deserialize radio resource config dedicated SCell function
     *
     * \param rrcdsc NrRrcSap::RadioResourceConfigDedicatedSCell *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeRadioResourceConfigDedicatedSCell(
        NrRrcSap::RadioResourceConfigDedicatedSCell* rrcdsc,
        Buffer::Iterator bIterator);
    /**
     * Deserialize physical config dedicated SCell function
     *
     * \param pcdsc NrRrcSap::PhysicalConfigDedicatedSCell *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializePhysicalConfigDedicatedSCell(
        NrRrcSap::PhysicalConfigDedicatedSCell* pcdsc,
        Buffer::Iterator bIterator);

    /**
     * This function prints the object, for debugging purposes.
     * @param os The output stream to use (i.e. std::cout)
     */
    void Print(std::ostream& os) const override;
    /**
     * This function prints RadioResourceConfigDedicated IE, for debugging purposes.
     * @param os The output stream to use (i.e. std::cout)
     * @param radioResourceConfigDedicated The information element to be printed
     */
    void Print(std::ostream& os,
               NrRrcSap::RadioResourceConfigDedicated radioResourceConfigDedicated) const;

    /// Stores RRC message type, according to 3GPP TS 36.331
    int m_messageType;
};

/**
 * This class only serves to discriminate which message type has been received
 * in uplink (ue to eNb) for channel DCCH
 */
class RrcUlDcchMessage : public RrcAsn1Header
{
  public:
    RrcUlDcchMessage();
    ~RrcUlDcchMessage() override;

    // Inherited from RrcAsn1Header
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;
    void PreSerialize() const override;

  protected:
    /**
     * Serialize UL DCCH message function
     *
     * \param msgType message type
     */
    void SerializeUlDcchMessage(int msgType) const;
    /**
     * Deserialize UL DCCH message function
     *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeUlDcchMessage(Buffer::Iterator bIterator);
};

/**
 * This class only serves to discriminate which message type has been received
 * in downlink (eNb to ue) for channel DCCH
 */
class RrcDlDcchMessage : public RrcAsn1Header
{
  public:
    RrcDlDcchMessage();
    ~RrcDlDcchMessage() override;

    // Inherited from RrcAsn1Header
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;
    void PreSerialize() const override;

  protected:
    /**
     * Serialize DL DCCH message function
     *
     * \param msgType message type
     */
    void SerializeDlDcchMessage(int msgType) const;
    /**
     * Deserialize DL DCCH message function
     *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeDlDcchMessage(Buffer::Iterator bIterator);
};

/**
 * This class only serves to discriminate which message type has been received
 * in uplink (ue to eNb) for channel CCCH
 */
class RrcUlCcchMessage : public RrcAsn1Header
{
  public:
    RrcUlCcchMessage();
    ~RrcUlCcchMessage() override;

    // Inherited from RrcAsn1Header
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;
    void PreSerialize() const override;

  protected:
    /**
     * Serialize UL CCCH message function
     *
     * \param msgType message type
     */
    void SerializeUlCcchMessage(int msgType) const;
    /**
     * Deserialize DL CCCH message function
     *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeUlCcchMessage(Buffer::Iterator bIterator);
};

/**
 * This class only serves to discriminate which message type has been received
 * in downlink (eNb to ue) for channel CCCH
 */
class RrcDlCcchMessage : public RrcAsn1Header
{
  public:
    RrcDlCcchMessage();
    ~RrcDlCcchMessage() override;

    // Inherited from RrcAsn1Header
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;
    void PreSerialize() const override;

  protected:
    /**
     * Serialize DL CCCH message function
     *
     * \param msgType message type
     */
    void SerializeDlCcchMessage(int msgType) const;
    /**
     * Deserialize DL CCCH message function
     *
     * \param bIterator buffer iterator
     * \returns buffer iterator
     */
    Buffer::Iterator DeserializeDlCcchMessage(Buffer::Iterator bIterator);
};

/**
 * This class manages the serialization/deserialization of NrRrcConnectionRequest IE
 */
class RrcConnectionRequestHeader : public RrcUlCcchMessage
{
  public:
    RrcConnectionRequestHeader();
    ~RrcConnectionRequestHeader() override;

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId();
    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a NrRrcConnectionRequest IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::NrRrcConnectionRequest msg);

    /**
     * Returns a NrRrcConnectionRequest IE from the values in the class attributes
     * @return A NrRrcConnectionRequest, as defined in NrRrcSap
     */
    NrRrcSap::NrRrcConnectionRequest GetMessage() const;

    /**
     * Get MMEC attribute
     * @return m_mmec attribute
     */
    std::bitset<8> GetMmec() const;

    /**
     * Get M-TMSI attribute
     * @return m_tmsi attribute
     */
    std::bitset<32> GetMtmsi() const;

  private:
    std::bitset<8> m_mmec;   ///< MMEC
    std::bitset<32> m_mTmsi; ///< TMSI
    bool m_redCap;

    /// EstablishmentCause enumeration
    enum
    {
        EMERGENCY = 0,
        HIGHPRIORITYACCESS,
        MT_ACCESS,
        MO_SIGNALLING,
        MO_DATA,
        SPARE3,
        SPARE2,
        SPARE1
    } m_establishmentCause; ///< the establishent cause

    std::bitset<1> m_spare; ///< spare bit
};

/**
 * This class manages the serialization/deserialization of RrcSetup IE
 */
class RrcSetupHeader : public RrcDlCcchMessage
{
  public:
    RrcSetupHeader();
    ~RrcSetupHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcSetup IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcSetup msg);

    /**
     * Returns a RrcSetup IE from the values in the class attributes
     * @return A RrcSetup, as defined in NrRrcSap
     */
    NrRrcSap::RrcSetup GetMessage() const;

    /**
     * Getter for m_rrcTransactionIdentifier
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

    /**
     * Getter for m_radioResourceConfigDedicated
     * @return m_radioResourceConfigDedicated
     */
    NrRrcSap::RadioResourceConfigDedicated GetRadioResourceConfigDedicated() const;

    /**
     * Gets m_radioResourceConfigDedicated.havePhysicalConfigDedicated
     * @return m_radioResourceConfigDedicated.havePhysicalConfigDedicated
     */
    bool HavePhysicalConfigDedicated() const;

    /**
     * Gets m_radioResourceConfigDedicated.physicalConfigDedicated
     * @return m_radioResourceConfigDedicated.physicalConfigDedicated
     */
    NrRrcSap::PhysicalConfigDedicated GetPhysicalConfigDedicated() const;

    /**
     * Gets m_radioResourceConfigDedicated.srbToAddModList
     * @return m_radioResourceConfigDedicated.srbToAddModList
     */
    std::list<NrRrcSap::SrbToAddMod> GetSrbToAddModList() const;

    /**
     * Gets m_radioResourceConfigDedicated.drbToAddModList
     * @return m_radioResourceConfigDedicated.drbToAddModList
     */
    std::list<NrRrcSap::DrbToAddMod> GetDrbToAddModList() const;

    /**
     * Gets m_radioResourceConfigDedicated.drbToReleaseList
     * @return m_radioResourceConfigDedicated.drbToReleaseList
     */
    std::list<uint8_t> GetDrbToReleaseList() const;





  private:
    uint8_t m_rrcTransactionIdentifier; ///< RRC transaction identifier
    mutable NrRrcSap::RadioResourceConfigDedicated
        m_radioResourceConfigDedicated; ///< radio resource config dedicated
    mutable NrRrcSap::SpCellConfig
        m_spCellConfig;
};

/**
 * This class manages the serialization/deserialization of RrcConnectionSetupComplete IE
 */
class RrcConnectionSetupCompleteHeader : public RrcUlDcchMessage
{
  public:
    RrcConnectionSetupCompleteHeader();
    ~RrcConnectionSetupCompleteHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionSetupCompleted IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionSetupCompleted msg);

    /**
     * Returns a RrcConnectionSetupCompleted IE from the values in the class attributes
     * @return A RrcConnectionSetupCompleted, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionSetupCompleted GetMessage() const;

    /**
     * Getter for m_rrcTransactionIdentifier
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

  private:
    uint8_t m_rrcTransactionIdentifier; ///< RRC transaction identifier
};

/**
 * This class manages the serialization/deserialization of RrcConnectionSetupComplete IE
 */
class RrcConnectionReconfigurationCompleteHeader : public RrcUlDcchMessage
{
  public:
    RrcConnectionReconfigurationCompleteHeader();
    ~RrcConnectionReconfigurationCompleteHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReconfigurationCompleted IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReconfigurationCompleted msg);

    /**
     * Returns a RrcConnectionReconfigurationCompleted IE from the values in the class attributes
     * @return A RrcConnectionReconfigurationCompleted, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReconfigurationCompleted GetMessage() const;

    /**
     * Getter for m_rrcTransactionIdentifier
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

  private:
    uint8_t m_rrcTransactionIdentifier; ///< RRC transaction identifier
};

/**
 * This class manages the serialization/deserialization of RrcReconfiguration IE
 */
class RrcReconfigurationHeader : public RrcDlDcchMessage
{
  public:
    RrcReconfigurationHeader();
    ~RrcReconfigurationHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcReconfiguration IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcReconfiguration msg);

    /**
     * Returns a RrcReconfiguration IE from the values in the class attributes
     * @return A RrcReconfiguration, as defined in NrRrcSap
     */
    NrRrcSap::RrcReconfiguration GetMessage() const;

    /**
     * Getter for m_haveRadioBearerConfig
     * @return m_haveRadioBearerConfig
     */
    bool GetHaveRadioBearerConfig() const;

    /**
     * Getter for m_radioBearerConfig
     * @return m_radioBearerConfig
     */
    NrRrcSap::RadioBearerConfig GetRadioBearerConfig();

    /**
     * Getter for m_rrcTransactionIdentifier
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

    /**
     * Gets m_radioResourceConfigDedicated.srbToAddModList
     * @return m_radioResourceConfigDedicated.srbToAddModList
     */
    std::list<NrRrcSap::SrbToAddMod> GetSrbToAddModList() const;

    /**
     * Gets m_radioResourceConfigDedicated.drbToAddModList
     * @return m_radioResourceConfigDedicated.drbToAddModList
     */
    std::list<NrRrcSap::DrbToAddMod> GetDrbToAddModList() const;

    /**
     * Gets m_radioResourceConfigDedicated.drbToReleaseList
     * @return m_radioResourceConfigDedicated.drbToReleaseList
     */
    std::list<uint8_t> GetDrbToReleaseList() const;

  private:
    uint8_t m_rrcTransactionIdentifier;                   ///< RRC transaction identifier
    bool m_haveRadioBearerConfig;              ///< have radio resource berer config?
    NrRrcSap::RadioBearerConfig
        m_radioBearerConfig; ///< the radio resource bearer config 

};

/**
 * This class manages the serialization/deserialization of HandoverPreparationInfo IE
 */
class HandoverPreparationInfoHeader : public RrcAsn1Header
{
  public:
    HandoverPreparationInfoHeader();

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a HandoverPreparationInfo IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::HandoverPreparationInfo msg);

    /**
     * Returns a HandoverPreparationInfo IE from the values in the class attributes
     * @return A HandoverPreparationInfo, as defined in NrRrcSap
     */
    NrRrcSap::HandoverPreparationInfo GetMessage() const;

    /**
     * Getter for m_asConfig
     * @return m_asConfig
     */
    NrRrcSap::AsConfig GetAsConfig() const;

  private:
    NrRrcSap::AsConfig m_asConfig; ///< AS config
};

/**
 * This class manages the serialization/deserialization of RRCResumeRequest IE
 */
class RrcResumeRequestHeader : public RrcUlCcchMessage
{
  public:
    RrcResumeRequestHeader();
    ~RrcResumeRequestHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcResumeRequestHeader IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcResumeRequest msg);

    /**
     * Returns a RrcResumeRequestHeader IE from the values in the class attributes
     * @return A RrcResumeRequestHeader, as defined in NrRrcSap
     */
    NrRrcSap::RrcResumeRequest GetMessage() const;

    /**
     * Getter for m_resumeIdentity
     * @return m_resumeIdentity
     */
    uint16_t  GetResumeIdentity() const;

    /**
     * Getter for m_resumeCause
     * @return m_resumeCause
     */
    NrRrcSap::ResumeCause GetResumeCause() const;

  private:
    uint8_t m_rrcTransactionIdentifier;
    uint16_t m_resumeIdentity;              ///< resumeIdentity
    NrRrcSap::ResumeCause m_resumeCause; ///< resume cause
};

/**
 * This class manages the serialization/deserialization of RRCResume IE
 */
class RrcResumeHeader : public RrcDlDcchMessage
{
  public:
    RrcResumeHeader();
    ~RrcResumeHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcResumeHeader IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcResume msg);

    /**
     * Returns a RrcResumeHeader IE from the values in the class attributes
     * @return A RrcResumeHeader, as defined in NrRrcSap
     */
    NrRrcSap::RrcResume GetMessage() const;


  private:
    uint8_t m_rrcTransactionIdentifier;
    NrRrcSap::RRCResume_IEs m_rrcResume;
    
};

class RrcResumeCompleteHeader : public RrcUlDcchMessage
{
  public:
    RrcResumeCompleteHeader();
    ~RrcResumeCompleteHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcResumeCompleteHeader IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcResumeComplete msg);

    /**
     * Returns a RrcResumeCompleteHeader IE from the values in the class attributes
     * @return A RrcResumeCompleteHeader, as defined in NrRrcSap
     */
    NrRrcSap::RrcResumeComplete GetMessage() const;


  private:
    uint8_t m_rrcTransactionIdentifier;
    NrRrcSap::RRCResumeComplete_IEs m_rrcResumeComplete;
};




/**
 * This class manages the serialization/deserialization of RRCConnectionReestablishmentRequest IE
 */
class RrcConnectionReestablishmentRequestHeader : public RrcUlCcchMessage
{
  public:
    RrcConnectionReestablishmentRequestHeader();
    ~RrcConnectionReestablishmentRequestHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReestablishmentRequest IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReestablishmentRequest msg);

    /**
     * Returns a RrcConnectionReestablishmentRequest IE from the values in the class attributes
     * @return A RrcConnectionReestablishmentRequest, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReestablishmentRequest GetMessage() const;

    /**
     * Getter for m_ueIdentity
     * @return m_ueIdentity
     */
    NrRrcSap::ReestabUeIdentity GetUeIdentity() const;

    /**
     * Getter for m_reestablishmentCause
     * @return m_reestablishmentCause
     */
    NrRrcSap::ReestablishmentCause GetReestablishmentCause() const;

  private:
    NrRrcSap::ReestabUeIdentity m_ueIdentity;              ///< UE identity
    NrRrcSap::ReestablishmentCause m_reestablishmentCause; ///< reestablishment cause
};

/**
 * This class manages the serialization/deserialization of RrcConnectionReestablishment IE
 */
class RrcConnectionReestablishmentHeader : public RrcDlCcchMessage
{
  public:
    RrcConnectionReestablishmentHeader();
    ~RrcConnectionReestablishmentHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReestablishment IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReestablishment msg);

    /**
     * Returns a RrcConnectionReestablishment IE from the values in the class attributes
     * @return A RrcConnectionReestablishment, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReestablishment GetMessage() const;

    /**
     * Getter for m_rrcTransactionIdentifier attribute
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

    /**
     * Getter for m_radioResourceConfigDedicated attribute
     * @return m_radioResourceConfigDedicated
     */
    NrRrcSap::RadioResourceConfigDedicated GetRadioResourceConfigDedicated() const;

  private:
    uint8_t m_rrcTransactionIdentifier; ///< RRC transaction identifier
    NrRrcSap::RadioResourceConfigDedicated
        m_radioResourceConfigDedicated; ///< radio resource config dedicated
};

/**
 * This class manages the serialization/deserialization of RrcConnectionReestablishmentComplete IE
 */
class RrcConnectionReestablishmentCompleteHeader : public RrcUlDcchMessage
{
  public:
    RrcConnectionReestablishmentCompleteHeader();

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReestablishmentComplete IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReestablishmentComplete msg);

    /**
     * Returns a RrcConnectionReestablishmentComplete IE from the values in the class attributes
     * @return A RrcConnectionReestablishmentComplete, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReestablishmentComplete GetMessage() const;

    /**
     * Getter for m_rrcTransactionIdentifier attribute
     * @return m_rrcTransactionIdentifier
     */
    uint8_t GetRrcTransactionIdentifier() const;

  private:
    uint8_t m_rrcTransactionIdentifier; ///< RRC transaction identifier
};

/**
 * This class manages the serialization/deserialization of RrcConnectionReestablishmentReject IE
 */
class RrcConnectionReestablishmentRejectHeader : public RrcDlCcchMessage
{
  public:
    RrcConnectionReestablishmentRejectHeader();
    ~RrcConnectionReestablishmentRejectHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReestablishmentReject IE and stores the contents into the class
     * attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReestablishmentReject msg);

    /**
     * Returns a RrcConnectionReestablishmentReject IE from the values in the class attributes
     * @return A RrcConnectionReestablishmentReject, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReestablishmentReject GetMessage() const;

  private:
    NrRrcSap::RrcConnectionReestablishmentReject
        m_rrcConnectionReestablishmentReject; ///< RRC connection reestablishmnet reject
};

/**
 * This class manages the serialization/deserialization of RrcRelease IE
 */
class RrcReleaseHeader : public RrcDlDcchMessage
{
  public:
    RrcReleaseHeader();
    ~RrcReleaseHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcRelease IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcRelease msg);

    /**
     * Returns a RrcRelease IE from the values in the class attributes
     * @return A RrcRelease, as defined in NrRrcSap
     */
    NrRrcSap::RrcRelease GetMessage() const;

  private:
    uint8_t m_rrcTransactionIdentifier;
    bool m_hasSuspendConfig{false};
    NrRrcSap::RRCRelease_IEs m_rrcRelease; ///< RRC connection release
};

/**
 * This class manages the serialization/deserialization of RrcConnectionReject IE
 */
class RrcConnectionRejectHeader : public RrcDlCcchMessage
{
  public:
    RrcConnectionRejectHeader();
    ~RrcConnectionRejectHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a RrcConnectionReject IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::RrcConnectionReject msg);

    /**
     * Returns a RrcConnectionReject IE from the values in the class attributes
     * @return A RrcConnectionReject, as defined in NrRrcSap
     */
    NrRrcSap::RrcConnectionReject GetMessage() const;

  private:
    NrRrcSap::RrcConnectionReject m_rrcConnectionReject; ///< RRC connection reject
};

/**
 * This class manages the serialization/deserialization of MeasurementReport IE
 */
class MeasurementReportHeader : public RrcUlDcchMessage
{
  public:
    MeasurementReportHeader();
    ~MeasurementReportHeader() override;

    // Inherited from RrcAsn1Header
    void PreSerialize() const override;
    uint32_t Deserialize(Buffer::Iterator bIterator) override;
    void Print(std::ostream& os) const override;

    /**
     * Receives a MeasurementReport IE and stores the contents into the class attributes
     * @param msg The information element to parse
     */
    void SetMessage(NrRrcSap::MeasurementReport msg);

    /**
     * Returns a MeasurementReport IE from the values in the class attributes
     * @return A MeasurementReport, as defined in NrRrcSap
     */
    NrRrcSap::MeasurementReport GetMessage() const;

  private:
    NrRrcSap::MeasurementReport m_measurementReport; ///< measurement report
};

} // namespace ns3

#endif // RRC_HEADER_H
