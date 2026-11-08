/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#define NS_LOG_APPEND_CONTEXT                                                                      \
    do                                                                                             \
    {                                                                                              \
        std::clog << " [ CellId " << GetCellId() << ", bwpId " << GetBwpId() << "] ";              \
    } while (false);

#include "nr-mac-scheduler-ofdma.h"
#include "nr-campaign-stats.h"

#include <ns3/log.h>

#include <algorithm>

namespace ns3
{
NS_LOG_COMPONENT_DEFINE("NrMacSchedulerOfdma");
NS_OBJECT_ENSURE_REGISTERED(NrMacSchedulerOfdma);

TypeId
NrMacSchedulerOfdma::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NrMacSchedulerOfdma")
            .SetParent<NrMacSchedulerTdma>()
            .AddTraceSource(
                "SymPerBeam",
                "Number of assigned symbol per beam. Gets called every time an assignment is made",
                MakeTraceSourceAccessor(&NrMacSchedulerOfdma::m_tracedValueSymPerBeam),
                "ns3::TracedValueCallback::Uint32");
    return tid;
}

NrMacSchedulerOfdma::NrMacSchedulerOfdma()
    : NrMacSchedulerTdma()
{
  lastProcessedSlot = SfnSf(0,0,0,1);
}

/**
 *
 * \brief Calculate the number of symbols to assign to each beam
 * \param symAvail Number of available symbols
 * \param activeDl Map of active DL UE and their beam
 *
 * Each beam has a different requirement in terms of byte that should be
 * transmitted with that beam. That requirement depends on the number of UE
 * that are inside such beam, and how many bytes they have to transmit.
 *
 * For the beam \f$ b \f$, the number of assigned symbols is the following:
 *
 * \f$ sym_{b} = BufSize(b) * \frac{symAvail}{BufSizeTotal} \f$
 */
NrMacSchedulerOfdma::BeamSymbolMap
NrMacSchedulerOfdma::GetSymPerBeam(uint32_t symAvail,
                                   const NrMacSchedulerNs3::ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    GetSecond GetUeVector;
    GetSecond GetUeBufSize;
    GetFirst GetBeamId;
    double bufTotal = 0.0;
    uint8_t symUsed = 0;
    BeamSymbolMap ret;

    // Compute buf total
    for (const auto& el : activeDl)
    {
        for (const auto& ue : GetUeVector(el))
        {
            bufTotal += GetUeBufSize(ue);
        }
    }

    for (const auto& el : activeDl)
    {
        uint32_t bufSizeBeam = 0;
        for (const auto& ue : GetUeVector(el))
        {
            bufSizeBeam += GetUeBufSize(ue);
        }

        double tmp = symAvail / bufTotal;
        uint32_t symForBeam = static_cast<uint32_t>(bufSizeBeam * tmp);
        symUsed += symForBeam;
        ret.emplace(std::make_pair(GetBeamId(el), symForBeam));
        NS_LOG_DEBUG("Assigned to beam " << GetBeamId(el) << " symbols " << symForBeam);
    }

    NS_ASSERT(symAvail >= symUsed);
    if (symAvail - symUsed > 0)
    {
        uint8_t symToRedistribute = symAvail - symUsed;
        while (symToRedistribute > 0)
        {
            BeamSymbolMap::iterator min = ret.end();
            for (auto it = ret.begin(); it != ret.end(); ++it)
            {
                if (min == ret.end() || it->second < min->second)
                {
                    min = it;
                }
            }
            min->second += 1;
            symToRedistribute--;
            NS_LOG_DEBUG("Assigned to beam "
                         << min->first << " an additional symbol, for a total of " << min->second);
        }
    }

    // Trigger the trace source firing, using const_cast as we don't change
    // the internal state of the class
    for (const auto& v : ret)
    {
        const_cast<NrMacSchedulerOfdma*>(this)->m_tracedValueSymPerBeam = v.second;
    }

    return ret;
}

/**
 * \brief Assign the available DL RBG to the UEs
 * \param symAvail Available symbols
 * \param activeDl Map of active UE and their beams
 * \return a map between beams and the symbol they need
 *
 * The algorithm redistributes the frequencies to all the UEs inside a beam.
 * The pre-requisite is to calculate the symbols for each beam, done with
 * the function GetSymPerBeam().
 * The pseudocode is the following (please note that sym_of_beam is a value
 * returned by the GetSymPerBeam() function):
 * <pre>
 * while frequencies > 0:
 *    sort (ueVector);
 *    ueVector.first().m_dlRBG += 1 * sym_of_beam;
 *    frequencies--;
 *    UpdateUeDlMetric (ueVector.first());
 * </pre>
 *
 * To sort the UEs, the method uses the function returned by GetUeCompareDlFn().
 * Two fairness helper are hard-coded in the method: the first one is avoid
 * to assign resources to UEs that already have their buffer requirement covered,
 * and the other one is avoid to assign symbols when all the UEs have their
 * requirements covered.
 */
NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignDLRBG(uint32_t symAvail, const ActiveUeMap& activeDl) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    // Iterate through the different beams
    for (const auto& el : activeDl)
    {
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        uint32_t rbgAssignable = 1 * beamSym;
        std::vector<UePtrAndBufferReq> ueVector;
        FTResources assigned(0, 0);
        const std::vector<uint8_t> dlNotchedRBGsMask = GetDlNotchedRbgMask();
        uint32_t resources = dlNotchedRBGsMask.size() > 0
                                 ? std::count(dlNotchedRBGsMask.begin(), dlNotchedRBGsMask.end(), 1)
                                 : GetBandwidthInRbg();
        NS_ASSERT(resources > 0);

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
        }

        for (auto& ue : ueVector)
        {
            BeforeDlSched(ue, FTResources(rbgAssignable * beamSym, beamSym));
        }

        while (resources > 0)
        {
            GetFirst GetUe;
            std::sort(ueVector.begin(), ueVector.end(), GetUeCompareDlFn());
            auto schedInfoIt = ueVector.begin();

            // Ensure fairness: pass over UEs which already has enough resources to transmit
            while (schedInfoIt != ueVector.end())
            {
                uint32_t bufQueueSize = schedInfoIt->second;

                // if there are two streams we add the TbSizes of the two
                // streams to satisfy the bufQueueSize
                uint32_t tbSize = 0;
                for (const auto& it : GetUe(*schedInfoIt)->m_dlTbSize)
                {
                    tbSize += it;
                }

                if (tbSize >= std::max(bufQueueSize, 10U))
                {
                    if (GetUe(*schedInfoIt)->m_dlTbSize.size() > 1)
                    {
                        // This "if" is purely for MIMO. In MIMO, for example, if the
                        // first TB size is big enough to empty the buffer then we
                        // should not allocate anything to the second stream. In this
                        // case, if we allocate bytes to the second stream, the UE
                        // would expect the TB but the gNB would not be able to transmit
                        // it. This would break HARQ TX state machine at UE PHY.

                        uint8_t streamCounter = 0;
                        uint32_t copyBufQueueSize = bufQueueSize;
                        auto dlTbSizeIt = GetUe(*schedInfoIt)->m_dlTbSize.begin();
                        while (dlTbSizeIt != GetUe(*schedInfoIt)->m_dlTbSize.end())
                        {
                            if (copyBufQueueSize != 0)
                            {
                                NS_LOG_DEBUG("Stream " << +streamCounter << " with TB size "
                                                       << *dlTbSizeIt << " needed to TX MIMO TB");
                                if (*dlTbSizeIt >= copyBufQueueSize)
                                {
                                    copyBufQueueSize = 0;
                                }
                                else
                                {
                                    copyBufQueueSize = copyBufQueueSize - *dlTbSizeIt;
                                }
                                streamCounter++;
                                dlTbSizeIt++;
                            }
                            else
                            {
                                // if we are here, that means previously iterated
                                // streams were enough to empty the buffer. We do
                                // not need this stream. Make its TB size zero.
                                NS_LOG_DEBUG("Stream " << +streamCounter << " with TB size "
                                                       << *dlTbSizeIt
                                                       << " not needed to TX MIMO TB");
                                *dlTbSizeIt = 0;
                                streamCounter++;
                                dlTbSizeIt++;
                            }
                        }
                    }
                    schedInfoIt++;
                }
                else
                {
                    break;
                }
            }

            // In the case that all the UE already have their requirements fullfilled,
            // then stop the beam processing and pass to the next
            if (schedInfoIt == ueVector.end())
            {
                break;
            }

            // Assign 1 RBG for each available symbols for the beam,
            // and then update the count of available resources
            GetUe(*schedInfoIt)->m_dlRBG += rbgAssignable;
            assigned.m_rbg += rbgAssignable;

            GetUe(*schedInfoIt)->m_dlSym = beamSym;
            assigned.m_sym = beamSym;

            resources -= 1; // Resources are RBG, so they do not consider the beamSym

            // Update metrics
            NS_LOG_DEBUG("Assigned " << rbgAssignable << " DL RBG, spanned over " << beamSym
                                     << " SYM, to UE " << GetUe(*schedInfoIt)->m_rnti);
            // Following call to AssignedDlResources would update the
            // TB size in the NrMacSchedulerUeInfo of this particular UE
            // according the Rank Indicator reported by it. Only one call
            // to this method is enough even if the UE reported rank indicator 2,
            // since the number of RBG assigned to both the streams are the same.
            AssignedDlResources(*schedInfoIt, FTResources(rbgAssignable, beamSym), assigned);

            // Update metrics for the unsuccessfull UEs (who did not get any resource in this
            // iteration)
            for (auto& ue : ueVector)
            {
                if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                {
                    NotAssignedDlResources(ue, FTResources(rbgAssignable, beamSym), assigned);
                }
            }
        }
    }


    return symPerBeam;
}

NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignDLRBG_manager(uint32_t symAvail, const ActiveUeMap& activeDl, const SfnSf& dlSfnSf) 
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeDl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeDl);

    // Iterate through the different beams
    for (const auto& el : activeDl)
    {
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
       
        std::vector<UePtrAndBufferReq> ueVector;

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
        }
        GetFirst GetUe;
        
        //sort ueVector for fair scheduling
        std::sort(ueVector.begin(),ueVector.end(), [](UePtrAndBufferReq a,UePtrAndBufferReq b){return a.first->m_rnti < b.first->m_rnti;} ); 
        //find starting ue
        uint32_t rotateIndex = 0;
        if(ueVector.size() >1)
        {
          auto sortingIt = ueVector.begin();
          uint16_t schedPriority = 0xFFFF;
          while(sortingIt != ueVector.end())
          {
            if(GetUe(*sortingIt)->m_rnti-schedStartUe >=0)
            {
                if(GetUe(*sortingIt)->m_rnti-schedStartUe < schedPriority)
                {
                  schedPriority = GetUe(*sortingIt)->m_rnti-schedStartUe;
                  rotateIndex = std::distance(ueVector.begin(),sortingIt);
                }
            }
            ++sortingIt;
          }

        }

        if(lastProcessedSlot.GetFrame() !=  dlSfnSf.GetFrame() || lastProcessedSlot.GetSubframe() != dlSfnSf.GetSubframe() ||lastProcessedSlot.GetSlot() != dlSfnSf.GetSlot())
        {
          //scheduling of this slot is done. reset stats
          m_manager->resetFreeRessources(dlSfnSf);
          lastProcessedSlot = dlSfnSf;
        }

        // rotate for round robin scheduling
        std::rotate(ueVector.begin(),ueVector.begin()+rotateIndex,ueVector.end());

       auto schedInfoIt = ueVector.begin();
        while (schedInfoIt != ueVector.end())
        {
          uint32_t bufQueueSize = schedInfoIt->second;
                // if there are two streams we add the TbSizes of the two
                // streams to satisfy the bufQueueSize
                uint32_t tbSize = 0;
                for (const auto& it : GetUe(*schedInfoIt)->m_dlTbSize)
                {
                    tbSize += it;
                }

                if (tbSize >= std::max(bufQueueSize, 10U))
                {
                    if (GetUe(*schedInfoIt)->m_dlTbSize.size() > 1)
                    {
                        // This "if" is purely for MIMO. In MIMO, for example, if the
                        // first TB size is big enough to empty the buffer then we
                        // should not allocate anything to the second stream. In this
                        // case, if we allocate bytes to the second stream, the UE
                        // would expect the TB but the gNB would not be able to transmit
                        // it. This would break HARQ TX state machine at UE PHY.

                        uint8_t streamCounter = 0;
                        uint32_t copyBufQueueSize = bufQueueSize;
                        auto dlTbSizeIt = GetUe(*schedInfoIt)->m_dlTbSize.begin();
                        while (dlTbSizeIt != GetUe(*schedInfoIt)->m_dlTbSize.end())
                        {
                            if (copyBufQueueSize != 0)
                            {
                                NS_LOG_DEBUG("Stream " << +streamCounter << " with TB size "
                                                       << *dlTbSizeIt << " needed to TX MIMO TB");
                                if (*dlTbSizeIt >= copyBufQueueSize)
                                {
                                    copyBufQueueSize = 0;
                                }
                                else
                                {
                                    copyBufQueueSize = copyBufQueueSize - *dlTbSizeIt;
                                }
                                streamCounter++;
                                dlTbSizeIt++;
                            }
                            else
                            {
                                // if we are here, that means previously iterated
                                // streams were enough to empty the buffer. We do
                                // not need this stream. Make its TB size zero.
                                NS_LOG_DEBUG("Stream " << +streamCounter << " with TB size "
                                                       << *dlTbSizeIt
                                                       << " not needed to TX MIMO TB");
                                *dlTbSizeIt = 0;
                                streamCounter++;
                                dlTbSizeIt++;
                            }
                        }
                    }
                    
                }
                else
                {
                  //TODO_test MIMO functionality? Assuming only one stream
                    //Calculate the rbg needed for the transmission and schedule them with the ressource manager
                    
                    
                    uint32_t rbPacket = 0;
                    uint32_t tbs = 0;
                    if( m_manager->m_use5MHz)
                    {
                      while(tbs  < 625 && tbs  < (schedInfoIt->second)+4 ) //limit to 10Mbit/s
                      {
                        tbs = m_dlAmc->CalculateTbSize (GetUe (*schedInfoIt)->m_dlMcs.at(0),rbPacket);
                        rbPacket++;
                      }
                      if(tbs > 625)
                      {
                        rbPacket--;
                      }
                    }

                    else{
                      const uint32_t maxRbSymDl =
                          static_cast<uint32_t>(GetBandwidthInRbg()) *
                          m_macSchedSapUser->GetSymbolsPerSlot();
                      while(tbs  < (schedInfoIt->second)+7 && rbPacket< maxRbSymDl) //TODO_+4 to cover for overhead from headers which will get substracted 
                      {
                        tbs = m_dlAmc->CalculateTbSize (GetUe (*schedInfoIt)->m_dlMcs.at(0),rbPacket);
                        rbPacket++;
                      }
                    }
                   
                    RessourceSet* Res = new RessourceSet();
                    if(m_manager->pdcchAvailableInSearchSpace(false,dlSfnSf,GetUe (*schedInfoIt)->m_rnti, GetBwpId()))
                    {
                      Res = m_manager->scheduleData( GetBwpId (),GetUe (*schedInfoIt)->m_rnti,rbPacket,LteNrTddSlotType::DL,false,dlSfnSf,Res);
                    }
                    else{
                      Res->NumberRB=0;
                    }
                    if(Res->NumberRB!=0&&Res->NumberSymbols!=0)
                    {
                      //UE got data scheduled. Increment the first ue to be scheduled in the next slot and update freeRessources
                      //For now DL doesnt advance Round-robin scheduling
                      //schedStartUe = GetUe (*schedInfoIt)->m_rnti +1;
            
                      //Check for active Searchspaces and remove UEs which are not reachable
                      if(!m_manager->markViablePdcchRessource(false,dlSfnSf,GetUe (*schedInfoIt)->m_rnti, GetBwpId()))
                      {
                          std::cout<<"Error: Cannot mark pdcch ressources although they should be available."<<std::endl;
                      }
                     
                    }    
                    else{
                      NS_LOG_DEBUG("Could not Schedule Data for ue with rnti "+ GetUe (*schedInfoIt)->m_rnti);
                    }
                    delete Res;
                    
                }
            schedInfoIt++;
             // In the case that all the UE already have their requirements fullfilled,
            // then stop the beam processing and pass to the next
            if (schedInfoIt == ueVector.end())
            {
                break;
            }

            

          }
          

        std::vector<std::vector<int>> slotRessources = m_manager->getSlotRessources(GetBwpId(),dlSfnSf,LteNrTddSlotType::DL);
      //update ue paraeters
      
      //uint8_t rbgAssignable = GetBandwidthInRbg(); //We can assign a full symbol at once
      uint8_t rbgAssignable = 1;
      FTResources assigned(0, 0);
      uint16_t ueNum =0;
      uint64_t numRB =0;
      uint16_t bw = GetBandwidthInRbg();
       if(bw == 266)
      {
        //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
        //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
        bw = 265;
      }
       if(bw == 213)
      {
        //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
        //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
        bw = 212;
      }
      if(bw == 160)
      {
        //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
        //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
        bw = 159;
      }

       std::list<uint16_t> assignedRntiList;
      for( size_t i =0; i< slotRessources.size(); i++)
      {
        for(size_t rb =0; rb< bw;rb++)
        {
          if(slotRessources[i][rb] > 0 || ueNum >0)
          {
              if(slotRessources[i][rb]==ueNum&& rb !=0)
              {
                numRB++;
              }
              
              else{
                std::list<uint16_t>::iterator findIter = std::find(assignedRntiList.begin(), assignedRntiList.end(), ueNum);

                  bool alreadyAssigned = findIter != assignedRntiList.end();
                  if(alreadyAssigned)
                  {
                    ueNum =slotRessources[i][rb];
                   numRB=1;
                    continue;
                  }
                uint dlSymbols = 1;
                uint usedRb= numRB;
                if(rb != 0)
                {
                
                  while( i+dlSymbols<slotRessources.size() &&slotRessources[i+dlSymbols][rb-1] ==ueNum )
                  {
                    
                      ++dlSymbols; 
                      numRB +=  usedRb;                              
                  }
                }
                else{
                  
                  while(i+dlSymbols<=slotRessources.size() && slotRessources[i+dlSymbols-1][bw-1] ==ueNum)
                  {
                  
                      ++dlSymbols; 
                      numRB +=  usedRb;                              
                  }

                }
                auto assignDlIt = ueVector.begin();
                while(assignDlIt != ueVector.end())
                {
                  if(GetUe(*assignDlIt)->m_rnti == ueNum)
                  {                      
                      GetUe(*assignDlIt)->m_dlRBG = numRB;
                      GetUe(*assignDlIt)->m_dlSym =dlSymbols; 
                      beamSym = GetUe(*assignDlIt)->m_dlSym;
                      assigned.m_rbg= numRB;
                      assigned.m_sym = beamSym;
                      AssignedDlResources(*assignDlIt, FTResources(rbgAssignable, beamSym), assigned);

                      assignedRntiList.emplace_back(ueNum);
  
                  
                        


                      // Update metrics for the unsuccessfull UEs (who did not get any resource in this iteration)
                    for (auto & ue : ueVector)
                    {
                      if (GetUe (ue)->m_rnti != GetUe (*assignDlIt)->m_rnti)
                        {
                          NotAssignedDlResources (ue, FTResources (rbgAssignable, beamSym),
                                                  assigned);
                        }
                    }
                      break;
                  }
                  {
                    assignDlIt++;
                  }
                } 
                ueNum =slotRessources[i][rb];
                numRB=1;
              }
            }
          }
      }

           
        
    }

  return symPerBeam;
}

