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
 * Authors: Nicola Baldo <nbaldo@cttc.es>
 *          Lluis Parcerisa <lparcerisa@cttc.cat>
 */

#ifndef NR_RRC_SAP_H
#define NR_RRC_SAP_H

#include <ns3/ptr.h>
#include <ns3/simulator.h>

#include <list>
#include <stdint.h>
#include <ns3/lte-rrc-sap.h>
#include "ns3/packet.h"

namespace ns3
{

class LteRlcSapUser;
class LtePdcpSapUser;
class LteRlcSapProvider;
class LtePdcpSapProvider;


/**
 * \ingroup lte
 *
 * \brief Class holding definition common to all UE/eNodeB SAP Users/Providers.
 *
 * See 3GPP TS 36.331 for reference.
 *
 * Note that only those values that are (expected to be) used by the
 * ns-3 model are mentioned here. The naming of the variables that are
 * defined here is the same of 36.331, except for removal of "-" and
 * conversion to CamelCase or ALL_CAPS where needed in order to follow
 * the ns-3 coding style. Due to the 1-to-1 mapping with TS 36.331,
 * detailed doxygen documentation is omitted, so please refer to
 * 36.331 for the meaning of these data structures / fields.
 */
class NrRrcSap : public LteRrcSap
{
  public:
    virtual ~NrRrcSap();
    enum Bwp_InactivityTimer
    {
        ms2, ms3, ms4, ms5, ms6, ms8, ms10, ms20,
        ms30, ms40,ms50, ms60, ms80, ms100, ms200,
        ms300, ms500, ms750, ms1280, ms1920, ms2560,
        spare10, spare9, spare8, spare7, spare6,
        spare5, spare4, spare3, spare2, spare1 
    };

    enum Pagingcycle
    {
        rf32, rf64, rf128, rf256
    };

    enum ExtendedPagingCycle_r17
    {
        e_rf256, e_rf512, e_rf1024, e_spare1
    };

    enum PeriodicRNAU_TimerValue
    {
        min5, min10, min20, min30, min60, min120, min360, min720
    };

    enum ResumeCause
    {
        emergency, highPriorityAccess, mt_Access, mo_Signalling,
        mo_Data, mo_VoiceCall, mo_VideoCall, mo_SMS, rna_Update, mps_PriorityAccess,
        mcs_PriorityAccess
    };


    struct BWP
    {
        uint16_t locationAndBandwidth;  //INTEGER (0..37949)
        //subcarrierSpacing          
        //cyclicPrefix                 
    };

    struct Bwp_UplinkCommon
    {
        BWP genericParameters;

        // rach-ConfigCommon  

        // pusch-ConfigCommon                    

        // pucch-ConfigCommon    
    };

    struct BWP_Uplink
    {
        uint8_t bwp_Id;             

        Bwp_UplinkCommon bwp_UplinkCommon;      
        //bwp-Dedicated       
    };

    struct Bwp_DownlinkCommon
    {
        BWP genericParameters;
        //pdcch-ConfigCommon  
        //pdsch-ConfigCommon  
    };

    struct BWP_Downlink
    {
        uint8_t bwp_Id;
        Bwp_DownlinkCommon bwp_dlCommon;
        //Bwp_Dedicated bwp_Dedicated;

    };


    struct UplinkConfig
    {
        //initialUplinkBWP 
        std::list<uint8_t> uplinkBWP_ToReleaseList;   
        std::list<BWP_Uplink> uplinkBWP_ToAddModList; 
        uint8_t firstActiveUplinkBWP_Id;
        //pusch-ServingCellConfig SetupRelease 
        //carrierSwitching SetupRelease 

    };

    //ETSI TS 38.331 version 17.0.0 Release 17 795 ETSI TS 138 331 V17.0.0 (2022-05)
    struct ServingCellConfig
    {
        //tdd-UL-DL-ConfigurationDedicated    
        //initialDownlinkBWP                  
        std::list<uint8_t>downlinkBWP_ToReleaseList;           
        std::list<BWP_Downlink>downlinkBWP_ToAddModList;   
        uint8_t firstActiveDownlinkBWP_Id;     
        enum Bwp_InactivityTimer bwp_InactivityTimer;      
        //defaultDownlinkBWP-Id       BWP-Id      OPTIONAL,   -- Need M
        UplinkConfig uplinkConfig;
        //supplementaryUplink        
        //pdcch-ServingCellConfig SetupRelease 
        //pdsch-ServingCellConfig     
        //csi-MeasConfig              
        //carrierSwitching            
        //sCellDeactivationTimer     
        //crossCarrierSchedulingConfig    
        //tag-Id                        
        //dummy1 
        //pathlossReferenceLinking       
        //servingCellMO MeasObjectId 
    };

