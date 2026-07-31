/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "nr-rrc-protocol-real.h"

#include "nr-gnb-net-device.h"
#include "nr-ue-net-device.h"

#include "nr-gnb-rrc.h"
#include "nr-ue-rrc.h"
#include <ns3/fatal-error.h>
#include <ns3/log.h>
#include <ns3/node-list.h>
#include <ns3/node.h>
#include <ns3/nstime.h>
#include <ns3/simulator.h>

NS_LOG_COMPONENT_DEFINE("nrRrcProtocolReal");

namespace ns3
{

static const Time RRC_REAL_MSG_DELAY = MilliSeconds(0);

NS_OBJECT_ENSURE_REGISTERED(nrUeRrcProtocolReal);

nrUeRrcProtocolReal::nrUeRrcProtocolReal()
    : m_ueRrcSapProvider(nullptr),
      m_enbRrcSapProvider(nullptr)
{
    m_ueRrcSapUser = new MemberNrUeRrcSapUser<nrUeRrcProtocolReal>(this);
    m_completeSetupParameters.srb0SapUser =
        new LteRlcSpecificLteRlcSapUser<nrUeRrcProtocolReal>(this);
    m_completeSetupParameters.srb1SapUser =
        new LtePdcpSpecificLtePdcpSapUser<nrUeRrcProtocolReal>(this);
}

nrUeRrcProtocolReal::~nrUeRrcProtocolReal()
{
}

void
nrUeRrcProtocolReal::DoDispose()
{
    NS_LOG_FUNCTION(this);
    delete m_ueRrcSapUser;
    delete m_completeSetupParameters.srb0SapUser;
    delete m_completeSetupParameters.srb1SapUser;
    m_rrc = nullptr;
}

TypeId
nrUeRrcProtocolReal::GetTypeId()
{
    static TypeId tid = TypeId("ns3::nrUeRrcProtocolReal")
                            .SetParent<Object>()
                            .AddConstructor<nrUeRrcProtocolReal>();
    return tid;
}

void
nrUeRrcProtocolReal::SetNrUeRrcSapProvider(NrUeRrcSapProvider* p)
{
    m_ueRrcSapProvider = p;
}

NrUeRrcSapUser*
nrUeRrcProtocolReal::GetNrUeRrcSapUser()
{
    return m_ueRrcSapUser;
}

void
nrUeRrcProtocolReal::SetUeRrc(Ptr<NrUeRrc> rrc)
{
    m_rrc = rrc;
}

void
nrUeRrcProtocolReal::DoSetup(NrUeRrcSapUser::SetupParameters params)
{
    NS_LOG_FUNCTION(this);
    m_setupParameters.srb0SapProvider = params.srb0SapProvider;
    m_setupParameters.srb1SapProvider = params.srb1SapProvider;
    m_ueRrcSapProvider->CompleteSetup(m_completeSetupParameters);
}

void
nrUeRrcProtocolReal::DoSendRrcConnectionRequest(NrRrcSap::NrRrcConnectionRequest msg, uint16_t grantBytes)
{
    // initialize the RNTI and get the EnbNrRrcSapProvider for the
    // gNB we are currently attached to
    m_rnti = m_rrc->GetRnti();
    SetGnbRrcSapProvider();

    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionRequestHeader rrcConnectionRequestHeader;
    rrcConnectionRequestHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionRequestHeader);

    // LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    // transmitPdcpPduParameters.pdcpPdu = packet;
    // transmitPdcpPduParameters.rnti = m_rnti;
    // transmitPdcpPduParameters.lcid = 0;

    m_setupParameters.srb0SapProvider->SendMsg3(packet,grantBytes);

    
    // LteMacSapUser::TxOpportunityParameters txParams;
    // txParams.lcid = 0;
    // txParams.rnti = m_rnti;
    // //txParams.bytes = grant.bytes;
    // txParams.bytes = 10;
    // txParams.layer = 0;
    // //txParams.harqId = m_ulDci->m_harqProcess;
    // //txParams.componentCarrierId = GetBwpId();
    // txParams.componentCarrierId = 0;

    