/*
NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignULRBG(uint32_t symAvail, const ActiveUeMap& activeUl) const
{
    NS_LOG_FUNCTION(this);

    NS_LOG_DEBUG("# beams active flows: " << activeUl.size() << ", # sym: " << symAvail);

    GetFirst GetBeamId;
    GetSecond GetUeVector;
    BeamSymbolMap symPerBeam = GetSymPerBeam(symAvail, activeUl);

    // Iterate through the different beams
    for (const auto& el : activeUl)
    {
        // Distribute the RBG evenly among UEs of the same beam
        uint32_t beamSym = symPerBeam.at(GetBeamId(el));
        uint32_t rbgAssignable = 1 * beamSym;
        std::vector<UePtrAndBufferReq> ueVector;
        FTResources assigned(0, 0);
        const std::vector<uint8_t> ulNotchedRBGsMask = GetUlNotchedRbgMask();
        uint32_t resources = ulNotchedRBGsMask.size() > 0
                                 ? std::count(ulNotchedRBGsMask.begin(), ulNotchedRBGsMask.end(), 1)
                                 : GetBandwidthInRbg();
        NS_ASSERT(resources > 0);

        for (const auto& ue : GetUeVector(el))
        {
            ueVector.emplace_back(ue);
        }

        for (auto& ue : ueVector)
        {
            BeforeUlSched(ue, FTResources(rbgAssignable * beamSym, beamSym));
        }

        while (resources > 0)
        {
            GetFirst GetUe;
          //std::sort (ueVector.begin (), ueVector.end (), GetUeCompareUlFn ()); //Comment out this line to assign the packets in order
            auto schedInfoIt = ueVector.begin();

            // Ensure fairness: pass over UEs which already has enough resources to transmit
            while (schedInfoIt != ueVector.end())
            {
                uint32_t bufQueueSize = schedInfoIt->second;
                if (GetUe(*schedInfoIt)->m_ulTbSize >= std::max(bufQueueSize, 12U))
                {
                    schedInfoIt++;
                }
                else
                {
                    break;
                }
            }

            // In the case that all the UE already have their requirements fullfilled,
            // then stop the beam processing and pass to the next
            if (schedInfoIt == ueVector.end())
            {
                break;
            }

            // Assign 1 RBG for each available symbols for the beam,
            // and then update the count of available resources
            GetUe(*schedInfoIt)->m_ulRBG += rbgAssignable;
            assigned.m_rbg += rbgAssignable;

            GetUe(*schedInfoIt)->m_ulSym = beamSym;
            assigned.m_sym = beamSym;

            resources -= 1; // Resources are RBG, so they do not consider the beamSym

            // Update metrics
            NS_LOG_DEBUG("Assigned " << rbgAssignable << " UL RBG, spanned over " << beamSym
                                     << " SYM, to UE " << GetUe(*schedInfoIt)->m_rnti);
            AssignedUlResources(*schedInfoIt, FTResources(rbgAssignable, beamSym), assigned);

            // Update metrics for the unsuccessfull UEs (who did not get any resource in this
            // iteration)
            for (auto& ue : ueVector)
            {
                if (GetUe(ue)->m_rnti != GetUe(*schedInfoIt)->m_rnti)
                {
                    NotAssignedUlResources(ue, FTResources(rbgAssignable, beamSym), assigned);
                }
            }
        }
    }

    return symPerBeam;
}*/

/**
 * \brief Create the DL DCI in OFDMA mode
 * \param spoint Starting point
 * \param ueInfo UE representation
 * \param maxSym Maximum symbols to use
 * \return a pointer to the newly created instance
 *
 * The function calculates the TBS and then call CreateDci().
 */
std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateDlDci(NrMacSchedulerNs3::PointInFTPlane* spoint,
                                 const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                 uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    uint16_t countLessThanMinBytes = 0;

    // we do not need to recalculate the TB size here because we already
    // computed it in side the method AssignDLRBG called before this method.
    // otherwise, we need to repeat the logic of NrMacSchedulerUeInfo::UpdateDlMetric
    // here to cover MIMO

    // Due to MIMO implementation MCS, TB size, ndi, rv, are vectors
    std::vector<uint8_t> ndi;
    ndi.resize(ueInfo->m_dlTbSize.size());
    std::vector<uint8_t> rv;
    rv.resize(ueInfo->m_dlTbSize.size());

    for (uint32_t numTb = 0; numTb < ueInfo->m_dlTbSize.size(); numTb++)
    {
        uint32_t tbs = ueInfo->m_dlTbSize.at(numTb);
        if (tbs < 10)
        {
            countLessThanMinBytes++;
            NS_LOG_DEBUG("While creating DCI for UE "
                         << ueInfo->m_rnti << " stream " << numTb << " assigned " << ueInfo->m_dlRBG
                         << " DL RBG, but TBS < 10, reseting its size to zero in UE info");
            ueInfo->m_dlTbSize.at(numTb) = 0;
            ndi.at(numTb) = 0;
            rv.at(numTb) = 0;
            continue;
        }
        ndi.at(numTb) = 1;
        rv.at(numTb) = 0;
    }

    NS_ASSERT_MSG(ueInfo->m_dlRBG % maxSym == 0,
                  " MaxSym " << maxSym << " RBG: " << ueInfo->m_dlRBG);
    NS_ASSERT(ueInfo->m_dlRBG <= maxSym * GetBandwidthInRbg());
    NS_ASSERT(spoint->m_rbg < GetBandwidthInRbg());
    NS_ASSERT(maxSym <= UINT8_MAX);

    // If the size of all the TBs is less than 10 bytes,
    // i.e. 7 bytes (the minimum TX opportunity by RLC AM) + 3 bytes of the MAC header
    // then we can't transmit any new data, so don't create dci.
    if (countLessThanMinBytes == ueInfo->m_dlTbSize.size())
    {
        NS_LOG_DEBUG("While creating DCI for UE " << ueInfo->m_rnti << " assigned "
                                                  << ueInfo->m_dlRBG << " DL RBG, but TBS < 10");
        return nullptr;
    }

    uint32_t RBGNum = ueInfo->m_dlRBG / maxSym;
    std::vector<uint8_t> rbgBitmask = GetDlNotchedRbgMask();

    if (rbgBitmask.size() == 0)
    {
        rbgBitmask = std::vector<uint8_t>(GetBandwidthInRbg(), 1);
    }

    // rbgBitmask is all 1s or have 1s in the place we are allowed to transmit.

    NS_ASSERT(rbgBitmask.size() == GetBandwidthInRbg());

    uint32_t lastRbg = spoint->m_rbg;

    // Limit the places in which we can transmit following the starting point
    // and the number of RBG assigned to the UE
    for (uint32_t i = 0; i < GetBandwidthInRbg(); ++i)
    {
        if (i >= spoint->m_rbg && RBGNum > 0 && rbgBitmask[i] == 1)
        {
            // assigned! Decrement RBGNum and continue the for
            RBGNum--;
            lastRbg = i;
        }
        else
        {
            // Set to 0 the position < spoint->m_rbg OR the remaining RBG when
            // we already assigned the number of requested RBG
            rbgBitmask[i] = 0;
        }
    }

    NS_ASSERT_MSG(
        RBGNum == 0,
        "If you see this message, it means that the AssignRBG and CreateDci method are unaligned");

    std::ostringstream oss;
    for (const auto& x : rbgBitmask)
    {
        oss << std::to_string(x) << " ";
    }

    NS_LOG_INFO("UE " << ueInfo->m_rnti << " assigned RBG from " << spoint->m_rbg << " with mask "
                      << oss.str() << " for " << static_cast<uint32_t>(maxSym) << " SYM.");

    std::shared_ptr<DciInfoElementTdma> dci =
        std::make_shared<DciInfoElementTdma>(ueInfo->m_rnti,
                                             DciInfoElementTdma::DL,
                                             spoint->m_sym,
                                             maxSym,
                                             ueInfo->m_dlMcs,
                                             ueInfo->m_dlTbSize,
                                             ndi,
                                             rv,
                                             DciInfoElementTdma::DATA,
                                             GetBwpId(),
                                             GetTpc());

    dci->m_rbgBitmask = std::move(rbgBitmask);
      
    NS_ASSERT(std::count(dci->m_rbgBitmask.begin(), dci->m_rbgBitmask.end(), 0) !=
              GetBandwidthInRbg());

    spoint->m_rbg = lastRbg + 1;

    return dci;
}

