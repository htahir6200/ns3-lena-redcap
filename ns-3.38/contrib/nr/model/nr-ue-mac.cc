/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#define NS_LOG_APPEND_CONTEXT                                                                      \
    do                                                                                             \
    {                                                                                              \
        std::clog << " [ CellId " << GetCellId() << ", bwpId " << GetBwpId() << ", rnti "          \
                  << m_rnti << "] ";                                                               \
    } while (false);

#include "nr-ue-mac.h"
#include "nr-campaign-stats.h"

#include "nr-control-messages.h"
#include "nr-mac-header-vs.h"
#include "nr-mac-short-bsr-ce.h"
#include "nr-phy-sap.h"
#include "../helper/nr-access-stats.h"

#include <ns3/boolean.h>
#include <ns3/log.h>
#include <ns3/lte-radio-bearer-tag.h>
#include <ns3/random-variable-stream.h>

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrUeMac");
NS_OBJECT_ENSURE_REGISTERED(NrUeMac);

//uint8_t NrUeMac::g_raPreambleId = 0;

///////////////////////////////////////////////////////////
// SAP forwarders
///////////////////////////////////////////////////////////

class UeMemberLteUeCmacSapProvider : public LteUeCmacSapProvider
{
  public:
    UeMemberLteUeCmacSapProvider(NrUeMac* mac);

    // inherited from LteUeCmacSapProvider
    void ConfigureRach(RachConfig rc) override;
    void StartContentionBasedRandomAccessProcedure(bool redcap, bool do_2step_sdt) override;
    void StartNonContentionBasedRandomAccessProcedure(uint16_t rnti,
                                                      uint8_t preambleId,
                                                      uint8_t prachMask) override;
    void AddLc(uint8_t lcId,
               LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
               LteMacSapUser* msu) override;
    void RemoveLc(uint8_t lcId) override;
    void Reset() override;
    void SetRnti(uint16_t rnti) override;
    void NotifyConnectionSuccessful() override;
    void SetImsi(uint64_t imsi) override;
    void DeactivateBwp(LteNrTddSlotType type) override;
    void ActivateBwp(LteNrTddSlotType type) override;
    void NotifyDrx(uint frames) override;
    void NotifyeDrx(uint frames) override;
    void SetPRnti(uint16_t prnti) override;

   


  private:
    NrUeMac* m_mac;
};

UeMemberLteUeCmacSapProvider::UeMemberLteUeCmacSapProvider(NrUeMac* mac)
    : m_mac(mac)
{
}

void
UeMemberLteUeCmacSapProvider::ConfigureRach(RachConfig rc)
{
    m_mac->DoConfigureRach(rc);
}

void
UeMemberLteUeCmacSapProvider::StartContentionBasedRandomAccessProcedure(bool redcap, bool do_2step_sdt)
{
    m_mac->DoStartContentionBasedRandomAccessProcedure(redcap,do_2step_sdt);
}

void
UeMemberLteUeCmacSapProvider::StartNonContentionBasedRandomAccessProcedure(uint16_t rnti,
                                                                          uint8_t preambleId,
                                                                          uint8_t prachMask)
{
    m_mac->DoStartNonContentionBasedRandomAccessProcedure(rnti, preambleId, prachMask);
}

void
UeMemberLteUeCmacSapProvider::AddLc(uint8_t lcId, LogicalChannelConfig lcConfig, LteMacSapUser* msu)
{
    m_mac->AddLc(lcId, lcConfig, msu);
}

void
UeMemberLteUeCmacSapProvider::RemoveLc(uint8_t lcid)
{
    m_mac->DoRemoveLc(lcid);
}

void
UeMemberLteUeCmacSapProvider::Reset()
{
    m_mac->DoReset();
}

void
UeMemberLteUeCmacSapProvider::SetRnti(uint16_t rnti)
{
    m_mac->SetRnti(rnti);
}

void
UeMemberLteUeCmacSapProvider::NotifyConnectionSuccessful()
{
    m_mac->DoNotifyConnectionSuccessful();
}

void
UeMemberLteUeCmacSapProvider::SetImsi(uint64_t imsi)
{
    m_mac->DoSetImsi(imsi);
}

void
UeMemberLteUeCmacSapProvider::ActivateBwp(LteNrTddSlotType type)
{
    m_mac->DoActivateBwp(type);
}

void
UeMemberLteUeCmacSapProvider::DeactivateBwp(LteNrTddSlotType type)
{
    m_mac->DoDeactivateBwp(type);
}


void
UeMemberLteUeCmacSapProvider::NotifyDrx(uint frames)
{
    m_mac->DoNotifyDrx(frames);
}

void
UeMemberLteUeCmacSapProvider::SetPRnti(uint16_t prnti)
{
    m_mac->DoSetPRnti(prnti);
}


void
UeMemberLteUeCmacSapProvider::NotifyeDrx(uint frames)
{
    m_mac->DoNotifyeDrx(frames);
}






class UeMemberNrMacSapProvider : public LteMacSapProvider
{
  public:
    UeMemberNrMacSapProvider(NrUeMac* mac);

    // inherited from LteMacSapProvider
    void TransmitPdu(TransmitPduParameters params) override;
    void ReportBufferStatus(ReportBufferStatusParameters params) override;
    std::unordered_map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> GetBufferStatus() override;
    void TransmitMsg3(TransmitPduParameters params) override;

  private:
    NrUeMac* m_mac;
};

UeMemberNrMacSapProvider::UeMemberNrMacSapProvider(NrUeMac* mac)
    : m_mac(mac)
{
}

void
UeMemberNrMacSapProvider::TransmitPdu(TransmitPduParameters params)
{
    m_mac->DoTransmitPdu(params);
}

void
UeMemberNrMacSapProvider::TransmitMsg3(TransmitPduParameters params)
{
    m_mac->DoTransmitMsg3(params);
}

void
UeMemberNrMacSapProvider::ReportBufferStatus(ReportBufferStatusParameters params)
{
    m_mac->DoReportBufferStatus(params);
}


std::unordered_map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>
UeMemberNrMacSapProvider::GetBufferStatus()
{
    return m_mac->DoGetBufferStatus();
} 

class NrUePhySapUser;

class MacUeMemberPhySapUser : public NrUePhySapUser
{
  public:
    MacUeMemberPhySapUser(NrUeMac* mac);

    void ReceivePhyPdu(Ptr<Packet> p) override;

    void ReceiveControlMessage(Ptr<NrControlMessage> msg) override;

    void SlotIndication(SfnSf sfn) override;

    // virtual void NotifyHarqDeliveryFailure (uint8_t harqId);

    uint8_t GetNumHarqProcess() const override;


  virtual void RecvSib1(NrSib1Message sib1, NrPhySapProvider::PrachConfig prachOccasions) override;
 private:
    NrUeMac* m_mac;
};

MacUeMemberPhySapUser::MacUeMemberPhySapUser(NrUeMac* mac)
    : m_mac(mac)
{
}

void
MacUeMemberPhySapUser::ReceivePhyPdu(Ptr<Packet> p)
{
    m_mac->DoReceivePhyPdu(p);
}

void
MacUeMemberPhySapUser::ReceiveControlMessage(Ptr<NrControlMessage> msg)
{
    m_mac->DoReceiveControlMessage(msg);
}

void
MacUeMemberPhySapUser::RecvSib1(NrSib1Message sib1, NrPhySapProvider::PrachConfig prachOccasions)
{
    m_mac->DoRecvSib1(sib1, prachOccasions);
}

void
MacUeMemberPhySapUser::SlotIndication(SfnSf sfn)
{
    m_mac->DoSlotIndication(sfn);
}

uint8_t
MacUeMemberPhySapUser::GetNumHarqProcess() const
{
    return m_mac->GetNumHarqProcess();
}

//-----------------------------------------------------------------------