    // m_setupParameters.srb0SapProvider->DoNotifyTxOpportunity(txParams);
    


    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvRrcConnectionRequest,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendRrcResumeRequest(bool sdt, NrRrcSap::RrcResumeRequest msg, uint16_t grantedBytes)
{
    // initialize the RNTI and get the EnbNrRrcSapProvider for the
    // gNB we are currently attached to
    m_rnti = m_rrc->GetRnti();
    SetGnbRrcSapProvider();

    Ptr<Packet> packet = Create<Packet>();

    RrcResumeRequestHeader rrcResumeRequestHeader;
    rrcResumeRequestHeader.SetMessage(msg);

    packet->AddHeader(rrcResumeRequestHeader);

    if( sdt ==true)
    {
        for(auto it = msg.sdtData.begin(); it != msg.sdtData.end(); ++it)
        {
             packet->AddAtEnd(*it);
        }
       

    }
    m_setupParameters.srb0SapProvider->SendMsg3(packet,grantedBytes);

}

void
nrUeRrcProtocolReal::DoSendRrcConnectionSetupCompleted(NrRrcSap::RrcConnectionSetupCompleted msg)
{
    Ptr<Packet> packet = Create<Packet>();
    RrcConnectionSetupCompleteHeader rrcConnectionSetupCompleteHeader;
    rrcConnectionSetupCompleteHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionSetupCompleteHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = m_rnti;
    transmitPdcpSduParameters.lcid = 1;

    if (m_setupParameters.srb1SapProvider)
    {
        m_setupParameters.srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    }

    // Simulator::Schedule(RRC_REAL_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvRrcConnectionSetupCompleted,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendRrcResumeComplete(NrRrcSap::RrcResumeComplete msg)
{
    Ptr<Packet> packet = Create<Packet>();

    RrcResumeCompleteHeader rrcResumeCompleteHeader;
    rrcResumeCompleteHeader.SetMessage(msg);

    packet->AddHeader(rrcResumeCompleteHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = m_rnti;
    transmitPdcpSduParameters.lcid = 1;

    if (m_setupParameters.srb1SapProvider)
    {
        m_setupParameters.srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    }

}

void
nrUeRrcProtocolReal::DoSendRrcConnectionReconfigurationCompleted(
    NrRrcSap::RrcConnectionReconfigurationCompleted msg)
{
    // re-initialize the RNTI and get the EnbNrRrcSapProvider for the
    // gNB we are currently attached to
    m_rnti = m_rrc->GetRnti();
    SetGnbRrcSapProvider();

    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionReconfigurationCompleteHeader rrcConnectionReconfigurationCompleteHeader;
    rrcConnectionReconfigurationCompleteHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionReconfigurationCompleteHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = m_rnti;

    transmitPdcpSduParameters.lcid = 1;

    m_setupParameters.srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);


    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvRrcConnectionReconfigurationCompleted,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendRrcConnectionReestablishmentRequest(
    NrRrcSap::RrcConnectionReestablishmentRequest msg)
{

    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionReestablishmentRequestHeader rrcConnectionReestablishmentRequestHeader;
    rrcConnectionReestablishmentRequestHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionReestablishmentRequestHeader);

    LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    transmitPdcpPduParameters.pdcpPdu = packet;
    transmitPdcpPduParameters.rnti = m_rnti;
    transmitPdcpPduParameters.lcid = 0;

    m_setupParameters.srb0SapProvider->TransmitPdcpPdu(transmitPdcpPduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvRrcConnectionReestablishmentRequest,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendRrcConnectionReestablishmentComplete(
    NrRrcSap::RrcConnectionReestablishmentComplete msg)
{

    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionReestablishmentCompleteHeader rrcConnectionReestablishmentCompleteHeader;
    rrcConnectionReestablishmentCompleteHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionReestablishmentCompleteHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = m_rnti;
    transmitPdcpSduParameters.lcid = 1;

    m_setupParameters.srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvRrcConnectionReestablishmentComplete,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendMeasurementReport(NrRrcSap::MeasurementReport msg)
{
    // re-initialize the RNTI and get the EnbNrRrcSapProvider for the
    // eNB we are currently attached to
    m_rnti = m_rrc->GetRnti();
    SetGnbRrcSapProvider();

    Ptr<Packet> packet = Create<Packet>();

    MeasurementReportHeader measurementReportHeader;
    measurementReportHeader.SetMessage(msg);

    packet->AddHeader(measurementReportHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = m_rnti;
    transmitPdcpSduParameters.lcid = 1;

    m_setupParameters.srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrGnbRrcSapProvider::RecvMeasurementReport,
    //                     m_enbRrcSapProvider,
    //                     m_rnti,
    //                     msg);
}

void
nrUeRrcProtocolReal::DoSendIdealUeContextRemoveRequest(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    uint16_t cellId = m_rrc->GetCellId();
    // re-initialize the RNTI and get the EnbNrRrcSapProvider for the
    // eNB we are currently attached to or attempting random access to
    // a target eNB
    m_rnti = m_rrc->GetRnti();

    NS_LOG_DEBUG("RNTI " << rnti << " sending UE context remove request to cell id " << cellId);
    NS_ABORT_MSG_IF(m_rnti != rnti, "RNTI mismatch");

    SetGnbRrcSapProvider(); // the provider has to be reset since the cell might
                            //  have changed due to handover
    // ideally informing eNB
    Simulator::Schedule(RRC_REAL_MSG_DELAY,
                        &NrGnbRrcSapProvider::RecvIdealUeContextRemoveRequest,
                        m_enbRrcSapProvider,
                        rnti);
}

void 
nrUeRrcProtocolReal::DoUpdateBwp(uint16_t rnti, uint8_t bwpID)
{
    m_enbRrcSapProvider->UpdateGnbBwpMap(rnti,bwpID);
}



void
nrUeRrcProtocolReal::SetGnbRrcSapProvider()
{
    uint16_t bwpId = m_rrc->GetCellId();

    // walk list of all nodes to get the peer gNB
    Ptr<NrGnbNetDevice> gnbDev;
    NodeList::Iterator listEnd = NodeList::End();
    bool found = false;
    for (NodeList::Iterator i = NodeList::Begin(); (i != listEnd) && (!found); ++i)
    {
        Ptr<Node> node = *i;
        int nDevs = node->GetNDevices();
        for (int j = 0; (j < nDevs) && (!found); j++)
        {
            gnbDev = node->GetDevice(j)->GetObject<NrGnbNetDevice>();
            if (gnbDev != nullptr)
            {
                for (uint32_t h = 0; h < gnbDev->GetCcMapSize(); ++h)
                {
                    if (gnbDev->GetBwpId(h) == bwpId)
                    {
                        found = true;
                        break;
                    }
                }
            }
        }
    }
    NS_ASSERT_MSG(found, " Unable to find gNB with BwpID =" << bwpId);
    m_enbRrcSapProvider = gnbDev->GetRrc()->GetNrGnbRrcSapProvider();
    Ptr<NrGnbRrcProtocolReal> enbRrcProtocolReal =
        gnbDev->GetRrc()->GetObject<NrGnbRrcProtocolReal>();
    enbRrcProtocolReal->SetUeRrcSapProvider(m_rnti, m_ueRrcSapProvider);
}

void
nrUeRrcProtocolReal::DoReceivePdcpPdu(Ptr<Packet> p)
{
    // Get type of message received
    RrcDlCcchMessage rrcDlCcchMessage;
    p->PeekHeader(rrcDlCcchMessage);

    // Declare possible headers to receive
    RrcConnectionReestablishmentHeader rrcConnectionReestablishmentHeader;
    RrcConnectionReestablishmentRejectHeader rrcConnectionReestablishmentRejectHeader;
    RrcSetupHeader RrcSetupHeader;
    RrcConnectionRejectHeader rrcConnectionRejectHeader;

    // Declare possible messages
    NrRrcSap::RrcConnectionReestablishment rrcConnectionReestablishmentMsg;
    NrRrcSap::RrcConnectionReestablishmentReject rrcConnectionReestablishmentRejectMsg;
    NrRrcSap::RrcSetup RrcSetupMsg;
    NrRrcSap::RrcConnectionReject rrcConnectionRejectMsg;

    // Deserialize packet and call member recv function with appropriate structure
    switch (rrcDlCcchMessage.GetMessageType())
    {
    case 0:
        // RrcConnectionReestablishment
        p->RemoveHeader(rrcConnectionReestablishmentHeader);
        rrcConnectionReestablishmentMsg = rrcConnectionReestablishmentHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcConnectionReestablishment(rrcConnectionReestablishmentMsg);
        break;
    case 1:
        // RrcConnectionReestablishmentReject
        p->RemoveHeader(rrcConnectionReestablishmentRejectHeader);
        rrcConnectionReestablishmentRejectMsg =
            rrcConnectionReestablishmentRejectHeader.GetMessage();
        // m_ueRrcSapProvider->RecvRrcConnectionReestablishmentReject
        // (rrcConnectionReestablishmentRejectMsg);
        break;
    case 2:
        // RrcConnectionReject
        p->RemoveHeader(rrcConnectionRejectHeader);
        rrcConnectionRejectMsg = rrcConnectionRejectHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcConnectionReject(rrcConnectionRejectMsg);
        break;
    case 3:
        // RrcSetup
        p->RemoveHeader(RrcSetupHeader);
        RrcSetupMsg = RrcSetupHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcSetup(RrcSetupMsg);
        break;

    }
}

void
nrUeRrcProtocolReal::DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params)
{
    // Get type of message received
    RrcDlDcchMessage rrcDlDcchMessage;
    params.pdcpSdu->PeekHeader(rrcDlDcchMessage);

    // Declare possible headers to receive
    RrcReconfigurationHeader rrcReconfigurationHeader;
    RrcReleaseHeader rrcReleaseHeader;
    RrcResumeHeader rrcResumeHeader;

    // Declare possible messages to receive
    NrRrcSap::RrcReconfiguration rrcReconfigurationMsg;
    NrRrcSap::RrcRelease rrcReleaseMsg;
    NrRrcSap::RrcResume rrcResumeMsg;

    // Deserialize packet and call member recv function with appropriate structure
    switch (rrcDlDcchMessage.GetMessageType())
    {
    case 4:
        params.pdcpSdu->RemoveHeader(rrcReconfigurationHeader);
        rrcReconfigurationMsg = rrcReconfigurationHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcReconfiguration(rrcReconfigurationMsg);
        break;
    case 5:
        params.pdcpSdu->RemoveHeader(rrcReleaseHeader);
        rrcReleaseMsg = rrcReleaseHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcRelease (rrcReleaseMsg);
        break;

    case 6:
        //Rrc Resume
        params.pdcpSdu->RemoveHeader(rrcResumeHeader);
        rrcResumeMsg = rrcResumeHeader.GetMessage();
        m_ueRrcSapProvider->RecvRrcResume(rrcResumeMsg);
        break;
    }

}

//------------------------------------------------------------------------------------------------------------------------------------------
NS_OBJECT_ENSURE_REGISTERED(NrGnbRrcProtocolReal);

NrGnbRrcProtocolReal::NrGnbRrcProtocolReal()
    : m_enbRrcSapProvider(nullptr)
{
    NS_LOG_FUNCTION(this);
    m_enbRrcSapUser = new MemberNrGnbRrcSapUser<NrGnbRrcProtocolReal>(this);
}

NrGnbRrcProtocolReal::~NrGnbRrcProtocolReal()
{
    NS_LOG_FUNCTION(this);
}

void
NrGnbRrcProtocolReal::DoDispose()
{
    NS_LOG_FUNCTION(this);
    delete m_enbRrcSapUser;
    for (std::map<uint16_t, NrGnbRrcSapProvider::CompleteSetupUeParameters>::iterator it =
             m_completeSetupUeParametersMap.begin();
         it != m_completeSetupUeParametersMap.end();
         ++it)
    {
        delete it->second.srb0SapUser;
        delete it->second.srb1SapUser;
    }
    m_completeSetupUeParametersMap.clear();
}

TypeId
NrGnbRrcProtocolReal::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrGnbRrcProtocolReal")
                            .SetParent<Object>()
                            .AddConstructor<NrGnbRrcProtocolReal>();
    return tid;
}

void
NrGnbRrcProtocolReal::SetNrGnbRrcSapProvider(NrGnbRrcSapProvider* p)
{
    m_enbRrcSapProvider = p;
}

NrGnbRrcSapUser*
NrGnbRrcProtocolReal::GetNrGnbRrcSapUser()
{
    return m_enbRrcSapUser;
}

NrUeRrcSapProvider*
NrGnbRrcProtocolReal::GetUeRrcSapProvider(uint16_t rnti)
{
    std::map<uint16_t, NrUeRrcSapProvider*>::const_iterator it;
    it = m_enbRrcSapProviderMap.find(rnti);
    NS_ASSERT_MSG(it != m_enbRrcSapProviderMap.end(), "could not find RNTI = " << rnti);
    return it->second;
}

void
NrGnbRrcProtocolReal::SetUeRrcSapProvider(uint16_t rnti, NrUeRrcSapProvider* p)
{
    std::map<uint16_t, NrUeRrcSapProvider*>::iterator it;
    it = m_enbRrcSapProviderMap.find(rnti);
    if(it != m_enbRrcSapProviderMap.end())
    {
        it->second = p;
    }
    //NS_ASSERT_MSG(it != m_enbRrcSapProviderMap.end(), "could not find RNTI = " << rnti);
   
}

void
NrGnbRrcProtocolReal::DoSetupUe(uint16_t rnti, NrGnbRrcSapUser::SetupUeParameters params)
{
    NS_LOG_FUNCTION(this << rnti);
    m_enbRrcSapProviderMap[rnti] = nullptr;
    // Store SetupUeParameters
    m_setupUeParametersMap[rnti] = params;

    NrGnbRrcSapProvider::CompleteSetupUeParameters completeSetupUeParameters;
    std::map<uint16_t, NrGnbRrcSapProvider::CompleteSetupUeParameters>::iterator csupIt =
        m_completeSetupUeParametersMap.find(rnti);
    if (csupIt == m_completeSetupUeParametersMap.end())
    {
        // Create LteRlcSapUser, LtePdcpSapUser
        LteRlcSapUser* srb0SapUser = new NrRealProtocolRlcSapUser(this, rnti);
        LtePdcpSapUser* srb1SapUser =
            new LtePdcpSpecificLtePdcpSapUser<NrGnbRrcProtocolReal>(this);
        completeSetupUeParameters.srb0SapUser = srb0SapUser;
        completeSetupUeParameters.srb1SapUser = srb1SapUser;
        // Store LteRlcSapUser, LtePdcpSapUser
        m_completeSetupUeParametersMap[rnti] = completeSetupUeParameters;
    }
    else
    {
        completeSetupUeParameters = csupIt->second;
    }
    m_enbRrcSapProvider->CompleteSetupUe(rnti, completeSetupUeParameters);
}

void
NrGnbRrcProtocolReal::DoRemoveUe(uint16_t rnti)
{
    NS_LOG_FUNCTION(this << rnti);
    std::map<uint16_t, NrGnbRrcSapProvider::CompleteSetupUeParameters>::iterator it =
        m_completeSetupUeParametersMap.find(rnti);
    NS_ASSERT(it != m_completeSetupUeParametersMap.end());
    delete it->second.srb0SapUser;
    delete it->second.srb1SapUser;
    m_completeSetupUeParametersMap.erase(it);
    m_enbRrcSapProviderMap.erase(rnti);
    m_setupUeParametersMap.erase(rnti);
}

void
NrGnbRrcProtocolReal::DoSendSystemInformation(uint16_t cellId, NrRrcSap::SystemInformation msg)
{
    NS_LOG_FUNCTION(this << cellId);
    // walk list of all nodes to get UEs with this cellId
    Ptr<NrUeRrc> ueRrc;
    for (NodeList::Iterator i = NodeList::Begin(); i != NodeList::End(); ++i)
    {
        Ptr<Node> node = *i;
        int nDevs = node->GetNDevices();
        for (int j = 0; j < nDevs; ++j)
        {
            Ptr<NrUeNetDevice> nrUeDev = node->GetDevice(j)->GetObject<NrUeNetDevice>();
            if (nrUeDev)
            {
                Ptr<NrUeRrc> ueRrc = nrUeDev->GetRrc();
                NS_LOG_LOGIC("considering UE IMSI " << nrUeDev->GetImsi() << " that has cellId "
                                                    << ueRrc->GetCellId());
                if (ueRrc->GetCellId() == cellId)
                {
                    NS_LOG_LOGIC("sending SI to IMSI " << nrUeDev->GetImsi());
                    ueRrc->GetNrUeRrcSapProvider()->RecvSystemInformation(msg);
                    Simulator::Schedule(RRC_REAL_MSG_DELAY,
                                        &NrUeRrcSapProvider::RecvSystemInformation,
                                        ueRrc->GetNrUeRrcSapProvider(),
                                        msg);
                }
            }
        }
    }
}

void
NrGnbRrcProtocolReal::DoSendRrcSetup(uint16_t rnti, NrRrcSap::RrcSetup msg)
{
    
    Ptr<Packet> packet = Create<Packet>();

    RrcSetupHeader RrcSetupHeader;
    RrcSetupHeader.SetMessage(msg);

    packet->AddHeader(RrcSetupHeader);

    LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    transmitPdcpPduParameters.pdcpPdu = packet;
    transmitPdcpPduParameters.rnti = rnti;

   
    transmitPdcpPduParameters.lcid = 1;
       

    m_setupUeParametersMap.at(rnti).srb0SapProvider->TransmitPdcpPdu(transmitPdcpPduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcSetup,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcResume(uint16_t rnti, NrRrcSap::RrcResume msg)
{
    Ptr<Packet> packet = Create<Packet>();

    RrcResumeHeader rrcResumeHeader;
    rrcResumeHeader.SetMessage(msg);

    packet->AddHeader(rrcResumeHeader);


    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = rnti;
    transmitPdcpSduParameters.lcid = 1;
   

    m_setupUeParametersMap.at(rnti).srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcSetup,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcReconfiguration(
    uint16_t rnti,
    NrRrcSap::RrcReconfiguration msg)
{

    Ptr<Packet> packet = Create<Packet>();

    RrcReconfigurationHeader RrcReconfigurationHeader;
    RrcReconfigurationHeader.SetMessage(msg);

    packet->AddHeader(RrcReconfigurationHeader);

    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = rnti;
    transmitPdcpSduParameters.lcid = 1;
    
    m_setupUeParametersMap[rnti].srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcConnectionReconfiguration,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcConnectionReestablishment(
    uint16_t rnti,
    NrRrcSap::RrcConnectionReestablishment msg)
{

    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionReestablishmentHeader rrcConnectionReestablishmentHeader;
    rrcConnectionReestablishmentHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionReestablishmentHeader);

    LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    transmitPdcpPduParameters.pdcpPdu = packet;
    transmitPdcpPduParameters.rnti = rnti;
    transmitPdcpPduParameters.lcid = 0;

    m_setupUeParametersMap[rnti].srb0SapProvider->TransmitPdcpPdu(transmitPdcpPduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcConnectionReestablishment,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcConnectionReestablishmentReject(
    uint16_t rnti,
    NrRrcSap::RrcConnectionReestablishmentReject msg)
{
    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionReestablishmentRejectHeader rrcConnectionReestablishmentRejectHeader;
    rrcConnectionReestablishmentRejectHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionReestablishmentRejectHeader);

    LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    transmitPdcpPduParameters.pdcpPdu = packet;
    transmitPdcpPduParameters.rnti = rnti;
    transmitPdcpPduParameters.lcid = 0;

    m_setupUeParametersMap[rnti].srb0SapProvider->TransmitPdcpPdu(transmitPdcpPduParameters);
    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcConnectionReestablishmentReject,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcRelease(uint16_t rnti,
                                                  NrRrcSap::RrcRelease msg)
{
    Ptr<Packet> packet = Create<Packet>();

    RrcReleaseHeader rrcReleaseHeader;
    rrcReleaseHeader.SetMessage(msg);

    packet->AddHeader(rrcReleaseHeader);


    LtePdcpSapProvider::TransmitPdcpSduParameters transmitPdcpSduParameters;
    transmitPdcpSduParameters.pdcpSdu = packet;
    transmitPdcpSduParameters.rnti = rnti;
    transmitPdcpSduParameters.lcid = 1;

    m_setupUeParametersMap[rnti].srb1SapProvider->TransmitPdcpSdu(transmitPdcpSduParameters);

    // Simulator::Schedule(RRC_REAL_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcRelease,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}

void
NrGnbRrcProtocolReal::DoSendRrcConnectionReject(uint16_t rnti, NrRrcSap::RrcConnectionReject msg)
{
    Ptr<Packet> packet = Create<Packet>();

    RrcConnectionRejectHeader rrcConnectionRejectHeader;
    rrcConnectionRejectHeader.SetMessage(msg);

    packet->AddHeader(rrcConnectionRejectHeader);

    LteRlcSapProvider::TransmitPdcpPduParameters transmitPdcpPduParameters;
    transmitPdcpPduParameters.pdcpPdu = packet;
    transmitPdcpPduParameters.rnti = rnti;
    transmitPdcpPduParameters.lcid = 0;

    m_setupUeParametersMap[rnti].srb0SapProvider->TransmitPdcpPdu(transmitPdcpPduParameters);

    // Simulator::Schedule(RRC_Real_MSG_DELAY,
    //                     &NrUeRrcSapProvider::RecvRrcConnectionReject,
    //                     GetUeRrcSapProvider(rnti),
    //                     msg);
}


void
NrGnbRrcProtocolReal::DoReceivePdcpPdu(uint16_t rnti, Ptr<Packet> p)
{
    // Get type of message received
    RrcUlCcchMessage rrcUlCcchMessage;
    p->PeekHeader(rrcUlCcchMessage);

    // Declare possible headers to receive
    RrcConnectionReestablishmentRequestHeader rrcConnectionReestablishmentRequestHeader;
    RrcConnectionRequestHeader rrcConnectionRequestHeader;
    RrcResumeRequestHeader rrcResumeRequestHeader;

    // Declare possible messages to receive
    NrRrcSap::RrcConnectionReestablishmentRequest rrcConnectionReestablishmentRequestMsg;
    NrRrcSap::NrRrcConnectionRequest rrcConnectionRequestMsg;
    NrRrcSap::RrcResumeRequest rrcResumeRequestMsg;

    // Deserialize packet and call member recv function with appropriate structure
    switch (rrcUlCcchMessage.GetMessageType())
    {
    case 0:
        p->RemoveHeader(rrcConnectionReestablishmentRequestHeader);
        rrcConnectionReestablishmentRequestMsg =
            rrcConnectionReestablishmentRequestHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcConnectionReestablishmentRequest(
            rnti,
            rrcConnectionReestablishmentRequestMsg);
        break;
    case 1:
        p->RemoveHeader(rrcConnectionRequestHeader);
        rrcConnectionRequestMsg = rrcConnectionRequestHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcConnectionRequest(rnti, rrcConnectionRequestMsg);
        break;
    
    case 2:
        p->RemoveHeader(rrcResumeRequestHeader);
        rrcResumeRequestMsg = rrcResumeRequestHeader.GetMessage();
        rrcResumeRequestMsg.sdtData.emplace_back(p); //all data is inserted as one package even when multiple smaller packages are transmitted.
        m_enbRrcSapProvider->RecvRrcResumeRequest(rnti, rrcResumeRequestMsg);
        break;
    }


}

void
NrGnbRrcProtocolReal::DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params)
{
    // Get type of message received
    RrcUlDcchMessage rrcUlDcchMessage;
    params.pdcpSdu->PeekHeader(rrcUlDcchMessage);

    // Declare possible headers to receive
    MeasurementReportHeader measurementReportHeader;
    RrcConnectionReconfigurationCompleteHeader rrcConnectionReconfigurationCompleteHeader;
    RrcConnectionReestablishmentCompleteHeader rrcConnectionReestablishmentCompleteHeader;
    RrcConnectionSetupCompleteHeader rrcConnectionSetupCompleteHeader;
    RrcResumeCompleteHeader rrcResumeCompleteHeader;

    // Declare possible messages to receive
    NrRrcSap::MeasurementReport measurementReportMsg;
    NrRrcSap::RrcConnectionReconfigurationCompleted rrcConnectionReconfigurationCompleteMsg;
    NrRrcSap::RrcConnectionReestablishmentComplete rrcConnectionReestablishmentCompleteMsg;
    NrRrcSap::RrcConnectionSetupCompleted rrcConnectionSetupCompletedMsg;
    NrRrcSap::RrcResumeComplete rrcResumeCompleteMsg;

    // Deserialize packet and call member recv function with appropriate structure
    switch (rrcUlDcchMessage.GetMessageType())
    {
    case 1:
        params.pdcpSdu->RemoveHeader(measurementReportHeader);
        measurementReportMsg = measurementReportHeader.GetMessage();
        m_enbRrcSapProvider->RecvMeasurementReport(params.rnti, measurementReportMsg);
        break;
    case 2:
        params.pdcpSdu->RemoveHeader(rrcConnectionReconfigurationCompleteHeader);
        rrcConnectionReconfigurationCompleteMsg =
            rrcConnectionReconfigurationCompleteHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcConnectionReconfigurationCompleted(
            params.rnti,
            rrcConnectionReconfigurationCompleteMsg);
        break;
    case 3:
        params.pdcpSdu->RemoveHeader(rrcConnectionReestablishmentCompleteHeader);
        rrcConnectionReestablishmentCompleteMsg =
            rrcConnectionReestablishmentCompleteHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcConnectionReestablishmentComplete(
            params.rnti,
            rrcConnectionReestablishmentCompleteMsg);
        break;
    case 4:
        params.pdcpSdu->RemoveHeader(rrcConnectionSetupCompleteHeader);
        rrcConnectionSetupCompletedMsg = rrcConnectionSetupCompleteHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcConnectionSetupCompleted(params.rnti,
                                                             rrcConnectionSetupCompletedMsg);
        break;

    case 5:
        params.pdcpSdu->RemoveHeader(rrcResumeCompleteHeader);
        rrcResumeCompleteMsg = rrcResumeCompleteHeader.GetMessage();
        m_enbRrcSapProvider->RecvRrcResumeComplete(params.rnti,
                                                             rrcResumeCompleteMsg);

    }
}

/*
 * The purpose of NrGnbRrcProtocolIdeal is to avoid encoding
 * messages. In order to do so, we need to have some form of encoding for
 * inter-node RRC messages like HandoverPreparationInfo and HandoverCommand. Doing so
 * directly is not practical (these messages includes a lot of
 * information elements, so encoding all of them would defeat the
 * purpose of NrGnbRrcProtocolIdeal. The workaround is to store the
 * actual message in a global map, so that then we can just encode the
 * key in a header and send that between gNBs over X2.
 *
 */

static std::map<uint32_t, NrRrcSap::HandoverPreparationInfo> g_handoverPreparationInfoMsgMap;
static uint32_t g_handoverPreparationInfoMsgIdCounter = 0;

/*
 * This header encodes the map key discussed above. We keep this
 * private since it should not be used outside this file.
 *
 */
class NrRealHandoverPreparationInfoHeader : public Header
{
  public:
    uint32_t GetMsgId();
    void SetMsgId(uint32_t id);
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint32_t m_msgId;
};

uint32_t
NrRealHandoverPreparationInfoHeader::GetMsgId()
{
    return m_msgId;
}

void
NrRealHandoverPreparationInfoHeader::SetMsgId(uint32_t id)
{
    m_msgId = id;
}

TypeId
NrRealHandoverPreparationInfoHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrRealHandoverPreparationInfoHeader")
                            .SetParent<Header>()
                            .AddConstructor<NrRealHandoverPreparationInfoHeader>();
    return tid;
}

TypeId
NrRealHandoverPreparationInfoHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
NrRealHandoverPreparationInfoHeader::Print(std::ostream& os) const
{
    os << " msgId=" << m_msgId;
}

uint32_t
NrRealHandoverPreparationInfoHeader::GetSerializedSize() const
{
    return 4;
}

void
NrRealHandoverPreparationInfoHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU32(m_msgId);
}

uint32_t
NrRealHandoverPreparationInfoHeader::Deserialize(Buffer::Iterator start)
{
    m_msgId = start.ReadU32();
    return GetSerializedSize();
}

Ptr<Packet>
NrGnbRrcProtocolReal::DoEncodeHandoverPreparationInformation(
    NrRrcSap::HandoverPreparationInfo msg)
{
    uint32_t msgId = ++g_handoverPreparationInfoMsgIdCounter;
    NS_ASSERT_MSG(g_handoverPreparationInfoMsgMap.find(msgId) ==
                      g_handoverPreparationInfoMsgMap.end(),
                  "msgId " << msgId << " already in use");
    NS_LOG_INFO(" encoding msgId = " << msgId);
    g_handoverPreparationInfoMsgMap.insert(
        std::pair<uint32_t, NrRrcSap::HandoverPreparationInfo>(msgId, msg));
    NrRealHandoverPreparationInfoHeader h;
    h.SetMsgId(msgId);
    Ptr<Packet> p = Create<Packet>();
    p->AddHeader(h);
    return p;
}

NrRrcSap::HandoverPreparationInfo
NrGnbRrcProtocolReal::DoDecodeHandoverPreparationInformation(Ptr<Packet> p)
{
    NrRealHandoverPreparationInfoHeader h;
    p->RemoveHeader(h);
    uint32_t msgId = h.GetMsgId();
    NS_LOG_INFO(" decoding msgId = " << msgId);
    std::map<uint32_t, NrRrcSap::HandoverPreparationInfo>::iterator it =
        g_handoverPreparationInfoMsgMap.find(msgId);
    NS_ASSERT_MSG(it != g_handoverPreparationInfoMsgMap.end(), "msgId " << msgId << " not found");
    NrRrcSap::HandoverPreparationInfo msg = it->second;
    g_handoverPreparationInfoMsgMap.erase(it);
    return msg;
}

static std::map<uint32_t, NrRrcSap::RrcConnectionReconfiguration> g_handoverCommandMsgMap;
static uint32_t g_handoverCommandMsgIdCounter = 0;

/*
 * This header encodes the map key discussed above. We keep this
 * private since it should not be used outside this file.
 *
 */
class NrRealHandoverCommandHeader : public Header
{
  public:
    uint32_t GetMsgId();
    void SetMsgId(uint32_t id);
    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;
    void Print(std::ostream& os) const override;
    uint32_t GetSerializedSize() const override;
    void Serialize(Buffer::Iterator start) const override;
    uint32_t Deserialize(Buffer::Iterator start) override;

  private:
    uint32_t m_msgId;
};

uint32_t
NrRealHandoverCommandHeader::GetMsgId()
{
    return m_msgId;
}

void
NrRealHandoverCommandHeader::SetMsgId(uint32_t id)
{
    m_msgId = id;
}

TypeId
NrRealHandoverCommandHeader::GetTypeId()
{
    static TypeId tid = TypeId("ns3::NrRealHandoverCommandHeader")
                            .SetParent<Header>()
                            .AddConstructor<NrRealHandoverCommandHeader>();
    return tid;
}

TypeId
NrRealHandoverCommandHeader::GetInstanceTypeId() const
{
    return GetTypeId();
}

void
NrRealHandoverCommandHeader::Print(std::ostream& os) const
{
    os << " msgId=" << m_msgId;
}

uint32_t
NrRealHandoverCommandHeader::GetSerializedSize() const
{
    return 4;
}

void
NrRealHandoverCommandHeader::Serialize(Buffer::Iterator start) const
{
    start.WriteU32(m_msgId);
}

uint32_t
NrRealHandoverCommandHeader::Deserialize(Buffer::Iterator start)
{
    m_msgId = start.ReadU32();
    return GetSerializedSize();
}

Ptr<Packet>
NrGnbRrcProtocolReal::DoEncodeHandoverCommand(NrRrcSap::RrcConnectionReconfiguration msg)
{
    uint32_t msgId = ++g_handoverCommandMsgIdCounter;
    NS_ASSERT_MSG(g_handoverCommandMsgMap.find(msgId) == g_handoverCommandMsgMap.end(),
                  "msgId " << msgId << " already in use");
    NS_LOG_INFO(" encoding msgId = " << msgId);
    g_handoverCommandMsgMap.insert(
        std::pair<uint32_t, NrRrcSap::RrcConnectionReconfiguration>(msgId, msg));
    NrRealHandoverCommandHeader h;
    h.SetMsgId(msgId);
    Ptr<Packet> p = Create<Packet>();
    p->AddHeader(h);
    return p;
}

NrRrcSap::RrcConnectionReconfiguration
NrGnbRrcProtocolReal::DoDecodeHandoverCommand(Ptr<Packet> p)
{
    NrRealHandoverCommandHeader h;
    p->RemoveHeader(h);
    uint32_t msgId = h.GetMsgId();
    NS_LOG_INFO(" decoding msgId = " << msgId);
    std::map<uint32_t, NrRrcSap::RrcConnectionReconfiguration>::iterator it =
        g_handoverCommandMsgMap.find(msgId);
    NS_ASSERT_MSG(it != g_handoverCommandMsgMap.end(), "msgId " << msgId << " not found");
    NrRrcSap::RrcConnectionReconfiguration msg = it->second;
    g_handoverCommandMsgMap.erase(it);
    return msg;
}



//////////////////////////////////////////////////////

NrRealProtocolRlcSapUser::NrRealProtocolRlcSapUser(NrGnbRrcProtocolReal* pdcp, uint16_t rnti)
    : m_pdcp(pdcp),
      m_rnti(rnti)
{
}

NrRealProtocolRlcSapUser::NrRealProtocolRlcSapUser()
{
}

void
NrRealProtocolRlcSapUser::ReceivePdcpPdu(Ptr<Packet> p)
{
    m_pdcp->DoReceivePdcpPdu(m_rnti, p);
}


} // namespace ns3
