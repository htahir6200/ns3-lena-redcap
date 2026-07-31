/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#include "bwp-manager-ue.h"

#include "bwp-manager-algorithm.h"
#include "nr-control-messages.h"

#include <ns3/log.h>
#include <ns3/pointer.h>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BwpManagerUe");
NS_OBJECT_ENSURE_REGISTERED(BwpManagerUe);

BwpManagerUe::BwpManagerUe()
    : SimpleUeComponentCarrierManager()
{
    NS_LOG_FUNCTION(this);
}

BwpManagerUe::~BwpManagerUe()
{
    NS_LOG_FUNCTION(this);
}

void
BwpManagerUe::SetBwpManagerAlgorithm(const Ptr<BwpManagerAlgorithm>& algorithm)
{
    NS_LOG_FUNCTION(this);
    m_algorithm = algorithm;
}

TypeId
BwpManagerUe::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BwpManagerUe")
                            .SetParent<SimpleUeComponentCarrierManager>()
                            .SetGroupName("nr")
                            .AddConstructor<BwpManagerUe>()
                            .AddAttribute("BwpManagerAlgorithm",
                                          "The algorithm pointer",
                                          PointerValue(),
                                          MakePointerAccessor(&BwpManagerUe::m_algorithm),
                                          MakePointerChecker<BwpManagerAlgorithm>());
    return tid;
}

void
BwpManagerUe::DoReportBufferStatus(LteMacSapProvider::ReportBufferStatusParameters params)
{
    NS_LOG_FUNCTION(this);
    NS_ASSERT(m_algorithm != nullptr);
    
    uint8_t bwpIndex = m_activeBwp;
    m_componentCarrierLcMap.at(bwpIndex).at(params.lcid)->ReportBufferStatus(params);
}

void
BwpManagerUe::DoStartBwpSwitching(uint8_t bwpID)
{
    NS_LOG_FUNCTION(this);
    m_bwpInactivityTimout.Cancel();
    if(bwpID != m_activeBwp)
    {
        //ETSI 38.133 8.6.3 
        //Switching is currently only rrc based and therefore rrc switching delay has to be included

        //TODO used values deviate from the tables in ETSI due to incompatibility
        Time BwpSwitchingDelayRrc = MilliSeconds(3); //ETSI 38.133 8.6.3 
        // Procedure Delay for Rrc Setup/Reconfiguration
        Time rrcProcedureDelay = MilliSeconds(0); //ETSI 38.331 Clause 12
        Simulator::Schedule(BwpSwitchingDelayRrc+rrcProcedureDelay,&BwpManagerUe::SwitchBwp,this,bwpID);
    }
}

void 
BwpManagerUe::SwitchBwp(const uint8_t bwpID)
{
    //Get Rlc Data from the old BWP
    std::unordered_map<uint8_t, LteMacSapProvider::ReportBufferStatusParameters> bufferStatus;
    //only getting buffer values for Lcid 1: get all data?
    bufferStatus = m_componentCarrierLcMap.at(m_activeBwp).at(1)->GetBufferStatus();
    m_activeBwp = bwpID;

    for (auto it = bufferStatus.cbegin(); it != bufferStatus.cend(); ++it)
    {
        if(((*it).second.txQueueSize + (*it).second.retxQueueSize + (*it).second.statusPduSize) >0 )
        {
            //For now dont transfer bufferstatus to the new bwp
            //m_componentCarrierLcMap.at(m_activeBwp).at((*it).second.lcid)->ReportBufferStatus((*it).second);
        }
    }
    m_ccmRrcSapUser->NotifySwitchedBwp(bwpID);

    if(m_activeBwp !=0)
    {
        
        m_bwpInactivityTimout = Simulator::Schedule(m_bwpInactivityTimer,
                                        &BwpManagerUe::BwpInactivityTimeout,this);

    }
    
}