/*
std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateUlDci(PointInFTPlane* spoint,
                                 const std::shared_ptr<NrMacSchedulerUeInfo>& ueInfo,
                                 uint32_t maxSym) const
{
    NS_LOG_FUNCTION(this);

    uint32_t tbs = m_ulAmc->CalculateTbSize(ueInfo->m_ulMcs, ueInfo->m_ulRBG * GetNumRbPerRbg());

    // If is less than 12, i.e., 7 (3 mac header, 2 rlc header, 2 data) + 5 bytes for
    // the SHORT_BSR. then we can't transmit any new data, so don't create dci.
    if (tbs < 12)
    {
        NS_LOG_DEBUG("While creating UL DCI for UE " << ueInfo->m_rnti << " assigned "
                                                     << ueInfo->m_ulRBG << " UL RBG, but TBS < 12");
        return nullptr;
    }

    uint32_t RBGNum = ueInfo->m_ulRBG / maxSym;
    std::vector<uint8_t> rbgBitmask = GetUlNotchedRbgMask();

    if (rbgBitmask.size() == 0)
    {
        rbgBitmask = std::vector<uint8_t>(GetBandwidthInRbg(), 1);
    }

    // rbgBitmask is all 1s or have 1s in the place we are allowed to transmit.

    NS_ASSERT(rbgBitmask.size() == GetBandwidthInRbg());

    uint32_t lastRbg = spoint->m_rbg;
    uint32_t assigned = RBGNum;

    // Limit the places in which we can transmit following the starting point
    // and the number of RBG assigned to the UE
    for (uint32_t i = 0; i < GetBandwidthInRbg(); ++i)
    {
        if (i >= spoint->m_rbg && RBGNum > 0 && rbgBitmask[i] == 1)
        {
            // assigned! Decrement RBGNum and continue the for
            RBGNum--;
            lastRbg = i;
        }
        else
        {
            // Set to 0 the position < spoint->m_rbg OR the remaining RBG when
            // we already assigned the number of requested RBG
            rbgBitmask[i] = 0;
        }
    }

    NS_ASSERT_MSG(
        RBGNum == 0,
        "If you see this message, it means that the AssignRBG and CreateDci method are unaligned");

    NS_LOG_INFO("UE " << ueInfo->m_rnti << " assigned RBG from " << spoint->m_rbg << " to "
                      << spoint->m_rbg + assigned << " for " << static_cast<uint32_t>(maxSym)
                      << " SYM.");

    // Due to MIMO implementation MCS, TB size, ndi, rv, are vectors
    std::vector<uint8_t> ulMcs = {ueInfo->m_ulMcs};
    std::vector<uint32_t> ulTbs = {tbs};
    std::vector<uint8_t> ndi = {1};
    std::vector<uint8_t> rv = {0};

    NS_ASSERT(spoint->m_sym >= maxSym);
    std::shared_ptr<DciInfoElementTdma> dci =
        std::make_shared<DciInfoElementTdma>(ueInfo->m_rnti,
                                             DciInfoElementTdma::UL,
                                             spoint->m_sym - maxSym,
                                             maxSym,
                                             ulMcs,
                                             ulTbs,
                                             ndi,
                                             rv,
                                             DciInfoElementTdma::DATA,
                                             GetBwpId(),
                                             GetTpc());

    dci->m_rbgBitmask = std::move(rbgBitmask);

    std::ostringstream oss;
    for (auto x : dci->m_rbgBitmask)
    {
        oss << std::to_string(x) << " ";
    }
    NS_LOG_INFO("UE " << ueInfo->m_rnti << " DCI RBG mask: " << oss.str());

    NS_ASSERT(std::count(dci->m_rbgBitmask.begin(), dci->m_rbgBitmask.end(), 0) !=
              GetBandwidthInRbg());

    spoint->m_rbg = lastRbg + 1;

    return dci;
}*/

void
NrMacSchedulerOfdma::ChangeDlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_rbg = 0;
    spoint->m_sym += symOfBeam;
}

void
NrMacSchedulerOfdma::ChangeUlBeam(PointInFTPlane* spoint, uint32_t symOfBeam) const
{
    spoint->m_rbg = 0;
    spoint->m_sym -= symOfBeam;
}

uint8_t
NrMacSchedulerOfdma::GetTpc() const
{
    NS_LOG_FUNCTION(this);
  return 1; // 1 is mapped to 0 for Accumulated mode, and to -1 in Absolute mode TS38.213 Table Table 7.1.1-1
}

NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignULRBG (uint32_t symAvail, const ActiveUeMap &activeUl ) const
{
  NS_LOG_FUNCTION (this);
   NS_ASSERT_MSG(false,"This function is no longer used. AssignULRBG_manager should be used");
   BeamSymbolMap symPerBeam = GetSymPerBeam (symAvail, activeUl);
  return symPerBeam;
//   NS_LOG_DEBUG ("# beams active flows: " << activeUl.size () << ", # sym: " << symAvail);

//   GetFirst GetBeamId;
//   GetSecond GetUeVector;
//   BeamSymbolMap symPerBeam = GetSymPerBeam (symAvail, activeUl);

//   GetFirst GetUeRnti;
//   GetSecond GetRBcounter;
//   bool firstSym = true;

//   // Iterate through the different beams
//   if (m_schType_OFDMA == 1)
//   {
//       for (const auto &el : activeUl)
//         {
//           // Distribute the RBG evenly among UEs of the same beam
//           uint32_t beamSym = symPerBeam.at (GetBeamId (el));
//           uint32_t rbgAssignable = 1 * beamSym;
//           std::vector<UePtrAndBufferReq> ueVector;
//           FTResources assigned (0,0);
//           const std::vector<uint8_t> ulNotchedRBGsMask = GetUlNotchedRbgMask ();
//           uint32_t resources = ulNotchedRBGsMask.size () > 0 ? std::count (ulNotchedRBGsMask.begin (),
//                                                                          ulNotchedRBGsMask.end (),
//                                                                          1) : GetBandwidthInRbg ();
//           NS_ASSERT (resources > 0);

//           for (const auto &ue : GetUeVector (el))
//             {
//               ueVector.emplace_back (ue);
//             }

//           for (auto & ue : ueVector)
//             {
//               BeforeUlSched (ue, FTResources (rbgAssignable * beamSym, beamSym));
//             }

//           while (resources > 0)
//             {
//               GetFirst GetUe;
//               //std::sort (ueVector.begin (), ueVector.end (), GetUeCompareUlFn ()); //Comment out this line to assign the packets in order
//               auto schedInfoIt = ueVector.begin ();

//               // Ensure fairness: pass over UEs which already has enough resources to transmit
//               while (schedInfoIt != ueVector.end ())
//                 {
//                   uint32_t bufQueueSize = schedInfoIt->second;
//                   if (GetUe (*schedInfoIt)->m_ulTbSize >= std::max (bufQueueSize, 7U))
//                     {
//                       schedInfoIt++;
//                     }
//                   else
//                     {
//                       break;
//                     }
//                 }

//               // In the case that all the UE already have their requirements fullfilled,
//               // then stop the beam processing and pass to the next
//               if (schedInfoIt == ueVector.end ())
//                 {
//                   break;
//                 }

//               // Assign 1 RBG for each available symbols for the beam,
//               // and then update the count of available resources
//               GetUe (*schedInfoIt)->m_ulRBG += rbgAssignable;
//               assigned.m_rbg += rbgAssignable;

//               GetUe (*schedInfoIt)->m_ulSym = beamSym;
//               assigned.m_sym = beamSym;

//               resources -= 1; // Resources are RBG, so they do not consider the beamSym

//               // Update metrics
//               NS_LOG_DEBUG ("Assigned " << rbgAssignable <<
//                             " UL RBG, spanned over " << beamSym << " SYM, to UE " <<
//                             GetUe (*schedInfoIt)->m_rnti);
//               AssignedUlResources (*schedInfoIt, FTResources (rbgAssignable, beamSym),
//                                    assigned);

//               // Update metrics for the unsuccessfull UEs (who did not get any resource in this iteration)
//               for (auto & ue : ueVector)
//                 {
//                   if (GetUe (ue)->m_rnti != GetUe (*schedInfoIt)->m_rnti)
//                     {
//                       NotAssignedUlResources (ue, FTResources (rbgAssignable, beamSym),
//                                               assigned);
//                     }
//                 }
//             }
//         }
//   }
//   else
//   {
//       for (const auto &el : activeUl)
//           {
//             uint32_t beamSym = symPerBeam.at (GetBeamId (el));
//             std::vector<UePtrAndBufferReq> ueVector;
//             FTResources assigned (0,0);
//             const std::vector<uint8_t> ulNotchedRBGsMask = GetUlNotchedRbgMask ();
//             uint32_t rbgInOneSymbol = ulNotchedRBGsMask.size () > 0 ? std::count (ulNotchedRBGsMask.begin (),
//                                                                            ulNotchedRBGsMask.end (),
//                                                                            1) : GetBandwidthInRbg ();

//             std::vector<SchedUeMap> ueSchedVector;
//             std::vector<SchedUeMapFirstSym> ueSchedVectorFirstSym;
//             std::vector<uint16_t> rntiOrder;

//             uint32_t symAssignable = 1 ;
//             uint32_t resources = rbgInOneSymbol*beamSym;
        
           
//             NS_ASSERT (resources > 0);

//             for (const auto &ue : GetUeVector(el))
//               {
//                 ueVector.emplace_back (ue);
//               }

//             for (auto & ue : ueVector)
//               {
//                 BeforeUlSched (ue, FTResources (resources, beamSym));
//               }

//             //Find the minimum RB to assign 1 TBS
//             // We could find the optimal RB to assign

//             uint32_t rbPacket = 2;
//             uint32_t rbgAssignable = 2;
//             GetFirst GetUe;

//             // Calculate the RBs to schedule for a UE
//             auto it_packetTBS = ueVector.begin ();
//             uint32_t tbs = 0;
//             while(tbs < it_packetTBS->second)
//              {
//             //GetUe (*it_packetTBS)->m_ulMcs
//                 tbs = m_ulAmc->CalculateTbSize (GetUe (*it_packetTBS)->m_ulMcs,rbPacket);
//                 rbPacket++;
//             }
//             std::vector<uint32_t> v_rbgAssignable;
//             if (v_rbgAssignable.size() == 0)
//             {
//                 v_rbgAssignable = std::vector<uint32_t> (rbgInOneSymbol/2 , 0);
//             }
//             uint8_t posRBassignable = 0;
//             uint32_t rbgInOneSymbolPrime = 0;
//             rbgInOneSymbolPrime = rbgInOneSymbol;
//             uint32_t rbAssignableMin = 0;
//             uint32_t  rbAssignableMinStored = 1000;
//             bool alreadyAssigned = false;
//             if (m_schType_OFDMA==3)
//             {
//                 while(1)
//                 {
//                     while(rbgAssignable < rbgInOneSymbolPrime)
//                      {
//                        if (rbgInOneSymbolPrime%rbgAssignable == 0)
//                         {
//                           for(uint8_t posFind = 0;posFind < posRBassignable; posFind++)
//                           {
//                               if (v_rbgAssignable[posFind]==rbgAssignable)
//                               {
//                                   alreadyAssigned = true;
//                                   break;
//                               }
//                           }
//                           if (!alreadyAssigned)
//                           {
//                              v_rbgAssignable[posRBassignable] = rbgAssignable;
//                              posRBassignable++;
//                              v_rbgAssignable[posRBassignable] = rbgInOneSymbolPrime/rbgAssignable;
//                              posRBassignable++;

//                              std::cout << "Assigned RBs: First = " << v_rbgAssignable[posRBassignable-1] << " and Second = "<< v_rbgAssignable[posRBassignable-2] <<'\n';
//                           }
//                           alreadyAssigned = false;
//                         }
//                        rbgAssignable ++;
//                      }

//                     if (posRBassignable == 0)
//                     {
//                       rbgAssignable = 2;
//                       rbgInOneSymbolPrime = rbgInOneSymbol-1;
//                     }
//                     else
//                     {
//                         break;
//                     }
//                 }

//                 for (uint8_t ii = 0; ii < v_rbgAssignable.size(); ii++)
//                 {
//                     if(v_rbgAssignable[ii] == 0)
//                     {
//                         v_rbgAssignable[ii] = 1000;
//                     }
//                     else
//                     {
//                         int numRbgAssignable = (rbgInOneSymbolPrime/(ueVector.size()))/v_rbgAssignable[ii];
//                         if (numRbgAssignable == 0)
//                         {
//                           numRbgAssignable = 1;
//                         }
//                         double numSym = ceil(rbPacket/(v_rbgAssignable[ii]*numRbgAssignable));
//                         if (rbPacket%(v_rbgAssignable[ii]*numRbgAssignable)!= 0){numSym = rbPacket/(v_rbgAssignable[ii]*numRbgAssignable) +1;} //ceil
//                         rbAssignableMin = uint32_t(v_rbgAssignable[ii]*numRbgAssignable*numSym - rbPacket);

//                         if (rbAssignableMin < rbAssignableMinStored)
//                         {
//                           // Only if the packet enters in less than one slot
//                           if (numSym < 12)
//                            {
//                                rbgAssignable =  v_rbgAssignable[ii];
//                                rbAssignableMinStored = rbAssignableMin;
//                                std::cout << "Assignable RBs: " << rbgAssignable << " we are going to loss " << rbAssignableMin << " resources"<<'\n';
//                            }
//                         }
//                     }
//                 }
//             }


//             if (m_schType_OFDMA==2)
//             {
//                 rbgAssignable = 2;
//             }

//             static uint8_t nextSymbol = 1;
//             static uint8_t nextUE = 0;
//             static uint8_t initSym = 1;

//             int countPos = 0;
//             int initRNTIpos = 0;

//             while (resources > 0)
//               {
//                 auto schedInfoIt = ueVector.begin ();

//                 uint32_t vectorSize = ueVector.size();

//                 GetFirst GetFirstSym;
//                 GetSecond GetUeRBGcounter;
//                 GetSecond GetRB;
//                 auto UeScheduling_sym = ueSchedVectorFirstSym.begin ();

//                 // Calculate the current symbol number and select the UE to be scheduled
//                 uint8_t sym = 1;
//                 while (sym <= beamSym)
//                // while (sym <= 4)
//                   {

//                     uint32_t scheduledUEs = 0;
//                     if (resources-1 > ((rbgInOneSymbol*beamSym)-(rbgInOneSymbol*sym)))
//                       {
//                         if (m_schType_OFDMA==3)
//                           {
//                             schedInfoIt = ueVector.begin () + initRNTIpos;
//                           }
//                         else
//                           {
//                             schedInfoIt = ueVector.begin ();
//                           }

//                         while (schedInfoIt != ueVector.end ())
//                           {
//                             uint32_t bufQueueSize = schedInfoIt->second;
//                             if (GetUe (*schedInfoIt)->m_ulTbSize < std::max (bufQueueSize, 10U))
//                               {
//                                 if (m_schType_OFDMA==2)
//                                   {
//                                     if (nextSymbol < sym)
//                                       {
//                                         nextSymbol ++;

//                                         if(ueSchedVectorFirstSym.size()>1 || ueSchedVector.size()>1)
//                                           {
//                                             GetUe (*schedInfoIt)->m_ulTbSize = 0;
//                                             GetUe (*schedInfoIt)->m_ulSym = 0;
//                                             GetUe (*schedInfoIt)->m_ulRBG = 0;

//                                             ueSchedVectorFirstSym.clear();
//                                             ueSchedVector.clear();
//                                             goto scheduleUE;
//                                         }
//                                       }
//                                     if (GetUe (*schedInfoIt)->m_ulSym == 1)
//                                       {
//                                         goto scheduleUE;
//                                       }
//                                     else
//                                       {
//                                         if (GetUe (*schedInfoIt)->m_ulSym < sym)
//                                           {
//                                             goto scheduleUE;
//                                           }
//                                       }
//                                   }

//                                 if (m_schType_OFDMA==3)
//                                   {
//                                     if (sym == initSym)
//                                       {
//                                         if ((ueVector.begin () + countPos) == ueVector.end())
//                                           {
//                                             countPos = initRNTIpos;
//                                           }

//                                         schedInfoIt = ueVector.begin () + countPos;

//                                         countPos++;
//                                         goto scheduleUE;
//                                       }
//                                     else
//                                      {
//                                         nextUE = 0;
//                                         if ((GetUe (*schedInfoIt)->m_ulSym+initSym-1) < sym)
//                                           {
//                                             auto UeScheduling = ueSchedVector.begin ();

//                                             while (UeScheduling != ueSchedVector.end ())
//                                               {
//                                                 if (GetUe (*schedInfoIt)->m_rnti == (GetUeRnti (*UeScheduling)))
//                                                   {
//                                                     goto scheduleUE;
//                                                   }
//                                                 UeScheduling++;
//                                               }
//                                           }
//                                      }
//                                   }

//                               }
//                             else
//                               {
//                                 scheduledUEs ++;

//                                 if(m_schType_OFDMA==3)
//                                     {
//                                       uint16_t vectorSizeOfUe = UeScheduling_sym->second.size();
//                                       bool applyConstrain = false;
//                                       bool clearSchedVector = false;
//                                       if (sym == beamSym && vectorSizeOfUe>1)
//                                         {
//                                           applyConstrain = true;
//                                         }
//                                       else
//                                         {
//                                           if (nextUE <= scheduledUEs)
//                                             {
//                                               if (scheduledUEs == vectorSizeOfUe)
//                                                 {
//                                                   if (scheduledUEs > 1)
//                                                   {
//                                                       if (resources == (rbgInOneSymbol*beamSym)-(rbgInOneSymbol*(sym-1)))
//                                                         {
//                                                           clearSchedVector = true;
//                                                         }
//                                                       else
//                                                         {
//                                                           applyConstrain = true;
//                                                         }
//                                                   }
//                                                   else if (scheduledUEs == 1)
//                                                   {
//                                                       clearSchedVector = true;
//                                                   }

//                                                 }

//                                             }
//                                         }
//                                       if (applyConstrain)
//                                         {
//                                          auto schedInfoUeIt = ueVector.begin ();
//                                          while (schedInfoUeIt != ueVector.end ())
//                                             {
//                                               uint16_t rntiPrueba = GetUe (*schedInfoUeIt)->m_rnti;
//                                               NS_LOG_INFO ("Rnti: "<<rntiPrueba);

//                                               auto ueSym = ueSchedVectorFirstSym.begin();
//                                               ueSchedVector = GetUeRBGcounter(*ueSym);

//                                               auto UeScheduling = ueSchedVector.begin ();

//                                               while (UeScheduling != ueSchedVector.end ())
//                                                 {
//                                                   if (GetUe (*schedInfoUeIt)->m_rnti == (GetUeRnti (*UeScheduling)))
//                                                     {
//                                                       if (resources-1 >= ((rbgInOneSymbol*beamSym)-(rbgInOneSymbol*(sym-1))))
//                                                       {
//                                                           sym = sym-1;
//                                                       }
//                                                       if ( GetUe (*schedInfoUeIt)->m_ulRBG < ((sym-initSym+1)*GetRBcounter(*UeScheduling))) //+1 �?
//                                                         {
//                                                           GetUe (*schedInfoUeIt)->m_ulRBG = (sym-initSym+1)*GetRBcounter(*UeScheduling) ;
//                                                           GetUe (*schedInfoUeIt)->m_ulSym = (sym-initSym+1) ;
//                                                           assigned.m_rbg = GetUe (*schedInfoUeIt)->m_ulRBG;
//                                                           assigned.m_sym =  GetUe (*schedInfoUeIt)->m_ulSym;
//                                                           AssignedUlResources (*schedInfoUeIt, FTResources (rbgAssignable, symAssignable),
//                                                                                assigned);
//                                                         }
//                                                       break;
//                                                     }
//                                                   UeScheduling++;
//                                                 }
//                                               schedInfoUeIt++;
//                                             }
//                                           resources = (rbgInOneSymbol*beamSym)-(rbgInOneSymbol*sym);
//                                           sym++;
//                                           clearSchedVector = true;
//                                         }
//                                       applyConstrain = false;
//                                       if (clearSchedVector)
//                                         {
//                                           ueSchedVectorFirstSym.clear();
//                                           ueSchedVector.clear();
//                                           initSym = sym;
//                                           nextUE = scheduledUEs;
//                                           clearSchedVector = false;
//                                           initRNTIpos = countPos;
//                                           if (initRNTIpos == int(ueVector.size()))
//                                           {
//                                               rbgAssignable = 2;
//                                           }
//                                         }
//                                     }
//                               }
//                             schedInfoIt++;
//                           }
//                         if (schedInfoIt == ueVector.end ())
//                           {
//                             if (scheduledUEs == vectorSize)
//                             {
//                               sym = beamSym+1;
//                               goto scheduleUE;
//                             }
//                             resources = (rbgInOneSymbol*beamSym)-(rbgInOneSymbol*sym);
//                           }
//                       }
//                       sym=sym+1;
//                   }

//                 // Schedule the selected UE
//                 scheduleUE:

//                 if (sym >= beamSym+1)
//                   {
//                     nextUE = 0;
//                     initSym = 1;
//                     nextSymbol = 1;

//                     if(ueSchedVectorFirstSym.size()>1)
//                       {
//                       auto schedInfoIt = ueVector.begin ();
//                       while (schedInfoIt != ueVector.end ())
//                          {
//                           uint8_t sizeUE = ueSchedVector.size();
//                           auto ueRNTI = ueSchedVector.begin()+sizeUE-1;

//                            if (GetUe (*schedInfoIt)->m_rnti == GetUeRnti(*ueRNTI))
//                              {
//                                GetUe (*schedInfoIt)->m_ulTbSize = 0;
//                                GetUe (*schedInfoIt)->m_ulSym = 0;
//                                GetUe (*schedInfoIt)->m_ulRBG = 0;
//                                break;
//                              }
//                            schedInfoIt++;
//                          }
//                      }
//                     break;
//                   }

//                 while (UeScheduling_sym != ueSchedVectorFirstSym.end ())
//                   {
//                     ueSchedVector = GetUeRBGcounter(*UeScheduling_sym);

//                     auto UeScheduling = ueSchedVector.begin ();

//                     while (UeScheduling != ueSchedVector.end ())
//                       {
//                         if (GetUeRnti (*UeScheduling) == GetUe (*schedInfoIt)->m_rnti)
//                           {
//                             if (GetFirstSym (*UeScheduling_sym) == sym)
//                                {
//                                   GetUe (*schedInfoIt)->m_ulSym = 1;
//                                   GetUe (*schedInfoIt)->m_ulRBG += rbgAssignable;
//                                   assigned.m_rbg += rbgAssignable;
//                                   GetRBcounter(*UeScheduling) = GetUe (*schedInfoIt)->m_ulRBG  ;
//                                   GetUeRBGcounter(*UeScheduling_sym)= ueSchedVector;
//                                   if (resources > rbgAssignable)
//                                   {
//                                      resources -= rbgAssignable;
//                                   }
//                                   else
//                                   {
//                                       resources = 1;
//                                   }
//                                   goto UEscheduled;
//                                }
//                             else
//                               {
//                                 GetUe (*schedInfoIt)->m_ulSym = (sym-(GetFirstSym (*UeScheduling_sym)))+1;
//                                 // Apply constraint for RBOFDMA: copy the same number
//                                 // of RBs even if it does not need all of them.
//                                 GetUe (*schedInfoIt)->m_ulRBG += GetRB(*UeScheduling);
//                                 if (m_schType_OFDMA==2)
//                                 {
//                                     assigned.m_rbg = GetUe (*schedInfoIt)->m_ulRBG;
//                                 }
//                                 else
//                                 {
//                                     assigned.m_rbg += GetRB(*UeScheduling) ;
//                                 }
//                                 resources -= GetRB(*UeScheduling);

//                                 goto UEscheduled;
//                               }
//                           }
//                         else
//                           {
//                             UeScheduling++;
//                           }
//                       }
//                     if (UeScheduling == ueSchedVector.end())
//                       {
//                         if (GetFirstSym (*UeScheduling_sym) == sym)
//                           {
//                             GetUe (*schedInfoIt)->m_ulSym = 1;
//                             GetUe (*schedInfoIt)->m_ulRBG += rbgAssignable;
//                             assigned.m_rbg += rbgAssignable;
//                             ueSchedVector.emplace_back(GetUe (*schedInfoIt)->m_rnti,GetUe (*schedInfoIt)->m_ulRBG );
//                             GetUeRBGcounter(*UeScheduling_sym)= ueSchedVector;
//                             if (resources > rbgAssignable)
//                             {
//                               resources -= rbgAssignable;
//                             }
//                             else
//                             {
//                                 resources = 1;
//                             }

//                             rntiOrder.emplace_back(GetUe (*schedInfoIt)->m_rnti);

//                             goto UEscheduled;

//                           }
//                         UeScheduling_sym++;
//                       }
//                   }

//                 // Update metrics after allocating resources to the UE
//                 UEscheduled:

//                 if (UeScheduling_sym == ueSchedVectorFirstSym.end ())
//                   {
//                     if (!firstSym)
//                       {
//                         ueSchedVector.clear ();
//                         UeScheduling_sym ++;
//                       }
//                       if (resources < rbgAssignable)
//                       {
//                           resources = 1;
//                       }
//                       else
//                       {
//                           GetUe (*schedInfoIt)->m_ulSym = 1;
//                           GetUe (*schedInfoIt)->m_ulRBG += rbgAssignable;
//                           assigned.m_rbg += rbgAssignable;
//                           ueSchedVector.emplace_back(GetUe (*schedInfoIt)->m_rnti,GetUe (*schedInfoIt)->m_ulRBG );
//                           ueSchedVectorFirstSym.emplace_back(sym,ueSchedVector);
//                           resources -= rbgAssignable;
//                       }

//                       firstSym = false;

//                       rntiOrder.emplace_back(GetUe (*schedInfoIt)->m_rnti);
//                     }

//                 assigned.m_sym = GetUe (*schedInfoIt)->m_ulSym;

//                 // NS_LOG_DEBUG ("Assigned " << assigned.m_rbg <<
//                 //              " UL RBG, spanned over " << symAssignable << " SYM, to UE " <<
//                 //              GetUe (*schedInfoIt)->m_rnti);

//                 NS_LOG_DEBUG ("Assigned " << GetUe (*schedInfoIt)->m_ulRBG <<
//                               " UL RBG, spanned over " << uint32_t(GetUe (*schedInfoIt)->m_ulSym) << " SYM, to UE " <<
//                               GetUe (*schedInfoIt)->m_rnti);


//                 AssignedUlResources (*schedInfoIt, FTResources (rbgAssignable, symAssignable),
//                                      assigned);

//                 // Update metrics for the unsuccessful UEs (who did not get any resource in this iteration)
//                 for (auto & ue : ueVector)
//                   {
//                     if (GetUe (ue)->m_rnti != GetUe (*schedInfoIt)->m_rnti)
//                       {
//                         NotAssignedUlResources (ue, FTResources (rbgAssignable, symAssignable),
//                                                 assigned);
//                       }
//                   }

//                 if (assigned.m_rbg == rbgInOneSymbol || assigned.m_rbg == rbgInOneSymbol-1)
//                   {
//                     assigned.m_rbg = 0;
//                     resources = (rbgInOneSymbol*beamSym)-(rbgInOneSymbol*sym);
//                   }

//                 if (resources == 0)
//                   {
//                     nextUE = 0;
//                     initSym = 1;
//                     nextSymbol = 1;

//                     if(m_schType_OFDMA==2 && (ueSchedVectorFirstSym.size()>1 || ueSchedVector.size()>1))
//                       {
//                         GetUe (*schedInfoIt)-> = 0;
//                         GetUe (*schedInfoIt)->m_ulSym = 0;
//                         GetUe (*schedInfoIt)->m_ulRBG = 0;

//                         ueSchedVectorFirstSym.clear();
//                         ueSchedVector.clear();
//                      }


//                   }
//               }
//           }
//   }
//   return symPerBeam;
 }