    struct CellGroupConfig
    {
        uint8_t cellGroupId; //maxSecondaryCellGroups = 3
        //TODO!

    };


    struct SpCellConfig
    {
        ServingCellConfig spCellConfigDedicated;
    };

    
    struct SDT_Config_r17
    {
        std::list<uint8_t> sdt_DRB_List_r17; //SIZE (0..maxDRB)) ,32
        //sdt-SRB2-Indication-r17 ENUMERATED {allowed} 
        //sdt-MAC-PHY-CG-Config-r17      SetupRelease {SDT-CG-Config-r17} OPTIONAL, -- Need M

        //sdt-DRB-ContinueROHC-r17       ENUMERATED { cell, rna } OPTIONAL -- Need S
    };



    struct SuspendConfig{

        uint16_t fullRnti;
        uint16_t shortRnti;
        enum Pagingcycle ran_PagingCycle;
        //ran-NotificationAreaInfo RAN-NotificationAreaInfo OPTIONAL, -- Need M
        enum PeriodicRNAU_TimerValue t380;
        //nextHopChainingCount NextHopChainingCount,
        //sl-ServingCellInfo-r17 SL-ServingCellInfo-r17 OPTIONAL, -- Cond L2RemoteUE
        SDT_Config_r17 sdt_Config_r17;
        //srs-PosRRC-InactiveConfig-r17 SRS-PosRRC-InactiveConfig-r17
        enum ExtendedPagingCycle_r17 ran_ExtendedPagingCycle_r17;
    
    };

    struct RadioBearerConfig{
        std::list<SrbToAddMod> srbToAddModList;          ///< SRB to add mod list
        //bool srb3_ToRelease{true};
        std::list<DrbToAddMod> drbToAddModList;          ///< DRB to add mod list
        std::list<uint8_t> drbToReleaseList;             ///< DRB to release list
        //SecurityConfig securityConfig;
        
        // mrb-ToAddModList-r17 MRB-ToAddModList-r17 OPTIONAL, -- Need N
        // mrb-ToReleaseList-r17 MRB-ToReleaseList-r17 OPTIONAL, -- Need N
        // srb-ToAddModListExt-r17 SRB-ToAddModListExt-r17 OPTIONAL, -- Need N
        // srb4-ToRelease-r17 ENUMERATED{true} OPTIONAL -- Need N
        
    };

    struct RRCResume_IEs{
        bool haveRadioBearerConfig;
        RadioBearerConfig radioBearerConfig;
        //masterCellGroup
        SpCellConfig spCellconfig; //part of CellGroupConfig


        //MeasConfig measConfig;
        //fullConfig ENUMERATED {true} OPTIONAL, -- Need N
        //lateNonCriticalExtension OCTET STRING OPTIONAL,
        //nonCriticalExtension RRCResume-v1560-IEs OPTIONAL
    };

    struct RRCReconfiguration_IEs
    {
        bool haveRadioBearerConfig;
        RadioBearerConfig radioBearerConfig;
        //secondaryCellGroup OCTET STRING (CONTAINING CellGroupConfig) OPTIONAL, -- Cond SCG
        //measConfig MeasConfig OPTIONAL, -- Need M
        //lateNonCriticalExtension OCTET STRING OPTIONAL,
        //nonCriticalExtension RRCReconfiguration-v1530-IEs 
    };

    struct RRCResumeRequest_IEs{
        uint16_t resumeIdentity; //24 bits
        //resumeMAC-I BIT STRING //16 bits
        enum ResumeCause resumeCause;
        // spare BIT STRING //1 bit
    };


    struct RRCResumeComplete_IEs{
        //dedicatedNAS-Message DedicatedNAS-Message OPTIONAL,
        //selectedPLMN-Identity INTEGER (1..maxPLMN) OPTIONAL,
        //uplinkTxDirectCurrentList UplinkTxDirectCurrentList OPTIONAL,
        //lateNonCriticalExtension OCTET STRING OPTIONAL,
        //nonCriticalExtension RRCResumeComplete-v1610-IEs 

    };

    struct RRCSetup_IEs{
        //using LTE setupConfig for easier usage. 

        //RadioBearerConfig radioBearerConfig;
        //masterCellGroup
        //lateNonCriticalExtension OCTET STRING OPTIONAL,
        //nonCriticalExtension RRCResume-v1560-IEs OPTIONA



    };

    struct RRCRelease_IEs{
        bool hasSuspendConfig;
        
        //redirectedCarrierInfo
        //cellReselectionPriorities 
        SuspendConfig suspendConfig;
        // deprioritisationReq SEQUENCE {
        //     deprioritisationType ENUMERATED {frequency, nr},
        //     deprioritisationTimer ENUMERATED {min5, min10, min15, min30}
        //lateNonCriticalExtension 
        //nonCriticalExtension 
    };
 