TypeId
NrUeMac::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NrUeMac")
            .SetParent<Object>()
            .AddConstructor<NrUeMac>()
            .AddAttribute(
                "NumHarqProcess",
                "Number of concurrent stop-and-wait Hybrid ARQ processes per user",
                UintegerValue(200),
                MakeUintegerAccessor(&NrUeMac::SetNumHarqProcess, &NrUeMac::GetNumHarqProcess),
                MakeUintegerChecker<uint8_t>())
            .AddTraceSource("UeMacRxedCtrlMsgsTrace",
                            "Ue MAC Control Messages Traces.",
                            MakeTraceSourceAccessor(&NrUeMac::m_macRxedCtrlMsgsTrace),
                            "ns3::NrMacRxTrace::RxedUeMacCtrlMsgsTracedCallback")
            .AddTraceSource("UeMacTxedCtrlMsgsTrace",
                            "Ue MAC Control Messages Traces.",
                            MakeTraceSourceAccessor(&NrUeMac::m_macTxedCtrlMsgsTrace),
                            "ns3::NrMacRxTrace::TxedUeMacCtrlMsgsTracedCallback")

            .AddAttribute ("ActiveBwp",
                        "Only an active Bwp can be used for transmission/Reception",
                        BooleanValue (false),
                        MakeBooleanAccessor (&NrUeMac::SetActiveBwpStatus,
                                                &NrUeMac::GetActiveBwpStatus),
                        MakeBooleanChecker ());
    return tid;
}

NrUeMac::NrUeMac()
    : Object()
{
    NS_LOG_FUNCTION(this);
    m_cmacSapProvider = new UeMemberLteUeCmacSapProvider(this);
    m_macSapProvider = new UeMemberNrMacSapProvider(this);
    m_phySapUser = new MacUeMemberPhySapUser(this);
    m_raPreambleUniformVariable = CreateObject<UniformRandomVariable>();
}

NrUeMac::~NrUeMac()
{
}

void
NrUeMac::DoDispose()
{
    m_miUlHarqProcessesPacket.clear();
    m_miUlHarqProcessesPacketTimer.clear();
    m_ulBsrReceived.clear();
    m_lcInfoMap.clear();
    m_raPreambleUniformVariable = nullptr;
    delete m_macSapProvider;
    delete m_cmacSapProvider;
    delete m_phySapUser;
}

void
NrUeMac::SetRnti(uint16_t rnti)
{
    NS_LOG_FUNCTION(this);
    m_rnti = rnti;
}

void
NrUeMac::DoNotifyConnectionSuccessful()
{
    NS_LOG_FUNCTION(this);
    m_phySapProvider->NotifyConnectionSuccessful();

    // starting a timer for reception of a dci to transmit ACK
    Time waitingTime = MilliSeconds(30); //TODO_ how long would a UE wait until it sends a SR?
    // Short DIT / release-reconnect can leave SR machine in TO_SEND/ACTIVE
    // (e.g. after SrSchedulingTimeout) when RRC reports connection success again.
    if (m_srState != INACTIVE)
    {
        NrCampaignStats::SoftFail("UEMAC_SR_RESET_ON_CONNECT@ue");
        NS_LOG_WARN("DoNotifyConnectionSuccessful with srState="
                    << +m_srState << " imsi=" << m_imsi
                    << " — resetting SR machine (dense short-DIT hardening).");
        if (m_waitingForDciReception.IsRunning())
        {
            m_waitingForDciReception.Cancel();
        }
        if (m_waitingForSchedulingTimer.IsRunning())
        {
            m_waitingForSchedulingTimer.Cancel();
        }
    }
    m_srState = WAIT_SCHEDULING;
    m_waitingForDciReception =
        Simulator::Schedule(waitingTime, &NrUeMac::DciReceptionTimeout, this);
}

void
NrUeMac::DciReceptionTimeout()
{
    std::cout<<"DciReceptionTimeout: "<<m_imsi<<std::endl;
     NS_LOG_FUNCTION(this);
     if(m_srState == WAIT_SCHEDULING)
     {
        NrCampaignStats::Inc("ue.dci_reception_timeout");
        m_srState = INACTIVE;
        
     }
     else
     {
        // Concurrent SR / reconnect under short DIT can advance srState first.
        NrCampaignStats::SoftFail("UEMAC_DCI_TIMEOUT_BAD_STATE@ue");
        NS_LOG_WARN("DciReceptionTimeout with srState="
                    << +m_srState << " imsi=" << m_imsi
                    << " — ignoring (dense short-DIT hardening).");
     }
}

void
NrUeMac::DoSetImsi(uint64_t imsi)
{
    NS_LOG_FUNCTION(this);
    m_imsi = imsi;
}

void
NrUeMac::DoActivateBwp(LteNrTddSlotType type)
{
    if(type == LteNrTddSlotType::UL)
    {
        m_activeBwpUL = true;
    }
    else{
        m_activeBwpDL = true;
    }
}

void
NrUeMac::DoDeactivateBwp(LteNrTddSlotType type)
{
    if(type == LteNrTddSlotType::UL)
    {
        m_activeBwpUL = false;
    }
    else{
        m_activeBwpDL = false;
    }
}

void
NrUeMac::DoNotifyDrx(uint frames)
{
    m_drxConfigured = true;
    m_drx = frames;
}
void
NrUeMac::DoNotifyeDrx(uint frames)
{
    m_edrxConfigured = true;
    m_edrx = frames;
}

void
NrUeMac::DoSetPRnti(uint16_t prnti)
{
    m_pRnti = prnti;
}

uint16_t
NrUeMac::GetBwpId() const
{
    if (m_phySapProvider)
    {
        return m_phySapProvider->GetBwpId();
    }
    else
    {
        return UINT16_MAX;
    }
}

uint16_t
NrUeMac::GetCellId() const
{
    if (m_phySapProvider)
    {
        return m_phySapProvider->GetCellId();
    }
    else
    {
        return UINT16_MAX;
    }
}