NrMacSchedulerNs3::BeamSymbolMap
NrMacSchedulerOfdma::AssignULRBG_manager (uint32_t symAvail, const ActiveUeMap &activeUl, const SfnSf& ulSfn) 
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG ("# beams active flows: " << activeUl.size () << ", # sym: " << symAvail);

  GetSecond GetUeVector;
  BeamSymbolMap symPerBeam = GetSymPerBeam (symAvail, activeUl);

  GetFirst GetUe;
  std::vector<UePtrAndBufferReq> ueVector;
  
 
  for (const auto &el : activeUl)
    { 
    for (const auto &ue : GetUeVector(el))
      {
        ueVector.emplace_back(ue);
      }
    //sort ueVector for fair scheduling
    std::sort(ueVector.begin(),ueVector.end(), [](UePtrAndBufferReq a,UePtrAndBufferReq b){return a.first->m_rnti < b.first->m_rnti;} );
    //find starting ue
    uint32_t rotateIndex = 0;
    if(ueVector.size() >1)
    {
      auto sortingIt = ueVector.begin();
      uint16_t schedPriority = 0xFFFF;
      while(sortingIt != ueVector.end())
      {
        if(GetUe(*sortingIt)->m_rnti-schedStartUe >=0)
        {
            if(GetUe(*sortingIt)->m_rnti-schedStartUe < schedPriority)
            {
              schedPriority = GetUe(*sortingIt)->m_rnti-schedStartUe;
              rotateIndex = std::distance(ueVector.begin(),sortingIt);
            }
        }
        ++sortingIt;
      }

    }

  if(lastProcessedSlot.GetFrame() !=  ulSfn.GetFrame() || lastProcessedSlot.GetSubframe() != ulSfn.GetSubframe() ||lastProcessedSlot.GetSlot() != ulSfn.GetSlot())
  {
    //std::cout<<"Resetting Free Ressources"<<std::endl;
    //scheduling of this slot is done. reset stats
    m_manager->resetFreeRessources(ulSfn);
    lastProcessedSlot = ulSfn;
  }
 

    // rotate for round robin scheduling
     std::rotate(ueVector.begin(),ueVector.begin()+rotateIndex,ueVector.end());
    //  std::vector<std::vector<int>> test;
    //     test.resize(50, std::vector<int>());
    //     uint16_t testsize = 40;
    //     for (size_t i = 0; i < test.size (); ++i)
    //     {
    //        test[i].resize (testsize, 0); 
    //     }
    //    test.clear();
    
     auto schedInfoIt = ueVector.begin();
     //std::cout<<"data in buffer "<<uint(schedInfoIt->second)<< std::endl;
      while (schedInfoIt != ueVector.end ())
      {
        if( !GetUe (*schedInfoIt)->transmitMsg3)
        {        
          uint32_t tbs = 0;
          uint32_t rbPacket=0;
          // Cap search to this BWP's RE budget (was hardcoded 51*13 — wrong for true 5 MHz).
          const uint32_t maxRbSym =
              static_cast<uint32_t>(GetBandwidthInRbg()) * m_macSchedSapUser->GetSymbolsPerSlot();
          while(tbs < (schedInfoIt->second) +8 && rbPacket< maxRbSym)//+5 for header extraction TODO: use a better way 
             {
                tbs = m_ulAmc->CalculateTbSize (GetUe (*schedInfoIt)->m_ulMcs,rbPacket);
                rbPacket++;
            }
          //std::cout<<"scheduleData for "<<uint(GetUe (*schedInfoIt)->m_rnti)<<" in " <<ulSfn<<"for Bwp "<< GetBwpId ()<<std::endl;
          //std::cout<<"MCS für IMSI: "<< uint(GetUe (*schedInfoIt)->m_rnti)<<","<<uint((GetUe (*schedInfoIt)->m_ulMcs))<<std::endl;
          RessourceSet* Res = new RessourceSet();
        
          //std::cout<<"Schedule Data with MCS: "<< uint(GetUe (*schedInfoIt)->m_ulMcs)<< "and imi: "<< uint(GetUe (*schedInfoIt)->m_imsi)<<std::endl;
          NrCampaignStats::Inc("sched.ul.ue_slot_candidates");
          if(m_manager->pdcchAvailableInSearchSpace(false,ulSfn,GetUe (*schedInfoIt)->m_rnti, GetBwpId()))
          {
            Res = m_manager->scheduleData( GetBwpId (),GetUe (*schedInfoIt)->m_rnti,rbPacket,LteNrTddSlotType::UL,false,ulSfn,Res);
            if (Res->NumberRB == 0 || Res->NumberSymbols == 0)
            {
              NrCampaignStats::Inc("sched.ul.not_scheduled.no_rb");
            }
          }
          else{
            NrCampaignStats::Inc("sched.ul.not_scheduled.pdcch");
            // if(GetUe (*schedInfoIt)->m_rnti <= 20 )
            // {
            //   std::cout<<"imi: "<< uint(GetUe (*schedInfoIt)->m_imsi)<<" didnt get PDCCH ressources"<<std::endl;
            // }
            Res->NumberRB = 0;
          }
          
          if(Res->NumberRB!=0 && Res->NumberSymbols !=0)
          {
            NrCampaignStats::Inc("sched.ul.grants");
            NrCampaignStats::Inc("sched.ul.grant_rb_sum", Res->NumberRB);
            NrCampaignStats::Inc("sched.ul.grant_sym_sum", Res->NumberSymbols);
            NrCampaignStats::Inc("sched.ul.grant_needed_rbsym_sum", rbPacket);
            // std::cout<<"Got "<<uint(Res->NumberRB)<<slotRessources.size()<<std::endl;
            //UE got data scheduled. Increment the first ue to be scheduled in the next slot and update freeRessources
            schedStartUe = GetUe (*schedInfoIt)->m_rnti +1;
            //reeRessources -= Res->NumberRB;
            //Check for active Searchspaces and remove UEs which are not reachable
            
            if(!m_manager->markViablePdcchRessource(false,ulSfn,GetUe (*schedInfoIt)->m_rnti, GetBwpId()))
            {
                  NrCampaignStats::Inc("sched.ul.pdcch_mark_fail");
                  std::cout<<"Error: Cannot mark pdcch ressources although they should be available."<<std::endl;
            }

            
          }    
          else{
            NS_LOG_DEBUG("Could not Schedule Data for ue with rnti "+ GetUe (*schedInfoIt)->m_rnti);
          }
          delete Res;

          
         
        }
        schedInfoIt++;  
      }
      
    //Right now the scheduler doesnt use a specific scheduling method. The scheduling method should be implemented in the manager and keep track
    // of overlapping BWPs for a fair scheduling. BWPs which are scheduled first can restrict ressources immensly under
    // high load for other BWPs which share ressources.

  } 

 
  //assign Ressources for this slot

  std::vector<std::vector<int>> slotRessources = m_manager->getSlotRessources(GetBwpId(),ulSfn,LteNrTddSlotType::UL);
  
  //update ue paraeters
  int32_t resNum = 0;
  uint64_t numRB;
  //uint8_t rbgAssignable = GetBandwidthInRbg(); //We can assign a full symbol at once
  uint8_t rbgAssignable =1;
  FTResources assigned(0, 0);
  uint8_t beamSym;
  numRB =0;
  uint16_t bw = GetBandwidthInRbg();
  if(bw == 266)
  {
    //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
    //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
    bw = 265;
  }
   if(bw == 213)
      {
        //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
        //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
        bw = 212;
      }
      if(bw == 160)
      {
        //Bw for 100Mhz has 1 more RB than 5x20Mhz which is used for slotResspurces
        //Therefore it has to be adjusted for now to prevent accessing out of bounce indices
        bw = 159;
      }
  std::list<uint16_t> assignedRntiList;
  for( size_t i =0; i< slotRessources.size(); ++i)
  {
    for(size_t rb =0; rb< bw;++rb)
    {
       if(slotRessources[i][rb] > 0 || resNum >0)
       {
          if(slotRessources[i][rb]==resNum&& rb !=0)
          {
            numRB++;
          }
          
          else{
             std::list<uint16_t>::iterator findIter = std::find(assignedRntiList.begin(), assignedRntiList.end(), resNum);

              bool alreadyAssigned = findIter != assignedRntiList.end();
              if(alreadyAssigned)
              {
                resNum =slotRessources[i][rb];
                numRB=1;
                continue;
              }
            uint ulSymbols = 1;
            uint usedRb= numRB;
            if(rb != 0)
            {
             
               while( i+ulSymbols<slotRessources.size()  &&slotRessources[i+ulSymbols][rb-1] > 0 &&slotRessources[i+ulSymbols][rb-1] ==resNum )
              {
                 
                  ++ulSymbols; 
                  numRB +=  usedRb;                              
              }
            }
            else{
              
               while(i+ulSymbols<slotRessources.size() && slotRessources[i+ulSymbols-1][bw-1] >0 && slotRessources[i+ulSymbols-1][bw-1]==resNum)
              {
              
                  ++ulSymbols; 
                  numRB +=  usedRb;                              
              }

            }
            auto assignUlIt = ueVector.begin();
            while(assignUlIt != ueVector.end())
            {
              if(GetUe(*assignUlIt)->m_rnti == resNum)
              {
                  
                  
                  GetUe(*assignUlIt)->m_ulRBG = numRB;
                  GetUe(*assignUlIt)->m_ulSym =ulSymbols; 
                  beamSym = GetUe(*assignUlIt)->m_ulSym;
                  assigned.m_rbg= numRB;
                  assigned.m_sym = beamSym;
                  AssignedUlResources(*assignUlIt, FTResources(rbgAssignable, beamSym), assigned);
                  if( !GetUe (*assignUlIt)->transmitMsg3)
                  {
                    //TODO_Transmission of msg3 doesnt work as intended. This fix is needed for now
                    assignedRntiList.emplace_back(uint(resNum));
                  }
              
                    


                  // Update metrics for the unsuccessfull UEs (who did not get any resource in this iteration)
                for (auto & ue : ueVector)
                {
                  if (GetUe (ue)->m_rnti != GetUe (*assignUlIt)->m_rnti)
                    {
                      NotAssignedUlResources (ue, FTResources (rbgAssignable, beamSym),
                                              assigned);
                    }
                }
                  break;
              }
              {
                assignUlIt++;
              }
            } 
            resNum =slotRessources[i][rb];
            numRB=1;
          }
        }
      }
  }
 
  // NS_LOG_DEBUG ("Assigned " << GetUe (*schedInfoIt)->m_ulRBG <<
  //                             " UL RBG, spanned over " << uint32_t(GetUe (*schedInfoIt)->m_ulSym) << " SYM, to UE " <<
  //                             GetUe (*schedInfoIt)->m_rnti);

  return symPerBeam;
}                   