void 
BwpManagerUe::SetBwpInactivityTimer(Time inactTimer)
{
   NS_LOG_FUNCTION(this);
   //m_bwpInactivityTimer =inactTimer;
   //Hardcoded for now
   m_bwpInactivityTimer = MilliSeconds(6000000);
}

void 
BwpManagerUe::DoRefreshBwpInactivityTimer()
{
   NS_LOG_FUNCTION(this);
   m_bwpInactivityTimout.Cancel();
   if(m_activeBwp !=0)
   {
     m_bwpInactivityTimout = Simulator::Schedule(m_bwpInactivityTimer,
                                        &BwpManagerUe::BwpInactivityTimeout,this);
   }
}

void
BwpManagerUe::BwpInactivityTimeout()
{
    //38.133 Table 8.6.2-1 Type 1 
    //Delay for DCI-based switching
    //maybe switch to Type 2 delay
    Time switchingDelay = MilliSeconds(1); // Only applicable for numerology 0 and 1. Has to change for higher ones
    Simulator::Schedule(switchingDelay,&BwpManagerUe::SwitchBwp,this,0);

}

std::vector<LteUeCcmRrcSapProvider::LcsConfig>
BwpManagerUe::DoAddLc(uint8_t lcId,
                      LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
                      LteMacSapUser* msu)
{
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("For LC ID " << static_cast<uint32_t>(lcId) << " bearer qci "
                             << static_cast<uint32_t>(lcConfig.priority) << " from priority "
                             << static_cast<uint32_t>(lcConfig.priority));

    // see lte-enb-rrc.cc:453
    m_lcToBearerMap.insert(std::make_pair(lcId, static_cast<EpsBearer::Qci>(lcConfig.priority)));

    return SimpleUeComponentCarrierManager::DoAddLc(lcId, lcConfig, msu);
}

LteMacSapUser*
BwpManagerUe::DoConfigureSignalBearer(uint8_t lcId,
                                      LteUeCmacSapProvider::LogicalChannelConfig lcConfig,
                                      LteMacSapUser* msu)
{
    NS_LOG_FUNCTION(this);

    // Ignore signaling bearers for the moment. These are for an advanced use.
    // m_lcToBearerMap.insert (std::make_pair (lcId, EpsBearer::FromPriority
    // (lcConfig.priority).qci));

    return SimpleUeComponentCarrierManager::DoConfigureSignalBearer(lcId, lcConfig, msu);
}

uint8_t
BwpManagerUe::RouteDlHarqFeedback(const DlHarqInfo& m) const
{
    NS_LOG_FUNCTION(this);

    return m.m_bwpIndex;
}

void
BwpManagerUe::SetOutputLink(uint32_t sourceBwp, uint32_t outputBwp)
{
    NS_LOG_FUNCTION(this);
    m_outputLinks.insert(std::make_pair(sourceBwp, outputBwp));
}

uint8_t
BwpManagerUe::RouteOutgoingCtrlMsg(const Ptr<NrControlMessage>& msg, uint8_t sourceBwpId) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Msg type " << msg->GetMessageType() << " that wants to go out from UE");

    if (m_outputLinks.empty())
    {
        NS_LOG_INFO("No linked BWP, routing outgoing msg to the source: " << +sourceBwpId);
        return sourceBwpId;
    }

    auto it = m_outputLinks.find(sourceBwpId);
    if (it == m_outputLinks.end())
    {
        NS_LOG_INFO("Source BWP not in the map, routing outgoing msg to itself: " << +sourceBwpId);
        return sourceBwpId;
    }

    NS_LOG_INFO("routing outgoing msg to bwp: " << +it->second);
    return it->second;
}

uint8_t
BwpManagerUe::RouteIngoingCtrlMsg(const Ptr<NrControlMessage>& msg, uint8_t sourceBwpId) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_INFO("Msg type " << msg->GetMessageType() << " comes from BWP " << +sourceBwpId
                            << " that wants to go in the UE, goes in BWP " << msg->GetSourceBwp());
    return msg->GetSourceBwp();
}

} // namespace ns3