uint32_t
NrUeMac::GetTotalBufSize() const
{
    uint32_t ret = 0;
    for (auto it = m_ulBsrReceived.cbegin(); it != m_ulBsrReceived.cend(); ++it)
    {
        ret += ((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
    }
    return ret;
}

/**
 * \brief Sets the number of HARQ processes
 * \param numHarqProcesses the maximum number of harq processes
 */
void
NrUeMac::SetNumHarqProcess(uint8_t numHarqProcess)
{
    m_numHarqProcess = numHarqProcess;

    m_miUlHarqProcessesPacket.resize(GetNumHarqProcess());
    for (std::size_t i = 0; i < m_miUlHarqProcessesPacket.size(); ++i)
    {
        if (m_miUlHarqProcessesPacket.at(i).m_pktBurst == nullptr)
        {
            Ptr<PacketBurst> pb = CreateObject<PacketBurst>();
            m_miUlHarqProcessesPacket.at(i).m_pktBurst = pb;
        }
    }
    m_miUlHarqProcessesPacketTimer.resize(GetNumHarqProcess(), 0);
}

/**
 * \return number of HARQ processes
 */
uint8_t
NrUeMac::GetNumHarqProcess() const
{
    return m_numHarqProcess;
}

// forwarded from MAC SAP
void
NrUeMac::DoTransmitPdu(LteMacSapProvider::TransmitPduParameters params)
{ 
   
    NS_ASSERT(m_ulDci->m_harqProcess == params.harqProcessId);

    m_miUlHarqProcessesPacket.at(params.harqProcessId).m_lcidList.push_back(params.lcid);

    NrMacHeaderVs header;
    header.SetLcId(params.lcid);
    header.SetSize(params.pdu->GetSize());
    params.pdu->AddHeader(header);


    LteRadioBearerTag bearerTag(params.rnti, params.lcid, params.layer);
    params.pdu->AddPacketTag(bearerTag);

    m_miUlHarqProcessesPacket.at(params.harqProcessId).m_pktBurst->AddPacket(params.pdu);
    m_miUlHarqProcessesPacketTimer.at(params.harqProcessId) = GetNumHarqProcess();


    // Transmit if the PDU fits in the remaining TB. SHORT_BSR is sent
    // separately (SendReportBufferStatus); requiring +5 B here dropped all
    // PDUs on tiny dense/eRedCap grants and left PHY with an empty burst.
    const uint32_t tbSize = m_ulDci->m_tbSize.at(0);
    const uint32_t remaining = (tbSize > m_ulDciTotalUsed) ? (tbSize - m_ulDciTotalUsed) : 0;
    if (params.pdu->GetSize() <= remaining)
    {
        m_ulDciTotalUsed += params.pdu->GetSize();
        NS_ASSERT_MSG(m_ulDciTotalUsed <= tbSize, "We used more data than the DCI allowed us.");
        m_phySapProvider->SendMacPdu(params.pdu, m_ulDciSfnsf, m_ulDci->m_symStart, params.layer);
    }
    else
    {
        NS_LOG_WARN("DoTransmitPdu: PDU " << params.pdu->GetSize() << " B does not fit in remaining "
                                          << remaining << " B of TB " << tbSize);
    }
}

void
NrUeMac::DoTransmitMsg3(LteMacSapProvider::TransmitPduParameters params)
{
    NS_LOG_FUNCTION(this);

    m_miUlHarqProcessesPacket.at(params.harqProcessId).m_lcidList.push_back(params.lcid);

    SfnSf sendframe = std::get<0>(m_msg3Grant);
    uint8_t startSymbol = std::get<1>(m_msg3Grant);

    NrMacHeaderVs header;
    
    
    header.SetLcId(params.lcid);
    header.SetSize(params.pdu->GetSize());
    params.pdu->AddHeader(header);

    LteRadioBearerTag bearerTag(params.rnti, params.lcid, params.layer);
    params.pdu->AddPacketTag(bearerTag);

    m_miUlHarqProcessesPacket.at(params.harqProcessId).m_pktBurst->AddPacket(params.pdu);
    m_miUlHarqProcessesPacketTimer.at(params.harqProcessId) = GetNumHarqProcess();


    m_phySapProvider->SendMacPdu(params.pdu, sendframe,startSymbol, params.layer);
}





void
NrUeMac::DoReportBufferStatus(LteMacSapProvider::ReportBufferStatusParameters params)
{
    NS_LOG_FUNCTION(this << static_cast<uint32_t>(params.lcid));

    auto it = m_ulBsrReceived.find(params.lcid);

    NS_LOG_INFO("Received BSR for LC Id" << static_cast<uint32_t>(params.lcid));

    if (it != m_ulBsrReceived.end())
    {
        // update entry
        (*it).second = params;

    }
    else
    {
        // update entry
        it = m_ulBsrReceived.insert(std::make_pair(params.lcid, params)).first;
    }
    
    if (m_srState == INACTIVE)
    {
        NS_LOG_INFO("INACTIVE -> TO_SEND, bufSize " << GetTotalBufSize());
        m_srState = TO_SEND;
    }
    
}

std::unordered_map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>
NrUeMac::DoGetBufferStatus()
{
    NS_LOG_FUNCTION(this);
    return m_ulBsrReceived;
}


void
NrUeMac::SendReportBufferStatus(const SfnSf& dataSfn, uint8_t symStart)
{
    NS_LOG_FUNCTION(this);

    if (m_rnti == 0)
    {
        NS_LOG_INFO("MAC not initialized, BSR deferred");
        return;
    }

    if (m_ulBsrReceived.size() == 0)
    {
        NS_LOG_INFO("No BSR report to transmit");
        return;
    }
    MacCeElement bsr = MacCeElement();
    bsr.m_rnti = m_rnti;
    bsr.m_macCeType = MacCeElement::BSR;

    // BSR is reported for each LCG
    std::unordered_map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters>::iterator it;
    std::vector<uint32_t> queue(4, 0); // one value per each of the 4 LCGs, initialized to 0
    for (it = m_ulBsrReceived.begin(); it != m_ulBsrReceived.end(); it++)
    {
        uint8_t lcid = it->first;
        std::unordered_map<uint8_t, LcInfo>::iterator lcInfoMapIt;
        lcInfoMapIt = m_lcInfoMap.find(lcid);
        NS_ASSERT(lcInfoMapIt != m_lcInfoMap.end());
        // NS_ASSERT_MSG((lcid != 0) ||
        //                   (((*it).second.txQueueSize == 0) && ((*it).second.retxQueueSize == 0) &&
        //                    ((*it).second.statusPduSize == 0)),
        //               "BSR should not be used for LCID 0");
        uint8_t lcg = lcInfoMapIt->second.lcConfig.logicalChannelGroup;
        queue.at(lcg) +=
            ((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize);
        
        if (queue.at(lcg) != 0)
        {
            NS_LOG_DEBUG("Adding 5 bytes for SHORT_BSR.");
            queue.at(lcg) += 5;
        }
        if ((*it).second.txQueueSize > 0)
        {
            NS_LOG_DEBUG("Adding 3 bytes for TX subheader.");
            queue.at(lcg) += 3;
        }
        if ((*it).second.retxQueueSize > 0)
        {
            NS_LOG_DEBUG("Adding 3 bytes for RX subheader.");
            queue.at(lcg) += 3;
        }
    }

    NS_LOG_INFO("Sending BSR with this info for the LCG: "
                << queue.at(0) << " " << queue.at(1) << " " << queue.at(2) << " " << queue.at(3));
    // FF API says that all 4 LCGs are always present
    bsr.m_macCeValue.m_bufferStatus.push_back(NrMacShortBsrCe::FromBytesToLevel(queue.at(0)));
    bsr.m_macCeValue.m_bufferStatus.push_back(NrMacShortBsrCe::FromBytesToLevel(queue.at(1)));
    bsr.m_macCeValue.m_bufferStatus.push_back(NrMacShortBsrCe::FromBytesToLevel(queue.at(2)));
    bsr.m_macCeValue.m_bufferStatus.push_back(NrMacShortBsrCe::FromBytesToLevel(queue.at(3)));

    // create the message. It is used only for tracing, but we don't send it...
    Ptr<NrBsrMessage> msg = Create<NrBsrMessage>();
    msg->SetSourceBwp(GetBwpId());
    msg->SetBsr(bsr);

    m_macTxedCtrlMsgsTrace(m_currentSlot, GetCellId(), bsr.m_rnti, GetBwpId(), msg);

    // Here we send the real SHORT_BSR, as a subpdu.
    Ptr<Packet> p = Create<Packet>();

    // Please note that the levels are defined from the standard. In this case,
    // we have 5 bit available, so use such standard levels. In the future,
    // when LONG BSR will be implemented, this have to change.
    NrMacShortBsrCe header;
    header.m_bufferSizeLevel_0 = NrMacShortBsrCe::FromBytesToLevel(queue.at(0));
    header.m_bufferSizeLevel_1 = NrMacShortBsrCe::FromBytesToLevel(queue.at(1));
    header.m_bufferSizeLevel_2 = NrMacShortBsrCe::FromBytesToLevel(queue.at(2));
    header.m_bufferSizeLevel_3 = NrMacShortBsrCe::FromBytesToLevel(queue.at(3));

    p->AddHeader(header);

    LteRadioBearerTag bearerTag(m_rnti, NrMacHeaderFsUl::SHORT_BSR, 0);
    p->AddPacketTag(bearerTag);

    if (m_ulDciTotalUsed + p->GetSize() > m_ulDci->m_tbSize.at(0))
    {
        NS_LOG_WARN("Skipping SHORT_BSR: would use "
                    << (m_ulDciTotalUsed + p->GetSize()) << " B > TB " << m_ulDci->m_tbSize.at(0));
        return;
    }
    m_ulDciTotalUsed += p->GetSize();
    // MIMO is not supported for UL yet.
    // Therefore, there will be only
    // one stream with stream Id 0.
    uint8_t streamId = 0;
    
    m_phySapProvider->SendMacPdu(p, dataSfn, symStart, streamId);
}

void
NrUeMac::SetUeCmacSapUser(LteUeCmacSapUser* s)
{
    m_cmacSapUser = s;
}

LteUeCmacSapProvider*
NrUeMac::GetUeCmacSapProvider()
{
    return m_cmacSapProvider;
}

void
NrUeMac::RefreshHarqProcessesPacketBuffer()
{
    NS_LOG_FUNCTION(this);

    for (std::size_t i = 0; i < m_miUlHarqProcessesPacketTimer.size(); i++)
    {
        if (m_miUlHarqProcessesPacketTimer.at(i) == 0 && m_miUlHarqProcessesPacket.at(i).m_pktBurst)
        {
            if (m_miUlHarqProcessesPacket.at(i).m_pktBurst->GetSize() > 0)
            {
                // timer expired: drop packets in buffer for this process
                NS_LOG_INFO("HARQ Proc Id " << i << " packets buffer expired");
                Ptr<PacketBurst> emptyPb = CreateObject<PacketBurst>();
                m_miUlHarqProcessesPacket.at(i).m_pktBurst = emptyPb;
                m_miUlHarqProcessesPacket.at(i).m_lcidList.clear();
            }
        }
        else
        {
            // m_miUlHarqProcessesPacketTimer.at (i)--;  // ignore HARQ timeout
        }
    }
}

void
NrUeMac::DoSlotIndication(const SfnSf& sfn)
{
    NS_LOG_FUNCTION(this);
    m_currentSlot = sfn;
    NS_LOG_INFO("Slot " << m_currentSlot);

    //RefreshHarqProcessesPacketBuffer();

    if (m_srState == TO_SEND)
    {
        NS_LOG_INFO("Sending SR to PHY in slot " << sfn);
        SendSR();
        m_srState = ACTIVE;
        m_waitingForSchedulingTimer = Simulator::Schedule(MilliSeconds(100), &NrUeMac::SrSchedulingTimeout, this);
    }
    if(m_sib1Received && hasPrach(m_currentSlot))
    {
        if(m_startingRach)
        {
            RandomlySelectAndSendRaPreamble();
            m_startingRach = false;
        }
    
    
        if(m_waitForPrachOcc)
        {
            if( m_currentSlot.GetSubframe() ==m_prachSubframe && m_currentSlot.GetSlot() == m_prachSlot)
            {
                m_waitForPrachOcc = false;
                SendRaPreamble(true);
            }
        }
    }
}

void
NrUeMac::SendSR() const
{
    NS_LOG_FUNCTION(this);
    if (m_rnti == 0)
    {
        NS_LOG_INFO("MAC not initialized, SR deferred");
        NrCampaignStats::Inc("ue.sr_deferred_no_rnti");
        return;
    }
    NrCampaignStats::Inc("ue.sr_sent");

    // create the SR to send to the gNB
    Ptr<NrSRMessage> msg = Create<NrSRMessage>();
    msg->SetSourceBwp(GetBwpId());
    msg->SetRNTI(m_rnti);

    m_macTxedCtrlMsgsTrace(m_currentSlot, GetCellId(), m_rnti, GetBwpId(), msg);
    m_phySapProvider->SendControlMessage(msg);
}

void
NrUeMac::DoReceivePhyPdu(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this);

    LteRadioBearerTag tag;
    p->RemovePacketTag(tag);

    if (tag.GetRnti() != m_rnti) // Packet is for another user
    {
        return;
    }

    NrMacHeaderVs header;
    p->RemoveHeader(header);

    LteMacSapUser::ReceivePduParameters rxParams;
    rxParams.p = p;
    rxParams.rnti = m_rnti;
    rxParams.lcid = header.GetLcId();

    auto it = m_lcInfoMap.find(header.GetLcId());

    // p can be empty. Well, right now no, but when someone will add CE in downlink,
    // then p can be empty.
    if (rxParams.p->GetSize() > 0)
    {
        it->second.macSapUser->ReceivePdu(rxParams);
    }
}

void
NrUeMac::RecvRaResponse(BuildRarListElement_s raResponse)
{
    NS_LOG_FUNCTION(this);
    m_waitingForRaResponse = false;
    m_noRaResponseReceivedEvent.Cancel();
    m_rnti = raResponse.m_rnti;
 
    m_cmacSapUser->SetTemporaryCellRnti(m_rnti);
    m_msg3Grant = std::make_tuple(SfnSf(raResponse.m_grant.m_Framenumber,raResponse.m_grant.m_Subframenumber,raResponse.m_grant.m_Slotnumber,m_currentSlot.GetNumerology()),raResponse.m_grant.m_StartSymbol);
    m_cmacSapUser->NotifyRandomAccessSuccessful(raResponse.m_grant,false);
    NrAccessStats::IncRaSuccess();
   
    //transmitMSG3(raResponse.m_grant ,m_rnti);

}

void
NrUeMac::ProcessUlDci(const Ptr<NrUlDciMessage>& dciMsg)
{
    NS_LOG_FUNCTION(this);
    if(m_waitingForDciReception.IsRunning())
    {
        m_waitingForDciReception.Cancel();
       
    }
     if(m_waitingForSchedulingTimer.IsRunning())
     {
        m_waitingForSchedulingTimer.Cancel();
     }
    
    SfnSf dataSfn = m_currentSlot;
    dataSfn.Add(dciMsg->GetKDelay());

    // Saving the data we need in DoTransmitPdu
    m_ulDciSfnsf = dataSfn;
    m_ulDciTotalUsed = 0;
    m_ulDci = dciMsg->GetDciInfoElement();
    NrCampaignStats::Inc("ue.ul_dci");
    NrCampaignStats::Inc("ue.ul_dci_tbs_bytes", m_ulDci->m_tbSize.at(0));
    NrCampaignStats::Max("ue.ul_dci_max_streams", m_ulDci->m_tbSize.size());

    m_macRxedCtrlMsgsTrace(m_currentSlot, GetCellId(), m_rnti, GetBwpId(), dciMsg);

    NS_LOG_INFO("UL DCI received, transmit data in slot "
                << dataSfn << " Harq Process " << +m_ulDci->m_harqProcess << " TBS "
                << m_ulDci->m_tbSize.at(0) << " total queue " << GetTotalBufSize());

    if (m_ulDci->m_ndi.at(0) == 0)
    {
        // This method will retransmit the data saved in the harq buffer
        TransmitRetx();

        // This method will transmit a new BSR.
        SendReportBufferStatus(dataSfn, m_ulDci->m_symStart);
    }
    else if (m_ulDci->m_ndi.at(0) == 1)
    {
        SendNewData();

        NS_LOG_INFO("After sending NewData, bufSize " << GetTotalBufSize());

        // Send a new BSR. SendNewData() already took into account the size of
        // the BSR.
        SendReportBufferStatus(dataSfn, m_ulDci->m_symStart);

        NS_LOG_INFO("UL DCI processing done, sent to PHY a total of "
                    << m_ulDciTotalUsed << " B out of " << m_ulDci->m_tbSize.at(0)
                    << " allocated bytes ");

        NrCampaignStats::Inc("ue.ul_dci_used_bytes", m_ulDciTotalUsed);
        if (m_ulDciTotalUsed == 0)
        {
            NrCampaignStats::Inc("ue.ul_dci_unused");
        }
        if (GetTotalBufSize() == 0)
        {
            m_srState = INACTIVE;

            NS_LOG_INFO("ACTIVE -> INACTIVE, bufSize " << GetTotalBufSize());
        }
        // Always emit at least one PDU if the grant was unused. Tiny UL TBs
        // (< SHORT_BSR) can leave the PHY with an empty burst and abort.
        if (m_ulDciTotalUsed == 0)
        {
            NS_LOG_WARN("No byte used for this UL-DCI, sending empty PDU (bufSize "
                        << GetTotalBufSize() << ")");

            LteMacSapProvider::TransmitPduParameters txParams;

            txParams.pdu = Create<Packet>();
            txParams.lcid = 3;
            txParams.rnti = m_rnti;
            txParams.layer = 0;
            txParams.harqProcessId = m_ulDci->m_harqProcess;
            txParams.componentCarrierId = GetBwpId();

            DoTransmitPdu(txParams);
        }
    }
}

void
NrUeMac::TransmitRetx()
{
    NS_LOG_FUNCTION(this);

    Ptr<PacketBurst> pb = m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_pktBurst;

    if (pb == nullptr)
    {
        NS_LOG_WARN(
            "The previous transmission did not contain any new data; "
            "probably it was BSR only. To not send an old BSR to the scheduler, "
            "we don't send anything back in this allocation. Eventually, "
            "the Harq timer at gnb will expire, and soon this allocation will be forgotten.");
        return;
    }

    NS_LOG_DEBUG("UE MAC RETX HARQ " << +m_ulDci->m_harqProcess);

    NS_ASSERT(pb->GetNPackets() > 0);

    for (std::list<Ptr<Packet>>::const_iterator j = pb->Begin(); j != pb->End(); ++j)
    {
        Ptr<Packet> pkt = (*j)->Copy();
        LteRadioBearerTag bearerTag;
        if (!pkt->PeekPacketTag(bearerTag))
        {
            NS_FATAL_ERROR("No radio bearer tag");
        }
        // MIMO is not supported for UL yet.
        // Therefore, there will be only
        // one stream with stream Id 0.
        uint8_t streamId = 0;
        m_phySapProvider->SendMacPdu(pkt, m_ulDciSfnsf, m_ulDci->m_symStart, streamId);
    }

    m_miUlHarqProcessesPacketTimer.at(m_ulDci->m_harqProcess) = GetNumHarqProcess();
}

void
NrUeMac::SendRetxData(uint32_t usefulTbs, uint32_t activeLcsRetx)
{
    NS_LOG_FUNCTION(this);

    if (activeLcsRetx == 0)
    {
        return;
    }

    uint32_t bytesPerLcId = usefulTbs / activeLcsRetx;

    for (auto& itBsr : m_ulBsrReceived)
    {
        auto& bsr = itBsr.second;

        if (m_ulDciTotalUsed + bytesPerLcId <= usefulTbs)
        {
            LteMacSapUser::TxOpportunityParameters txParams;
            txParams.lcid = bsr.lcid;
            txParams.rnti = m_rnti;
            txParams.bytes = bytesPerLcId;
            txParams.layer = 0;
            txParams.harqId = m_ulDci->m_harqProcess;
            txParams.componentCarrierId = GetBwpId();

            NS_LOG_INFO("Notifying RLC of LCID " << +bsr.lcid
                                                 << " of a TxOpp "
                                                    "of "
                                                 << bytesPerLcId << " B for a RETX PDU");
            m_lcInfoMap.at(bsr.lcid).macSapUser->NotifyTxOpportunity(txParams);
            // After this call, m_ulDciTotalUsed has been updated with the
            // correct amount of bytes... but it is up to us in updating the BSR
            // value, substracting the amount of bytes transmitted

            // We need to use std::min here because bytesPerLcId can be
            // greater than bsr.txQueueSize because scheduler can assign
            // more bytes than needed due to how TB size is computed.
            bsr.retxQueueSize -= std::min(bytesPerLcId, bsr.retxQueueSize);
        }
        else
        {
            NS_LOG_DEBUG("Something wrong with the calculation of overhead."
                         "Active LCS Retx: "
                         << activeLcsRetx << " assigned to this: " << bytesPerLcId
                         << ", with TBS of " << m_ulDci->m_tbSize.at(0) << " usefulTbs "
                         << usefulTbs << " and total used " << m_ulDciTotalUsed);
        }
    }
}

void
NrUeMac::SendTxData(uint32_t usefulTbs, uint32_t activeTx)
{
    NS_LOG_FUNCTION(this);
    if (activeTx == 0)
    {
        return;
    }
    

    uint32_t bytesPerLcId = usefulTbs / activeTx;

    for (auto& itBsr : m_ulBsrReceived)
    {
        auto& bsr = itBsr.second;

        if (m_ulDciTotalUsed + bytesPerLcId <= usefulTbs)
        {
            LteMacSapUser::TxOpportunityParameters txParams;
            txParams.lcid = bsr.lcid;
            txParams.rnti = m_rnti;
            txParams.bytes = bytesPerLcId;
            txParams.layer = 0;
            txParams.harqId = m_ulDci->m_harqProcess;
            txParams.componentCarrierId = GetBwpId();

            NS_LOG_INFO("Notifying RLC of LCID " << +bsr.lcid
                                                 << " of a TxOpp "
                                                    "of "
                                                 << bytesPerLcId << " B for a TX PDU");
            m_lcInfoMap.at(bsr.lcid).macSapUser->NotifyTxOpportunity(txParams);
            // After this call, m_ulDciTotalUsed has been updated with the
            // correct amount of bytes... but it is up to us in updating the BSR
          // value, substracting the amount of bytes transmitted

            // We need to use std::min here because bytesPerLcId can be
            // greater than bsr.txQueueSize because scheduler can assign
            // more bytes than needed due to how TB size is computed.
            bsr.txQueueSize -= std::min(bytesPerLcId, bsr.txQueueSize);
        }
        else
        {
            NS_LOG_DEBUG("Something wrong with the calculation of overhead."
                         "Active LCS Retx: "
                         << activeTx << " assigned to this: " << bytesPerLcId << ", with TBS of "
                         << m_ulDci->m_tbSize.at(0) << " usefulTbs " << usefulTbs
                         << " and total used " << m_ulDciTotalUsed);
        }
    }
}

void
NrUeMac::SendNewData()
{
    NS_LOG_FUNCTION(this);
    // New transmission -> empty pkt buffer queue (for deleting eventual pkts not acked )
    Ptr<PacketBurst> pb = CreateObject<PacketBurst>();
    m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_pktBurst = pb;
    m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_lcidList.clear();
    NS_LOG_INFO("Reset HARQP " << +m_ulDci->m_harqProcess);

    // Sending the status data has no boundary: let's try to send the ACK as
    // soon as possible, filling the TBS, if necessary.
    SendNewStatusData();

    // Let's count how many LC we have, that are waiting with some data
    uint16_t activeLcsRetx = 0;
    uint16_t activeLcsTx = 0;
    for (const auto& itBsr : m_ulBsrReceived)
    {
        if (itBsr.second.retxQueueSize > 0)
        {
            activeLcsRetx++;
        }
        if (itBsr.second.txQueueSize > 0)
        {
            activeLcsTx++;
        }
    }

    // Of the TBS we received in the DCI, one part is gone for the status pdu,
    // where we didn't check much as it is the most important data, that has to go
    // out. For the rest that we have left, we can use only a part of it because of
    // the overhead of the SHORT_BSR, which is 5 bytes.
    // Under dense / eRedCap scheduling the UL grant TB can be < 5 B; do not abort —
    // skip SHORT_BSR reservation and let the caller skip sending BSR if no room.
    const uint32_t shortBsrBytes = 5;
    const uint32_t tbSize = m_ulDci->m_tbSize.at(0);
    uint32_t usefulTbs = 0;
    if (m_ulDciTotalUsed + shortBsrBytes <= tbSize)
    {
        usefulTbs = tbSize - m_ulDciTotalUsed - shortBsrBytes;
    }
    else if (m_ulDciTotalUsed < tbSize)
    {
        usefulTbs = tbSize - m_ulDciTotalUsed;
        NS_LOG_WARN("UL TB " << tbSize << " B with " << m_ulDciTotalUsed
                             << " B already used; too small for SHORT_BSR, using remaining for data");
    }
    else
    {
        NS_LOG_WARN("UL TB " << tbSize << " B fully consumed before SHORT_BSR reservation ("
                             << m_ulDciTotalUsed << " B used)");
    }

    // Now, we have 3 bytes of overhead for each subPDU. Let's try to serve all
    // the queues with some RETX data.
    if (activeLcsRetx == 0 && activeLcsTx == 0 && usefulTbs > 0)
    {
        NS_LOG_LOGIC("This UE tx opportunity will be wasted: " << usefulTbs << " bytes.");
    }

    // this check is needed, because if there are no active LCS we should not
    // enter into else and call the function SendRetxData
    if (activeLcsRetx > 0 && usefulTbs > 0) // the queues with some RETX data.
    {
        // 10 because 3 bytes will go for MAC subheader
        // and we should ensure to pass to RLC AM at least 7 bytes
        if (activeLcsRetx * 10 > usefulTbs)
        {
            NS_LOG_DEBUG("The overhead for transmitting retx data is greater than the space for "
                         "transmitting it."
                         "Ignore the TBS of "
                         << usefulTbs << " B.");
        }
        else
        {
            usefulTbs -= activeLcsRetx * 3;
            SendRetxData(usefulTbs, activeLcsRetx);
        }
    }

    // Now we have to update our useful TBS for the next transmission.
    // Remember that m_ulDciTotalUsed keep count of data and overhead that we
    // used till now.
    if (m_ulDciTotalUsed + shortBsrBytes <= tbSize)
    {
        usefulTbs = tbSize - m_ulDciTotalUsed - shortBsrBytes;
    }
    else if (m_ulDciTotalUsed < tbSize)
    {
        usefulTbs = tbSize - m_ulDciTotalUsed;
    }
    else
    {
        usefulTbs = 0;
    }

    // The last part is for the queues with some non-RETX data. If there is no space left,
    // then nothing.
    if (activeLcsTx > 0 && usefulTbs > 0) // the queues with some TX data.
    {
        // 10 because 3 bytes will go for MAC subheader
        // and we should ensure to pass to RLC AM at least 7 bytes
        if (activeLcsTx * 10 > usefulTbs)
        {
            NS_LOG_DEBUG("The overhead for transmitting new data is greater than the space for "
                         "transmitting it."
                         "Ignore the TBS of "
                         << usefulTbs << " B.");
        }
        else
        {
            usefulTbs -= activeLcsTx * 3;
            SendTxData(usefulTbs, activeLcsTx);
            
        }
    }

    // If we did not used the packet burst, explicitly signal it to the HARQ
    // retx, if any.
    if (m_ulDciTotalUsed == 0)
    {

        //m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_pktBurst = nullptr;
         Ptr<PacketBurst> pb = CreateObject<PacketBurst>();
        m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_pktBurst = pb;
        m_miUlHarqProcessesPacket.at(m_ulDci->m_harqProcess).m_lcidList.clear();
    }
}

void
NrUeMac::SendNewStatusData()
{
    NS_LOG_FUNCTION(this);

    bool hasStatusPdu = false;
    bool sentOneStatusPdu = false;

    for (auto& bsrIt : m_ulBsrReceived)
    {
        auto& bsr = bsrIt.second;

        if (bsr.statusPduSize > 0)
        {
            hasStatusPdu = true;

            // Check if we have room to transmit the statusPdu
            if (m_ulDciTotalUsed + bsr.statusPduSize <= m_ulDci->m_tbSize.at(0))
            {
                LteMacSapUser::TxOpportunityParameters txParams;
                txParams.lcid = bsr.lcid;
                txParams.rnti = m_rnti;
                txParams.bytes = bsr.statusPduSize;
                txParams.layer = 0;
                txParams.harqId = m_ulDci->m_harqProcess;
                txParams.componentCarrierId = GetBwpId();

                NS_LOG_INFO("Notifying RLC of LCID " << +bsr.lcid
                                                     << " of a TxOpp "
                                                        "of "
                                                     << bsr.statusPduSize << " B for a status PDU");

                m_lcInfoMap.at(bsr.lcid).macSapUser->NotifyTxOpportunity(txParams);
                // After this call, m_ulDciTotalUsed has been updated with the
                // correct amount of bytes... but it is up to us in updating the BSR
                // value, subtracting the amount of bytes transmitted
                bsr.statusPduSize = 0;
                sentOneStatusPdu = true;
            }
            else
            {
                NS_LOG_INFO("Cannot send StatusPdu of " << bsr.statusPduSize
                                                        << " B, we already used all the TBS");
            }
        }
    }

    NS_ABORT_MSG_IF(hasStatusPdu && !sentOneStatusPdu,
                    "The TBS of size " << m_ulDci->m_tbSize.at(0)
                                       << " doesn't allow us "
                                          "to send one status PDU...");
}

void
NrUeMac::DoReceiveControlMessage(Ptr<NrControlMessage> msg)
{
    NS_LOG_FUNCTION(this << msg);
    switch (msg->GetMessageType())
    {
    case (NrControlMessage::DL_DCI): {
        m_cmacSapUser->RefreshBwpInactivityTimer();
        break;
    }
    case (NrControlMessage::UL_DCI): {
        //sanity check
        //NS_ABORT_IF(!m_activeBwpDL||!m_activeBwpUL);
        // the bwp should stay active when it is used to transmit data
        m_cmacSapUser->RefreshBwpInactivityTimer();
        ProcessUlDci(DynamicCast<NrUlDciMessage>(msg));
        break;
    }
    case (NrControlMessage::RAR): {
        NS_LOG_INFO("Received RAR in slot " << m_currentSlot);
        NS_ABORT_IF(!m_activeBwpDL||!m_activeBwpUL);
        m_macRxedCtrlMsgsTrace(m_currentSlot, GetCellId(), m_rnti, GetBwpId(), msg);

        if (m_waitingForRaResponse == true)
        {
            Ptr<NrRarMessage> rarMsg = DynamicCast<NrRarMessage>(msg);
            for (std::list<NrRarMessage::Rar>::const_iterator it =
                        rarMsg->RarListBegin();
                    it != rarMsg->RarListEnd();
                    ++it)
            {
                uint16_t raRnti = it->rarPayload.raRnti;
                NS_LOG_LOGIC(this << "got RAR with RA-RNTI " << (uint32_t)raRnti << ", expecting "
                                << (uint32_t)m_raRnti);
                if (raRnti == m_raRnti) // RAR is for my RO
                {
                    if (it->rapId == m_raPreambleId) // RAR is for me
                    {
                        RecvRaResponse(it->rarPayload);
                        break;
                    }
                }
            }
        }
    
        break;
    }

    case (NrControlMessage::PAGING): {
        Ptr<NrPagingMessage> pagMsg = DynamicCast<NrPagingMessage>(msg);
        if(pagMsg->GetPRnti() == m_pRnti)
        {
            //std::cout<<"Paging angekommen"<<std::endl;
            if(!m_startingRach && !m_waitForPrachOcc)
            {
                DoStartContentionBasedRandomAccessProcedure(true,false);
            }

        }     
    break;
    }

    default:
        NS_LOG_LOGIC("Control message not supported/expected");
    }
}

NrUePhySapUser*
NrUeMac::GetPhySapUser()
{
    return m_phySapUser;
}

void
NrUeMac::SetPhySapProvider(NrPhySapProvider* ptr)
{
    m_phySapProvider = ptr;
}

void
NrUeMac::DoConfigureRach([[maybe_unused]] LteUeCmacSapProvider::RachConfig rc)
{
    NS_LOG_FUNCTION(this);
}

void
NrUeMac::DoStartContentionBasedRandomAccessProcedure(bool redcap, bool do_2step_sdt)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT_MSG(m_activeBwpUL,"Tried to start a random access in an inactive BWP. This should not occur.");

    m_preambleTransmissionCounter = 1;
    m_backoffParameter = 0;
    m_startingRach = true;
    
}

void
NrUeMac::RandomlySelectAndSendRaPreamble()
{
    NS_LOG_FUNCTION(this);
    NS_LOG_DEBUG(m_currentSlot << " Received System Information, send to PHY the RA preamble");
    
    uint8_t prachOcc;
    if(hasPrach(m_currentSlot))
    {
        prachOcc = ChooseRandomPrachOccasionThisSlot();
    }
    else{
        m_waitForPrachOcc = false;
        m_startingRach = true;
        return;
    }
    m_prachOcc = prachOcc;

    if(m_currentSlot.GetSubframe() == m_prachSubframe && m_currentSlot.GetSlot() == m_prachSlot)
    {
        NS_LOG_DEBUG("Chose PRACH-occasion numer: "<< prachOcc << " with corresponding RaRnti: "<<m_raRnti);
        SendRaPreamble(true);
    }
    else{
        m_waitForPrachOcc = true;
    }
        
}

uint16_t 
NrUeMac::ChooseRandomPrachOccasionThisSlot()
{
    std::vector<uint16_t> possiblePrachOcc; 
    int fdm = pow(2,static_cast<int>(m_sib.GetSib1().siSchedulingInfo.sIRequestConfig.rachOccasionsSI.rachConfigSI.choice));
    uint8_t numSlotInSf = pow(2,m_currentSlot.GetNumerology());

    //check which PRACH Slots are viable
    std::vector<uint8_t>::iterator iter = std::find(m_prachSfn.begin(),m_prachSfn.end(),m_currentSlot.GetSubframe());
    
    //a check for a viable transmission slot is needed
    if(iter != m_prachSfn.end() )
    {
        for(uint8_t index = 0; index <fdm ; ++index)
        {
            for(uint8_t countPrachOcc = 0; countPrachOcc<m_prachOccInSlot ; ++countPrachOcc)
            {
                possiblePrachOcc.emplace_back(index*m_prachOccInSlot*m_prachSfn.size()*m_numPrachSlots + *iter *m_prachOccInSlot*m_numPrachSlots +countPrachOcc );
            }
        }
        
    }
    else{
        //TODO
        NS_ASSERT(false);
    }
    

    //generate a random Occasion from all viable ones
    uint8_t prachOcc = possiblePrachOcc[m_raPreambleUniformVariable->GetInteger(0, possiblePrachOcc.size()-1)];
    m_prachSubframe =  m_currentSlot.GetSubframe();

    uint8_t s_id =m_prachStartSym + (prachOcc%m_prachOccInSlot * m_prachDuration) ; // where s_id is the index of the first OFDM symbol of the PRACH occasion (0 ≤ s_id < 14)
    NS_ASSERT(s_id >=0 && s_id <14);
    uint8_t t_id;
    if(m_numPrachSlots ==1)
    {
        m_prachSlot =  numSlotInSf-1; //last slot
        t_id = m_prachSubframe*numSlotInSf + m_prachSlot; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
    }
    else
    {
        //note: only valid for numerology= 1 and 2
        if(prachOcc%(m_numPrachSlots*m_prachOccInSlot) >= m_prachOccInSlot )
        {
            // second slot of subframe is used
            m_prachSlot = 1;
            t_id = m_prachSubframe*numSlotInSf + m_prachSlot; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
        }
        else
        {
            // first slot of subframe is used
            m_prachSlot =0;
            t_id =m_prachSubframe*numSlotInSf ; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
        }

    }

    NS_ASSERT(t_id >=0 && t_id <80);
    uint8_t f_id = prachOcc/(m_prachOccInSlot*m_prachSfn.size()*m_numPrachSlots); // f_id is the index of the PRACH occasion in the frequency domain (0 ≤ f_id < 8)


    uint8_t ul_carrier_id =0; // ul_carrier_id is the UL carrier used for Random Access Preamble transmission (0 for NUL carrier, and 1 for SUL carrier)
     //TS 138 321 - V17.0.0 5.1.3
    m_raRnti = 1 + s_id + 14 * t_id + 14 * 80 * f_id + 14 * 80 * 8 * ul_carrier_id;
    return prachOcc;


}

uint16_t 
NrUeMac::ChooseRandomPrachOccasion()
{
    uint8_t StartPrachOcc = 0;
    int fdm = pow(2,static_cast<int>(m_sib.GetSib1().siSchedulingInfo.sIRequestConfig.rachOccasionsSI.rachConfigSI.choice));
    uint8_t numSlotInSf = pow(2,m_currentSlot.GetNumerology());

    //check which PRACH Slots are viable
    while(m_currentSlot.GetSubframe() > m_prachSfn.at(StartPrachOcc/(m_prachOccInSlot*m_numPrachSlots)))
    {
        
        StartPrachOcc+=(m_prachOccInSlot*m_numPrachSlots);
        if(StartPrachOcc/m_prachOccInSlot == m_prachSfn.size())
        {
            StartPrachOcc=0;
            break;
        }
    }
    // if we transmit in this subframe, a check for a viable transmission slot is needed
    if( m_currentSlot.GetSubframe() == m_prachSfn.at(StartPrachOcc/(m_prachOccInSlot*m_numPrachSlots)) && m_numPrachSlots != 1 && m_currentSlot.GetSlot() != 0)
    {
        //Prach config has occasions in slot 0 but current Slotnumber is 1
        //Add Occasions of slot 0 to StartPrachOcc
        StartPrachOcc += m_prachOccInSlot;
    }
    

    //generate a random Occasion from all viable ones
    uint8_t prachOcc = m_raPreambleUniformVariable->GetInteger(StartPrachOcc, m_prachOccInSlot*m_prachSfn.size()*m_numPrachSlots-1);
    m_prachSubframe =  m_prachSfn.at(prachOcc/(m_prachOccInSlot*m_numPrachSlots));

    uint8_t s_id =m_prachStartSym + (prachOcc%m_prachOccInSlot * m_prachDuration) ; // where s_id is the index of the first OFDM symbol of the PRACH occasion (0 ≤ s_id < 14)
    NS_ASSERT(s_id >=0 && s_id <14);
    uint8_t t_id;
    if(m_numPrachSlots ==1)
    {
        m_prachSlot =  numSlotInSf-1; //last slot
        t_id = m_prachSubframe*numSlotInSf + m_prachSlot; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
    }
    else
    {
        //note: only valid for numerology= 1 and 2
        if(prachOcc%(m_numPrachSlots*m_prachOccInSlot) >= m_prachOccInSlot )
        {
            // second slot of subframe is used
            m_prachSlot = 1;
            t_id = m_prachSubframe*numSlotInSf + m_prachSlot; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
        }
        else
        {
            // first slot of subframe is used
            m_prachSlot =0;
            t_id =m_prachSubframe*numSlotInSf ; //t_id is the index of the first slot of the PRACH occasion in a system frame (0 ≤ t_id < 80)
        }

    }

    NS_ASSERT(t_id >=0 && t_id <80);
    uint8_t f_id = m_raPreambleUniformVariable->GetInteger(0, fdm-1); // f_id is the index of the PRACH occasion in the frequency domain (0 ≤ f_id < 8)

    //adjust used occasion with frequency domain
    NS_ASSERT_MSG(uint16_t(prachOcc+(f_id*m_prachOccInSlot*m_prachSfn.size()*m_numPrachSlots))< 256,"Exceeded maximum capacity of an uint8_t for prachOccasions. Change to uin16_t needed");

    prachOcc = prachOcc+(f_id*m_prachOccInSlot*m_prachSfn.size()*m_numPrachSlots);

    uint8_t ul_carrier_id =0; // ul_carrier_id is the UL carrier used for Random Access Preamble transmission (0 for NUL carrier, and 1 for SUL carrier)
     //TS 138 321 - V17.0.0 5.1.3
    m_raRnti = 1 + s_id + 14 * t_id + 14 * 80 * f_id + 14 * 80 * 8 * ul_carrier_id;
    return prachOcc;


}

bool
NrUeMac::hasPrach(SfnSf sfn)
{
    //38.211 5.3.2
    bool inFrame = sfn.GetFrame()%m_nfX ==m_nfY;
    bool inSubframe = std::find(m_prachSfn.begin(),m_prachSfn.end(), sfn.GetSubframe())  !=m_prachSfn.end();
    //a SCS of 30kHz is always used. Has to be adapted for hihger SCS 
    bool inSlot;
    if( sfn.GetNumerology() == 1)
    {
        if(m_numPrachSlots ==2)
        {
            inSlot = true;
        }
        else{
            inSlot = sfn.GetSlot() == 1;
        }
    }
    else{
        //numerology = 0, other numerologys not supported
        inSlot = true;
    }

    if(inFrame && inSubframe && inSlot && GetBwpId() == 0) //only the first bwp has Prach ressources in it
    {
        return true;
    }
    return false;
}

void
NrUeMac::SendRaPreamble([[maybe_unused]] bool contention)
{
    NS_LOG_INFO(this);
    NS_ASSERT(m_sib.GetSib1().servingCellConfigCommon.uplinkConfigCommon.rach_ConfigCommon.totalNumberOfRA_Preambles<=64);
    m_raPreambleId =
        m_raPreambleUniformVariable->GetInteger(0, m_sib.GetSib1().servingCellConfigCommon.uplinkConfigCommon.rach_ConfigCommon.totalNumberOfRA_Preambles-1); //every occasion can have 64 differen preambles    



    Ptr<NrRachPreambleMessage> rachMsg = Create<NrRachPreambleMessage>();
    rachMsg->SetSourceBwp(GetBwpId());
    m_macTxedCtrlMsgsTrace(m_currentSlot, GetCellId(), m_rnti, GetBwpId(), rachMsg);

    uint32_t prachNumber = Simulator::Now().GetMilliSeconds()/10; //TODO 

    m_phySapProvider->SendRachPreamble(m_raPreambleId, m_raRnti,m_prachOcc, m_imsi, prachNumber);
    NrAccessStats::IncPreambleTx();
    m_powerStateChangedCallback(NrEnergyModel::PowerState::RRC_SENDING_PRACH); //
    //schedule a switch back to connected state. For now only the duration of the prach is considered. Therefore it might not use the correct
    //symcols in the slot
    Time prachDuration = NanoSeconds(m_prachDuration* (10e5/(pow(2,m_currentSlot.GetNumerology())))/14);
    Simulator::Schedule(prachDuration,[=]() {
        m_powerStateChangedCallback(NrEnergyModel::PowerState::RRC_Connected);
        });
    // 3GPP 36.321 5.1.4 
    Time raWindowBegin = MilliSeconds (1); 
    //Time raWindowEnd = MilliSeconds (3 + m_sib.GetSib1().siSchedulingInfo.sIRequestConfig.rachOccasionsSI.rachConfigSI.window);
    Time raWindowEnd = MilliSeconds (3 + 8);
    //TODO transform "m_sib.GetSib1().siSchedulingInfo.sIRequestConfig.rachOccasionsSI.rachConfigSI.window" to milliseconds

    Simulator::Schedule (raWindowBegin, &NrUeMac::StartWaitingForRaResponse, this);
    m_noRaResponseReceivedEvent = Simulator::Schedule (raWindowEnd, &NrUeMac::RaResponseTimeout, this, contention);    
}

void 
NrUeMac::StartWaitingForRaResponse ()
{
    NS_LOG_FUNCTION (this);
    m_waitingForRaResponse = true;
}

void 
NrUeMac::RaResponseTimeout (bool contention)
{
    NS_LOG_FUNCTION (this << contention);
    m_waitingForRaResponse = false;
    m_waitForPrachOcc = false;
    // 3GPP 36.321 5.1.4
    ++m_preambleTransmissionCounter;
    NrAccessStats::IncRarTimeout();
    //if (m_preambleTransmissionCounter > m_sib.GetSib1().siSchedulingInfo.sIRequestConfig.rachOccasionsSI.rachConfigSI.amount)
    if (m_preambleTransmissionCounter > 100)
    {
        // Dense-campaign hardening: notify RRC instead of aborting; IncRaFailed is counted in RRC.
        NrCampaignStats::SoftFail("UEMAC_RA_PREAMBLE_MAX@ue");
        NS_LOG_WARN("RAR timeout, preambleTransMax reached => giving up (imsi=" << m_imsi << ")");
        m_cmacSapUser->NotifyRandomAccessFailed ();
    }
    else
    {
        NS_LOG_INFO ("RAR timeout, re-send preamble");
        if (contention)
        {
            RandomlySelectAndSendRaPreamble ();
        }
        else
        {
            SendRaPreamble (contention);
        }
    }
}

void
NrUeMac::SrSchedulingTimeout()
{
    std::cout<<"SrSchedulingTimeout for imsi: "<<m_imsi<<" in Bwp: "<<GetBwpId()<<std::endl;
    if(m_srState == ACTIVE)
     {
        NrCampaignStats::Inc("ue.sr_unanswered");
        m_srState = TO_SEND;
        //we waited for an UL_DCi but didnt get one. Try it again
     }
    else if (m_srState == INACTIVE)
    {
        // Released / reset while the timer was still pending.
        NrCampaignStats::SoftFail("UEMAC_SR_TIMEOUT_INACTIVE@ue");
        NS_LOG_WARN("SrSchedulingTimeout while INACTIVE imsi="
                    << m_imsi << " — ignoring (dense short-DIT hardening).");
    }
}

void
NrUeMac::DoStartNonContentionBasedRandomAccessProcedure(uint16_t rnti,
                                                        [[maybe_unused]] uint8_t preambleId,
                                                        uint8_t prachMask)
{
    NS_LOG_FUNCTION(this << " rnti" << rnti);
    NS_ASSERT_MSG(prachMask == 0,
                  "requested PRACH MASK = " << (uint32_t)prachMask
                                            << ", but only PRACH MASK = 0 is supported");
    m_rnti = rnti;
}

void
NrUeMac::AddLc(uint8_t lcId,
               LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
               LteMacSapUser* msu)
{
    NS_LOG_FUNCTION(this << " lcId" << (uint32_t)lcId);
    // NS_ASSERT_MSG(m_lcInfoMap.find(lcId) == m_lcInfoMap.end(),
    //               "cannot add channel because LCID " << lcId << " is already present");
    //LC is overwritten when transistioning back to connected after a rlf

    LcInfo lcInfo;
    lcInfo.lcConfig = lcConfig;
    lcInfo.macSapUser = msu;
    m_lcInfoMap[lcId] = lcInfo;
}

void
NrUeMac::DoRemoveLc(uint8_t lcId)
{
    NS_LOG_FUNCTION(this << " lcId" << lcId);
    NS_ASSERT_MSG(m_lcInfoMap.find(lcId) != m_lcInfoMap.end(), "could not find LCID " << lcId);
    m_lcInfoMap.erase(lcId);
    m_ulBsrReceived.erase(lcId); // empty BSR buffer for this lcId
}

LteMacSapProvider*
NrUeMac::GetUeMacSapProvider()
{
    return m_macSapProvider;
}

void
NrUeMac::DoReset()
{
    m_rnti = 0;
    m_ulBsrReceived.clear();
    if(GetBwpId() == 0) 
    {
        m_activeBwpDL=true;
        m_activeBwpUL =true;
    }
    else{
        m_activeBwpDL = false;
        m_activeBwpUL = false;
    }
    if(m_noRaResponseReceivedEvent.IsRunning())
    {
         m_noRaResponseReceivedEvent.Cancel();
    }  
    m_srState = INACTIVE;
    if(m_waitingForDciReception.IsRunning())
    {
        m_waitingForDciReception.Cancel();
    }
    if(m_waitingForSchedulingTimer.IsRunning())
    {
        m_waitingForSchedulingTimer.Cancel();
    }
    
    NS_LOG_FUNCTION(this);
}

//////////////////////////////////////////////

int64_t
NrUeMac::AssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_raPreambleUniformVariable->SetStream(stream);
    return 1;
}