    //messages

    /// NrRrcConnectionRequest structure
    struct NrRrcConnectionRequest
    {
        uint64_t ueIdentity; ///< UE identity
        bool redcap{false}; 
    };


    /// RrcSetup structure
    struct RrcSetup
    {
        uint8_t rrcTransactionIdentifier; ///< RRC transaction identifier
        //RRCSetup_IEs rrcSetup; //not using RRCSetup_IEs yet

        RadioResourceConfigDedicated
            radioResourceConfigDedicated; ///< radio resource config dedicated
        SpCellConfig spCellconfig;

    };

    /// RrcRelease structure
    struct RrcRelease
    {
        uint8_t rrcTransactionIdentifier; ///< RRC transaction identifie
        RRCRelease_IEs rrcRelease;
    };

     /// RrcReconfiguration structure
    struct RrcReconfiguration
    {
        uint8_t rrcTransactionIdentifier; ///< RRC transaction identifie
        RRCReconfiguration_IEs rrcReconfiguration;
        
    };

      /// RrcResumeRequest structure
    struct RrcResumeRequest
    {
        uint8_t rrcTransactionIdentifier; ///< RRC transaction identifie
        RRCResumeRequest_IEs rrcResumeRequest;
        std::vector<Ptr<Packet>> sdtData; // Information element to receive the data at the gNB easily. Not in the standard
    };

    

     /// RrcResume structure
    struct RrcResume
    {
        uint8_t rrcTransactionIdentifier; ///< RRC transaction identifie
        RRCResume_IEs rrcResume;
        
    };

    /// RrcResumeComplete
    struct RrcResumeComplete
    {
        uint8_t rrcTransactionIdentifier;
        RRCResumeComplete_IEs rrcResumeComplete;
    };
 
};
   
/**
 * \brief Part of the RRC protocol. This Service Access Point (SAP) is used by
 *        the UE RRC to send messages to the eNB. Each method defined in this
 *        class corresponds to the transmission of a message that is defined in
 *        Section 6.2.2 of TS 36.331.
 */
class NrUeRrcSapUser : public NrRrcSap
{
  public:
    /// SetupParameters structure
    struct SetupParameters
    {
        LteRlcSapProvider* srb0SapProvider;  ///< SRB0 SAP provider
        LtePdcpSapProvider* srb1SapProvider; ///< SRB1 SAP provider
    };

    /**
     * \brief Setup function
     * \param params the setup parameters
     */
    virtual void Setup(SetupParameters params) = 0;

    /**
     * \brief Send an _NrRrcConnectionRequest message to the serving eNodeB
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param msg the message
     */
    virtual void SendRrcConnectionRequest(NrRrcConnectionRequest msg, uint16_t grantedBytes) = 0;


    virtual void SendRrcResumeRequest(bool sdt, RrcResumeRequest msg, uint16_t grantedBytes) = 0;

    /**
     * \brief Send an _RRCConnectionSetupComplete_ message to the serving eNodeB
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param msg the message
     */
    virtual void SendRrcConnectionSetupCompleted(RrcConnectionSetupCompleted msg) = 0;

    virtual void SendRrcResumeComplete(RrcResumeComplete msg) = 0;

    /**
     * \brief Send an _RRCConnectionReconfigurationComplete_ message to the serving eNodeB
     *        during an RRC connection reconfiguration procedure
     *        (Section 5.3.5 of TS 36.331).
     * \param msg the message
     */
    virtual void SendRrcConnectionReconfigurationCompleted(
        RrcConnectionReconfigurationCompleted msg) = 0;

    /**
     * \brief Send an _RRCConnectionReestablishmentRequest_ message to the serving eNodeB
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param msg the message
     */
    virtual void SendRrcConnectionReestablishmentRequest(
        RrcConnectionReestablishmentRequest msg) = 0;

    /**
     * \brief Send an _RRCConnectionReestablishmentComplete_ message to the serving eNodeB
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param msg the message
     */
    virtual void SendRrcConnectionReestablishmentComplete(
        RrcConnectionReestablishmentComplete msg) = 0;

    /**
     * \brief Send a _MeasurementReport_ message to the serving eNodeB
     *        during a measurement reporting procedure
     *        (Section 5.5.5 of TS 36.331).
     * \param msg the message
     */
    virtual void SendMeasurementReport(MeasurementReport msg) = 0;

    /**
     * \brief Send UE context remove request function
     *
     * Request eNodeB to remove UE context once radio link failure or
     * random access failure is detected. It is needed since no RLF
     * detection mechanism at eNodeB is implemented.
     *
     * \param rnti the C-RNTI of the UE
     */
    virtual void SendIdealUeContextRemoveRequest(uint16_t rnti) = 0;

