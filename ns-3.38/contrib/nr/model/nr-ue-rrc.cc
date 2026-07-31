/*
 * Copyright (c) 2011, 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 * Copyright (c) 2018 Fraunhofer ESK : RLF extensions
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
 * Author: Nicola Baldo <nbaldo@cttc.es>
 *         Budiarto Herman <budiarto.herman@magister.fi>
 * Modified by:
 *          Danilo Abrignani <danilo.abrignani@unibo.it> (Carrier Aggregation - GSoC 2015)
 *          Biljana Bojovic <biljana.bojovic@cttc.es> (Carrier Aggregation)
 *          Vignesh Babu <ns3-dev@esk.fraunhofer.de> (RLF extensions)
 */

#include "nr-ue-rrc.h"

#include "../helper/nr-access-stats.h"
#include "nr-control-messages.h"

#include <ns3/fatal-error.h>
#include <ns3/log.h>
#include <ns3/lte-pdcp.h>
#include <ns3/lte-radio-bearer-info.h>
#include <ns3/lte-rlc-am.h>
#include <ns3/lte-rlc-tm.h>
#include <ns3/lte-rlc-um.h>
#include <ns3/lte-rlc.h>
#include <ns3/object-factory.h>
#include <ns3/object-map.h>
#include <ns3/simulator.h>

#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrUeRrc");

/////////////////////////////
// CMAC SAP forwarder
/////////////////////////////

/// UeMemberLteUeCmacSapUser class
class UeMemberLteUeCmacSapUser : public LteUeCmacSapUser
{
  public:
    /**
     * Constructor
     *
     * \param rrc the RRC class
     */
    UeMemberLteUeCmacSapUser(NrUeRrc* rrc);

    void SetTemporaryCellRnti(uint16_t rnti) override;
    void NotifyRandomAccessSuccessful(UlGrant_s grant, bool sdt) override;
    void NotifyRandomAccessFailed() override;
    void RefreshBwpInactivityTimer() override;

  private:
    NrUeRrc* m_rrc; ///< the RRC class
};

UeMemberLteUeCmacSapUser::UeMemberLteUeCmacSapUser(NrUeRrc* rrc)
    : m_rrc(rrc)
{
}

void
UeMemberLteUeCmacSapUser::SetTemporaryCellRnti(uint16_t rnti)
{
    m_rrc->DoSetTemporaryCellRnti(rnti);
}

void
UeMemberLteUeCmacSapUser::NotifyRandomAccessSuccessful(UlGrant_s grant, bool sdt)
{
    m_rrc->DoNotifyRandomAccessSuccessful(grant, sdt);
}

void
UeMemberLteUeCmacSapUser::NotifyRandomAccessFailed()
{
    m_rrc->DoNotifyRandomAccessFailed();
}

void 
UeMemberLteUeCmacSapUser::RefreshBwpInactivityTimer()
{
    m_rrc->RefreshBwpInactivityTimer();
}


/// Map each of UE RRC states to its string representation.
static const std::string g_ueRrcStateName[NrUeRrc::NUM_STATES] = {
    "IDLE_START",
    "IDLE_CELL_SEARCH",
    "IDLE_WAIT_MIB_SIB1",
    "IDLE_WAIT_MIB",
    "IDLE_WAIT_SIB1",
    "IDLE_CAMPED_NORMALLY",
    "IDLE_WAIT_SIB2",
    "IDLE_RANDOM_ACCESS",
    "IDLE_RANDOM_ACCESS_INACTIVE",
    "IDLE_CONNECTING",
    "INACTIVE_CONNECTING",
    "INACTIVE_SMALL_DATA_TRANSMISSION",
    "CONNECTED_NORMALLY",
    "CONNECTED_HANDOVER",
    "CONNECTED_PHY_PROBLEM",
    "CONNECTED_REESTABLISHING",
    "RA_FAILED",
};

/// Map each of bwpInactivityTimer to its time representation.
static const Time g_bwpInactivityTimer[32] = {
    MilliSeconds(2),
    MilliSeconds(3),
    MilliSeconds(4),
    MilliSeconds(5),
    MilliSeconds(6),
    MilliSeconds(8),
    MilliSeconds(10),
    MilliSeconds(20),
    MilliSeconds(30),
    MilliSeconds(40),
    MilliSeconds(50),
    MilliSeconds(60),
    MilliSeconds(80),
    MilliSeconds(100),
    MilliSeconds(200),
    MilliSeconds(300),
    MilliSeconds(500),
    MilliSeconds(750),
    MilliSeconds(1280),
    MilliSeconds(1920),
    MilliSeconds(2560),
  
};

/// Map each of t380 value to its time representation.
static const Time g_t380[8] = {
    Minutes(5),
    Minutes(10),
    Minutes(20),
    Minutes(30),
    Minutes(60),
    Minutes(120),
    Minutes(360),
    Minutes(720),
};

/// Map each of drx value to its time representation.
static const uint  g_DrxCycle[4] = {
    32,
    64,
    128,
    256,
};

/// Map each of drx value to its time representation.
static const uint  g_eDrxCycle[4] = {
    256,
    512,
    1024,
    2048, //not defined but a logical continuation
};

/////////////////////////////
// ue RRC methods
/////////////////////////////

NS_OBJECT_ENSURE_REGISTERED(NrUeRrc);

NrUeRrc::NrUeRrc()
    : m_cmacSapProvider(0),
      m_rrcSapUser(nullptr),
      m_macSapProvider(nullptr),
      m_asSapUser(nullptr),
      m_ccmRrcSapProvider(nullptr),
      m_state(IDLE_START),
      m_imsi(0),
      m_rnti(0),
      m_cellId(0),
      m_useRlcSm(true),
      m_connectionPending(false),
      m_hasReceivedMib(false),
      m_hasReceivedSib1(false),
      m_hasReceivedSib2(false),
      m_csgWhiteList(0),
      m_noOfSyncIndications(0),
      m_leaveConnectedMode(false),
      m_previousCellId(0),
      m_connEstFailCountLimit(0),
      m_connEstFailCount(0),
      m_numberOfComponentCarriers(MIN_NO_CC)
{
    NS_LOG_FUNCTION(this);
    m_cphySapUser.push_back(new MemberNrUeCphySapUser<NrUeRrc>(this));
    m_cmacSapUser.push_back(new UeMemberLteUeCmacSapUser(this));
    m_cphySapProvider.push_back(nullptr);
    m_cmacSapProvider.push_back(nullptr);
    m_rrcSapProvider = new MemberNrUeRrcSapProvider<NrUeRrc>(this);
    m_drbPdcpSapUser = new LtePdcpSpecificLtePdcpSapUser<NrUeRrc>(this);
    m_asSapProvider = new MemberLteAsSapProvider<NrUeRrc>(this);
    m_ccmRrcSapUser = new MemberLteUeCcmRrcSapUser<NrUeRrc>(this);
}

NrUeRrc::~NrUeRrc()
{
    NS_LOG_FUNCTION(this);
}

void
NrUeRrc::DoDispose()
{
    NS_LOG_FUNCTION(this);
    for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        delete m_cphySapUser.at(i);
        delete m_cmacSapUser.at(i);
    }
    m_cphySapUser.clear();
    m_cmacSapUser.clear();
    delete m_rrcSapProvider;
    delete m_drbPdcpSapUser;
    delete m_asSapProvider;
    delete m_ccmRrcSapUser;
    m_cphySapProvider.erase(m_cphySapProvider.begin(), m_cphySapProvider.end());
    m_cphySapProvider.clear();
    m_cmacSapProvider.erase(m_cmacSapProvider.begin(), m_cmacSapProvider.end());
    m_cmacSapProvider.clear();
    m_drbMap.clear();
}