void 
NrUeMac::DoRecvSib1(NrSib1Message sib1, NrPhySapProvider::PrachConfig prachOccasions)
{
    m_sib = sib1;
    m_sibReceived = true;
    m_prachSfn = prachOccasions.m_SfN;
    m_prachStartSym = prachOccasions.m_symStart;
    m_prachDuration = prachOccasions.m_duration;
    m_nfX = prachOccasions.m_nfX;
    m_nfY = prachOccasions.m_nfY;
    m_numPrachSlots = prachOccasions.m_numPrachSlots;
    m_preambleFormat = prachOccasions.m_PreambleFormat;
    m_prachOccInSlot = prachOccasions.m_prachOccInSlot;
    m_sib1Received = true;
}


void
NrUeMac::ProcessULPacket()
{
  NS_LOG_FUNCTION (this);
  // Saving the data we need in DoTransmitPdu
  m_ulDciTotalUsed = 0;


  //m_macRxedCtrlMsgsTrace (m_currentSlot, GetCellId (), m_rnti, GetBwpId (), dciMsg);

  NS_LOG_INFO ("UL DCI received, transmit data in slot " << m_ulDciSfnsf <<
               " Harq Process " << +m_ulDci->m_harqProcess <<
               " TBS " << m_ulDci->m_tbSize.at (0) << " total queue " << GetTotalBufSize ());


  if (m_ulDci->m_ndi.at (0) == 0)
    {
      // This method will retransmit the data saved in the harq buffer
      TransmitRetx ();

      // This method will transmit a new BSR.
      SendReportBufferStatus (m_ulDciSfnsf, m_ulDci->m_symStart);
    }
  else if (m_ulDci->m_ndi.at (0) == 1)
    {
      SendNewData ();

      NS_LOG_INFO ("After sending NewData, bufSize " << GetTotalBufSize ());

      // Send a new BSR. SendNewData() already took into account the size of
      // the BSR.
      SendReportBufferStatus (m_ulDciSfnsf, m_ulDci->m_symStart);
      NS_LOG_INFO ("UL DCI processing done, sent to PHY a total of " << m_ulDciTotalUsed <<
                   " B out of " << m_ulDci->m_tbSize.at (0) << " allocated bytes ");

      if (m_ulDciTotalUsed == 0)
        {
          NS_LOG_WARN ("No byte used for this UL-DCI, sending empty PDU (bufSize "
                       << GetTotalBufSize () << ")");

          LteMacSapProvider::TransmitPduParameters txParams;

          txParams.pdu = Create<Packet> ();
          txParams.lcid = 3;
          txParams.rnti = m_rnti;
          txParams.layer = 0;
          txParams.harqProcessId = m_ulDci->m_harqProcess;
          txParams.componentCarrierId = GetBwpId ();

          DoTransmitPdu (txParams);
        }
    }
}


bool
NrUeMac::GetActiveBwpStatus () const
{
  return m_activeBwpDL || m_activeBwpUL;
}

void
NrUeMac::SetActiveBwpStatus (bool state)
{
    //initialization value
  
  m_activeBwpDL = state;
  m_activeBwpUL = state;
}

void
NrUeMac::SetPowerStateChangedCallback(Callback<void,NrEnergyModel::PowerState> pscc)
{
    m_powerStateChangedCallback = pscc;
}



}