    virtual void updateBwp(uint16_t rnti, uint8_t bwpID) =0;


};

/**
 * \brief Part of the RRC protocol. This Service Access Point (SAP) is used to
 *        let the UE RRC receive a message from the eNB RRC. Each method defined
 *        in this class corresponds to the reception of a message that is
 *        defined in Section 6.2.2 of TS 36.331.
 */
class NrUeRrcSapProvider : public NrRrcSap
{
  public:
    /// CompleteSetupParameters structure
    struct CompleteSetupParameters
    {
        LteRlcSapUser* srb0SapUser;  ///< SRB0 SAP user
        LtePdcpSapUser* srb1SapUser; ///< SRB1 SAP user
    };

    /**
     * \brief Complete setup function
     * \param params the complete setup parameters
     */
    virtual void CompleteSetup(CompleteSetupParameters params) = 0;

    /**
     * \brief Receive a _SystemInformation_ message from the serving eNodeB
     *        during a system information acquisition procedure
     *        (Section 5.2.2 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvSystemInformation(SystemInformation msg) = 0;

    /**
     * \brief Receive an _RrcSetup_ message from the serving eNodeB
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvRrcSetup(RrcSetup msg) = 0;

    /**
     * \brief Receive an _RRCReconfiguration_ message from the serving eNodeB
     *        during an RRC connection reconfiguration procedure
     *       
     * \param msg the message
     */
    virtual void RecvRrcReconfiguration(RrcReconfiguration msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReestablishment_ message from the serving eNodeB
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvRrcConnectionReestablishment(RrcConnectionReestablishment msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReestablishmentReject_ message from the serving eNodeB
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvRrcConnectionReestablishmentReject(RrcConnectionReestablishmentReject msg) = 0;

    /**
     * \brief Receive an _RRCRelease_ message from the serving eNodeB
     *        during an RRC connection release procedure
     *        (Section 5.3.8 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvRrcRelease(RrcRelease msg) = 0;

    virtual void RecvRrcResume(RrcResume msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReject_ message from the serving eNodeB
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param msg the message
     */
    virtual void RecvRrcConnectionReject(RrcConnectionReject msg) = 0;
};

/**
 * \brief Part of the RRC protocol. This Service Access Point (SAP) is used by
 *        the eNB RRC to send messages to the UE RRC.  Each method defined in
 *        this class corresponds to the transmission of a message that is
 *        defined in Section 6.2.2 of TS 36.331.
 */
class NrGnbRrcSapUser : public NrRrcSap
{
  public:
    /// SetupUeParameters structure
    struct SetupUeParameters
    {
        LteRlcSapProvider* srb0SapProvider;  ///< SRB0 SAP provider
        LtePdcpSapProvider* srb1SapProvider; ///< SRB1 SAP provider
    };

    /**
     * \brief Setup UE function
     * \param rnti the RNTI
     * \param params the setup UE parameters
     */
    virtual void SetupUe(uint16_t rnti, SetupUeParameters params) = 0;
    /**
     * \brief Remove UE function
     * \param rnti the RNTI
     */
    virtual void RemoveUe(uint16_t rnti) = 0;

    /**
     * \brief Send a _SystemInformation_ message to all attached UEs
     *        during a system information acquisition procedure
     *        (Section 5.2.2 of TS 36.331).
     * \param cellId cell ID
     * \param msg the message
     */
    virtual void SendSystemInformation(uint16_t cellId, SystemInformation msg) = 0;

    /**
     * \brief Send an _RrcSetup_ message to a UE
     *        during an RRC connection establishment procedure

     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcSetup(uint16_t rnti, RrcSetup msg) = 0;

    virtual void SendRrcResume(uint16_t rnti, RrcResume msg) = 0;

    /**
     * \brief Send an _RRCReconfiguration_ message to a UE
     *        during an RRC connection reconfiguration procedure
     *        
     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcReconfiguration(uint16_t rnti,
                                                  RrcReconfiguration msg) = 0;

    /**
     * \brief Send an _RRCConnectionReestablishment_ message to a UE
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcConnectionReestablishment(uint16_t rnti,
                                                  RrcConnectionReestablishment msg) = 0;

    /**
     * \brief Send an _RRCConnectionReestablishmentReject_ message to a UE
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcConnectionReestablishmentReject(uint16_t rnti,
                                                        RrcConnectionReestablishmentReject msg) = 0;

    /**
     * \brief Send an _RRCRelease_ message to a UE
     *        during an RRC connection release procedure
     *        (Section 5.3.8 of TS 36.331).
     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcRelease(uint16_t rnti, RrcRelease msg) = 0;

    /**
     * \brief Send an _RRCConnectionReject_ message to a UE
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param rnti the RNTI of the destination UE
     * \param msg the message
     */
    virtual void SendRrcConnectionReject(uint16_t rnti, RrcConnectionReject msg) = 0;

    /**
     * \brief Encode handover prepration information
     * \param msg HandoverPreparationInfo
     * \returns the packet
     */
    virtual Ptr<Packet> EncodeHandoverPreparationInformation(HandoverPreparationInfo msg) = 0;
    /**
     * \brief Decode handover prepration information
     * \param p the packet
     * \returns HandoverPreparationInfo
     */
    virtual HandoverPreparationInfo DecodeHandoverPreparationInformation(Ptr<Packet> p) = 0;
    /**
     * \brief Encode handover command
     * \param msg RrcConnectionReconfiguration
     * \returns the packet
     */
    virtual Ptr<Packet> EncodeHandoverCommand(RrcConnectionReconfiguration msg) = 0;
    /**
     * \brief Decode handover command
     * \param p the packet
     * \returns RrcConnectionReconfiguration
     */
    virtual RrcConnectionReconfiguration DecodeHandoverCommand(Ptr<Packet> p) = 0;
};

/**
 * \brief Part of the RRC protocol. This Service Access Point (SAP) is used to
 *        let the eNB RRC receive a message from a UE RRC.  Each method defined
 *        in this class corresponds to the reception of a message that is
 *        defined in Section 6.2.2 of TS 36.331.
 */
class NrGnbRrcSapProvider : public NrRrcSap
{
  public:
    /// CompleteSetupUeParameters structure
    struct CompleteSetupUeParameters
    {
        LteRlcSapUser* srb0SapUser;  ///< SRB0 SAP user
        LtePdcpSapUser* srb1SapUser; ///< SRB1 SAP user
    };

    /**
     * \brief Complete setup UE function
     * \param rnti the RNTI of UE which sent the message
     * \param params CompleteSetupUeParameters
     */
    virtual void CompleteSetupUe(uint16_t rnti, CompleteSetupUeParameters params) = 0;

    /**
     * \brief Receive an _NrRrcConnectionRequest_ message from a UE
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvRrcConnectionRequest(uint16_t rnti, NrRrcConnectionRequest msg) = 0;

    virtual void RecvRrcResumeRequest(uint16_t rnti, RrcResumeRequest msg) = 0;

    /**
     * \brief Receive an _RRCConnectionSetupComplete_ message from a UE
     *        during an RRC connection establishment procedure
     *        (Section 5.3.3 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvRrcConnectionSetupCompleted(uint16_t rnti,
                                                 RrcConnectionSetupCompleted msg) = 0;

    virtual void RecvRrcResumeComplete(uint16_t rnti,
                                                 RrcResumeComplete msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReconfigurationComplete_ message from a UE
     *        during an RRC connection reconfiguration procedure
     *        (Section 5.3.5 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvRrcConnectionReconfigurationCompleted(
        uint16_t rnti,
        RrcConnectionReconfigurationCompleted msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReestablishmentRequest_ message from a UE
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvRrcConnectionReestablishmentRequest(
        uint16_t rnti,
        RrcConnectionReestablishmentRequest msg) = 0;

    /**
     * \brief Receive an _RRCConnectionReestablishmentComplete_ message from a UE
     *        during an RRC connection re-establishment procedure
     *        (Section 5.3.7 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvRrcConnectionReestablishmentComplete(
        uint16_t rnti,
        RrcConnectionReestablishmentComplete msg) = 0;

    /**
     * \brief Receive a _MeasurementReport_ message from a UE
     *        during a measurement reporting procedure
     *        (Section 5.5.5 of TS 36.331).
     * \param rnti the RNTI of UE which sent the message
     * \param msg the message
     */
    virtual void RecvMeasurementReport(uint16_t rnti, MeasurementReport msg) = 0;

    /**
     * \brief Receive ideal UE context remove request from the UE RRC.
     *
     * Receive the notification from UE to remove the UE context
     * once radio link failure or random access failure is detected.
     * It is needed since no RLF detection mechanism at eNodeB is implemented.
     *
     * \param rnti the C-RNTI of the UE
     */
    virtual void RecvIdealUeContextRemoveRequest(uint16_t rnti) = 0;

    virtual void UpdateGnbBwpMap(uint16_t rnti, uint8_t bwpID) =0;

};

////////////////////////////////////
//   templates
////////////////////////////////////

/**
 * Template for the implementation of the NrUeRrcSapUser as a member
 * of an owner class of type C to which all methods are forwarded
 *
 */
template <class C>
class MemberNrUeRrcSapUser : public NrUeRrcSapUser
{
  public:
    /**
     * Constructor
     *
     * \param owner the owner class
     */
    MemberNrUeRrcSapUser(C* owner);

    // inherited from NrUeRrcSapUser
    void Setup(SetupParameters params) override;
    void SendRrcConnectionRequest(NrRrcConnectionRequest msg, uint16_t grantedBytes) override;
    void SendRrcResumeRequest(bool sdt, RrcResumeRequest msg, uint16_t grantedBytes) override;
    void SendRrcConnectionSetupCompleted(RrcConnectionSetupCompleted msg) override;
    void SendRrcResumeComplete(RrcResumeComplete msg) override;
    void SendRrcConnectionReconfigurationCompleted(
        RrcConnectionReconfigurationCompleted msg) override;
    void SendRrcConnectionReestablishmentRequest(RrcConnectionReestablishmentRequest msg) override;
    void SendRrcConnectionReestablishmentComplete(
        RrcConnectionReestablishmentComplete msg) override;
    void SendMeasurementReport(MeasurementReport msg) override;
    void SendIdealUeContextRemoveRequest(uint16_t rnti) override;
    void updateBwp(uint16_t rnti, uint8_t bwpID) override;



  private:
    MemberNrUeRrcSapUser();
    C* m_owner; ///< the owner class
};

template <class C>
MemberNrUeRrcSapUser<C>::MemberNrUeRrcSapUser(C* owner)
    : m_owner(owner)
{
}

template <class C>
MemberNrUeRrcSapUser<C>::MemberNrUeRrcSapUser()
{
}

template <class C>
void
MemberNrUeRrcSapUser<C>::Setup(SetupParameters params)
{
    m_owner->DoSetup(params);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcConnectionRequest(NrRrcConnectionRequest msg, uint16_t grantedBytes)
{
    m_owner->DoSendRrcConnectionRequest(msg, grantedBytes);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcResumeRequest(bool sdt, RrcResumeRequest msg, uint16_t grantedBytes)
{
    m_owner->DoSendRrcResumeRequest(sdt,msg,grantedBytes);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcConnectionSetupCompleted(RrcConnectionSetupCompleted msg)
{
    m_owner->DoSendRrcConnectionSetupCompleted(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcResumeComplete(RrcResumeComplete msg)
{
    m_owner->DoSendRrcResumeComplete(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcConnectionReconfigurationCompleted(
    RrcConnectionReconfigurationCompleted msg)
{
    m_owner->DoSendRrcConnectionReconfigurationCompleted(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcConnectionReestablishmentRequest(
    RrcConnectionReestablishmentRequest msg)
{
    m_owner->DoSendRrcConnectionReestablishmentRequest(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendRrcConnectionReestablishmentComplete(
    RrcConnectionReestablishmentComplete msg)
{
    m_owner->DoSendRrcConnectionReestablishmentComplete(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendMeasurementReport(MeasurementReport msg)
{
    m_owner->DoSendMeasurementReport(msg);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::SendIdealUeContextRemoveRequest(uint16_t rnti)
{
    m_owner->DoSendIdealUeContextRemoveRequest(rnti);
}

template <class C>
void
MemberNrUeRrcSapUser<C>::updateBwp(uint16_t rnti, uint8_t bwpID)
{
    m_owner->DoUpdateBwp(rnti,bwpID);
}




/**
 * Template for the implementation of the NrUeRrcSapProvider as a member
 * of an owner class of type C to which all methods are forwarded
 */
template <class C>
class MemberNrUeRrcSapProvider : public NrUeRrcSapProvider
{
  public:
    /**
     * Constructor
     *
     * \param owner the owner class
     */
    MemberNrUeRrcSapProvider(C* owner);

    // methods inherited from NrUeRrcSapProvider go here
    void CompleteSetup(CompleteSetupParameters params) override;
    void RecvSystemInformation(SystemInformation msg) override;
    void RecvRrcSetup(RrcSetup msg) override;
    void RecvRrcReconfiguration(RrcReconfiguration msg) override;
    void RecvRrcConnectionReestablishment(RrcConnectionReestablishment msg) override;
    void RecvRrcConnectionReestablishmentReject(RrcConnectionReestablishmentReject msg) override;
    void RecvRrcRelease(RrcRelease msg) override;
    void RecvRrcResume(RrcResume msg) override;
    void RecvRrcConnectionReject(RrcConnectionReject msg) override;

  private:
    MemberNrUeRrcSapProvider();
    C* m_owner; ///< the owner class
};

template <class C>
MemberNrUeRrcSapProvider<C>::MemberNrUeRrcSapProvider(C* owner)
    : m_owner(owner)
{
}

template <class C>
MemberNrUeRrcSapProvider<C>::MemberNrUeRrcSapProvider()
{
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::CompleteSetup(CompleteSetupParameters params)
{
    m_owner->DoCompleteSetup(params);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvSystemInformation(SystemInformation msg)
{
    Simulator::ScheduleNow(&C::DoRecvSystemInformation, m_owner, msg);
}




template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcSetup(RrcSetup msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcSetup, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcReconfiguration(RrcReconfiguration msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcReconfiguration, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcConnectionReestablishment(RrcConnectionReestablishment msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReestablishment, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcConnectionReestablishmentReject(
    RrcConnectionReestablishmentReject msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReestablishmentReject, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcRelease(RrcRelease msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcRelease, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcResume(RrcResume msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcResume, m_owner, msg);
}

template <class C>
void
MemberNrUeRrcSapProvider<C>::RecvRrcConnectionReject(RrcConnectionReject msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReject, m_owner, msg);
}

/**
 * Template for the implementation of the NrGnbRrcSapUser as a member
 * of an owner class of type C to which all methods are forwarded
 *
 */
template <class C>
class MemberNrGnbRrcSapUser : public NrGnbRrcSapUser
{
  public:
    /**
     * Constructor
     *
     * \param owner the owner class
     */
    MemberNrGnbRrcSapUser(C* owner);

    // inherited from NrGnbRrcSapUser

    void SetupUe(uint16_t rnti, SetupUeParameters params) override;
    void RemoveUe(uint16_t rnti) override;
    void SendSystemInformation(uint16_t cellId, SystemInformation msg) override;
    void SendRrcSetup(uint16_t rnti, RrcSetup msg) override;
    void SendRrcResume(uint16_t rnti, RrcResume msg) override;
    void SendRrcReconfiguration(uint16_t rnti, RrcReconfiguration msg) override;
    void SendRrcConnectionReestablishment(uint16_t rnti, RrcConnectionReestablishment msg) override;
    void SendRrcConnectionReestablishmentReject(uint16_t rnti,
                                                RrcConnectionReestablishmentReject msg) override;
    void SendRrcRelease(uint16_t rnti, RrcRelease msg) override;
    void SendRrcConnectionReject(uint16_t rnti, RrcConnectionReject msg) override;
    Ptr<Packet> EncodeHandoverPreparationInformation(HandoverPreparationInfo msg) override;
    HandoverPreparationInfo DecodeHandoverPreparationInformation(Ptr<Packet> p) override;
    Ptr<Packet> EncodeHandoverCommand(RrcConnectionReconfiguration msg) override;
    RrcConnectionReconfiguration DecodeHandoverCommand(Ptr<Packet> p) override;

  private:
    MemberNrGnbRrcSapUser();
    C* m_owner; ///< the owner class
};

template <class C>
MemberNrGnbRrcSapUser<C>::MemberNrGnbRrcSapUser(C* owner)
    : m_owner(owner)
{
}

template <class C>
MemberNrGnbRrcSapUser<C>::MemberNrGnbRrcSapUser()
{
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SetupUe(uint16_t rnti, SetupUeParameters params)
{
    m_owner->DoSetupUe(rnti, params);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::RemoveUe(uint16_t rnti)
{
    m_owner->DoRemoveUe(rnti);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendSystemInformation(uint16_t cellId, SystemInformation msg)
{
    m_owner->DoSendSystemInformation(cellId, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcSetup(uint16_t rnti, RrcSetup msg)
{
    m_owner->DoSendRrcSetup(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcResume(uint16_t rnti, RrcResume msg)
{
    m_owner->DoSendRrcResume(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcReconfiguration(uint16_t rnti,
                                                            RrcReconfiguration msg)
{
    m_owner->DoSendRrcReconfiguration(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcConnectionReestablishment(uint16_t rnti,
                                                            RrcConnectionReestablishment msg)
{
    m_owner->DoSendRrcConnectionReestablishment(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcConnectionReestablishmentReject(
    uint16_t rnti,
    RrcConnectionReestablishmentReject msg)
{
    m_owner->DoSendRrcConnectionReestablishmentReject(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcRelease(uint16_t rnti, RrcRelease msg)
{
    m_owner->DoSendRrcRelease(rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapUser<C>::SendRrcConnectionReject(uint16_t rnti, RrcConnectionReject msg)
{
    m_owner->DoSendRrcConnectionReject(rnti, msg);
}

template <class C>
Ptr<Packet>
MemberNrGnbRrcSapUser<C>::EncodeHandoverPreparationInformation(HandoverPreparationInfo msg)
{
    return m_owner->DoEncodeHandoverPreparationInformation(msg);
}

template <class C>
NrRrcSap::HandoverPreparationInfo
MemberNrGnbRrcSapUser<C>::DecodeHandoverPreparationInformation(Ptr<Packet> p)
{
    return m_owner->DoDecodeHandoverPreparationInformation(p);
}

template <class C>
Ptr<Packet>
MemberNrGnbRrcSapUser<C>::EncodeHandoverCommand(RrcConnectionReconfiguration msg)
{
    return m_owner->DoEncodeHandoverCommand(msg);
}

template <class C>
NrRrcSap::RrcConnectionReconfiguration
MemberNrGnbRrcSapUser<C>::DecodeHandoverCommand(Ptr<Packet> p)
{
    return m_owner->DoDecodeHandoverCommand(p);
}

/**
 * Template for the implementation of the NrGnbRrcSapProvider as a member
 * of an owner class of type C to which all methods are forwarded
 */
template <class C>
class MemberNrGnbRrcSapProvider : public NrGnbRrcSapProvider
{
  public:
    /**
     * Constructor
     *
     * \param owner
     */
    MemberNrGnbRrcSapProvider(C* owner);

    // methods inherited from NrGnbRrcSapProvider go here

    void CompleteSetupUe(uint16_t rnti, CompleteSetupUeParameters params) override;
    void RecvRrcConnectionRequest(uint16_t rnti, NrRrcConnectionRequest msg) override;
    void RecvRrcResumeRequest(uint16_t rnti, RrcResumeRequest msg) override;
    void RecvRrcConnectionSetupCompleted(uint16_t rnti, RrcConnectionSetupCompleted msg) override;
    void RecvRrcResumeComplete(uint16_t rnti, RrcResumeComplete msg) override;
    void RecvRrcConnectionReconfigurationCompleted(
        uint16_t rnti,
        RrcConnectionReconfigurationCompleted msg) override;
    void RecvRrcConnectionReestablishmentRequest(uint16_t rnti,
                                                 RrcConnectionReestablishmentRequest msg) override;
    void RecvRrcConnectionReestablishmentComplete(
        uint16_t rnti,
        RrcConnectionReestablishmentComplete msg) override;
    void RecvMeasurementReport(uint16_t rnti, MeasurementReport msg) override;
    void RecvIdealUeContextRemoveRequest(uint16_t rnti) override;
    void UpdateGnbBwpMap(uint16_t rnti, uint8_t bwpID) override;


  private:
    MemberNrGnbRrcSapProvider();
    C* m_owner; ///< the owner class
};

template <class C>
MemberNrGnbRrcSapProvider<C>::MemberNrGnbRrcSapProvider(C* owner)
    : m_owner(owner)
{
}

template <class C>
MemberNrGnbRrcSapProvider<C>::MemberNrGnbRrcSapProvider()
{
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::CompleteSetupUe(uint16_t rnti, CompleteSetupUeParameters params)
{
    m_owner->DoCompleteSetupUe(rnti, params);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcConnectionRequest(uint16_t rnti, NrRrcConnectionRequest msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionRequest, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcResumeRequest(uint16_t rnti, RrcResumeRequest msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcResumeRequest, m_owner, rnti, msg);
}


template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcConnectionSetupCompleted(uint16_t rnti,
                                                               RrcConnectionSetupCompleted msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionSetupCompleted, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcResumeComplete(uint16_t rnti,
                                                               RrcResumeComplete msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcResumeComplete, m_owner, rnti, msg);
}


template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcConnectionReconfigurationCompleted(
    uint16_t rnti,
    RrcConnectionReconfigurationCompleted msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReconfigurationCompleted, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcConnectionReestablishmentRequest(
    uint16_t rnti,
    RrcConnectionReestablishmentRequest msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReestablishmentRequest, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvRrcConnectionReestablishmentComplete(
    uint16_t rnti,
    RrcConnectionReestablishmentComplete msg)
{
    Simulator::ScheduleNow(&C::DoRecvRrcConnectionReestablishmentComplete, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvMeasurementReport(uint16_t rnti, MeasurementReport msg)
{
    Simulator::ScheduleNow(&C::DoRecvMeasurementReport, m_owner, rnti, msg);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::RecvIdealUeContextRemoveRequest(uint16_t rnti)
{
    Simulator::ScheduleNow(&C::DoRecvIdealUeContextRemoveRequest, m_owner, rnti);
}

template <class C>
void
MemberNrGnbRrcSapProvider<C>::UpdateGnbBwpMap(uint16_t rnti, uint8_t bwpID)
{
    Simulator::ScheduleNow(&C::DoUpdateGnbBwpMap, m_owner, rnti,bwpID);
}


 
    
} // namespace ns3

#endif // NR_RRC_SAP_H