TypeId
NrUeRrc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NrUeRrc")
            .SetParent<Object>()
            .SetGroupName("Nr")
            .AddConstructor<NrUeRrc>()
            .AddAttribute("DataRadioBearerMap",
                          "List of UE RadioBearerInfo for Data Radio Bearers by LCID.",
                          ObjectMapValue(),
                          MakeObjectMapAccessor(&NrUeRrc::m_drbMap),
                          MakeObjectMapChecker<LteDataRadioBearerInfo>())
            .AddAttribute("Srb0",
                          "SignalingRadioBearerInfo for SRB0",
                          PointerValue(),
                          MakePointerAccessor(&NrUeRrc::m_srb0),
                          MakePointerChecker<LteSignalingRadioBearerInfo>())
            .AddAttribute("Srb1",
                          "SignalingRadioBearerInfo for SRB1",
                          PointerValue(),
                          MakePointerAccessor(&NrUeRrc::m_srb1),
                          MakePointerChecker<LteSignalingRadioBearerInfo>())
            .AddAttribute("CellId",
                          "Serving cell identifier",
                          UintegerValue(0), // unused, read-only attribute
                          MakeUintegerAccessor(&NrUeRrc::GetCellId),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("C-RNTI",
                          "Cell Radio Network Temporary Identifier",
                          UintegerValue(0), // unused, read-only attribute
                          MakeUintegerAccessor(&NrUeRrc::GetRnti),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute(
                "T300",
                "Timer for the RRC Connection Establishment procedure "
                "(i.e., the procedure is deemed as failed if it takes longer than this). "
                "Standard values: 100ms, 200ms, 300ms, 400ms, 600ms, 1000ms, 1500ms, 2000ms",
                TimeValue(MilliSeconds(
                    2000)), // see 3GPP 36.331 UE-TimersAndConstants & RLF-TimersAndConstants
                MakeTimeAccessor(&NrUeRrc::m_t300),
                MakeTimeChecker(MilliSeconds(100), MilliSeconds(2000)))
            .AddAttribute(
                "T310",
                "Timer for detecting the Radio link failure "
                "(i.e., the radio link is deemed as failed if this timer expires). "
                "Standard values: 0ms 50ms, 100ms, 200ms, 500ms, 1000ms, 2000ms",
                TimeValue(MilliSeconds(
                    2000)), // see 3GPP 36.331 UE-TimersAndConstants & RLF-TimersAndConstants
                MakeTimeAccessor(&NrUeRrc::m_t310),
                MakeTimeChecker(MilliSeconds(0), MilliSeconds(2000)))
            .AddAttribute(
                "N310",
                "This specifies the maximum number of out-of-sync indications. "
                "Standard values: 1, 2, 3, 4, 6, 8, 10, 20",
                UintegerValue(6), // see 3GPP 36.331 UE-TimersAndConstants & RLF-TimersAndConstants
                MakeUintegerAccessor(&NrUeRrc::m_n310),
                MakeUintegerChecker<uint8_t>(1, 20))
            .AddAttribute(
                "N311",
                "This specifies the maximum number of in-sync indications. "
                "Standard values: 1, 2, 3, 4, 5, 6, 8, 10",
                UintegerValue(2), // see 3GPP 36.331 UE-TimersAndConstants & RLF-TimersAndConstants
                MakeUintegerAccessor(&NrUeRrc::m_n311),
                MakeUintegerChecker<uint8_t>(1, 10))
            .AddTraceSource("MibReceived",
                            "trace fired upon reception of Master Information Block",
                            MakeTraceSourceAccessor(&NrUeRrc::m_mibReceivedTrace),
                            "ns3::NrUeRrc::MibSibHandoverTracedCallback")
            .AddTraceSource("Sib1Received",
                            "trace fired upon reception of System Information Block Type 1",
                            MakeTraceSourceAccessor(&NrUeRrc::m_sib1ReceivedTrace),
                            "ns3::NrUeRrc::MibSibHandoverTracedCallback")
            .AddTraceSource("Sib2Received",
                            "trace fired upon reception of System Information Block Type 2",
                            MakeTraceSourceAccessor(&NrUeRrc::m_sib2ReceivedTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("StateTransition",
                            "trace fired upon every UE RRC state transition",
                            MakeTraceSourceAccessor(&NrUeRrc::m_stateTransitionTrace),
                            "ns3::NrUeRrc::StateTracedCallback")
            .AddTraceSource("InitialCellSelectionEndOk",
                            "trace fired upon successful initial cell selection procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_initialCellSelectionEndOkTrace),
                            "ns3::NrUeRrc::CellSelectionTracedCallback")
            .AddTraceSource("InitialCellSelectionEndError",
                            "trace fired upon failed initial cell selection procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_initialCellSelectionEndErrorTrace),
                            "ns3::NrUeRrc::CellSelectionTracedCallback")
            .AddTraceSource("RandomAccessSuccessful",
                            "trace fired upon successful completion of the random access procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_randomAccessSuccessfulTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("RandomAccessError",
                            "trace fired upon failure of the random access procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_randomAccessErrorTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("ConnectionEstablished",
                            "trace fired upon successful RRC connection establishment",
                            MakeTraceSourceAccessor(&NrUeRrc::m_connectionEstablishedTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("ConnectionTimeout",
                            "trace fired upon timeout RRC connection establishment because of T300",
                            MakeTraceSourceAccessor(&NrUeRrc::m_connectionTimeoutTrace),
                            "ns3::NrUeRrc::ImsiCidRntiCountTracedCallback")
            .AddTraceSource("Reconfiguration",
                            "trace fired upon RRC connection reconfiguration",
                            MakeTraceSourceAccessor(&NrUeRrc::m_reconfigurationTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("HandoverStart",
                            "trace fired upon start of a handover procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_handoverStartTrace),
                            "ns3::NrUeRrc::MibSibHandoverTracedCallback")
            .AddTraceSource("HandoverEndOk",
                            "trace fired upon successful termination of a handover procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_handoverEndOkTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("HandoverEndError",
                            "trace fired upon failure of a handover procedure",
                            MakeTraceSourceAccessor(&NrUeRrc::m_handoverEndErrorTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("SCarrierConfigured",
                            "trace fired after configuring secondary carriers",
                            MakeTraceSourceAccessor(&NrUeRrc::m_sCarrierConfiguredTrace),
                            "ns3::NrUeRrc::SCarrierConfiguredTracedCallback")
            .AddTraceSource("Srb1Created",
                            "trace fired after SRB1 is created",
                            MakeTraceSourceAccessor(&NrUeRrc::m_srb1CreatedTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource("DrbCreated",
                            "trace fired after DRB is created",
                            MakeTraceSourceAccessor(&NrUeRrc::m_drbCreatedTrace),
                            "ns3::NrUeRrc::ImsiCidRntiLcIdTracedCallback")
            .AddTraceSource("RadioLinkFailure",
                            "trace fired upon failure of radio link",
                            MakeTraceSourceAccessor(&NrUeRrc::m_radioLinkFailureTrace),
                            "ns3::NrUeRrc::ImsiCidRntiTracedCallback")
            .AddTraceSource(
                "PhySyncDetection",
                "trace fired upon receiving in Sync or out of Sync indications from UE PHY",
                MakeTraceSourceAccessor(&NrUeRrc::m_phySyncDetectionTrace),
                "ns3::NrUeRrc::PhySyncDetectionTracedCallback");
            
    return tid;
}

void
NrUeRrc::SetNrUecphySapProvider(NrUecphySapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_cphySapProvider.at(0) = s;
}

void
NrUeRrc::SetNrUecphySapProvider(NrUecphySapProvider* s, uint8_t index)
{
    NS_LOG_FUNCTION(this << s);
    m_cphySapProvider.at(index) = s;
}

NrUeCphySapUser*
NrUeRrc::GetNrUeCphySapUser()
{
    NS_LOG_FUNCTION(this);
    return m_cphySapUser.at(0);
}

NrUeCphySapUser*
NrUeRrc::GetNrUeCphySapUser(uint8_t index)
{
    NS_LOG_FUNCTION(this);
    return m_cphySapUser.at(index);
}

void
NrUeRrc::SetLteUeCmacSapProvider(LteUeCmacSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_cmacSapProvider.at(0) = s;
}

void
NrUeRrc::SetLteUeCmacSapProvider(LteUeCmacSapProvider* s, uint8_t index)
{
    NS_LOG_FUNCTION(this << s);
    m_cmacSapProvider.at(index) = s;
}

LteUeCmacSapUser*
NrUeRrc::GetLteUeCmacSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_cmacSapUser.at(0);
}

LteUeCmacSapUser*
NrUeRrc::GetLteUeCmacSapUser(uint8_t index)
{
    NS_LOG_FUNCTION(this);
    return m_cmacSapUser.at(index);
}

void
NrUeRrc::SetNrUeRrcSapUser(NrUeRrcSapUser* s)
{
    NS_LOG_FUNCTION(this << s);
    m_rrcSapUser = s;
}

NrUeRrcSapProvider*
NrUeRrc::GetNrUeRrcSapProvider()
{
    NS_LOG_FUNCTION(this);
    return m_rrcSapProvider;
}

void
NrUeRrc::SetLteMacSapProvider(LteMacSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_macSapProvider = s;
}

void
NrUeRrc::SetLteCcmRrcSapProvider(LteUeCcmRrcSapProvider* s)
{
    NS_LOG_FUNCTION(this << s);
    m_ccmRrcSapProvider = s;
}

LteUeCcmRrcSapUser*
NrUeRrc::GetLteCcmRrcSapUser()
{
    NS_LOG_FUNCTION(this);
    return m_ccmRrcSapUser;
}

void
NrUeRrc::SetAsSapUser(LteAsSapUser* s)
{
    m_asSapUser = s;
}

LteAsSapProvider*
NrUeRrc::GetAsSapProvider()
{
    return m_asSapProvider;
}

void
NrUeRrc::SetImsi(uint64_t imsi)
{
    NS_LOG_FUNCTION(this << imsi);
    m_imsi = imsi;

    // Communicate the IMSI to MACs and PHYs for all the component carriers
    for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        m_cmacSapProvider.at(i)->SetImsi(m_imsi);
        m_cphySapProvider.at(i)->SetImsi(m_imsi);
    }
}

void
NrUeRrc::StorePreviousCellId(uint16_t cellId)
{
    NS_LOG_FUNCTION(this << cellId);
    m_previousCellId = cellId;
}

uint64_t
NrUeRrc::GetImsi() const
{
    return m_imsi;
}

uint16_t
NrUeRrc::GetRnti() const
{
    NS_LOG_FUNCTION(this);
    return m_rnti;
}

uint16_t
NrUeRrc::GetCellId() const
{
    NS_LOG_FUNCTION(this);
    return m_cellId;
}

bool
NrUeRrc::IsServingCell(uint16_t cellId) const
{
    NS_LOG_FUNCTION(this);
    for (auto& cphySap : m_cphySapProvider)
    {
        if (cellId == cphySap->GetCellId())
        {
            return true;
        }
    }
    return false;
}

uint8_t
NrUeRrc::GetUlBandwidth() const
{
    NS_LOG_FUNCTION(this);
    return m_ulBandwidth;
}

uint8_t
NrUeRrc::GetDlBandwidth() const
{
    NS_LOG_FUNCTION(this);
    return m_dlBandwidth;
}

uint32_t
NrUeRrc::GetDlEarfcn() const
{
    return m_dlEarfcn;
}

uint32_t
NrUeRrc::GetUlEarfcn() const
{
    NS_LOG_FUNCTION(this);
    return m_ulEarfcn;
}

NrUeRrc::State
NrUeRrc::GetState() const
{
    NS_LOG_FUNCTION(this);
    return m_state;
}

uint16_t
NrUeRrc::GetPreviousCellId() const
{
    NS_LOG_FUNCTION(this);
    return m_previousCellId;
}

void
NrUeRrc::SetUseRlcSm(bool val)
{
    NS_LOG_FUNCTION(this);
    m_useRlcSm = val;
}

void
NrUeRrc::DoInitialize()
{
    NS_LOG_FUNCTION(this);

    // setup the UE side of SRB0
    uint8_t lcid = 0; 

    Ptr<LteRlc> rlc = CreateObject<LteRlcTm>()->GetObject<LteRlc>();
    rlc->SetLteMacSapProvider(m_macSapProvider);
    rlc->SetRnti(m_rnti);
    rlc->SetLcId(lcid);

    m_srb0 = CreateObject<LteSignalingRadioBearerInfo>();
    m_srb0->m_rlc = rlc;
    m_srb0->m_srbIdentity = 0;
    NrUeRrcSapUser::SetupParameters ueParams;
    ueParams.srb0SapProvider = m_srb0->m_rlc->GetLteRlcSapProvider();
    ueParams.srb1SapProvider = nullptr;
    m_rrcSapUser->Setup(ueParams);

    // CCCH (LCID 0) is pre-configured, here is the hardcoded configuration:
    LteUeCmacSapProvider::LogicalChannelConfig lcConfig;
    lcConfig.priority = 0;                   // highest priority
    lcConfig.prioritizedBitRateKbps = 65535; // maximum
    lcConfig.bucketSizeDurationMs = 65535;   // maximum
    lcConfig.logicalChannelGroup = 0;        // all SRBs mapped to LCG 0
    LteMacSapUser* msu =
        m_ccmRrcSapProvider->ConfigureSignalBearer(lcid, lcConfig, rlc->GetLteMacSapUser());
    m_cmacSapProvider.at(0)->AddLc(lcid, lcConfig, msu);
}

void
NrUeRrc::InitializeSap()
{
    if (m_numberOfComponentCarriers < MIN_NO_CC || m_numberOfComponentCarriers > MAX_NO_CC)
    {
        // this check is needed in order to maintain backward compatibility with scripts and tests
        // if case lte-helper is not used (like in several tests) the m_numberOfComponentCarriers
        // is not set and then an error is raised
        // In this case m_numberOfComponentCarriers is set to 1
        m_numberOfComponentCarriers = MIN_NO_CC;
    }
    if (m_numberOfComponentCarriers > MIN_NO_CC)
    {
        for (uint16_t i = 1; i < m_numberOfComponentCarriers; i++)
        {
            m_cphySapUser.push_back(new MemberNrUeCphySapUser<NrUeRrc>(this));
            m_cmacSapUser.push_back(new UeMemberLteUeCmacSapUser(this));
            m_cphySapProvider.push_back(nullptr);
            m_cmacSapProvider.push_back(nullptr);
        }
    }
}

void
NrUeRrc::DoSendData(Ptr<Packet> packet, uint8_t bid)
{
    NS_LOG_FUNCTION(this << packet);
    uint8_t drbid = Bid2Drbid(bid);

    switch(m_state)
    {
        case IDLE_CAMPED_NORMALLY:
            m_packetStored.push_back(packet);
            StartConnection();
            break;

        case INACTIVE:
            m_packetStored.push_back(packet);
            ResumeConnection();
            break;

        case CONNECTED_NORMALLY:
        if (drbid != 0)
        {
            std::map<uint8_t, Ptr<LteDataRadioBearerInfo>>::iterator it = m_drbMap.find(drbid);
            NS_ASSERT_MSG(it != m_drbMap.end(), "could not find bearer with drbid == " << drbid);

            LtePdcpSapProvider::TransmitPdcpSduParameters params;
            params.pdcpSdu = packet;
            params.rnti = m_rnti;
            params.lcid = it->second->m_logicalChannelIdentity;

            NS_LOG_LOGIC(this << " RNTI=" << m_rnti << " sending packet " << packet << " on DRBID "
                            << (uint32_t)drbid << " (LCID " << (uint32_t)params.lcid << ")"
                            << " (" << packet->GetSize() << " bytes)");
            it->second->m_pdcp->GetLtePdcpSapProvider()->TransmitPdcpSdu(params);
        }
        else{
            //UE is connected but drb are not set yet. Reception of RRC Reconfiguration needed. 
            m_packetStored.push_back(packet);
        }

        break;

        case IDLE_RANDOM_ACCESS:
        {
            m_packetStored.push_back(packet);
            break;
        }

        case IDLE_CONNECTING:
        {   
            m_packetStored.push_back(packet);
            break;
        }
        default: 
             m_packetStored.push_back(packet);
             m_DataPending = true;
             //std::cout << "Data reception in an unknown state. imsi:"<< m_imsi << ",state" << m_state <<std::endl;
            //NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }



    
}

void
NrUeRrc::DoDisconnect()
{
    NS_LOG_FUNCTION(this);

    switch (m_state)
    {
    case IDLE_START:
    case IDLE_CELL_SEARCH:
    case IDLE_WAIT_MIB_SIB1:
    case IDLE_WAIT_MIB:
    case IDLE_WAIT_SIB1:
    case IDLE_CAMPED_NORMALLY:
        NS_LOG_INFO("already disconnected");
        break;

    case IDLE_WAIT_SIB2:
    case IDLE_CONNECTING:
    case INACTIVE_CONNECTING:
        NS_FATAL_ERROR("cannot abort connection setup procedure");
        break;

    case CONNECTED_NORMALLY:
    case CONNECTED_HANDOVER:
    case CONNECTED_PHY_PROBLEM:
    case CONNECTED_REESTABLISHING:
    case RELEASE_RECEIVED:
        LeaveConnectedMode();
        break;
    // case RA_FAILED:
    //     //a new connection is initialized after waiting for a bit. TODO_ increase window size?
    //     Time delay = MilliSeconds(100);
    //     Simulator::Schedule(delay, SwitchToState(IDLE_START); )

    default: // i.e. IDLE_RANDOM_ACCESS
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params)
{
    NS_LOG_FUNCTION(this);
    m_asSapUser->RecvData(params.pdcpSdu);
}

void
NrUeRrc::DoSetTemporaryCellRnti(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    m_rnti = rnti;
    m_srb0->m_rlc->SetRnti(m_rnti);
    if(m_srb1)
    {
        m_srb1->m_rlc->SetRnti(m_rnti);
    }
    

    //define RNTI for every Bwp, should this be set only at a bwp switch and rest on release?
    for(uint8_t bwpID =0; bwpID < m_cphySapProvider.size(); ++bwpID)
    {
        m_cphySapProvider.at(bwpID)->SetRnti(m_rnti);
        m_cmacSapProvider.at(bwpID)->SetRnti(m_rnti);
    }
   


}

void
NrUeRrc::DoNotifyRandomAccessSuccessful(UlGrant_s grant, bool sdt)
{
    NS_LOG_FUNCTION(this << m_imsi << ToString(m_state));

    m_randomAccessSuccessfulTrace(m_imsi, m_cellId, m_rnti);
   // m_connectionTimeout.Cancel();
    switch (m_state)
    {
    case INACTIVE:
    case IDLE_RANDOM_ACCESS_INACTIVE: {
        // we just received a RAR with a T-C-RNTI and an UL grant
        // send RRC resume request as message 3 of the random access procedure
        SwitchToState(INACTIVE_CONNECTING);
        //NrRrcSap::NrRrcConnectionRequest msg;
        //msg.ueIdentity = m_imsi;
        m_grantedBytes = grant.m_tbSize;
        //DoSendRrcConnectionRequest();
        // int64_t sendTime = grant.m_Framenumber*10+grant.m_Subframenumber+grant.m_Slotnumber*2; //TODO genaue Zeitpunkt mit Numerology bestimmen
        // Time delay = MilliSeconds(sendTime-Simulator::Now().GetMilliSeconds());
        // NS_ASSERT(delay.GetMilliSeconds()>0);
        //Simulator::Schedule(delay,&NrUeRrc::DoSendRrcConnectionRequest,this);
        DoSendRrcConnectionRequest();
        // make sure that DoSetTemporaryCellRnti is being executed after the UlDci is received. Otherwise the UlDci cant be received due to a wrong rnti
        int64_t sendTimeMicroSeconds = grant.m_Framenumber*10000+grant.m_Subframenumber*1000+grant.m_Slotnumber*500; //send
        Time delay = MicroSeconds(sendTimeMicroSeconds-Simulator::Now().GetMicroSeconds()+499); // 
        NS_ASSERT(delay.GetMilliSeconds()>0);
        Simulator::Schedule(delay,&NrUeRrc::DoSetTemporaryCellRnti,this,m_resumeIdentity);

    }
    break;

    case IDLE_RANDOM_ACCESS: {
        
        // we just received a RAR with a T-C-RNTI and an UL grant
        // send RRC connection request as message 3 of the random access procedure
        SwitchToState(IDLE_CONNECTING);
        //NrRrcSap::NrRrcConnectionRequest msg;
        //msg.ueIdentity = m_imsi;
        m_grantedBytes = grant.m_tbSize;
        DoSendRrcConnectionRequest();
        //m_rrcSapUser->SendRrcConnectionRequest(msg);
        


    }
    break;

    case CONNECTED_HANDOVER: {
        LteRrcSap::RrcConnectionReconfigurationCompleted msg;
        msg.rrcTransactionIdentifier = m_lastRrcTransactionIdentifier;
        m_rrcSapUser->SendRrcConnectionReconfigurationCompleted(msg);

        // 3GPP TS 36.331 section 5.5.6.1 Measurements related actions upon handover
        std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt;
        for (measIdIt = m_varMeasConfig.measIdList.begin();
             measIdIt != m_varMeasConfig.measIdList.end();
             ++measIdIt)
        {
            VarMeasReportListClear(measIdIt->second.measId);
        }

        SwitchToState(CONNECTED_NORMALLY);
        m_cmacSapProvider.at(0)->NotifyConnectionSuccessful(); // RA successful during handover
        m_handoverEndOkTrace(m_imsi, m_cellId, m_rnti);
    }
    break;

    default:
        NS_FATAL_ERROR("unexpected event in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoNotifyRandomAccessFailed()
{
    NS_LOG_FUNCTION(this << m_imsi << ToString(m_state));
    NrAccessStats::IncRaFailed();
    m_randomAccessErrorTrace(m_imsi, m_cellId, m_rnti);

    switch (m_state)
    {
    case IDLE_RANDOM_ACCESS: {
        SwitchToState(IDLE_CAMPED_NORMALLY);
        m_asSapUser->NotifyConnectionFailed();
    }
    break;

    case IDLE_RANDOM_ACCESS_INACTIVE: {
        SwitchToState(INACTIVE);
        m_asSapUser->NotifyConnectionFailed();
    }
    break;

    case CONNECTED_HANDOVER: {
        m_handoverEndErrorTrace(m_imsi, m_cellId, m_rnti);
        /**
         * \todo After a handover failure because of a random access failure,
         *       send an RRC Connection Re-establishment and switch to
         *       CONNECTED_REESTABLISHING state.
         */
        if (!m_leaveConnectedMode)
        {
            m_leaveConnectedMode = true;
            SwitchToState(RA_FAILED);
            m_rrcSapUser->SendIdealUeContextRemoveRequest(m_rnti);
            // we should have called NotifyConnectionFailed
            // but that method would immediately ask you UE to
            // connect rather than doing cell selection again.
            m_asSapUser->NotifyConnectionReleased();
        }
    }
    break;

    default:
        NS_FATAL_ERROR("unexpected event in state " << ToString(m_state));
        break;
    }
}

void 
NrUeRrc::RefreshBwpInactivityTimer()
{
    NS_LOG_FUNCTION(this);
    m_ccmRrcSapProvider->RefreshBwpInactivityTimer();
}



void
NrUeRrc::DoSetCsgWhiteList(uint32_t csgId)
{
    NS_LOG_FUNCTION(this << m_imsi << csgId);
    m_csgWhiteList = csgId;
}

void
NrUeRrc::DoStartCellSelection(uint32_t dlEarfcn)
{
    NS_LOG_FUNCTION(this << m_imsi << dlEarfcn);
    NS_ASSERT_MSG(m_state == IDLE_START,
                  "cannot start cell selection from state " << ToString(m_state));
    m_dlEarfcn = dlEarfcn;
    m_cphySapProvider.at(0)->StartCellSearch(dlEarfcn);
    SwitchToState(IDLE_CELL_SEARCH);
}

void
NrUeRrc::DoForceCampedOnEnb(uint16_t cellId, uint32_t dlEarfcn)
{
    NS_LOG_FUNCTION(this << m_imsi << cellId << dlEarfcn);

    switch (m_state)
    {
    case IDLE_START:
        m_cellId = cellId;
        m_dlEarfcn = dlEarfcn;
        m_cphySapProvider.at(0)->SynchronizeWithGnb(m_cellId, m_dlEarfcn);
        SwitchToState(IDLE_WAIT_MIB);
        break;

    case IDLE_CELL_SEARCH:
    case IDLE_WAIT_MIB_SIB1:
    case IDLE_WAIT_SIB1:
        NS_FATAL_ERROR("cannot abort cell selection " << ToString(m_state));
        break;

    case IDLE_WAIT_MIB:
        NS_LOG_INFO("already forced to camp to cell " << m_cellId);
        break;

    case IDLE_CAMPED_NORMALLY:
    case IDLE_WAIT_SIB2:
    case IDLE_RANDOM_ACCESS:
    case IDLE_RANDOM_ACCESS_INACTIVE:
    case IDLE_CONNECTING:
    case INACTIVE_CONNECTING:
        NS_LOG_INFO("already camped to cell " << m_cellId);
        break;

    case CONNECTED_NORMALLY:
    case CONNECTED_HANDOVER:
    case CONNECTED_PHY_PROBLEM:
    case CONNECTED_REESTABLISHING:
        NS_LOG_INFO("already connected to cell " << m_cellId);
        break;

    default:
        NS_FATAL_ERROR("unexpected event in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoConnect()
{
    NS_LOG_FUNCTION(this << m_imsi);

    switch (m_state)
    {
    case IDLE_START:
    case IDLE_CELL_SEARCH:
    case IDLE_WAIT_MIB_SIB1:
    case IDLE_WAIT_SIB1:
    case IDLE_WAIT_MIB:
        //m_connectionPending = true;
        break;

    case IDLE_CAMPED_NORMALLY:
        m_connectionPending = true;
        
        break;

    case IDLE_WAIT_SIB2:
    case IDLE_RANDOM_ACCESS:
    case IDLE_RANDOM_ACCESS_INACTIVE:
    case IDLE_CONNECTING:
    case INACTIVE_CONNECTING:
        NS_LOG_INFO("already connecting");
        break;

    case CONNECTED_NORMALLY:
    case CONNECTED_REESTABLISHING:
    case CONNECTED_HANDOVER:
        NS_LOG_INFO("already connected");
        break;

    case CONNECTED_PHY_PROBLEM:
        SwitchToState(IDLE_CAMPED_NORMALLY);
        m_connectionPending = true;
	break;

    default:
        NS_FATAL_ERROR("unexpected event in state " << ToString(m_state));
        break;
    }
}

// CPHY SAP methods

void
NrUeRrc::DoRecvMasterInformationBlock(uint16_t cellId, LteRrcSap::MasterInformationBlock msg)
{
    m_dlBandwidth = msg.dlBandwidth;
    m_cphySapProvider.at(0)->SetDlBandwidth(msg.dlBandwidth);
    m_hasReceivedMib = true;
    m_mibReceivedTrace(m_imsi, m_cellId, m_rnti, cellId);

    switch (m_state)
    {
    case IDLE_WAIT_MIB:
        // manual attachment
        SwitchToState(IDLE_CAMPED_NORMALLY);
        break;

    case IDLE_WAIT_MIB_SIB1:
        // automatic attachment from Idle mode cell selection
        SwitchToState(IDLE_WAIT_SIB1);
        break;

    default:
        // do nothing extra
        break;
    }
}

void
NrUeRrc::DoRecvSystemInformationBlockType1(uint16_t cellId,
                                            LteRrcSap::SystemInformationBlockType1 msg)
{
    NS_LOG_FUNCTION(this);
    switch (m_state)
    {
    case IDLE_WAIT_SIB1:
        NS_ASSERT_MSG(cellId == msg.cellAccessRelatedInfo.cellIdentity,
                      "Cell identity in SIB1 does not match with the originating cell");
        m_hasReceivedSib1 = true;
        m_lastSib1 = msg;
        m_sib1ReceivedTrace(m_imsi, m_cellId, m_rnti, cellId);
        EvaluateCellForSelection();
        break;

    case IDLE_CAMPED_NORMALLY:
    case IDLE_RANDOM_ACCESS:
    case IDLE_RANDOM_ACCESS_INACTIVE:
    case IDLE_CONNECTING:
    case INACTIVE_CONNECTING:
    case CONNECTED_NORMALLY:
    case CONNECTED_HANDOVER:
    case CONNECTED_PHY_PROBLEM:
    case CONNECTED_REESTABLISHING:
        NS_ASSERT_MSG(cellId == msg.cellAccessRelatedInfo.cellIdentity,
                      "Cell identity in SIB1 does not match with the originating cell");
        m_hasReceivedSib1 = true;
        m_lastSib1 = msg;
        m_sib1ReceivedTrace(m_imsi, m_cellId, m_rnti, cellId);
        break;

    case IDLE_WAIT_MIB_SIB1:
        // MIB has not been received, so ignore this SIB1
        break;

    default: // e.g. IDLE_START, IDLE_CELL_SEARCH, IDLE_WAIT_MIB, IDLE_WAIT_SIB2
        // do nothing
        break;
    }
}

void
NrUeRrc::DoReportUeMeasurements(NrUeCphySapUser::UeMeasurementsParameters params)
{
    NS_LOG_FUNCTION(this);

    // layer 3 filtering does not apply in IDLE mode
    bool useLayer3Filtering = (m_state == CONNECTED_NORMALLY);
    bool triggering = true;
    std::vector<NrUeCphySapUser::UeMeasurementsElement>::iterator newMeasIt;
    for (newMeasIt = params.m_ueMeasurementsList.begin();
         newMeasIt != params.m_ueMeasurementsList.end();
         ++newMeasIt)
    {
        if (params.m_componentCarrierId != 0)
        {
            triggering = false; // report is triggered only when an event is on the primary carrier
            // in this case the measurement received is related to secondary carriers
        }
        SaveUeMeasurements(newMeasIt->m_cellId,
                           newMeasIt->m_rsrp,
                           newMeasIt->m_rsrq,
                           useLayer3Filtering,
                           params.m_componentCarrierId);
    }

    if (m_state == IDLE_CELL_SEARCH)
    {
        // start decoding BCH
        SynchronizeToStrongestCell();
    }
    else
    {
        if (triggering)
        {
            std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt;
            for (measIdIt = m_varMeasConfig.measIdList.begin();
                 measIdIt != m_varMeasConfig.measIdList.end();
                 ++measIdIt)
            {
                MeasurementReportTriggering(measIdIt->first);
            }
        }
    }

} // end of NrUeRrc::DoReportUeMeasurements

// RRC SAP methods

void
NrUeRrc::DoCompleteSetup(NrUeRrcSapProvider::CompleteSetupParameters params)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    m_srb0->m_rlc->SetLteRlcSapUser(params.srb0SapUser);
    if (m_srb1)
    {
        m_srb1->m_pdcp->SetLtePdcpSapUser(params.srb1SapUser);
    }
}

void
NrUeRrc::DoRecvSystemInformation(LteRrcSap::SystemInformation msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);

    if (msg.haveSib2)
    {
        switch (m_state)
        {
        case IDLE_CAMPED_NORMALLY:
        case IDLE_WAIT_SIB2:
        case IDLE_RANDOM_ACCESS:
        case IDLE_RANDOM_ACCESS_INACTIVE:
        case IDLE_CONNECTING:
        case INACTIVE_CONNECTING:
        case CONNECTED_NORMALLY:
        case CONNECTED_HANDOVER:
        case CONNECTED_PHY_PROBLEM:
        case CONNECTED_REESTABLISHING:
            m_hasReceivedSib2 = true;
            m_ulBandwidth = msg.sib2.freqInfo.ulBandwidth;
            m_ulEarfcn = msg.sib2.freqInfo.ulCarrierFreq;
            m_sib2ReceivedTrace(m_imsi, m_cellId, m_rnti);
            LteUeCmacSapProvider::RachConfig rc;
            rc.numberOfRaPreambles = msg.sib2.radioResourceConfigCommon.rachConfigCommon
                                         .preambleInfo.numberOfRaPreambles;
            rc.preambleTransMax = msg.sib2.radioResourceConfigCommon.rachConfigCommon
                                      .raSupervisionInfo.preambleTransMax;
            rc.raResponseWindowSize = msg.sib2.radioResourceConfigCommon.rachConfigCommon
                                          .raSupervisionInfo.raResponseWindowSize;
            rc.connEstFailCount =
                msg.sib2.radioResourceConfigCommon.rachConfigCommon.txFailParam.connEstFailCount;
            m_connEstFailCountLimit = rc.connEstFailCount;
            NS_ASSERT_MSG(m_connEstFailCountLimit > 0 && m_connEstFailCountLimit < 5,
                          "SIB2 msg contains wrong value " << m_connEstFailCountLimit
                                                           << "of connEstFailCount");
            m_cmacSapProvider.at(0)->ConfigureRach(rc);
            m_cphySapProvider.at(0)->ConfigureUplink(m_ulEarfcn, m_ulBandwidth);
            m_cphySapProvider.at(0)->ConfigureReferenceSignalPower(
                msg.sib2.radioResourceConfigCommon.pdschConfigCommon.referenceSignalPower);
            if (m_state == IDLE_WAIT_SIB2)
            {
                NS_ASSERT(m_connectionPending);
                //StartConnection();
            }
            break;

        default: // IDLE_START, IDLE_CELL_SEARCH, IDLE_WAIT_MIB, IDLE_WAIT_MIB_SIB1, IDLE_WAIT_SIB1
            // do nothing
            break;
        }
    }
}

void
NrUeRrc::DoRecvRrcSetup(NrRrcSap::RrcSetup msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    
    //std::cout << "NrUeRrc::DoRecvRrcSetup, imsi: " << m_imsi << std::endl;
    
    switch (m_state)
    {
    case IDLE_CONNECTING: {
        ApplyRadioResourceConfigDedicated(msg.radioResourceConfigDedicated);
        ApplySpCellConfig(msg.spCellconfig);
        m_connEstFailCount = 0;
        m_connectionTimeout.Cancel();
        SwitchToState(CONNECTED_NORMALLY);
        m_powerStateChangedCallback(NrEnergyModel::PowerState::RRC_IDLE);
        m_leaveConnectedMode = false;
        LteRrcSap::RrcConnectionSetupCompleted msg2;
        msg2.rrcTransactionIdentifier = msg.rrcTransactionIdentifier;


        //save spCellconfig to configure after physical transmission of rrcConnectionSetupCompleted-> transmission has to be done on bwp 0
        //m_savedSpCellConfig =  msg.spCellconfig;
        
        
        m_asSapUser->NotifyConnectionSuccessful();
        m_cmacSapProvider.at(0)->NotifyConnectionSuccessful();
        
        //m_rrcSapUser->SendRrcConnectionSetupCompleted(msg2);
        Simulator::Schedule(MilliSeconds(10),&NrUeRrc::SendRrcSetupCompleted,this,msg2); //10 ms rrc procedure delay for rrc setup

        m_connectionEstablishedTrace(m_imsi, m_cellId, m_rnti);
        NrAccessStats::IncRrcConnected();
        NS_ABORT_MSG_IF(m_noOfSyncIndications > 0,
                        "Sync indications should be zero "
                        "when a new RRC connection is established. Current value = "
                            << (uint16_t)m_noOfSyncIndications);
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::SendRrcSetupCompleted(LteRrcSap::RrcConnectionSetupCompleted msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    m_rrcSapUser->SendRrcConnectionSetupCompleted(msg);
}

void
NrUeRrc::DoRecvRrcReconfiguration(NrRrcSap::RrcReconfiguration msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);

    //std::cout << "imsi: " << m_imsi << " with RNTI " << m_rnti<<" connected"<<std::endl;

    switch (m_state)
    {
    case CONNECTED_NORMALLY:
       
        if (msg.rrcReconfiguration.haveRadioBearerConfig)
        {
            ApplyRadioBearerConfig(msg.rrcReconfiguration.radioBearerConfig);
        }
        
        LteRrcSap::RrcConnectionReconfigurationCompleted msg2;
        msg2.rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
        m_rrcSapUser->SendRrcConnectionReconfigurationCompleted(msg2);
        
        //TODO_ trace doesnt connect
        //m_reconfigurationTrace(m_imsi, m_cellId, m_rnti);
        SendStoredPackages();
        break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoRecvRrcConnectionReestablishment(LteRrcSap::RrcConnectionReestablishment msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    //std::cout << "NrUeRrc::DoRecvRrcConnectionReestablishment, imsi: " << m_imsi << std::endl;
    switch (m_state)
    {
    case CONNECTED_REESTABLISHING: {
        /**
         * \todo After receiving RRC Connection Re-establishment, stop timer
         *       T301, fire a new trace source, reply with RRC Connection
         *       Re-establishment Complete, and finally switch to
         *       CONNECTED_NORMALLY state. See Section 5.3.7.5 of 3GPP TS
         *       36.331.
         */
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoRecvRrcConnectionReestablishmentReject(
    LteRrcSap::RrcConnectionReestablishmentReject msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    //std::cout << "NrUeRrc::DoRecvRrcConnectionReestablishmentReject, imsi: " << m_imsi << std::endl;
    switch (m_state)
    {
    case CONNECTED_REESTABLISHING: {
        /**
         * \todo After receiving RRC Connection Re-establishment Reject, stop
         *       timer T301. See Section 5.3.7.8 of 3GPP TS 36.331.
         */
        m_asSapUser->NotifyConnectionReleased(); // Inform upper layers
    }
    break;

    default:
        NS_FATAL_ERROR("method unexpected in state " << ToString(m_state));
        break;
    }
}

void
NrUeRrc::DoRecvRrcRelease(NrRrcSap::RrcRelease msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    //std::cout << "NrUeRrc::DoRecvRrcRelease, imsi: " << m_imsi << std::endl;
    m_connectionTimeout.Cancel();
    m_lastRrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    SwitchToState(RELEASE_RECEIVED);
    //ETSI TS 138 331 V175.3.8.3 
    //Simulator::Schedule(MilliSeconds(60),&NrUeRrc::ProcessRrcRelease,this,msg.rrcRelease);

    Simulator::Schedule(MilliSeconds(10),&NrUeRrc::ProcessRrcRelease,this,msg.rrcRelease);


    
    
}


void 
NrUeRrc::ProcessRrcRelease(NrRrcSap::RRCRelease_IEs rel)
{
    //ETSI TS 138 331 V175.3.8.3
    m_ccmRrcSapProvider->StartBwpSwitching(0);
    if(rel.hasSuspendConfig == true)
    {   
        m_powerStateChangedCallback(NrEnergyModel::PowerState::RRC_IDLE);
        m_resumeIdentity = rel.suspendConfig.fullRnti;
        m_cmacSapProvider.at(0)->SetPRnti(rel.suspendConfig.fullRnti);
        const uint32_t edrxFrames =
            g_eDrxCycle[rel.suspendConfig.ran_ExtendedPagingCycle_r17];
    	for (uint16_t i = 0; i < m_numberOfComponentCarriers; ++i)
        {
            m_cmacSapProvider.at(i)->Reset(); // reset the MAC
            m_cmacSapProvider.at(i)->NotifyDrx(g_DrxCycle[rel.suspendConfig.ran_PagingCycle]);
            m_cmacSapProvider.at(i)->NotifyeDrx(edrxFrames);
        }
        // Phase C: start PTW duty cycle on the energy model (TS 38.304 research).
        if (!m_edrxPtwConfigCallback.IsNull())
        {
            m_edrxPtwConfigCallback(edrxFrames);
        }

        for (auto& sdtDrb : rel.suspendConfig.sdt_Config_r17.sdt_DRB_List_r17)
        {
            m_sdtConfigured = true;
            std::map<uint8_t, Ptr<LteDataRadioBearerInfo>>::iterator drbMapIt =
            m_drbMap.find(sdtDrb);
            if (drbMapIt == m_drbMap.end())
            {
                NS_FATAL_ERROR("Couldnt find assigned SDT-DRB");
            }
            else
            {
                Ptr<LteDataRadioBearerInfo> drbInfo = drbMapIt->second;
                drbInfo->sdtConfigured = true;
                // 3> for each of the RLC bearer that is part of the UE configuration:
                //  4> re-establish the RLC entity as specified in TS 38.322 [4];
            }
            
                
            

        }

        m_t380 = g_t380[rel.suspendConfig.t380];
        //m_connectionTimeout = Simulator::Schedule(m_t300, &NrUeRrc::ConnectionTimeout, this); //TODO nicht connectionTimeout
 
        SwitchToState(INACTIVE);
    }
    else{

       m_powerStateChangedCallback(NrEnergyModel::PowerState::RRC_IDLE);
    
        //release resources at UE
        if (!m_leaveConnectedMode)
        {
            m_leaveConnectedMode = true;
            m_rrcSapUser->SendIdealUeContextRemoveRequest(m_rnti);
            m_asSapUser->NotifyConnectionReleased();
        }
        SwitchToState(IDLE_CAMPED_NORMALLY);
    }

    //reconnect if there are still packages to transmit
    // if(m_packetStored.size() > 0)
    // {
    //     if(m_state == INACTIVE)
    //     {
    //         ResumeConnection();
    //     }
    //     else{
    //         if(m_state == IDLE_CAMPED_NORMALLY)
    //         {
    //             StartConnection();
    //         }
    //     }
    // }

}

void
NrUeRrc::DoRecvRrcResume(NrRrcSap::RrcResume msg)
{
    NS_LOG_FUNCTION(this << " RNTI " << m_rnti);
    //std::cout << "NrUeRrc::DoRecvRrcResume, imsi: " << m_imsi << std::endl;
    m_connectionTimeout.Cancel();
    m_lastRrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    // //ETSI TS 138 331 V17 5.3.13.4
    if( msg.rrcResume.haveRadioBearerConfig)
    {
        //TODO_
    }
    ApplySpCellConfig(msg.rrcResume.spCellconfig);
    SwitchToState(CONNECTED_NORMALLY);
    NrRrcSap::RrcResumeComplete resumeCompleteMsg;
    resumeCompleteMsg.rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_rrcSapUser->SendRrcResumeComplete(resumeCompleteMsg);
}

void
NrUeRrc::DoRecvRrcConnectionReject(LteRrcSap::RrcConnectionReject msg)
{
    NS_LOG_FUNCTION(this);
    //std::cout << "NrUeRrc::DoRecvRrcConnectionReject, imsi: " << m_imsi << std::endl;
    m_connectionTimeout.Cancel();
    for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        m_cmacSapProvider.at(i)->Reset(); // reset the MAC
    }
    m_hasReceivedSib2 = false; // invalidate the previously received SIB2
    SwitchToState(IDLE_CAMPED_NORMALLY);
    m_asSapUser->NotifyConnectionFailed(); // inform upper layer
}

void
NrUeRrc::DoSetNumberOfComponentCarriers(uint16_t noOfComponentCarriers)
{
    NS_LOG_FUNCTION(this);
    m_numberOfComponentCarriers = noOfComponentCarriers;
}

void
NrUeRrc::DoNotifySwitchedBwp(uint8_t bwpID)
{
    NS_LOG_FUNCTION(this);
    for(uint8_t i = 0; i<m_cphySapProvider.size();++i)
    {
        if(i != bwpID)
        {
            //To be sure that only one bwp is active
            // m_cphySapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::DL);
            // m_cphySapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::UL);
            // m_cmacSapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::DL);
            // m_cmacSapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::UL);
            m_cphySapProvider.at(i)->Reset();
            m_cmacSapProvider.at(i)->Reset();
            m_cphySapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::DL);
            m_cphySapProvider.at(i)->DeactivateBwp(LteNrTddSlotType::UL);

        }
        else{
            m_cmacSapProvider.at(bwpID)->ActivateBwp(LteNrTddSlotType::DL);
            m_cmacSapProvider.at(bwpID)->ActivateBwp(LteNrTddSlotType::UL);
            m_cphySapProvider.at(bwpID)->ActivateBwp(LteNrTddSlotType::DL);
            m_cphySapProvider.at(bwpID)->ActivateBwp(LteNrTddSlotType::UL);
        }
    }
    if(m_srb1) //if srb1 is configured. srb1 is already removed when rrcRelease is received. 
    {
        DynamicCast<LteRlcAm>(m_srb1->m_rlc)->StopTimer();
        
    }
    
}


void
NrUeRrc::SynchronizeToStrongestCell()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_state == IDLE_CELL_SEARCH);

    uint16_t maxRsrpCellId = 0;
    double maxRsrp = -std::numeric_limits<double>::infinity();
    double minRsrp = -140.0; // Minimum RSRP in dBm a UE can report

    std::map<uint16_t, MeasValues>::iterator it;
    for (it = m_storedMeasValues.begin(); it != m_storedMeasValues.end(); it++)
    {
        /*
         * This block attempts to find a cell with strongest RSRP and has not
         * yet been identified as "acceptable cell".
         */
        if (maxRsrp < it->second.rsrp && it->second.rsrp > minRsrp)
        {
            std::set<uint16_t>::const_iterator itCell;
            itCell = m_acceptableCell.find(it->first);
            if (itCell == m_acceptableCell.end())
            {
                maxRsrpCellId = it->first;
                maxRsrp = it->second.rsrp;
            }
        }
    }

    if (maxRsrpCellId == 0)
    {
        NS_LOG_WARN(this << " Cell search is unable to detect surrounding cell to attach to");
    }
    else
    {
        NS_LOG_LOGIC(this << " cell " << maxRsrpCellId
                          << " is the strongest untried surrounding cell");
        m_cphySapProvider.at(0)->SynchronizeWithGnb(maxRsrpCellId, m_dlEarfcn);
        SwitchToState(IDLE_WAIT_MIB_SIB1);
    }

} // end of void NrUeRrc::SynchronizeToStrongestCell ()

void
NrUeRrc::EvaluateCellForSelection()
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_state == IDLE_WAIT_SIB1);
    NS_ASSERT(m_hasReceivedMib);
    NS_ASSERT(m_hasReceivedSib1);
    uint16_t cellId = m_lastSib1.cellAccessRelatedInfo.cellIdentity;

    // Cell selection criteria evaluation

    bool isSuitableCell = false;
    bool isAcceptableCell = false;
    std::map<uint16_t, MeasValues>::iterator storedMeasIt = m_storedMeasValues.find(cellId);
    double qRxLevMeas = storedMeasIt->second.rsrp;
    double qRxLevMin =
        EutranMeasurementMapping::IeValue2ActualQRxLevMin(m_lastSib1.cellSelectionInfo.qRxLevMin);
    NS_LOG_LOGIC(this << " cell selection to cellId=" << cellId << " qrxlevmeas=" << qRxLevMeas
                      << " dBm"
                      << " qrxlevmin=" << qRxLevMin << " dBm");

    if (qRxLevMeas - qRxLevMin > 0)
    {
        isAcceptableCell = true;

        uint32_t cellCsgId = m_lastSib1.cellAccessRelatedInfo.csgIdentity;
        bool cellCsgIndication = m_lastSib1.cellAccessRelatedInfo.csgIndication;

        isSuitableCell = (cellCsgIndication == false) || (cellCsgId == m_csgWhiteList);

        NS_LOG_LOGIC(this << " csg(ue/cell/indication)=" << m_csgWhiteList << "/" << cellCsgId
                          << "/" << cellCsgIndication);
    }

    // Cell selection decision

    if (isSuitableCell)
    {
        m_cellId = cellId;
        m_cphySapProvider.at(0)->SynchronizeWithGnb(cellId, m_dlEarfcn);
        m_cphySapProvider.at(0)->SetDlBandwidth(m_dlBandwidth);
        m_initialCellSelectionEndOkTrace(m_imsi, cellId);
        // Once the UE is connected, m_connectionPending is
        // set to false. So, when RLF occurs and UE performs
        // cell selection upon leaving RRC_CONNECTED state,
        // the following call to DoConnect will make the
        // m_connectionPending to be true again. Thus,
        // upon calling SwitchToState (IDLE_CAMPED_NORMALLY)
        // UE state is instantly change to IDLE_WAIT_SIB2.
        // This will make the UE to read the SIB2 message
        // and start random access
        

        //DoConnect is called when the application wants to send data while epc is not connected
        // if (!m_connectionPending)
        // {
        //     NS_LOG_DEBUG("Calling DoConnect in state = " << ToString(m_state));
        //     DoConnect();
        // }
        SwitchToState(IDLE_WAIT_SIB2);
    }
    else
    {
        // ignore the MIB and SIB1 received from this cell
        m_hasReceivedMib = false;
        m_hasReceivedSib1 = false;

        m_initialCellSelectionEndErrorTrace(m_imsi, cellId);

        if (isAcceptableCell)
        {
            /*
             * The cells inserted into this list will not be considered for
             * subsequent cell search attempt.
             */
            m_acceptableCell.insert(cellId);
        }

        SwitchToState(IDLE_CELL_SEARCH);
        SynchronizeToStrongestCell(); // retry to a different cell
    }

} // end of void NrUeRrc::EvaluateCellForSelection ()

void
NrUeRrc::ApplyRadioResourceConfigDedicatedSecondaryCarrier(
    LteRrcSap::NonCriticalExtensionConfiguration nonCec)
{
    NS_LOG_FUNCTION(this);

    m_sCellToAddModList = nonCec.sCellToAddModList;

    for (uint32_t sCellIndex : nonCec.sCellToReleaseList)
    {
        m_cphySapProvider.at(sCellIndex)->Reset();
        m_cmacSapProvider.at(sCellIndex)->Reset();
    }

    for (auto& scell : nonCec.sCellToAddModList)
    {
        uint8_t ccId = scell.sCellIndex;

        uint16_t physCellId = scell.cellIdentification.physCellId;
        uint16_t ulBand =
            scell.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulBandwidth;
        uint32_t ulEarfcn =
            scell.radioResourceConfigCommonSCell.ulConfiguration.ulFreqInfo.ulCarrierFreq;
        uint16_t dlBand = scell.radioResourceConfigCommonSCell.nonUlConfiguration.dlBandwidth;
        uint32_t dlEarfcn = scell.cellIdentification.dlCarrierFreq;
        uint8_t txMode = scell.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
                             .antennaInfo.transmissionMode;
        uint16_t srsIndex = scell.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
                                .soundingRsUlConfigDedicated.srsConfigIndex;

        m_cphySapProvider.at(ccId)->SynchronizeWithGnb(physCellId, dlEarfcn);
        m_cphySapProvider.at(ccId)->SetDlBandwidth(dlBand);
        m_cphySapProvider.at(ccId)->ConfigureUplink(ulEarfcn, ulBand);
        m_cphySapProvider.at(ccId)->ConfigureReferenceSignalPower(
            scell.radioResourceConfigCommonSCell.nonUlConfiguration.pdschConfigCommon
                .referenceSignalPower);
        m_cphySapProvider.at(ccId)->SetTransmissionMode(txMode);
        m_cphySapProvider.at(ccId)->SetRnti(m_rnti);
        m_cmacSapProvider.at(ccId)->SetRnti(m_rnti);
        // update PdschConfigDedicated (i.e. P_A value)
        LteRrcSap::PdschConfigDedicated pdschConfigDedicated =
            scell.radioResourceConfigDedicatedSCell.physicalConfigDedicatedSCell
                .pdschConfigDedicated;
        double paDouble = LteRrcSap::ConvertPdschConfigDedicated2Double(pdschConfigDedicated);
        m_cphySapProvider.at(ccId)->SetPa(paDouble);
        m_cphySapProvider.at(ccId)->SetSrsConfigurationIndex(srsIndex);
    }

    m_sCarrierConfiguredTrace(this, m_sCellToAddModList);
}

void
NrUeRrc::ApplyRadioResourceConfigDedicated(LteRrcSap::RadioResourceConfigDedicated rrcd)
{
    NS_LOG_FUNCTION(this);
    const struct LteRrcSap::PhysicalConfigDedicated& pcd = rrcd.physicalConfigDedicated;

    if (pcd.haveAntennaInfoDedicated)
    {
        m_cphySapProvider.at(0)->SetTransmissionMode(pcd.antennaInfo.transmissionMode);
    }
    if (pcd.haveSoundingRsUlConfigDedicated)
    {
        m_cphySapProvider.at(0)->SetSrsConfigurationIndex(
            pcd.soundingRsUlConfigDedicated.srsConfigIndex);
    }

    if (pcd.havePdschConfigDedicated)
    {
        // update PdschConfigDedicated (i.e. P_A value)
        m_pdschConfigDedicated = pcd.pdschConfigDedicated;
        double paDouble = LteRrcSap::ConvertPdschConfigDedicated2Double(m_pdschConfigDedicated);
        m_cphySapProvider.at(0)->SetPa(paDouble);
    }

    std::list<LteRrcSap::SrbToAddMod>::const_iterator stamIt = rrcd.srbToAddModList.begin();
    if (stamIt != rrcd.srbToAddModList.end())
    {
        if (!m_srb1)
        {
            // SRB1 not setup yet
            NS_ASSERT_MSG((m_state == IDLE_CONNECTING) || (m_state == CONNECTED_HANDOVER),
                          "unexpected state " << ToString(m_state));
            NS_ASSERT_MSG(stamIt->srbIdentity == 1, "only SRB1 supported");

            const uint8_t lcid = 1; // fixed LCID for SRB1

            Ptr<LteRlc> rlc = CreateObject<LteRlcAm>();
            rlc->SetLteMacSapProvider(m_macSapProvider);
            rlc->SetRnti(m_rnti);
            rlc->SetLcId(lcid);

            Ptr<LtePdcp> pdcp = CreateObject<LtePdcp>();
            pdcp->SetRnti(m_rnti);
            pdcp->SetLcId(lcid);
            pdcp->SetLtePdcpSapUser(m_drbPdcpSapUser);
            pdcp->SetLteRlcSapProvider(rlc->GetLteRlcSapProvider());
            rlc->SetLteRlcSapUser(pdcp->GetLteRlcSapUser());

            m_srb1 = CreateObject<LteSignalingRadioBearerInfo>();
            m_srb1->m_rlc = rlc;
            m_srb1->m_pdcp = pdcp;
            m_srb1->m_srbIdentity = 1;
            m_srb1CreatedTrace(m_imsi, m_cellId, m_rnti);

            m_srb1->m_logicalChannelConfig.priority = stamIt->logicalChannelConfig.priority;
            m_srb1->m_logicalChannelConfig.prioritizedBitRateKbps =
                stamIt->logicalChannelConfig.prioritizedBitRateKbps;
            m_srb1->m_logicalChannelConfig.bucketSizeDurationMs =
                stamIt->logicalChannelConfig.bucketSizeDurationMs;
            m_srb1->m_logicalChannelConfig.logicalChannelGroup =
                stamIt->logicalChannelConfig.logicalChannelGroup;

            struct LteUeCmacSapProvider::LogicalChannelConfig lcConfig;
            lcConfig.priority = stamIt->logicalChannelConfig.priority;
            lcConfig.prioritizedBitRateKbps = stamIt->logicalChannelConfig.prioritizedBitRateKbps;
            lcConfig.bucketSizeDurationMs = stamIt->logicalChannelConfig.bucketSizeDurationMs;
            lcConfig.logicalChannelGroup = stamIt->logicalChannelConfig.logicalChannelGroup;
            LteMacSapUser* msu =
                m_ccmRrcSapProvider->ConfigureSignalBearer(lcid, lcConfig, rlc->GetLteMacSapUser());

            //configure logical Channel for every Bwp
            for(uint8_t bwpID =0; bwpID < m_cmacSapProvider.size(); ++bwpID)
            {
                m_cmacSapProvider.at(bwpID)->AddLc(lcid, lcConfig, msu);
            }
           
            ++stamIt;
            NS_ASSERT_MSG(stamIt == rrcd.srbToAddModList.end(), "at most one SrbToAdd supported");

            NrUeRrcSapUser::SetupParameters ueParams;
            ueParams.srb0SapProvider = m_srb0->m_rlc->GetLteRlcSapProvider();
            ueParams.srb1SapProvider = m_srb1->m_pdcp->GetLtePdcpSapProvider();
            m_rrcSapUser->Setup(ueParams);
        }
        else
        {
            NS_LOG_INFO("request to modify SRB1 (skipping as currently not implemented)");
            // would need to modify m_srb1, and then propagate changes to the MAC
        }
    }
}
void
NrUeRrc::ApplyRadioBearerConfig(NrRrcSap::RadioBearerConfig rbc)
{
    NS_LOG_FUNCTION(this);

    std::list<LteRrcSap::SrbToAddMod>::const_iterator stamIt = rbc.srbToAddModList.begin();
    if (stamIt != rbc.srbToAddModList.end())
    {
        if (!m_srb1)
        {
            // SRB1 not setup yet
            NS_ASSERT_MSG((m_state == IDLE_CONNECTING) || (m_state == CONNECTED_HANDOVER),
                          "unexpected state " << ToString(m_state));
            NS_ASSERT_MSG(stamIt->srbIdentity == 1, "only SRB1 supported");

            const uint8_t lcid = 1; // fixed LCID for SRB1

            Ptr<LteRlc> rlc = CreateObject<LteRlcAm>();
            rlc->SetLteMacSapProvider(m_macSapProvider);
            rlc->SetRnti(m_rnti);
            rlc->SetLcId(lcid);

            Ptr<LtePdcp> pdcp = CreateObject<LtePdcp>();
            pdcp->SetRnti(m_rnti);
            pdcp->SetLcId(lcid);
            pdcp->SetLtePdcpSapUser(m_drbPdcpSapUser);
            pdcp->SetLteRlcSapProvider(rlc->GetLteRlcSapProvider());
            rlc->SetLteRlcSapUser(pdcp->GetLteRlcSapUser());

            m_srb1 = CreateObject<LteSignalingRadioBearerInfo>();
            m_srb1->m_rlc = rlc;
            m_srb1->m_pdcp = pdcp;
            m_srb1->m_srbIdentity = 1;
            m_srb1CreatedTrace(m_imsi, m_cellId, m_rnti);

            m_srb1->m_logicalChannelConfig.priority = stamIt->logicalChannelConfig.priority;
            m_srb1->m_logicalChannelConfig.prioritizedBitRateKbps =
                stamIt->logicalChannelConfig.prioritizedBitRateKbps;
            m_srb1->m_logicalChannelConfig.bucketSizeDurationMs =
                stamIt->logicalChannelConfig.bucketSizeDurationMs;
            m_srb1->m_logicalChannelConfig.logicalChannelGroup =
                stamIt->logicalChannelConfig.logicalChannelGroup;

            struct LteUeCmacSapProvider::LogicalChannelConfig lcConfig;
            lcConfig.priority = stamIt->logicalChannelConfig.priority;
            lcConfig.prioritizedBitRateKbps = stamIt->logicalChannelConfig.prioritizedBitRateKbps;
            lcConfig.bucketSizeDurationMs = stamIt->logicalChannelConfig.bucketSizeDurationMs;
            lcConfig.logicalChannelGroup = stamIt->logicalChannelConfig.logicalChannelGroup;
            LteMacSapUser* msu =
                m_ccmRrcSapProvider->ConfigureSignalBearer(lcid, lcConfig, rlc->GetLteMacSapUser());

            //configure logical Channel for every Bwp
            for(uint8_t bwpID =0; bwpID < m_cmacSapProvider.size(); ++bwpID)
            {
                m_cmacSapProvider.at(bwpID)->AddLc(lcid, lcConfig, msu);
            }
           
            ++stamIt;
            NS_ASSERT_MSG(stamIt == rbc.srbToAddModList.end(), "at most one SrbToAdd supported");

            NrUeRrcSapUser::SetupParameters ueParams;
            ueParams.srb0SapProvider = m_srb0->m_rlc->GetLteRlcSapProvider();
            ueParams.srb1SapProvider = m_srb1->m_pdcp->GetLtePdcpSapProvider();
            m_rrcSapUser->Setup(ueParams);
        }
        else
        {
            NS_LOG_INFO("request to modify SRB1 (skipping as currently not implemented)");
            // would need to modify m_srb1, and then propagate changes to the MAC
        }
    }


    std::list<LteRrcSap::DrbToAddMod>::const_iterator dtamIt;
    for (dtamIt = rbc.drbToAddModList.begin(); dtamIt != rbc.drbToAddModList.end(); ++dtamIt)
    {
        NS_LOG_INFO(this << " IMSI " << m_imsi << " adding/modifying DRBID "
                         << (uint32_t)dtamIt->drbIdentity << " LC "
                         << (uint32_t)dtamIt->logicalChannelIdentity);
        NS_ASSERT_MSG(dtamIt->logicalChannelIdentity > 2,
                      "LCID value " << dtamIt->logicalChannelIdentity << " is reserved for SRBs");

        std::map<uint8_t, Ptr<LteDataRadioBearerInfo>>::iterator drbMapIt =
            m_drbMap.find(dtamIt->drbIdentity);
        if (drbMapIt == m_drbMap.end())
        {
            NS_LOG_INFO("New Data Radio Bearer");

            TypeId rlcTypeId;
            if (m_useRlcSm)
            {
                rlcTypeId = LteRlcSm::GetTypeId();
            }
            else
            {
                switch (dtamIt->rlcConfig.choice)
                {
                case LteRrcSap::RlcConfig::AM:
                    rlcTypeId = LteRlcAm::GetTypeId();
                    break;

                case LteRrcSap::RlcConfig::UM_BI_DIRECTIONAL:
                    rlcTypeId = LteRlcUm::GetTypeId();
                    break;

                default:
                    NS_FATAL_ERROR("unsupported RLC configuration");
                    break;
                }
            }

            ObjectFactory rlcObjectFactory;
            rlcObjectFactory.SetTypeId(rlcTypeId);
            Ptr<LteRlc> rlc = rlcObjectFactory.Create()->GetObject<LteRlc>();
            rlc->SetLteMacSapProvider(m_macSapProvider);
            rlc->SetRnti(m_rnti);
            rlc->SetLcId(dtamIt->logicalChannelIdentity);

            Ptr<LteDataRadioBearerInfo> drbInfo = CreateObject<LteDataRadioBearerInfo>();
            drbInfo->m_rlc = rlc;
            drbInfo->m_epsBearerIdentity = dtamIt->epsBearerIdentity;
            drbInfo->m_logicalChannelIdentity = dtamIt->logicalChannelIdentity;
            drbInfo->m_drbIdentity = dtamIt->drbIdentity;

            // we need PDCP only for real RLC, i.e., RLC/UM or RLC/AM
            // if we are using RLC/SM we don't care of anything above RLC
            if (rlcTypeId != LteRlcSm::GetTypeId())
            {
                Ptr<LtePdcp> pdcp = CreateObject<LtePdcp>();
                pdcp->SetRnti(m_rnti);
                pdcp->SetLcId(dtamIt->logicalChannelIdentity);
                pdcp->SetLtePdcpSapUser(m_drbPdcpSapUser);
                pdcp->SetLteRlcSapProvider(rlc->GetLteRlcSapProvider());
                rlc->SetLteRlcSapUser(pdcp->GetLteRlcSapUser());
                drbInfo->m_pdcp = pdcp;
            }

            m_bid2DrbidMap[dtamIt->epsBearerIdentity] = dtamIt->drbIdentity;

            m_drbMap.insert(
                std::pair<uint8_t, Ptr<LteDataRadioBearerInfo>>(dtamIt->drbIdentity, drbInfo));

            m_drbCreatedTrace(m_imsi, m_cellId, m_rnti, dtamIt->drbIdentity);

            struct LteUeCmacSapProvider::LogicalChannelConfig lcConfig;
            lcConfig.priority = dtamIt->logicalChannelConfig.priority;
            lcConfig.prioritizedBitRateKbps = dtamIt->logicalChannelConfig.prioritizedBitRateKbps;
            lcConfig.bucketSizeDurationMs = dtamIt->logicalChannelConfig.bucketSizeDurationMs;
            lcConfig.logicalChannelGroup = dtamIt->logicalChannelConfig.logicalChannelGroup;

            NS_LOG_DEBUG(this << " UE RRC RNTI " << m_rnti << " Number Of Component Carriers "
                              << m_numberOfComponentCarriers << " lcID "
                              << (uint16_t)dtamIt->logicalChannelIdentity);
            // Call AddLc of UE component carrier manager
            std::vector<LteUeCcmRrcSapProvider::LcsConfig> lcOnCcMapping =
                m_ccmRrcSapProvider->AddLc(dtamIt->logicalChannelIdentity,
                                           lcConfig,
                                           rlc->GetLteMacSapUser());

            NS_LOG_DEBUG("Size of lcOnCcMapping vector " << lcOnCcMapping.size());
            std::vector<LteUeCcmRrcSapProvider::LcsConfig>::iterator itLcOnCcMapping =
                lcOnCcMapping.begin();
            NS_ASSERT_MSG(itLcOnCcMapping != lcOnCcMapping.end(),
                          "Component carrier manager failed to add LC for data radio bearer");

            for (itLcOnCcMapping = lcOnCcMapping.begin(); itLcOnCcMapping != lcOnCcMapping.end();
                 ++itLcOnCcMapping)
            {
                NS_LOG_DEBUG("RNTI " << m_rnti << " LCG id "
                                     << (uint16_t)itLcOnCcMapping->lcConfig.logicalChannelGroup
                                     << " ComponentCarrierId "
                                     << (uint16_t)itLcOnCcMapping->componentCarrierId);
                uint8_t index = itLcOnCcMapping->componentCarrierId;
                LteUeCmacSapProvider::LogicalChannelConfig lcConfigFromCcm =
                    itLcOnCcMapping->lcConfig;
                LteMacSapUser* msu = itLcOnCcMapping->msu;
                m_cmacSapProvider.at(index)->AddLc(dtamIt->logicalChannelIdentity,
                                                   lcConfigFromCcm,
                                                   msu);
            }

            rlc->Initialize();
        }
        else
        {
            NS_LOG_INFO("request to modify existing DRBID");
            Ptr<LteDataRadioBearerInfo> drbInfo = drbMapIt->second;
            /// \todo currently not implemented. Would need to modify drbInfo, and then propagate
            /// changes to the MAC
        }
    }

    std::list<uint8_t>::iterator dtdmIt;
    for (dtdmIt = rbc.drbToReleaseList.begin(); dtdmIt != rbc.drbToReleaseList.end(); ++dtdmIt)
    {
        uint8_t drbid = *dtdmIt;
        NS_LOG_INFO(this << " IMSI " << m_imsi << " releasing DRB " << (uint32_t)drbid);
        std::map<uint8_t, Ptr<LteDataRadioBearerInfo>>::iterator it = m_drbMap.find(drbid);
        NS_ASSERT_MSG(it != m_drbMap.end(), "could not find bearer with given lcid");
        m_drbMap.erase(it);
        m_bid2DrbidMap.erase(drbid);
        // Remove LCID
        for (uint32_t i = 0; i < m_numberOfComponentCarriers; i++)
        {
            m_cmacSapProvider.at(i)->RemoveLc(drbid + 2);
        }
    }
}

void
NrUeRrc::ApplySpCellConfig(NrRrcSap::SpCellConfig spcc)
{
    NS_LOG_FUNCTION(this);
    // for(auto bwpToAdd: spcc.spCellConfigDedicated.downlinkBWP_ToAddModList)
    // {
        
    
    //     //m_cmacSapProvider.at(bwpToAdd)->
    // }

    // for(auto bwpToAdd: spcc.spCellConfigDedicated.downlinkBWP_ToAddModList)
    // {

    
    //     //m_cmacSapProvider.at(bwpToAdd)->
    // }

    m_ccmRrcSapProvider->StartBwpSwitching(spcc.spCellConfigDedicated.firstActiveDownlinkBWP_Id);
    m_cphySapProvider.at(spcc.spCellConfigDedicated.firstActiveDownlinkBWP_Id)->ConfigureUplink(m_ulEarfcn, m_ulBandwidth);
    m_rrcSapUser->updateBwp(m_rnti,spcc.spCellConfigDedicated.firstActiveDownlinkBWP_Id);
    m_ccmRrcSapProvider->SetBwpInactivityTimer(g_bwpInactivityTimer[spcc.spCellConfigDedicated.bwp_InactivityTimer]);
    


    for(auto bwpToAdd: spcc.spCellConfigDedicated.downlinkBWP_ToAddModList)
    {
        //filter specific RIV values (N_size_bwp=275, Rb_start=0 for single BWP)
        const uint16_t riv = bwpToAdd.bwp_dlCommon.genericParameters.locationAndBandwidth;
        if(riv == 3299 )
        {
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(1000); 
        }
        else if(riv == 17874 )
        {
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(800); 
        }
        else if(riv == 32449 )
        {
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(600); 
        }
        else if(riv == 28875 )
        {
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(400); 
        }
        // True 5 MHz: N_RB=12 → RIV=275*11=3025; N_RB=11 → RIV=2750 (38.104 μ=1).
        // SetDlBandwidth units are 100 kHz → 50 = 5 MHz.
        else if(riv == 3025 || riv == 2750 )
        {
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(50);
        }
        else{
            m_cphySapProvider.at(bwpToAdd.bwp_Id)->SetDlBandwidth(200); 
        }
    }
    //GetBwFromConfig(spcc.spCellConfigDedicated.uplinkConfig.uplinkBWP_ToAddModList[0].)


}

// uint16_t
// NrUeRrc::GetBwFromConfig(uint16_t RIV)
// {
//     //I implemented the algorithm according to following LTE example

//     // Copyright 2012 Ben Wojtowicz
//     // This program is free software: you can redistribute it and/or modify
//     // it under the terms of the GNU Affero General Public License as published by
//     // the Free Software Foundation, either version 3 of the License, or
//     // (at your option) any later version.

//     // This program is distributed in the hope that it will be useful,
//     // but WITHOUT ANY WARRANTY; without even the implied warranty of
//     // MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//     // GNU Affero General Public License for more details.

//     // You should have received a copy of the GNU Affero General Public License
//     // along with this program.  If not, see <http://www.gnu.org/licenses/>.
//     // Following is from lte_dci_1a_unpack.m file in /octave folder
//     //         for(n=1:N_rb_dl)
//     //             for(m=0:(N_rb_dl-n))
//     //                 if((n-1) <= floor(N_rb_dl/2))
//     //                     if(RIV == N_rb_dl*(n-1)+m)
//     //                         L_crb    = n;
//     //                         RB_start = m;
//     //                     endif
//     //                 else
//     //                     if(RIV == N_rb_dl*(N_rb_dl-n+1)+(N_rb_dl-1-m))
//     //                         L_crb    = n;
//     //                         RB_start = m;
//     //                     endif
//     //                 endif
//     //             endfor
//     //         endfor

//     //Etsi 38.211 Table 4.4.2-1
//     uint16_t L_crb;
//     uint16_t RB_start;

//     uint16_t MAX_RB = 275; //MAX_RB = 138 for numerology 4. Only used in FR2 and therefore the implemented szenario
//     for(uint16_t n=1;n<=MAX_RB;++n)
//     {
//          for(uint16_t m=0;n<=MAX_RB-n;++n)
//          {
//             if((n-1) <= floor(MAX_RB/2))
//             {
//                 if(RIV == MAX_RB*(n-1)+m)
//                 {
//                      L_crb    = n;
//                      RB_start = m;
//                      return 200; //TODO?
//                 }
//                 else
//                 {
//                     if(RIV == MAX_RB*(MAX_RB-n+1)+(MAX_RB-1-m))
//                     {
//                         L_crb    = n;
//                         RB_start = m;
//                         return 200; //TODO?
//                     }
//                 }

//             }
//          }
//     }
//     NS_FATAL_ERROR("Couldnt decode Bw from RIV");

// }

Time
NrUeRrc::bwpInactivityToTime(NrRrcSap::Bwp_InactivityTimer value)
{
        NS_LOG_FUNCTION(this);
        

        return MilliSeconds(500);
}

void
NrUeRrc::ApplyMeasConfig(LteRrcSap::MeasConfig mc)
{
    NS_LOG_FUNCTION(this);

    // perform the actions specified in 3GPP TS 36.331 section 5.5.2.1

    // 3GPP TS 36.331 section 5.5.2.4 Measurement object removal
    for (std::list<uint8_t>::iterator it = mc.measObjectToRemoveList.begin();
         it != mc.measObjectToRemoveList.end();
         ++it)
    {
        uint8_t measObjectId = *it;
        NS_LOG_LOGIC(this << " deleting measObjectId " << (uint32_t)measObjectId);
        m_varMeasConfig.measObjectList.erase(measObjectId);
        std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
            m_varMeasConfig.measIdList.begin();
        while (measIdIt != m_varMeasConfig.measIdList.end())
        {
            if (measIdIt->second.measObjectId == measObjectId)
            {
                uint8_t measId = measIdIt->second.measId;
                NS_ASSERT(measId == measIdIt->first);
                NS_LOG_LOGIC(this << " deleting measId " << (uint32_t)measId
                                  << " because referring to measObjectId "
                                  << (uint32_t)measObjectId);
                // note: postfix operator preserves iterator validity
                m_varMeasConfig.measIdList.erase(measIdIt++);
                VarMeasReportListClear(measId);
            }
            else
            {
                ++measIdIt;
            }
        }
    }

    // 3GPP TS 36.331 section 5.5.2.5  Measurement object addition/ modification
    for (std::list<LteRrcSap::MeasObjectToAddMod>::iterator it = mc.measObjectToAddModList.begin();
         it != mc.measObjectToAddModList.end();
         ++it)
    {
        // simplifying assumptions
        NS_ASSERT_MSG(it->measObjectEutra.cellsToRemoveList.empty(),
                      "cellsToRemoveList not supported");
        NS_ASSERT_MSG(it->measObjectEutra.cellsToAddModList.empty(),
                      "cellsToAddModList not supported");
        NS_ASSERT_MSG(it->measObjectEutra.cellsToRemoveList.empty(),
                      "blackCellsToRemoveList not supported");
        NS_ASSERT_MSG(it->measObjectEutra.blackCellsToAddModList.empty(),
                      "blackCellsToAddModList not supported");
        NS_ASSERT_MSG(it->measObjectEutra.haveCellForWhichToReportCGI == false,
                      "cellForWhichToReportCGI is not supported");

        uint8_t measObjectId = it->measObjectId;
        std::map<uint8_t, LteRrcSap::MeasObjectToAddMod>::iterator measObjectIt =
            m_varMeasConfig.measObjectList.find(measObjectId);
        if (measObjectIt != m_varMeasConfig.measObjectList.end())
        {
            NS_LOG_LOGIC("measObjectId " << (uint32_t)measObjectId << " exists, updating entry");
            measObjectIt->second = *it;
            for (std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
                     m_varMeasConfig.measIdList.begin();
                 measIdIt != m_varMeasConfig.measIdList.end();
                 ++measIdIt)
            {
                if (measIdIt->second.measObjectId == measObjectId)
                {
                    uint8_t measId = measIdIt->second.measId;
                    NS_LOG_LOGIC(this << " found measId " << (uint32_t)measId
                                      << " referring to measObjectId " << (uint32_t)measObjectId);
                    VarMeasReportListClear(measId);
                }
            }
        }
        else
        {
            NS_LOG_LOGIC("measObjectId " << (uint32_t)measObjectId << " is new, adding entry");
            m_varMeasConfig.measObjectList[measObjectId] = *it;
        }
    }

    // 3GPP TS 36.331 section 5.5.2.6 Reporting configuration removal
    for (std::list<uint8_t>::iterator it = mc.reportConfigToRemoveList.begin();
         it != mc.reportConfigToRemoveList.end();
         ++it)
    {
        uint8_t reportConfigId = *it;
        NS_LOG_LOGIC(this << " deleting reportConfigId " << (uint32_t)reportConfigId);
        m_varMeasConfig.reportConfigList.erase(reportConfigId);
        std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
            m_varMeasConfig.measIdList.begin();
        while (measIdIt != m_varMeasConfig.measIdList.end())
        {
            if (measIdIt->second.reportConfigId == reportConfigId)
            {
                uint8_t measId = measIdIt->second.measId;
                NS_ASSERT(measId == measIdIt->first);
                NS_LOG_LOGIC(this << " deleting measId " << (uint32_t)measId
                                  << " because referring to reportConfigId "
                                  << (uint32_t)reportConfigId);
                // note: postfix operator preserves iterator validity
                m_varMeasConfig.measIdList.erase(measIdIt++);
                VarMeasReportListClear(measId);
            }
            else
            {
                ++measIdIt;
            }
        }
    }

    // 3GPP TS 36.331 section 5.5.2.7 Reporting configuration addition/ modification
    for (std::list<LteRrcSap::ReportConfigToAddMod>::iterator it =
             mc.reportConfigToAddModList.begin();
         it != mc.reportConfigToAddModList.end();
         ++it)
    {
        // simplifying assumptions
        NS_ASSERT_MSG(it->reportConfigEutra.triggerType == LteRrcSap::ReportConfigEutra::EVENT,
                      "only trigger type EVENT is supported");

        uint8_t reportConfigId = it->reportConfigId;
        std::map<uint8_t, LteRrcSap::ReportConfigToAddMod>::iterator reportConfigIt =
            m_varMeasConfig.reportConfigList.find(reportConfigId);
        if (reportConfigIt != m_varMeasConfig.reportConfigList.end())
        {
            NS_LOG_LOGIC("reportConfigId " << (uint32_t)reportConfigId
                                           << " exists, updating entry");
            m_varMeasConfig.reportConfigList[reportConfigId] = *it;
            for (std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
                     m_varMeasConfig.measIdList.begin();
                 measIdIt != m_varMeasConfig.measIdList.end();
                 ++measIdIt)
            {
                if (measIdIt->second.reportConfigId == reportConfigId)
                {
                    uint8_t measId = measIdIt->second.measId;
                    NS_LOG_LOGIC(this << " found measId " << (uint32_t)measId
                                      << " referring to reportConfigId "
                                      << (uint32_t)reportConfigId);
                    VarMeasReportListClear(measId);
                }
            }
        }
        else
        {
            NS_LOG_LOGIC("reportConfigId " << (uint32_t)reportConfigId << " is new, adding entry");
            m_varMeasConfig.reportConfigList[reportConfigId] = *it;
        }
    }

    // 3GPP TS 36.331 section 5.5.2.8 Quantity configuration
    if (mc.haveQuantityConfig)
    {
        NS_LOG_LOGIC(this << " setting quantityConfig");
        m_varMeasConfig.quantityConfig = mc.quantityConfig;
        // Convey the filter coefficient to PHY layer so it can configure the power control
        // parameter
        for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
        {
            m_cphySapProvider.at(i)->SetRsrpFilterCoefficient(
                mc.quantityConfig.filterCoefficientRSRP);
        }
        // we calculate here the coefficient a used for Layer 3 filtering, see 3GPP TS 36.331
        // section 5.5.3.2
        m_varMeasConfig.aRsrp = std::pow(0.5, mc.quantityConfig.filterCoefficientRSRP / 4.0);
        m_varMeasConfig.aRsrq = std::pow(0.5, mc.quantityConfig.filterCoefficientRSRQ / 4.0);
        NS_LOG_LOGIC(this << " new filter coefficients: aRsrp=" << m_varMeasConfig.aRsrp
                          << ", aRsrq=" << m_varMeasConfig.aRsrq);

        for (std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
                 m_varMeasConfig.measIdList.begin();
             measIdIt != m_varMeasConfig.measIdList.end();
             ++measIdIt)
        {
            VarMeasReportListClear(measIdIt->second.measId);
        }
    }

    // 3GPP TS 36.331 section 5.5.2.2 Measurement identity removal
    for (std::list<uint8_t>::iterator it = mc.measIdToRemoveList.begin();
         it != mc.measIdToRemoveList.end();
         ++it)
    {
        uint8_t measId = *it;
        NS_LOG_LOGIC(this << " deleting measId " << (uint32_t)measId);
        m_varMeasConfig.measIdList.erase(measId);
        VarMeasReportListClear(measId);

        // removing time-to-trigger queues
        m_enteringTriggerQueue.erase(measId);
        m_leavingTriggerQueue.erase(measId);
    }

    // 3GPP TS 36.331 section 5.5.2.3 Measurement identity addition/ modification
    for (std::list<LteRrcSap::MeasIdToAddMod>::iterator it = mc.measIdToAddModList.begin();
         it != mc.measIdToAddModList.end();
         ++it)
    {
        NS_LOG_LOGIC(this << " measId " << (uint32_t)it->measId
                          << " (measObjectId=" << (uint32_t)it->measObjectId
                          << ", reportConfigId=" << (uint32_t)it->reportConfigId << ")");
        NS_ASSERT(m_varMeasConfig.measObjectList.find(it->measObjectId) !=
                  m_varMeasConfig.measObjectList.end());
        NS_ASSERT(m_varMeasConfig.reportConfigList.find(it->reportConfigId) !=
                  m_varMeasConfig.reportConfigList.end());
        m_varMeasConfig.measIdList[it->measId] = *it; // side effect: create new entry if not exists
        std::map<uint8_t, VarMeasReport>::iterator measReportIt =
            m_varMeasReportList.find(it->measId);
        if (measReportIt != m_varMeasReportList.end())
        {
            measReportIt->second.periodicReportTimer.Cancel();
            m_varMeasReportList.erase(measReportIt);
        }
        NS_ASSERT(m_varMeasConfig.reportConfigList.find(it->reportConfigId)
                      ->second.reportConfigEutra.triggerType !=
                  LteRrcSap::ReportConfigEutra::PERIODICAL);

        // new empty queues for time-to-trigger
        std::list<PendingTrigger_t> s;
        m_enteringTriggerQueue[it->measId] = s;
        m_leavingTriggerQueue[it->measId] = s;
    }

    if (mc.haveMeasGapConfig)
    {
        NS_FATAL_ERROR("measurement gaps are currently not supported");
    }

    if (mc.haveSmeasure)
    {
        NS_FATAL_ERROR("s-measure is currently not supported");
    }

    if (mc.haveSpeedStatePars)
    {
        NS_FATAL_ERROR("SpeedStatePars are currently not supported");
    }
}

void
NrUeRrc::SaveUeMeasurements(uint16_t cellId,
                             double rsrp,
                             double rsrq,
                             bool useLayer3Filtering,
                             uint8_t componentCarrierId)
{
    NS_LOG_FUNCTION(this << cellId << +componentCarrierId << rsrp << rsrq << useLayer3Filtering);

    std::map<uint16_t, MeasValues>::iterator storedMeasIt = m_storedMeasValues.find(cellId);

    if (storedMeasIt != m_storedMeasValues.end())
    {
        if (useLayer3Filtering)
        {
            // F_n = (1-a) F_{n-1} + a M_n
            storedMeasIt->second.rsrp = (1 - m_varMeasConfig.aRsrp) * storedMeasIt->second.rsrp +
                                        m_varMeasConfig.aRsrp * rsrp;

            if (std::isnan(storedMeasIt->second.rsrq))
            {
                // the previous RSRQ measurements provided UE PHY are invalid
                storedMeasIt->second.rsrq = rsrq; // replace it with unfiltered value
            }
            else
            {
                storedMeasIt->second.rsrq =
                    (1 - m_varMeasConfig.aRsrq) * storedMeasIt->second.rsrq +
                    m_varMeasConfig.aRsrq * rsrq;
            }
        }
        else
        {
            storedMeasIt->second.rsrp = rsrp;
            storedMeasIt->second.rsrq = rsrq;
        }
    }
    else
    {
        // first value is always unfiltered
        MeasValues v;
        v.rsrp = rsrp;
        v.rsrq = rsrq;
        v.carrierFreq = m_cphySapProvider.at(componentCarrierId)->GetDlEarfcn();

        std::pair<uint16_t, MeasValues> val(cellId, v);
        std::pair<std::map<uint16_t, MeasValues>::iterator, bool> ret =
            m_storedMeasValues.insert(val);
        NS_ASSERT_MSG(ret.second == true, "element already existed");
        storedMeasIt = ret.first;
    }

    NS_LOG_DEBUG(this << " IMSI " << m_imsi << " state " << ToString(m_state) << ", measured cell "
                      << cellId << ", carrier component Id " << componentCarrierId << ", new RSRP "
                      << rsrp << " stored " << storedMeasIt->second.rsrp << ", new RSRQ " << rsrq
                      << " stored " << storedMeasIt->second.rsrq);

} // end of void SaveUeMeasurements

void
NrUeRrc::MeasurementReportTriggering(uint8_t measId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);

    std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
        m_varMeasConfig.measIdList.find(measId);
    NS_ASSERT(measIdIt != m_varMeasConfig.measIdList.end());
    NS_ASSERT(measIdIt->first == measIdIt->second.measId);

    std::map<uint8_t, LteRrcSap::ReportConfigToAddMod>::iterator reportConfigIt =
        m_varMeasConfig.reportConfigList.find(measIdIt->second.reportConfigId);
    NS_ASSERT(reportConfigIt != m_varMeasConfig.reportConfigList.end());
    LteRrcSap::ReportConfigEutra& reportConfigEutra = reportConfigIt->second.reportConfigEutra;

    std::map<uint8_t, LteRrcSap::MeasObjectToAddMod>::iterator measObjectIt =
        m_varMeasConfig.measObjectList.find(measIdIt->second.measObjectId);
    NS_ASSERT(measObjectIt != m_varMeasConfig.measObjectList.end());
    LteRrcSap::MeasObjectEutra& measObjectEutra = measObjectIt->second.measObjectEutra;

    std::map<uint8_t, VarMeasReport>::iterator measReportIt = m_varMeasReportList.find(measId);
    bool isMeasIdInReportList = (measReportIt != m_varMeasReportList.end());

    // we don't check the purpose field, as it is only included for
    // triggerType == periodical, which is not supported
    NS_ASSERT_MSG(reportConfigEutra.triggerType == LteRrcSap::ReportConfigEutra::EVENT,
                  "only triggerType == event is supported");
    // only EUTRA is supported, no need to check for it

    NS_LOG_LOGIC(this << " considering measId " << (uint32_t)measId);
    bool eventEntryCondApplicable = false;
    bool eventLeavingCondApplicable = false;
    ConcernedCells_t concernedCellsEntry;
    ConcernedCells_t concernedCellsLeaving;

    /*
     * Find which serving cell corresponds to measObjectEutra.carrierFreq
     * It is used, for example, by A1 event:
     * See TS 36.331 5.5.4.2: "for this measurement, consider the primary or
     * secondary cell that is configured on the frequency indicated in the
     * associated measObjectEUTRA to be the serving cell"
     */
    uint16_t servingCellId = 0;
    for (auto cphySapProvider : m_cphySapProvider)
    {
        if (cphySapProvider->GetDlEarfcn() == measObjectEutra.carrierFreq)
        {
            servingCellId = cphySapProvider->GetCellId();
        }
    }

    if (servingCellId == 0)
    {
        return;
    }

    switch (reportConfigEutra.eventId)
    {
    case LteRrcSap::ReportConfigEutra::EVENT_A1: {
        /*
         * Event A1 (Serving becomes better than threshold)
         * Please refer to 3GPP TS 36.331 Section 5.5.4.2
         */

        double ms;     // Ms, the measurement result of the serving cell
        double thresh; // Thresh, the threshold parameter for this event
        // Hys, the hysteresis parameter for this event.
        double hys =
            EutranMeasurementMapping::IeValue2ActualHysteresis(reportConfigEutra.hysteresis);

        switch (reportConfigEutra.triggerQuantity)
        {
        case LteRrcSap::ReportConfigEutra::RSRP:
            ms = m_storedMeasValues[servingCellId].rsrp;

            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            thresh = EutranMeasurementMapping::RsrpRange2Dbm(reportConfigEutra.threshold1.range);
            break;
        case LteRrcSap::ReportConfigEutra::RSRQ:
            ms = m_storedMeasValues[servingCellId].rsrq;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            thresh = EutranMeasurementMapping::RsrqRange2Db(reportConfigEutra.threshold1.range);
            break;
        default:
            NS_FATAL_ERROR("unsupported triggerQuantity");
            break;
        }

        // Inequality A1-1 (Entering condition): Ms - Hys > Thresh
        bool entryCond = ms - hys > thresh;

        if (entryCond)
        {
            if (!isMeasIdInReportList)
            {
                concernedCellsEntry.push_back(servingCellId);
                eventEntryCondApplicable = true;
            }
            else
            {
                /*
                 * This is to check that the triggered cell recorded in the
                 * VarMeasReportList is the serving cell.
                 */
                NS_ASSERT(measReportIt->second.cellsTriggeredList.find(servingCellId) !=
                          measReportIt->second.cellsTriggeredList.end());
            }
        }
        else if (reportConfigEutra.timeToTrigger > 0)
        {
            CancelEnteringTrigger(measId);
        }

        // Inequality A1-2 (Leaving condition): Ms + Hys < Thresh
        bool leavingCond = ms + hys < thresh;

        if (leavingCond)
        {
            if (isMeasIdInReportList)
            {
                /*
                 * This is to check that the triggered cell recorded in the
                 * VarMeasReportList is the serving cell.
                 */
                NS_ASSERT(measReportIt->second.cellsTriggeredList.find(m_cellId) !=
                          measReportIt->second.cellsTriggeredList.end());
                concernedCellsLeaving.push_back(m_cellId);
                eventLeavingCondApplicable = true;
            }
        }
        else if (reportConfigEutra.timeToTrigger > 0)
        {
            CancelLeavingTrigger(measId);
        }

        NS_LOG_LOGIC(this << " event A1: serving cell " << servingCellId << " ms=" << ms
                          << " thresh=" << thresh << " entryCond=" << entryCond
                          << " leavingCond=" << leavingCond);

    } // end of case LteRrcSap::ReportConfigEutra::EVENT_A1

    break;

    case LteRrcSap::ReportConfigEutra::EVENT_A2: {
        /*
         * Event A2 (Serving becomes worse than threshold)
         * Please refer to 3GPP TS 36.331 Section 5.5.4.3
         */

        double ms;     // Ms, the measurement result of the serving cell
        double thresh; // Thresh, the threshold parameter for this event
        // Hys, the hysteresis parameter for this event.
        double hys =
            EutranMeasurementMapping::IeValue2ActualHysteresis(reportConfigEutra.hysteresis);

        switch (reportConfigEutra.triggerQuantity)
        {
        case LteRrcSap::ReportConfigEutra::RSRP:
            ms = m_storedMeasValues[servingCellId].rsrp;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            thresh = EutranMeasurementMapping::RsrpRange2Dbm(reportConfigEutra.threshold1.range);
            break;
        case LteRrcSap::ReportConfigEutra::RSRQ:
            ms = m_storedMeasValues[servingCellId].rsrq;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            thresh = EutranMeasurementMapping::RsrqRange2Db(reportConfigEutra.threshold1.range);
            break;
        default:
            NS_FATAL_ERROR("unsupported triggerQuantity");
            break;
        }

        // Inequality A2-1 (Entering condition): Ms + Hys < Thresh
        bool entryCond = ms + hys < thresh;

        if (entryCond)
        {
            if (!isMeasIdInReportList)
            {
                concernedCellsEntry.push_back(servingCellId);
                eventEntryCondApplicable = true;
            }
            else
            {
                /*
                 * This is to check that the triggered cell recorded in the
                 * VarMeasReportList is the serving cell.
                 */
                NS_ASSERT(measReportIt->second.cellsTriggeredList.find(servingCellId) !=
                          measReportIt->second.cellsTriggeredList.end());
            }
        }
        else if (reportConfigEutra.timeToTrigger > 0)
        {
            CancelEnteringTrigger(measId);
        }

        // Inequality A2-2 (Leaving condition): Ms - Hys > Thresh
        bool leavingCond = ms - hys > thresh;

        if (leavingCond)
        {
            if (isMeasIdInReportList)
            {
                /*
                 * This is to check that the triggered cell recorded in the
                 * VarMeasReportList is the serving cell.
                 */
                NS_ASSERT(measReportIt->second.cellsTriggeredList.find(servingCellId) !=
                          measReportIt->second.cellsTriggeredList.end());
                concernedCellsLeaving.push_back(servingCellId);
                eventLeavingCondApplicable = true;
            }
        }
        else if (reportConfigEutra.timeToTrigger > 0)
        {
            CancelLeavingTrigger(measId);
        }

        NS_LOG_LOGIC(this << " event A2: serving cell " << servingCellId << " ms=" << ms
                          << " thresh=" << thresh << " entryCond=" << entryCond
                          << " leavingCond=" << leavingCond);

    } // end of case LteRrcSap::ReportConfigEutra::EVENT_A2

    break;

    case LteRrcSap::ReportConfigEutra::EVENT_A3: {
        /*
         * Event A3 (Neighbour becomes offset better than PCell)
         * Please refer to 3GPP TS 36.331 Section 5.5.4.4
         */

        double mn; // Mn, the measurement result of the neighbouring cell
        double ofn = measObjectEutra
                         .offsetFreq; // Ofn, the frequency specific offset of the frequency of the
        double ocn = 0.0;             // Ocn, the cell specific offset of the neighbour cell
        double mp;                    // Mp, the measurement result of the PCell
        double ofp = measObjectEutra
                         .offsetFreq; // Ofp, the frequency specific offset of the primary frequency
        double ocp = 0.0;             // Ocp, the cell specific offset of the PCell
        // Off, the offset parameter for this event.
        double off = EutranMeasurementMapping::IeValue2ActualA3Offset(reportConfigEutra.a3Offset);
        // Hys, the hysteresis parameter for this event.
        double hys =
            EutranMeasurementMapping::IeValue2ActualHysteresis(reportConfigEutra.hysteresis);

        switch (reportConfigEutra.triggerQuantity)
        {
        case LteRrcSap::ReportConfigEutra::RSRP:
            mp = m_storedMeasValues[m_cellId].rsrp;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            break;
        case LteRrcSap::ReportConfigEutra::RSRQ:
            mp = m_storedMeasValues[m_cellId].rsrq;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            break;
        default:
            NS_FATAL_ERROR("unsupported triggerQuantity");
            break;
        }

        for (std::map<uint16_t, MeasValues>::iterator storedMeasIt = m_storedMeasValues.begin();
             storedMeasIt != m_storedMeasValues.end();
             ++storedMeasIt)
        {
            uint16_t cellId = storedMeasIt->first;
            if (cellId == m_cellId)
            {
                continue;
            }

            // Only cell(s) on the frequency indicated in the associated measObject can trigger
            // event.
            if (m_storedMeasValues.at(cellId).carrierFreq != measObjectEutra.carrierFreq)
            {
                continue;
            }

            switch (reportConfigEutra.triggerQuantity)
            {
            case LteRrcSap::ReportConfigEutra::RSRP:
                mn = storedMeasIt->second.rsrp;
                break;
            case LteRrcSap::ReportConfigEutra::RSRQ:
                mn = storedMeasIt->second.rsrq;
                break;
            default:
                NS_FATAL_ERROR("unsupported triggerQuantity");
                break;
            }

            bool hasTriggered =
                isMeasIdInReportList && (measReportIt->second.cellsTriggeredList.find(cellId) !=
                                         measReportIt->second.cellsTriggeredList.end());

            // Inequality A3-1 (Entering condition): Mn + Ofn + Ocn - Hys > Mp + Ofp + Ocp + Off
            bool entryCond = mn + ofn + ocn - hys > mp + ofp + ocp + off;

            if (entryCond)
            {
                if (!hasTriggered)
                {
                    concernedCellsEntry.push_back(cellId);
                    eventEntryCondApplicable = true;
                }
            }
            else if (reportConfigEutra.timeToTrigger > 0)
            {
                CancelEnteringTrigger(measId, cellId);
            }

            // Inequality A3-2 (Leaving condition): Mn + Ofn + Ocn + Hys < Mp + Ofp + Ocp + Off
            bool leavingCond = mn + ofn + ocn + hys < mp + ofp + ocp + off;

            if (leavingCond)
            {
                if (hasTriggered)
                {
                    concernedCellsLeaving.push_back(cellId);
                    eventLeavingCondApplicable = true;
                }
            }
            else if (reportConfigEutra.timeToTrigger > 0)
            {
                CancelLeavingTrigger(measId, cellId);
            }

            NS_LOG_LOGIC(this << " event A3: neighbor cell " << cellId << " mn=" << mn
                              << " mp=" << mp << " offset=" << off << " entryCond=" << entryCond
                              << " leavingCond=" << leavingCond);

        } // end of for (storedMeasIt)

    } // end of case LteRrcSap::ReportConfigEutra::EVENT_A3

    break;

    case LteRrcSap::ReportConfigEutra::EVENT_A4: {
        /*
         * Event A4 (Neighbour becomes better than threshold)
         * Please refer to 3GPP TS 36.331 Section 5.5.4.5
         */

        double mn; // Mn, the measurement result of the neighbouring cell
        double ofn = measObjectEutra
                         .offsetFreq; // Ofn, the frequency specific offset of the frequency of the
        double ocn = 0.0;             // Ocn, the cell specific offset of the neighbour cell
        double thresh;                // Thresh, the threshold parameter for this event
        // Hys, the hysteresis parameter for this event.
        double hys =
            EutranMeasurementMapping::IeValue2ActualHysteresis(reportConfigEutra.hysteresis);

        switch (reportConfigEutra.triggerQuantity)
        {
        case LteRrcSap::ReportConfigEutra::RSRP:
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            thresh = EutranMeasurementMapping::RsrpRange2Dbm(reportConfigEutra.threshold1.range);
            break;
        case LteRrcSap::ReportConfigEutra::RSRQ:
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            thresh = EutranMeasurementMapping::RsrqRange2Db(reportConfigEutra.threshold1.range);
            break;
        default:
            NS_FATAL_ERROR("unsupported triggerQuantity");
            break;
        }

        for (std::map<uint16_t, MeasValues>::iterator storedMeasIt = m_storedMeasValues.begin();
             storedMeasIt != m_storedMeasValues.end();
             ++storedMeasIt)
        {
            uint16_t cellId = storedMeasIt->first;
            if (cellId == m_cellId)
            {
                continue;
            }

            switch (reportConfigEutra.triggerQuantity)
            {
            case LteRrcSap::ReportConfigEutra::RSRP:
                mn = storedMeasIt->second.rsrp;
                break;
            case LteRrcSap::ReportConfigEutra::RSRQ:
                mn = storedMeasIt->second.rsrq;
                break;
            default:
                NS_FATAL_ERROR("unsupported triggerQuantity");
                break;
            }

            bool hasTriggered =
                isMeasIdInReportList && (measReportIt->second.cellsTriggeredList.find(cellId) !=
                                         measReportIt->second.cellsTriggeredList.end());

            // Inequality A4-1 (Entering condition): Mn + Ofn + Ocn - Hys > Thresh
            bool entryCond = mn + ofn + ocn - hys > thresh;

            if (entryCond)
            {
                if (!hasTriggered)
                {
                    concernedCellsEntry.push_back(cellId);
                    eventEntryCondApplicable = true;
                }
            }
            else if (reportConfigEutra.timeToTrigger > 0)
            {
                CancelEnteringTrigger(measId, cellId);
            }

            // Inequality A4-2 (Leaving condition): Mn + Ofn + Ocn + Hys < Thresh
            bool leavingCond = mn + ofn + ocn + hys < thresh;

            if (leavingCond)
            {
                if (hasTriggered)
                {
                    concernedCellsLeaving.push_back(cellId);
                    eventLeavingCondApplicable = true;
                }
            }
            else if (reportConfigEutra.timeToTrigger > 0)
            {
                CancelLeavingTrigger(measId, cellId);
            }

            NS_LOG_LOGIC(this << " event A4: neighbor cell " << cellId << " mn=" << mn
                              << " thresh=" << thresh << " entryCond=" << entryCond
                              << " leavingCond=" << leavingCond);

        } // end of for (storedMeasIt)

    } // end of case LteRrcSap::ReportConfigEutra::EVENT_A4

    break;

    case LteRrcSap::ReportConfigEutra::EVENT_A5: {
        /*
         * Event A5 (PCell becomes worse than threshold1 and neighbour
         * becomes better than threshold2)
         * Please refer to 3GPP TS 36.331 Section 5.5.4.6
         */

        double mp; // Mp, the measurement result of the PCell
        double mn; // Mn, the measurement result of the neighbouring cell
        double ofn = measObjectEutra
                         .offsetFreq; // Ofn, the frequency specific offset of the frequency of the
        double ocn = 0.0;             // Ocn, the cell specific offset of the neighbour cell
        double thresh1;               // Thresh1, the threshold parameter for this event
        double thresh2;               // Thresh2, the threshold parameter for this event
        // Hys, the hysteresis parameter for this event.
        double hys =
            EutranMeasurementMapping::IeValue2ActualHysteresis(reportConfigEutra.hysteresis);

        switch (reportConfigEutra.triggerQuantity)
        {
        case LteRrcSap::ReportConfigEutra::RSRP:
            mp = m_storedMeasValues[m_cellId].rsrp;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            NS_ASSERT(reportConfigEutra.threshold2.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRP);
            thresh1 = EutranMeasurementMapping::RsrpRange2Dbm(reportConfigEutra.threshold1.range);
            thresh2 = EutranMeasurementMapping::RsrpRange2Dbm(reportConfigEutra.threshold2.range);
            break;
        case LteRrcSap::ReportConfigEutra::RSRQ:
            mp = m_storedMeasValues[m_cellId].rsrq;
            NS_ASSERT(reportConfigEutra.threshold1.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            NS_ASSERT(reportConfigEutra.threshold2.choice ==
                      LteRrcSap::ThresholdEutra::THRESHOLD_RSRQ);
            thresh1 = EutranMeasurementMapping::RsrqRange2Db(reportConfigEutra.threshold1.range);
            thresh2 = EutranMeasurementMapping::RsrqRange2Db(reportConfigEutra.threshold2.range);
            break;
        default:
            NS_FATAL_ERROR("unsupported triggerQuantity");
            break;
        }

        // Inequality A5-1 (Entering condition 1): Mp + Hys < Thresh1
        bool entryCond = mp + hys < thresh1;

        if (entryCond)
        {
            for (std::map<uint16_t, MeasValues>::iterator storedMeasIt = m_storedMeasValues.begin();
                 storedMeasIt != m_storedMeasValues.end();
                 ++storedMeasIt)
            {
                uint16_t cellId = storedMeasIt->first;
                if (cellId == m_cellId)
                {
                    continue;
                }

                switch (reportConfigEutra.triggerQuantity)
                {
                case LteRrcSap::ReportConfigEutra::RSRP:
                    mn = storedMeasIt->second.rsrp;
                    break;
                case LteRrcSap::ReportConfigEutra::RSRQ:
                    mn = storedMeasIt->second.rsrq;
                    break;
                default:
                    NS_FATAL_ERROR("unsupported triggerQuantity");
                    break;
                }

                bool hasTriggered =
                    isMeasIdInReportList && (measReportIt->second.cellsTriggeredList.find(cellId) !=
                                             measReportIt->second.cellsTriggeredList.end());

                // Inequality A5-2 (Entering condition 2): Mn + Ofn + Ocn - Hys > Thresh2

                entryCond = mn + ofn + ocn - hys > thresh2;

                if (entryCond)
                {
                    if (!hasTriggered)
                    {
                        concernedCellsEntry.push_back(cellId);
                        eventEntryCondApplicable = true;
                    }
                }
                else if (reportConfigEutra.timeToTrigger > 0)
                {
                    CancelEnteringTrigger(measId, cellId);
                }

                NS_LOG_LOGIC(this << " event A5: neighbor cell " << cellId << " mn=" << mn
                                  << " mp=" << mp << " thresh2=" << thresh2
                                  << " thresh1=" << thresh1 << " entryCond=" << entryCond);

            } // end of for (storedMeasIt)

        } // end of if (entryCond)
        else
        {
            NS_LOG_LOGIC(this << " event A5: serving cell " << m_cellId << " mp=" << mp
                              << " thresh1=" << thresh1 << " entryCond=" << entryCond);

            if (reportConfigEutra.timeToTrigger > 0)
            {
                CancelEnteringTrigger(measId);
            }
        }

        if (isMeasIdInReportList)
        {
            // Inequality A5-3 (Leaving condition 1): Mp - Hys > Thresh1
            bool leavingCond = mp - hys > thresh1;

            if (leavingCond)
            {
                if (reportConfigEutra.timeToTrigger == 0)
                {
                    // leaving condition #2 does not have to be checked

                    for (std::map<uint16_t, MeasValues>::iterator storedMeasIt =
                             m_storedMeasValues.begin();
                         storedMeasIt != m_storedMeasValues.end();
                         ++storedMeasIt)
                    {
                        uint16_t cellId = storedMeasIt->first;
                        if (cellId == m_cellId)
                        {
                            continue;
                        }

                        if (measReportIt->second.cellsTriggeredList.find(cellId) !=
                            measReportIt->second.cellsTriggeredList.end())
                        {
                            concernedCellsLeaving.push_back(cellId);
                            eventLeavingCondApplicable = true;
                        }
                    }
                } // end of if (reportConfigEutra.timeToTrigger == 0)
                else
                {
                    // leaving condition #2 has to be checked to cancel time-to-trigger

                    for (std::map<uint16_t, MeasValues>::iterator storedMeasIt =
                             m_storedMeasValues.begin();
                         storedMeasIt != m_storedMeasValues.end();
                         ++storedMeasIt)
                    {
                        uint16_t cellId = storedMeasIt->first;
                        if (cellId == m_cellId)
                        {
                            continue;
                        }

                        if (measReportIt->second.cellsTriggeredList.find(cellId) !=
                            measReportIt->second.cellsTriggeredList.end())
                        {
                            switch (reportConfigEutra.triggerQuantity)
                            {
                            case LteRrcSap::ReportConfigEutra::RSRP:
                                mn = storedMeasIt->second.rsrp;
                                break;
                            case LteRrcSap::ReportConfigEutra::RSRQ:
                                mn = storedMeasIt->second.rsrq;
                                break;
                            default:
                                NS_FATAL_ERROR("unsupported triggerQuantity");
                                break;
                            }

                            // Inequality A5-4 (Leaving condition 2): Mn + Ofn + Ocn + Hys < Thresh2

                            leavingCond = mn + ofn + ocn + hys < thresh2;

                            if (!leavingCond)
                            {
                                CancelLeavingTrigger(measId, cellId);
                            }

                            /*
                             * Whatever the result of leaving condition #2, this
                             * cell is still "in", because leaving condition #1
                             * is already true.
                             */
                            concernedCellsLeaving.push_back(cellId);
                            eventLeavingCondApplicable = true;

                            NS_LOG_LOGIC(this << " event A5: neighbor cell " << cellId
                                              << " mn=" << mn << " mp=" << mp
                                              << " thresh2=" << thresh2 << " thresh1=" << thresh1
                                              << " leavingCond=" << leavingCond);

                        } // end of if (measReportIt->second.cellsTriggeredList.find (cellId)
                          //            != measReportIt->second.cellsTriggeredList.end ())

                    } // end of for (storedMeasIt)

                } // end of else of if (reportConfigEutra.timeToTrigger == 0)

                NS_LOG_LOGIC(this << " event A5: serving cell " << m_cellId << " mp=" << mp
                                  << " thresh1=" << thresh1 << " leavingCond=" << leavingCond);

            } // end of if (leavingCond)
            else
            {
                if (reportConfigEutra.timeToTrigger > 0)
                {
                    CancelLeavingTrigger(measId);
                }

                // check leaving condition #2

                for (std::map<uint16_t, MeasValues>::iterator storedMeasIt =
                         m_storedMeasValues.begin();
                     storedMeasIt != m_storedMeasValues.end();
                     ++storedMeasIt)
                {
                    uint16_t cellId = storedMeasIt->first;
                    if (cellId == m_cellId)
                    {
                        continue;
                    }

                    if (measReportIt->second.cellsTriggeredList.find(cellId) !=
                        measReportIt->second.cellsTriggeredList.end())
                    {
                        switch (reportConfigEutra.triggerQuantity)
                        {
                        case LteRrcSap::ReportConfigEutra::RSRP:
                            mn = storedMeasIt->second.rsrp;
                            break;
                        case LteRrcSap::ReportConfigEutra::RSRQ:
                            mn = storedMeasIt->second.rsrq;
                            break;
                        default:
                            NS_FATAL_ERROR("unsupported triggerQuantity");
                            break;
                        }

                        // Inequality A5-4 (Leaving condition 2): Mn + Ofn + Ocn + Hys < Thresh2
                        leavingCond = mn + ofn + ocn + hys < thresh2;

                        if (leavingCond)
                        {
                            concernedCellsLeaving.push_back(cellId);
                            eventLeavingCondApplicable = true;
                        }

                        NS_LOG_LOGIC(this << " event A5: neighbor cell " << cellId << " mn=" << mn
                                          << " mp=" << mp << " thresh2=" << thresh2 << " thresh1="
                                          << thresh1 << " leavingCond=" << leavingCond);

                    } // end of if (measReportIt->second.cellsTriggeredList.find (cellId)
                      //            != measReportIt->second.cellsTriggeredList.end ())

                } // end of for (storedMeasIt)

            } // end of else of if (leavingCond)

        } // end of if (isMeasIdInReportList)

    } // end of case LteRrcSap::ReportConfigEutra::EVENT_A5

    break;

    default:
        NS_FATAL_ERROR("unsupported eventId " << reportConfigEutra.eventId);
        break;

    } // switch (event type)

    NS_LOG_LOGIC(this << " eventEntryCondApplicable=" << eventEntryCondApplicable
                      << " eventLeavingCondApplicable=" << eventLeavingCondApplicable);

    if (eventEntryCondApplicable)
    {
        if (reportConfigEutra.timeToTrigger == 0)
        {
            VarMeasReportListAdd(measId, concernedCellsEntry);
        }
        else
        {
            PendingTrigger_t t;
            t.measId = measId;
            t.concernedCells = concernedCellsEntry;
            t.timer = Simulator::Schedule(MilliSeconds(reportConfigEutra.timeToTrigger),
                                          &NrUeRrc::VarMeasReportListAdd,
                                          this,
                                          measId,
                                          concernedCellsEntry);
            std::map<uint8_t, std::list<PendingTrigger_t>>::iterator enteringTriggerIt =
                m_enteringTriggerQueue.find(measId);
            NS_ASSERT(enteringTriggerIt != m_enteringTriggerQueue.end());
            enteringTriggerIt->second.push_back(t);
        }
    }

    if (eventLeavingCondApplicable)
    {
        // reportOnLeave will only be set when eventId = eventA3
        bool reportOnLeave =
            (reportConfigEutra.eventId == LteRrcSap::ReportConfigEutra::EVENT_A3) &&
            reportConfigEutra.reportOnLeave;

        if (reportConfigEutra.timeToTrigger == 0)
        {
            VarMeasReportListErase(measId, concernedCellsLeaving, reportOnLeave);
        }
        else
        {
            PendingTrigger_t t;
            t.measId = measId;
            t.concernedCells = concernedCellsLeaving;
            t.timer = Simulator::Schedule(MilliSeconds(reportConfigEutra.timeToTrigger),
                                          &NrUeRrc::VarMeasReportListErase,
                                          this,
                                          measId,
                                          concernedCellsLeaving,
                                          reportOnLeave);
            std::map<uint8_t, std::list<PendingTrigger_t>>::iterator leavingTriggerIt =
                m_leavingTriggerQueue.find(measId);
            NS_ASSERT(leavingTriggerIt != m_leavingTriggerQueue.end());
            leavingTriggerIt->second.push_back(t);
        }
    }

} // end of void NrUeRrc::MeasurementReportTriggering (uint8_t measId)

void
NrUeRrc::CancelEnteringTrigger(uint8_t measId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator it1 =
        m_enteringTriggerQueue.find(measId);
    NS_ASSERT(it1 != m_enteringTriggerQueue.end());

    if (!it1->second.empty())
    {
        std::list<PendingTrigger_t>::iterator it2;
        for (it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
        {
            NS_ASSERT(it2->measId == measId);
            NS_LOG_LOGIC(this << " canceling entering time-to-trigger event at "
                              << Simulator::GetDelayLeft(it2->timer).GetSeconds());
            Simulator::Cancel(it2->timer);
        }

        it1->second.clear();
    }
}

void
NrUeRrc::CancelEnteringTrigger(uint8_t measId, uint16_t cellId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId << cellId);

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator it1 =
        m_enteringTriggerQueue.find(measId);
    NS_ASSERT(it1 != m_enteringTriggerQueue.end());

    std::list<PendingTrigger_t>::iterator it2 = it1->second.begin();
    while (it2 != it1->second.end())
    {
        NS_ASSERT(it2->measId == measId);

        ConcernedCells_t::iterator it3;
        for (it3 = it2->concernedCells.begin(); it3 != it2->concernedCells.end(); ++it3)
        {
            if (*it3 == cellId)
            {
                it3 = it2->concernedCells.erase(it3);
            }
        }

        if (it2->concernedCells.empty())
        {
            NS_LOG_LOGIC(this << " canceling entering time-to-trigger event at "
                              << Simulator::GetDelayLeft(it2->timer).GetSeconds());
            Simulator::Cancel(it2->timer);
            it2 = it1->second.erase(it2);
        }
        else
        {
            it2++;
        }
    }
}

void
NrUeRrc::CancelLeavingTrigger(uint8_t measId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator it1 =
        m_leavingTriggerQueue.find(measId);
    NS_ASSERT(it1 != m_leavingTriggerQueue.end());

    if (!it1->second.empty())
    {
        std::list<PendingTrigger_t>::iterator it2;
        for (it2 = it1->second.begin(); it2 != it1->second.end(); ++it2)
        {
            NS_ASSERT(it2->measId == measId);
            NS_LOG_LOGIC(this << " canceling leaving time-to-trigger event at "
                              << Simulator::GetDelayLeft(it2->timer).GetSeconds());
            Simulator::Cancel(it2->timer);
        }

        it1->second.clear();
    }
}

void
NrUeRrc::CancelLeavingTrigger(uint8_t measId, uint16_t cellId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId << cellId);

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator it1 =
        m_leavingTriggerQueue.find(measId);
    NS_ASSERT(it1 != m_leavingTriggerQueue.end());

    std::list<PendingTrigger_t>::iterator it2 = it1->second.begin();
    while (it2 != it1->second.end())
    {
        NS_ASSERT(it2->measId == measId);

        ConcernedCells_t::iterator it3;
        for (it3 = it2->concernedCells.begin(); it3 != it2->concernedCells.end(); ++it3)
        {
            if (*it3 == cellId)
            {
                it3 = it2->concernedCells.erase(it3);
            }
        }

        if (it2->concernedCells.empty())
        {
            NS_LOG_LOGIC(this << " canceling leaving time-to-trigger event at "
                              << Simulator::GetDelayLeft(it2->timer).GetSeconds());
            Simulator::Cancel(it2->timer);
            it2 = it1->second.erase(it2);
        }
        else
        {
            it2++;
        }
    }
}

void
NrUeRrc::VarMeasReportListAdd(uint8_t measId, ConcernedCells_t enteringCells)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);
    NS_ASSERT(!enteringCells.empty());

    std::map<uint8_t, VarMeasReport>::iterator measReportIt = m_varMeasReportList.find(measId);

    if (measReportIt == m_varMeasReportList.end())
    {
        VarMeasReport r;
        r.measId = measId;
        std::pair<uint8_t, VarMeasReport> val(measId, r);
        std::pair<std::map<uint8_t, VarMeasReport>::iterator, bool> ret =
            m_varMeasReportList.insert(val);
        NS_ASSERT_MSG(ret.second == true, "element already existed");
        measReportIt = ret.first;
    }

    NS_ASSERT(measReportIt != m_varMeasReportList.end());

    for (ConcernedCells_t::const_iterator it = enteringCells.begin(); it != enteringCells.end();
         ++it)
    {
        measReportIt->second.cellsTriggeredList.insert(*it);
    }

    NS_ASSERT(!measReportIt->second.cellsTriggeredList.empty());

    // #issue 224, schedule only when there is no periodic event scheduled already
    if (!measReportIt->second.periodicReportTimer.IsRunning())
    {
        measReportIt->second.numberOfReportsSent = 0;
        measReportIt->second.periodicReportTimer =
            Simulator::Schedule(UE_MEASUREMENT_REPORT_DELAY_NR,
                                &NrUeRrc::SendMeasurementReport,
                                this,
                                measId);
    }

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator enteringTriggerIt =
        m_enteringTriggerQueue.find(measId);
    NS_ASSERT(enteringTriggerIt != m_enteringTriggerQueue.end());
    if (!enteringTriggerIt->second.empty())
    {
        /*
         * Assumptions at this point:
         *  - the call to this function was delayed by time-to-trigger;
         *  - the time-to-trigger delay is fixed (not adaptive/dynamic); and
         *  - the first element in the list is associated with this function call.
         */
        enteringTriggerIt->second.pop_front();

        if (!enteringTriggerIt->second.empty())
        {
            /*
             * To prevent the same set of cells triggering again in the future,
             * we clean up the time-to-trigger queue. This case might occur when
             * time-to-trigger > 200 ms.
             */
            for (ConcernedCells_t::const_iterator it = enteringCells.begin();
                 it != enteringCells.end();
                 ++it)
            {
                CancelEnteringTrigger(measId, *it);
            }
        }

    } // end of if (!enteringTriggerIt->second.empty ())

} // end of NrUeRrc::VarMeasReportListAdd

void
NrUeRrc::VarMeasReportListErase(uint8_t measId, ConcernedCells_t leavingCells, bool reportOnLeave)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);
    NS_ASSERT(!leavingCells.empty());

    std::map<uint8_t, VarMeasReport>::iterator measReportIt = m_varMeasReportList.find(measId);
    NS_ASSERT(measReportIt != m_varMeasReportList.end());

    for (ConcernedCells_t::const_iterator it = leavingCells.begin(); it != leavingCells.end(); ++it)
    {
        measReportIt->second.cellsTriggeredList.erase(*it);
    }

    if (reportOnLeave)
    {
        // runs immediately without UE_MEASUREMENT_REPORT_DELAY_NR
        SendMeasurementReport(measId);
    }

    if (measReportIt->second.cellsTriggeredList.empty())
    {
        measReportIt->second.periodicReportTimer.Cancel();
        m_varMeasReportList.erase(measReportIt);
    }

    std::map<uint8_t, std::list<PendingTrigger_t>>::iterator leavingTriggerIt =
        m_leavingTriggerQueue.find(measId);
    NS_ASSERT(leavingTriggerIt != m_leavingTriggerQueue.end());
    if (!leavingTriggerIt->second.empty())
    {
        /*
         * Assumptions at this point:
         *  - the call to this function was delayed by time-to-trigger; and
         *  - the time-to-trigger delay is fixed (not adaptive/dynamic); and
         *  - the first element in the list is associated with this function call.
         */
        leavingTriggerIt->second.pop_front();

        if (!leavingTriggerIt->second.empty())
        {
            /*
             * To prevent the same set of cells triggering again in the future,
             * we clean up the time-to-trigger queue. This case might occur when
             * time-to-trigger > 200 ms.
             */
            for (ConcernedCells_t::const_iterator it = leavingCells.begin();
                 it != leavingCells.end();
                 ++it)
            {
                CancelLeavingTrigger(measId, *it);
            }
        }

    } // end of if (!leavingTriggerIt->second.empty ())

} // end of NrUeRrc::VarMeasReportListErase

void
NrUeRrc::VarMeasReportListClear(uint8_t measId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);

    // remove the measurement reporting entry for this measId from the VarMeasReportList
    std::map<uint8_t, VarMeasReport>::iterator measReportIt = m_varMeasReportList.find(measId);
    if (measReportIt != m_varMeasReportList.end())
    {
        NS_LOG_LOGIC(this << " deleting existing report for measId " << (uint16_t)measId);
        measReportIt->second.periodicReportTimer.Cancel();
        m_varMeasReportList.erase(measReportIt);
    }

    CancelEnteringTrigger(measId);
    CancelLeavingTrigger(measId);
}

void
NrUeRrc::SendMeasurementReport(uint8_t measId)
{
    NS_LOG_FUNCTION(this << (uint16_t)measId);
    //  3GPP TS 36.331 section 5.5.5 Measurement reporting

    std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt =
        m_varMeasConfig.measIdList.find(measId);
    NS_ASSERT(measIdIt != m_varMeasConfig.measIdList.end());

    std::map<uint8_t, LteRrcSap::ReportConfigToAddMod>::iterator reportConfigIt =
        m_varMeasConfig.reportConfigList.find(measIdIt->second.reportConfigId);
    NS_ASSERT(reportConfigIt != m_varMeasConfig.reportConfigList.end());
    LteRrcSap::ReportConfigEutra& reportConfigEutra = reportConfigIt->second.reportConfigEutra;

    LteRrcSap::MeasurementReport measurementReport;
    LteRrcSap::MeasResults& measResults = measurementReport.measResults;
    measResults.measId = measId;

    std::map<uint8_t, VarMeasReport>::iterator measReportIt = m_varMeasReportList.find(measId);
    if (measReportIt == m_varMeasReportList.end())
    {
        NS_LOG_ERROR("no entry found in m_varMeasReportList for measId " << (uint32_t)measId);
    }
    else
    {
        std::map<uint16_t, MeasValues>::iterator servingMeasIt = m_storedMeasValues.find(m_cellId);
        NS_ASSERT(servingMeasIt != m_storedMeasValues.end());
        measResults.measResultPCell.rsrpResult =
            EutranMeasurementMapping::Dbm2RsrpRange(servingMeasIt->second.rsrp);
        measResults.measResultPCell.rsrqResult =
            EutranMeasurementMapping::Db2RsrqRange(servingMeasIt->second.rsrq);
        NS_LOG_INFO(this << " reporting serving cell "
                            "RSRP "
                         << +measResults.measResultPCell.rsrpResult << " ("
                         << servingMeasIt->second.rsrp
                         << " dBm) "
                            "RSRQ "
                         << +measResults.measResultPCell.rsrqResult << " ("
                         << servingMeasIt->second.rsrq << " dB)");

        measResults.haveMeasResultServFreqList = false;
        for (uint16_t componentCarrierId = 1; componentCarrierId < m_numberOfComponentCarriers;
             componentCarrierId++)
        {
            const uint16_t cellId = m_cphySapProvider.at(componentCarrierId)->GetCellId();
            auto measValuesIt = m_storedMeasValues.find(cellId);
            if (measValuesIt != m_storedMeasValues.end())
            {
                measResults.haveMeasResultServFreqList = true;
                LteRrcSap::MeasResultServFreq measResultServFreq;
                measResultServFreq.servFreqId = componentCarrierId;
                measResultServFreq.haveMeasResultSCell = true;
                measResultServFreq.measResultSCell.rsrpResult =
                    EutranMeasurementMapping::Dbm2RsrpRange(measValuesIt->second.rsrp);
                measResultServFreq.measResultSCell.rsrqResult =
                    EutranMeasurementMapping::Db2RsrqRange(measValuesIt->second.rsrq);
                measResultServFreq.haveMeasResultBestNeighCell = false;
                measResults.measResultServFreqList.push_back(measResultServFreq);
            }
        }

        measResults.haveMeasResultNeighCells = false;

        if (!(measReportIt->second.cellsTriggeredList.empty()))
        {
            std::multimap<double, uint16_t> sortedNeighCells;
            for (std::set<uint16_t>::iterator cellsTriggeredIt =
                     measReportIt->second.cellsTriggeredList.begin();
                 cellsTriggeredIt != measReportIt->second.cellsTriggeredList.end();
                 ++cellsTriggeredIt)
            {
                uint16_t cellId = *cellsTriggeredIt;
                if (cellId != m_cellId)
                {
                    std::map<uint16_t, MeasValues>::iterator neighborMeasIt =
                        m_storedMeasValues.find(cellId);
                    double triggerValue;
                    switch (reportConfigEutra.triggerQuantity)
                    {
                    case LteRrcSap::ReportConfigEutra::RSRP:
                        triggerValue = neighborMeasIt->second.rsrp;
                        break;
                    case LteRrcSap::ReportConfigEutra::RSRQ:
                        triggerValue = neighborMeasIt->second.rsrq;
                        break;
                    default:
                        NS_FATAL_ERROR("unsupported triggerQuantity");
                        break;
                    }
                    sortedNeighCells.insert(std::pair<double, uint16_t>(triggerValue, cellId));
                }
            }

            std::multimap<double, uint16_t>::reverse_iterator sortedNeighCellsIt;
            uint32_t count;
            for (sortedNeighCellsIt = sortedNeighCells.rbegin(), count = 0;
                 sortedNeighCellsIt != sortedNeighCells.rend() &&
                 count < reportConfigEutra.maxReportCells;
                 ++sortedNeighCellsIt, ++count)
            {
                uint16_t cellId = sortedNeighCellsIt->second;
                std::map<uint16_t, MeasValues>::iterator neighborMeasIt =
                    m_storedMeasValues.find(cellId);
                NS_ASSERT(neighborMeasIt != m_storedMeasValues.end());
                LteRrcSap::MeasResultEutra measResultEutra;
                measResultEutra.physCellId = cellId;
                measResultEutra.haveCgiInfo = false;
                measResultEutra.haveRsrpResult = true;
                measResultEutra.rsrpResult =
                    EutranMeasurementMapping::Dbm2RsrpRange(neighborMeasIt->second.rsrp);
                measResultEutra.haveRsrqResult = true;
                measResultEutra.rsrqResult =
                    EutranMeasurementMapping::Db2RsrqRange(neighborMeasIt->second.rsrq);
                NS_LOG_INFO(this << " reporting neighbor cell "
                                 << (uint32_t)measResultEutra.physCellId << " RSRP "
                                 << (uint32_t)measResultEutra.rsrpResult << " ("
                                 << neighborMeasIt->second.rsrp << " dBm)"
                                 << " RSRQ " << (uint32_t)measResultEutra.rsrqResult << " ("
                                 << neighborMeasIt->second.rsrq << " dB)");
                measResults.measResultListEutra.push_back(measResultEutra);
                measResults.haveMeasResultNeighCells = true;
            }
        }
        else
        {
            NS_LOG_WARN(this << " cellsTriggeredList is empty");
        }

        /*
         * The current LteRrcSap implementation is broken in that it does not
         * allow for infinite values of reportAmount, which is probably the most
         * reasonable setting. So we just always assume infinite reportAmount.
         */
        measReportIt->second.numberOfReportsSent++;
        measReportIt->second.periodicReportTimer.Cancel();

        Time reportInterval;
        switch (reportConfigEutra.reportInterval)
        {
        case LteRrcSap::ReportConfigEutra::MS120:
            reportInterval = MilliSeconds(120);
            break;
        case LteRrcSap::ReportConfigEutra::MS240:
            reportInterval = MilliSeconds(240);
            break;
        case LteRrcSap::ReportConfigEutra::MS480:
            reportInterval = MilliSeconds(480);
            break;
        case LteRrcSap::ReportConfigEutra::MS640:
            reportInterval = MilliSeconds(640);
            break;
        case LteRrcSap::ReportConfigEutra::MS1024:
            reportInterval = MilliSeconds(1024);
            break;
        case LteRrcSap::ReportConfigEutra::MS2048:
            reportInterval = MilliSeconds(2048);
            break;
        case LteRrcSap::ReportConfigEutra::MS5120:
            reportInterval = MilliSeconds(5120);
            break;
        case LteRrcSap::ReportConfigEutra::MS10240:
            reportInterval = MilliSeconds(10240);
            break;
        case LteRrcSap::ReportConfigEutra::MIN1:
            reportInterval = Seconds(60);
            break;
        case LteRrcSap::ReportConfigEutra::MIN6:
            reportInterval = Seconds(360);
            break;
        case LteRrcSap::ReportConfigEutra::MIN12:
            reportInterval = Seconds(720);
            break;
        case LteRrcSap::ReportConfigEutra::MIN30:
            reportInterval = Seconds(1800);
            break;
        case LteRrcSap::ReportConfigEutra::MIN60:
            reportInterval = Seconds(3600);
            break;
        default:
            NS_FATAL_ERROR("Unsupported reportInterval "
                           << (uint16_t)reportConfigEutra.reportInterval);
            break;
        }

        // schedule the next measurement reporting
        measReportIt->second.periodicReportTimer =
            Simulator::Schedule(reportInterval, &NrUeRrc::SendMeasurementReport, this, measId);

        // send the measurement report to eNodeB
        m_rrcSapUser->SendMeasurementReport(measurementReport);
    }
}

void
NrUeRrc::StartConnection()
{
    //std::cout<<"Starting Connection: "<<m_imsi<<std::endl;
    if(m_imsi == 6)
    {
        NS_LOG_FUNCTION(this << m_imsi);
    }
    NS_LOG_FUNCTION(this << m_imsi);
    if(!m_hasReceivedMib)
    {
        SwitchToState(IDLE_WAIT_MIB);
        return;
    }

    if(!m_hasReceivedMib)
    {
        SwitchToState(IDLE_WAIT_SIB2);
        return;
    }

    m_connectionPending = false; // reset the flag
    
    SwitchToState(IDLE_RANDOM_ACCESS);
  
    m_cmacSapProvider.at(0)->StartContentionBasedRandomAccessProcedure(m_redCap,m_use_2step_sdt);
}

void
NrUeRrc::ResumeConnection()
{
    NS_LOG_FUNCTION(this << m_imsi);
    if(!m_hasReceivedMib)
    {
        SwitchToState(IDLE_WAIT_MIB);
    }

    if(!m_hasReceivedMib)
    {
        SwitchToState(IDLE_WAIT_SIB2);
    }
    m_connectionPending = false; // reset the flag
    SwitchToState(IDLE_RANDOM_ACCESS_INACTIVE);
    m_cmacSapProvider.at(0)->StartContentionBasedRandomAccessProcedure(m_redCap,m_use_2step_sdt);
}



void
NrUeRrc::LeaveConnectedMode()
{
    NS_LOG_FUNCTION(this << m_imsi);
    m_leaveConnectedMode = true;
    m_storedMeasValues.clear();
    ResetRlfParams();

    std::map<uint8_t, LteRrcSap::MeasIdToAddMod>::iterator measIdIt;
    for (measIdIt = m_varMeasConfig.measIdList.begin();
         measIdIt != m_varMeasConfig.measIdList.end();
         ++measIdIt)
    {
        VarMeasReportListClear(measIdIt->second.measId);
    }
    m_varMeasConfig.measIdList.clear();

    m_ccmRrcSapProvider->Reset();

    for (uint32_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        m_cmacSapProvider.at(i)->Reset(); // reset the MAC
    }

    m_drbMap.clear();
    m_bid2DrbidMap.clear();
    m_srb1 = nullptr;
    m_hasReceivedMib = false;
    m_hasReceivedSib1 = false;
    m_hasReceivedSib2 = false;

    for (uint32_t i = 0; i < m_numberOfComponentCarriers; i++)
    {
        m_cphySapProvider.at(i)->ResetPhyAfterRlf(); // reset the PHY
    }
    // SwitchToState(IDLE_START);
    // DoStartCellSelection(m_dlEarfcn);
    // Save the cell id UE was attached to
    StorePreviousCellId(m_cellId);
    //m_cellId = 0; //I assume only one cell and want to stay in it when transitioning into RRC_IDLE. Has to change for more Basestations
    m_rnti = 0;
    m_srb0->m_rlc->SetRnti(m_rnti);
}

void
NrUeRrc::ConnectionTimeout()
{
    NS_LOG_FUNCTION(this << m_imsi);
    ++m_connEstFailCount;
    NrAccessStats::IncRrcTimeout();
    if (m_connEstFailCount >= m_connEstFailCountLimit)
    {
        m_connectionTimeoutTrace(m_imsi, m_cellId, m_rnti, m_connEstFailCount);
        SwitchToState(CONNECTED_PHY_PROBLEM);
        // Assumption: The eNB connection request timer would expire
        // before the expiration of T300 at UE. Upon which, the eNB deletes
        // the UE context. Therefore, here we don't need to send the UE context
        // deletion request to the eNB.
        m_asSapUser->NotifyConnectionReleased();
        m_connEstFailCount = 0;
    }
    else
    {
        for (uint16_t i = 0; i < m_numberOfComponentCarriers; i++)
        {
            m_cmacSapProvider.at(i)->Reset(); // reset the MAC
        }
        m_hasReceivedSib2 = false; // invalidate the previously received SIB2
        SwitchToState(IDLE_CAMPED_NORMALLY);
        m_connectionTimeoutTrace(m_imsi, m_cellId, m_rnti, m_connEstFailCount);
        // Following call to UE NAS will force the UE to immediately
        // perform the random access to the same cell again.
        m_asSapUser->NotifyConnectionFailed(); // inform upper layer
    }
}

void
NrUeRrc::DisposeOldSrb1()
{
    NS_LOG_FUNCTION(this);
    m_srb1Old = nullptr;
}

uint8_t
NrUeRrc::Bid2Drbid(uint8_t bid)
{
    std::map<uint8_t, uint8_t>::iterator it = m_bid2DrbidMap.find(bid);
    // NS_ASSERT_MSG (it != m_bid2DrbidMap.end (), "could not find BID " << bid);
    if (it == m_bid2DrbidMap.end())
    {
        return 0;
    }
    else
    {
        return it->second;
    }
}

void
NrUeRrc::SwitchToState(State newState)
{
    NS_LOG_FUNCTION(this << ToString(newState));
    State oldState = m_state;
    m_state = newState;
    NS_LOG_INFO(this << " IMSI " << m_imsi << " RNTI " << m_rnti << " UeRrc " << ToString(oldState)
                     << " --> " << ToString(newState));
    m_stateTransitionTrace(m_imsi, m_cellId, m_rnti, oldState, newState);

    switch (newState)
    {
    case IDLE_START:
        if (m_leaveConnectedMode)
        {
            NS_LOG_INFO("Starting initial cell selection after RLF");
        }
        else
        {
            NS_FATAL_ERROR("cannot switch to an initial state");
        }

        break;

    case IDLE_CELL_SEARCH:
    case IDLE_WAIT_MIB_SIB1:
    case IDLE_WAIT_MIB:
    case IDLE_WAIT_SIB1:
        break;

    case IDLE_CAMPED_NORMALLY:
        if ((m_connectionPending||m_DataPending) &&m_hasReceivedSib2)
        {
            StartConnection();
        }
        break;
    
    case INACTIVE:
        if ((m_connectionPending||m_DataPending) &&m_hasReceivedSib2)
        {
            ResumeConnection();
        }
        break;

    case IDLE_WAIT_SIB2:
        if (m_hasReceivedSib2)
        {
            NS_ASSERT(m_connectionPending);
            SwitchToState(IDLE_CAMPED_NORMALLY);
            //StartConnection();
        }
        break;

    case IDLE_RANDOM_ACCESS:
    case IDLE_RANDOM_ACCESS_INACTIVE:
    case IDLE_CONNECTING:
    case INACTIVE_CONNECTING:
    case CONNECTED_NORMALLY:
    case CONNECTED_HANDOVER:
    case CONNECTED_PHY_PROBLEM:
    case CONNECTED_REESTABLISHING:
        break;

    default:
        break;
    }
}

void
NrUeRrc::RadioLinkFailureDetected()
{
    NS_LOG_FUNCTION(this << m_imsi << m_rnti);
    m_radioLinkFailureTrace(m_imsi, m_cellId, m_rnti);
    SwitchToState(CONNECTED_PHY_PROBLEM);
    m_rrcSapUser->SendIdealUeContextRemoveRequest(m_rnti);
    m_asSapUser->NotifyConnectionReleased();
}

void
NrUeRrc::DoNotifyInSync()
{
    NS_LOG_FUNCTION(this << m_imsi);
    m_noOfSyncIndications++;
    NS_LOG_INFO("noOfSyncIndications " << (uint16_t)m_noOfSyncIndications);
    m_phySyncDetectionTrace(m_imsi, m_rnti, m_cellId, "Notify in sync", m_noOfSyncIndications);
    if (m_noOfSyncIndications == m_n311)
    {
        ResetRlfParams();
    }
}

void
NrUeRrc::DoNotifyOutOfSync()
{
    NS_LOG_FUNCTION(this << m_imsi);
    m_noOfSyncIndications++;
    NS_LOG_INFO(this << " Total Number of Sync indications from PHY "
                     << (uint16_t)m_noOfSyncIndications << "N310 value : " << (uint16_t)m_n310);
    m_phySyncDetectionTrace(m_imsi, m_rnti, m_cellId, "Notify out of sync", m_noOfSyncIndications);
    if (m_noOfSyncIndications == m_n310)
    {
        m_radioLinkFailureDetected =
            Simulator::Schedule(m_t310, &NrUeRrc::RadioLinkFailureDetected, this);
        if (m_radioLinkFailureDetected.IsRunning())
        {
            NS_LOG_INFO("t310 started");
        }
        m_cphySapProvider.at(0)->StartInSnycDetection();
        m_noOfSyncIndications = 0;
    }
}

void
NrUeRrc::DoResetSyncIndicationCounter()
{
    NS_LOG_FUNCTION(this << m_imsi);

    NS_LOG_DEBUG("The number of sync indication received by RRC from PHY: "
                 << (uint16_t)m_noOfSyncIndications);
    m_noOfSyncIndications = 0;
}

void
NrUeRrc::ResetRlfParams()
{
    NS_LOG_FUNCTION(this << m_imsi);
    m_radioLinkFailureDetected.Cancel();
    m_noOfSyncIndications = 0;
    m_cphySapProvider.at(0)->ResetRlfParams();
}

const std::string
NrUeRrc::ToString(NrUeRrc::State s)
{
    return g_ueRrcStateName[s];
}

void
NrUeRrc::DoSendRrcConnectionRequest()
{
    if(m_state == INACTIVE_CONNECTING)
    {
        if(m_sdt)
        {
          SwitchToState(INACTIVE_SMALL_DATA_TRANSMISSION);
          NrRrcSap::RrcResumeRequest msg;
          msg.rrcTransactionIdentifier = m_lastRrcTransactionIdentifier;
          msg.rrcResumeRequest.resumeIdentity = m_resumeIdentity;
          msg.rrcResumeRequest.resumeCause = NrRrcSap::ResumeCause::mo_Data;
          std::vector<Ptr<Packet>>::iterator next_packet = m_packetStored.begin();
          uint16_t sdtBytes = 0;
          NS_ASSERT(m_grantedBytes >0);

        while(next_packet != m_packetStored.end() && sdtBytes + (*next_packet)->GetSize() < 125)
        {
            sdtBytes += (*next_packet)->GetSize();
            msg.sdtData.emplace_back(*next_packet); //msg.sdtData is used as information element to track transmitted sdt packages. Not in standard
            //++next_packet;
            m_packetStored.erase(m_packetStored.begin());
        }
        if(sdtBytes != 0){
            //we use SDT to transmit Data
             m_rrcSapUser->SendRrcResumeRequest(true, msg, m_grantedBytes); 
          
        }
        else{
            //Data packet is to large for SDT
            m_rrcSapUser->SendRrcResumeRequest(false, msg,m_grantedBytes); 
          }
        }
        else{
            
          NrRrcSap::RrcResumeRequest msg;
          msg.rrcTransactionIdentifier = m_lastRrcTransactionIdentifier;
          msg.rrcResumeRequest.resumeIdentity = m_resumeIdentity;
          msg.rrcResumeRequest.resumeCause = NrRrcSap::ResumeCause::mo_Data;
          m_rrcSapUser->SendRrcResumeRequest(false, msg,m_grantedBytes); 
        
        }
        //DoSetTemporaryCellRnti(m_resumeIdentity);

    }
    else{ //IDLE_CONNECTING
        NrRrcSap::NrRrcConnectionRequest msg;
        msg.ueIdentity = GetImsi(); //use Imsi instead of Rnti

        //sending redCap indication in Msg3. Information element not in standard. Normally the indication is done by using a specific logical channel for it. 
        //This implementation creates the same results as using a specifix logical channel
        msg.redcap = m_redCap;
        m_rrcSapUser->SendRrcConnectionRequest(msg,m_grantedBytes);
        

    }
    m_grantedBytes =0;
    m_connectionTimeout = Simulator::Schedule(m_t300, &NrUeRrc::ConnectionTimeout, this);

}


void 
NrUeRrc::SetSdt(bool state)
{
    m_sdt = state;
}

void 
NrUeRrc::SetRedCap(bool state)
{
    m_redCap = state;
}


void 
NrUeRrc::SendStoredPackages()
{
    m_DataPending = false; // TODO: just a quick fix for Cni-Szenario
    //std::cout<<"send StoredPackages for imsi: "<< uint(m_imsi)<< " with packages: "<<uint(m_packetStored.size())<<std::endl;
    while(m_packetStored.size() >0)
    {

        uint8_t drbid = 1; //TODO_ check bid
        std::map<uint8_t, Ptr<LteDataRadioBearerInfo>>::iterator it = m_drbMap.find(drbid);
            NS_ASSERT_MSG(it != m_drbMap.end(), "could not find bearer with drbid == " << drbid);

            LtePdcpSapProvider::TransmitPdcpSduParameters params;
            params.pdcpSdu = m_packetStored.front();
            params.rnti = m_rnti;
            params.lcid = it->second->m_logicalChannelIdentity;

            NS_LOG_LOGIC(this << " RNTI=" << m_rnti << " sending packet " << m_packetStored.front() << " on DRBID "
                            << (uint32_t)drbid << " (LCID " << (uint32_t)params.lcid << ")"
                            << " (" << m_packetStored.front()->GetSize() << " bytes)");
            it->second->m_pdcp->GetLtePdcpSapProvider()->TransmitPdcpSdu(params);
        m_packetStored.erase(m_packetStored.begin());
    }
    if(m_packetStored.size() ==0)
    {
        m_DataPending = false;
    }
    // else{
    //     Simulator::Schedule(MilliSeconds(10),&NrUeRrc::SendStoredPackages,this);
    // }
}

void
NrUeRrc::SetPowerStateChangedCallback(Callback<void,NrEnergyModel::PowerState> pscc)
{
    m_powerStateChangedCallback = pscc;
}

void
NrUeRrc::SetEdrxPtwConfigCallback(Callback<void, uint32_t> cb)
{
    m_edrxPtwConfigCallback = cb;
}


} // namespace ns3