std::shared_ptr<DciInfoElementTdma>
NrMacSchedulerOfdma::CreateUlDci (PointInFTPlane *spoint,
                                      const std::shared_ptr<NrMacSchedulerUeInfo> &ueInfo,
                                      uint32_t maxSym) const
{
  NS_LOG_FUNCTION (this);

  uint32_t tbs = m_ulAmc->CalculateTbSize (ueInfo->m_ulMcs,
                                           ueInfo->m_ulRBG * GetNumRbPerRbg ());

  // If is less than 7 (3 mac header, 2 rlc header, 2 data), then we can't
  // transmit any new data, so don't create dci.
  // However, to comply with the constraints imposed in the new scheduler (SymOFDMA and RBOFDMA),
  // this condition will not be taken into account.
  /*
  if (tbs < 7)
    {
      NS_LOG_DEBUG ("While creating DCI for UE " << ueInfo->m_rnti <<
                    " assigned " << ueInfo->m_ulRBG << " UL RBG, but TBS < 7");
      return nullptr;
    }*/

  // Calculate the number of RBs to be assigned per symbol
  uint32_t RBGNum = ueInfo->m_ulRBG /ueInfo->m_ulSym ;
  std::vector<uint8_t> rbgBitmask = GetUlNotchedRbgMask ();

  if (rbgBitmask.size () == 0)
    {
      rbgBitmask = std::vector<uint8_t> (GetBandwidthInRbg (), 1);
    }

  NS_ASSERT (rbgBitmask.size () == GetBandwidthInRbg ());

  uint32_t lastRbg = spoint->m_rbg;
  uint32_t assigned = RBGNum;


  // Limit the places in which we can transmit following the starting point
  // and the number of RBG assigned to the UE
  for (uint32_t i = 0; i < GetBandwidthInRbg (); ++i)
    {
      if (i >= spoint->m_rbg && RBGNum > 0 && rbgBitmask[i] == 1)
        {
          RBGNum--;
          lastRbg = i;
        }
      else
        {
          // Set to 0 the position < spoint->m_rbg OR the remaining RBG when
          // we already assigned the number of requested RBG
          rbgBitmask[i] = 0;
        }
    }

  NS_ASSERT_MSG (RBGNum == 0,
                 "If you see this message, it means that the AssignRBG and CreateDci method are unaligned");

  NS_LOG_INFO ("UE " << ueInfo->m_rnti << " assigned RBG from " <<
               static_cast<uint32_t> (spoint->m_rbg) << " to " <<
               static_cast<uint32_t> (spoint->m_rbg + assigned) << " for " <<
               static_cast<uint32_t> (ueInfo->m_ulSym) << " SYM.");

  //Due to MIMO implementation MCS, TB size, ndi, rv, are vectors
  std::vector<uint8_t> ulMcs = {ueInfo->m_ulMcs};
  std::vector<uint32_t> ulTbs = {tbs};
  std::vector<uint8_t> ndi = {1};
  std::vector<uint8_t> rv = {0};

   spoint->m_rbg = lastRbg + 1;

   std::shared_ptr<DciInfoElementTdma> dci = std::make_shared<DciInfoElementTdma>
      (ueInfo->m_rnti, DciInfoElementTdma::UL, spoint->m_sym, (ueInfo->m_ulSym), ulMcs,
       ulTbs, ndi, rv, DciInfoElementTdma::DATA, GetBwpId (), GetTpc());

   // New symbol (symbols are assigned in ascending order)
   if (spoint->m_rbg == GetBandwidthInRbg ())
     {
       spoint->m_sym += ueInfo->m_ulSym;
       spoint->m_rbg = 0;
     }

  dci->m_rbgBitmask = std::move (rbgBitmask);

  std::ostringstream oss;
  for (auto x: dci->m_rbgBitmask)
  {
   oss << std::to_string(x) << " ";
  }
  NS_LOG_INFO ("UE " << ueInfo->m_rnti << " DCI RBG mask: " << oss.str());

  NS_ASSERT (std::count (dci->m_rbgBitmask.begin (), dci->m_rbgBitmask.end (), 0) != GetBandwidthInRbg ());

  return dci;
}

} // namespace ns3
