#include <ns3/integer.h>
#include <ns3/log.h>
#include <ns3/pointer.h>
#include <ns3/uinteger.h>
#include <ns3/nr-mac-scheduler-ressource-manager.h>
#include "nr-campaign-stats.h"

#include <iostream>
#include <fstream>

#include <algorithm>

namespace ns3
{
    
    NS_LOG_COMPONENT_DEFINE("NrMacSchedulerRessourceManager");

    NrMacSchedulerRessourceManager::NrMacSchedulerRessourceManager(std::string pattern, uint8_t numerology , uint16_t simTime,uint32_t initTime, std::string logDir,uint8_t  bwpCount, bool use5MHz, uint16_t rbPerBwp)
    {
        // rbPerBwp: 51 for 20 MHz @ μ=1 (oh=0.08); 12 for true 5 MHz @ μ=1.
        // The last 2 BWPs always span the whole RedCap grid.
        NS_LOG_FUNCTION(this);
        SetPattern(pattern);
        uint64_t simTime_seconds = static_cast<uint64_t>(ceil(simTime+initTime));
        std::cout<<"simTime_seconds"<<simTime_seconds<<std::endl;
        m_numSlots = 10 * pow(2,numerology);//number of slots inside on frame
        m_numSym = 14; 
        m_numerology = numerology;
        m_logDir = logDir;
        m_use5MHz= use5MHz;
        NS_ABORT_MSG_IF(rbPerBwp == 0, "rbPerBwp must be > 0");
        m_rbPerBwp = rbPerBwp;
        std::cout<<"ResManager rbPerBwp="<<m_rbPerBwp<<" use5MHz="<<m_use5MHz<<std::endl;


        m_ressourceWindowSize = 9600; //9600 milliseconds to limit the memory space needed for bigger Simulations. Should be a multiple of 160ms for every PRACH-configuration to be at the same place
        //m_resBufferSize = 400; //400 millieconds additional buffer to avoid outOfBounce errors 
        NS_ABORT_MSG_IF(m_ressourceWindowSize%160 != 0, "m_ressourceWindowSize has to be a multiple of 160ms to ensure correct positioning of control channels");
        
       // m_ressourceWindowElements = (m_ressourceWindowSize) *m_numSlots * m_numSym/10;
        m_ressourceWindowElements = (m_ressourceWindowSize) *m_numSlots * m_numSym/10;


        m_ressourcen.resize(m_ressourceWindowElements, std::vector<int32_t>()); 
        //m_ressourcen.resize((simTime_seconds+0.01) * 100 *m_numSlots * m_numSym, std::vector<int>()); // +0.01 (10ms) for a few more resources in case the scheduler wants 
        m_numBwp = bwpCount;
        m_lastPrachNo =0;
        for (int i = 0; i < bwpCount-2; ++i)
        {
           freeSymbolsMap.insert({i, 13}); //PUCCH symbol subtracted
        }
        


        for (size_t i = 0; i < m_ressourcen.size (); ++i)
        {
           m_ressourcen[i].resize (m_rbPerBwp*(bwpCount-2), RessourceAllocationStatus::FREE);
        }
        reserveSystemInformations(bwpCount);
        Simulator::Schedule(MilliSeconds(m_ressourceWindowSize/2),&NrMacSchedulerRessourceManager::changeRessourceWindow,this);
    }



    void 
    NrMacSchedulerRessourceManager::reserveSystemInformations(uint8_t  bwpCount)
    {
        //https://www.sharetechnote.com/html/5G/5G_FrameStructure.html#SS_PBCH
        NS_LOG_FUNCTION(this);
        //reserve pbch: 30kHz SCS case B for 3GHz < f < 6GHz
        //Ignored pbch on CORESET symbols to prevent because this would corrupt the implented usage of pdcch
        //only first 2 slots integrated
        size_t i =0;
        size_t peridicity = 10; //ms
        size_t k=1;
        while(i < m_ressourcen.size())
        {      

            //int startRb = 51*(bwpCount-2)/2;

            //PBCH entsprechend der Limitierung des CMX eingebracht, um näher an der realen Messung zu sein
            // Cap PBCH footprint to the resource grid.
            // @ 20 MHz (51 RB): keep legacy 22 RB reservation.
            // @ 5 MHz (12 RB): do NOT reserve the whole grid as PBCH or Msg3/data starve
            // (attach collapses). Use a small edge reservation only.
            const uint16_t gridRb = m_rbPerBwp * (bwpCount > 2 ? (bwpCount - 2) : 1);
            uint16_t pbchRb = 0;
            if (gridRb >= 51)
            {
                pbchRb = 22;
            }
            else if (gridRb > 8)
            {
                pbchRb = 4;
            }
            for(uint rb =0; rb<pbchRb; ++rb)
            {
                for(uint j = 2; j<m_numSym;++j )
                {
                    m_ressourcen[i+j][rb] = RessourceAllocationStatus::PBCH;
                }
            }




            // //2x 4 symbols PBCH
            // for(int j = 4; j<12;++j )
            // {
            //     //PBCH in the middle of the bandwitdh for 20 rb

            //     //Use only 12 RB
            //     if(j==4 || j==8)
            //     {
            //         for(int rb = startRb+4; rb< startRb+20-4;++rb)
            //         {
            //             m_ressourcen[i+j][rb] = RessourceAllocationStatus::PBCH;
            //         }
            //     }

            //     //full 20RB
            //     else{
            //         for(int rb = startRb; rb< startRb+20;++rb)
            //         {
            //             m_ressourcen[i+j][rb] = RessourceAllocationStatus::PBCH;
            //         }
            //     }
            //}

             //3 Symbols and 4 symbols PBCH -- 3 Symbols to prevent interfering with CORESET
            // for(int j = 17; j<24;++j )
            // {
            //     //Use only 12 RB
            //     if(j==20)
            //     {
            //         for(int rb = startRb+4; rb< startRb+20-4;++rb)
            //         {
            //             m_ressourcen[i+j][rb] = RessourceAllocationStatus::PBCH;
            //         }
            //     }

            //     else{
            //          //PBCH in the middle of the bandwitdh for 20 rb
            //         for(int rb = startRb; rb< startRb+20;++rb)
            //         {
            //             m_ressourcen[i+j][rb] = RessourceAllocationStatus::PBCH;
            //         }
            //     }
            // }
            i = k * (peridicity * m_numSym*2); //sets i to the start slot for the next PBCH // *2 due to numerology
            k = k+1;
        }
    }

    void 
    NrMacSchedulerRessourceManager::reservePrachRessources(NrPhySapProvider::PrachConfig prachConfig)
    {
        NS_LOG_FUNCTION(this);
        if(!m_prachConfigured)
        {
            size_t i =0; //i zeit, j frequenzen
            size_t slotNumber= 0;
            size_t subframeNumber = 0;
            size_t frameNumber =0;
            size_t symbolNumber =0;
            while(i < m_ressourcen.size())
            {
                symbolNumber = i%m_numSym;
                slotNumber = i/m_numSym; 
                subframeNumber = int(slotNumber/(pow(2,m_numerology)))%10;
                frameNumber = i/(m_numSym*m_numSlots);
                
                 //PRACH occasions
                if(frameNumber%prachConfig.m_nfX ==prachConfig.m_nfY)
                {
                    if ( std::find(prachConfig.m_SfN.begin(), prachConfig.m_SfN.end(), subframeNumber) != prachConfig.m_SfN.end() )
                    {
                        //only applicable for numerology 0 and 1
                        if(prachConfig.m_numPrachSlots==2 || slotNumber%2==0)
                        {
                            if(symbolNumber >= prachConfig.m_symStart && symbolNumber < (prachConfig.m_symStart+prachConfig.m_prachOccInSlot*prachConfig.m_duration))
                            {
                                for(size_t rb = 0; rb <= bwpRessourceMap.at(0).getUpperBorder(); ++rb)
                                {
                                    m_ressourcen[i][rb] = RessourceAllocationStatus::PRACH;
                                }
                            }   
                        }    
                    }
                }
                i = i+1;
            }
            m_prachConfig = prachConfig;
            m_prachConfigured = true;
        }
    }

    void
    NrMacSchedulerRessourceManager::changeRessourceWindow()
    {
        collectResUsage();
        resetRessourceGrid();
       //moveRessourceBufferValues();

        Simulator::Schedule(MilliSeconds(m_ressourceWindowSize/2),&NrMacSchedulerRessourceManager::changeRessourceWindow,this);
    }

    void
    NrMacSchedulerRessourceManager::collectResUsage()
    {
        uint64_t slotNumber =0;
        //add saved tmpRessources from last window to UeStats
        // Short DIT / RRC release can leave stale RNTIs in tmpResMap after m_rntiMap erase.
        std::map<uint32_t, uint64_t>::iterator it;
        for (it = m_ueStats.tmpResMap.begin(); it != m_ueStats.tmpResMap.end(); it++)
        {
            if (m_rntiMap.count(it->first) == 0)
            {
                continue;
            }
            m_ueStats.ueResMap[m_rntiMap.at(it->first)] += it->second;
        }
        m_ueStats.tmpResMap.clear();

        uint64_t i = Simulator::Now().GetMilliSeconds()*m_numSlots/10*m_numSym % m_ressourceWindowElements == 0 ? (m_ressourceWindowElements/2) : 0;
        uint64_t endIndex = i+ m_ressourceWindowElements/2;

        for (; i < endIndex; ++i)
        {
            slotNumber = i/m_numSym; 
            for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
            {
                switch(m_ressourcen[i][rb])
                {
                    case FREE: if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                        m_ueStats.freeRessources++;
                        m_ressourceStats.freeRessources_UL++;
                    }
                    else{
                        //stats.freeRessources_DL++;
                        m_ressourceStats.freeRessources_DL++;
                    }
                        break;
                    case PRACH: m_ressourceStats.PrachRessources++;
                        break;
                    case RESERVED: m_ressourceStats.ControlRessources++;
                        break;
                    case CORESET: m_ressourceStats.PdcchRessources++;
                        break;
                    case SCH_CORESET: m_ressourceStats.PdcchUsed++;
                        break;
                    case PUCCH: m_ressourceStats.PucchRessources++;
                        break;
                    case SCH_MSG3: m_ressourceStats.usedRessources_UL++;
                        break;
                    
                    case PBCH: m_ressourceStats.ControlRessources++;
                        break;
                    default:
                        //no reserved ressources -> schedueled Data
                        if(m_ressourcen[i][rb] > 0) //sanity check
                        {
                            if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                            //m_ueStats.UlUsageArr[m_rntiMap.at(m_ressourcen[i][rb])]++;
                                if(m_rntiMap.count(m_ressourcen[i][rb]) != 0)
                                {
                                    m_ueStats.ueResMap[m_rntiMap.at(m_ressourcen[i][rb])]++;
                                    m_ressourceStats.usedRessources_UL++;
                                }
                                else{
                                    m_ueStats.tmpResMap[m_ressourcen[i][rb]]++;
                                    std::cout<<"Rnti "<< uint(m_ressourcen[i][rb]) << " nicht gefunden."<< std::endl; //abspeichern um die ressourcennutzung richtig zu bestimmen?
                                }
;
                            }
                            else{
                            //stats.usedRessources_DL++;
                            m_ressourceStats.usedRessources_DL++;
                            }
                        }
                }
            }
        }
    }

    void
    NrMacSchedulerRessourceManager::collectFinalResUsage()
    {
        uint64_t slotNumber =0;
        //add saved tmpRessources from last window to UeStats
        // Short DIT / RRC release can leave stale RNTIs in tmpResMap after m_rntiMap erase.
        std::map<uint32_t, uint64_t>::iterator it;
        for (it = m_ueStats.tmpResMap.begin(); it != m_ueStats.tmpResMap.end(); it++)
        {
            if (m_rntiMap.count(it->first) == 0)
            {
                continue;
            }
            m_ueStats.ueResMap[m_rntiMap.at(it->first)] += it->second;
        }
        m_ueStats.tmpResMap.clear();

        uint64_t timeIndex = Simulator::Now().GetMilliSeconds()*m_numSlots/10*m_numSym;
        uint64_t remaining = timeIndex%(m_ressourceWindowElements/2);
        uint64_t startIndex = timeIndex > m_ressourceWindowElements/2 ? m_ressourceWindowElements/2 : 0;
        for (uint i = startIndex; i < remaining; ++i)
        {
            slotNumber = i/m_numSym; 
            for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
            {
                switch(m_ressourcen[i][rb])
                {
                    case FREE: if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                        m_ueStats.freeRessources++;
                        m_ressourceStats.freeRessources_UL++;
                    }
                    else{
                        //stats.freeRessources_DL++;
                        m_ressourceStats.freeRessources_DL++;
                    }
                        break;
                    case PRACH: m_ressourceStats.PrachRessources++;
                        break;
                    case RESERVED: m_ressourceStats.ControlRessources++;
                        break;
                    case CORESET: m_ressourceStats.PdcchRessources++;
                        break;
                    case SCH_CORESET: m_ressourceStats.PdcchUsed++;
                        break;
                    case PUCCH: m_ressourceStats.PucchRessources++;
                        break;
                    case SCH_MSG3: m_ressourceStats.usedRessources_UL++;
                        break;
                    
                    case PBCH: m_ressourceStats.ControlRessources++;
                        break;
                    default:
                        //no reserved ressources -> schedueled Data
                        if(m_ressourcen[i][rb] > 0) //sanity check
                        {
                            if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                            //m_ueStats.UlUsageArr[m_rntiMap.at(m_ressourcen[i][rb])]++;
                                if(m_rntiMap.count(m_ressourcen[i][rb]) != 0)
                                {
                                    m_ueStats.ueResMap[m_rntiMap.at(m_ressourcen[i][rb])]++;
                                    m_ressourceStats.usedRessources_UL++;
                                }
                                else{
                                    m_ueStats.tmpResMap[m_ressourcen[i][rb]]++;
                                    std::cout<<"Rnti "<< uint(m_ressourcen[i][rb]) << " nicht gefunden."<< std::endl; //abspeichern um die ressourcennutzung richtig zu bestimmen?
                                }
;
                            }
                            else{
                            //stats.usedRessources_DL++;
                            m_ressourceStats.usedRessources_DL++;
                            }
                        }
                }
            }
        }
    }

    void
    NrMacSchedulerRessourceManager::resetRessourceGrid()
    {
        
        size_t i =  (Simulator::Now().GetMilliSeconds()*m_numSlots/10*m_numSym +m_ressourceWindowElements/2) %m_ressourceWindowElements;
        
        size_t endIndex = i+ m_ressourceWindowElements/2;
        while(i < endIndex)
        {
                                          
            for(uint16_t rb = 0; rb< m_ressourcen[0].size();rb++)
            {   
                if(m_ressourcen[i][rb]>0 ||m_ressourcen[i][rb] == SCH_MSG3 )
                {
                        m_ressourcen[i][rb] =0;
                }
                else if(m_ressourcen[i][rb] == SCH_CORESET )
                {
                        m_ressourcen[i][rb] = CORESET;
                }
            } 
            ++i;
        }
      
    }

    void
    NrMacSchedulerRessourceManager::moveRessourceBufferValues()
    {
        size_t i =0; 
        size_t bufferOffset = m_ressourceWindowSize *m_numSlots * m_numSym/10;
        while(i < m_resBufferSize  *m_numSlots * m_numSym/10)
        {
            for(uint16_t rb = 0; rb< m_ressourcen[0].size();++rb)
            {   

                if(m_ressourcen[i+bufferOffset][rb]>0 ||m_ressourcen[i+bufferOffset][rb] == SCH_MSG3 )
                {
                    m_ressourcen[i][rb] = m_ressourcen[i+bufferOffset][rb];
                    m_ressourcen[i+bufferOffset][rb] = 0;
                }
                else if(m_ressourcen[i+bufferOffset][rb] == SCH_CORESET )
                {
                    m_ressourcen[i][rb] = SCH_CORESET;
                    m_ressourcen[i+bufferOffset][rb] = CORESET;
                }
            } 
            ++i;
        }


    }

    void 
    NrMacSchedulerRessourceManager::configureBwp(uint16_t bwpIndex, uint16_t bwInRBG, uint8_t coresetSymbols )
    {
         NS_LOG_FUNCTION(this);
         bwpRessourceMap.insert({bwpIndex, BwpBorders(bwpIndex,m_numBwp,bwInRBG)});
         coresetMap.insert({bwpIndex, coresetSymbols});

        size_t i =0; //i zeit, j frequenzen
        size_t slotNumber= 0;
        while(i < m_ressourcen.size())
        {
            slotNumber = i/m_numSym;  
            if(m_pattern[slotNumber%m_pattern.size()] == ns3::DL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F ||m_pattern[slotNumber%m_pattern.size()] == ns3::S)
            {                                  
                for(uint16_t rb = bwpRessourceMap.at(bwpIndex).getLowerBorder(); rb<= bwpRessourceMap.at(bwpIndex).getUpperBorder();rb++)
                {   
                    for(uint8_t symNum = 0; symNum < coresetSymbols; ++symNum )
                    {
                        m_ressourcen[i+symNum][rb] = CORESET;
                    }     
                } 
            }

            if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F )
            {
                for(uint16_t rb = bwpRessourceMap.at(bwpIndex).getLowerBorder(); rb<= bwpRessourceMap.at(bwpIndex).getUpperBorder();rb++)
                {   
                    for(uint8_t symNum = 13; symNum < m_numSym; ++symNum )
                    {
                        m_ressourcen[i+symNum][rb] = PUCCH;
                    }
                }  
            }
            i = i+m_numSym; //get next slot      
        }
    }


    RessourceSet*
    NrMacSchedulerRessourceManager::scheduleData(uint16_t bwpIndex,uint16_t ue,uint32_t numRB, LteNrTddSlotType slotType, bool Msg3, const SfnSf& sfnSf ,RessourceSet* res)
    {   
        

        NS_LOG_FUNCTION(this);
        uint8_t slotsInSF = pow(2,m_numerology);
        uint64_t slotnumber = sfnSf.GetFrame() * slotsInSF * 10 + sfnSf.GetSubframe() * slotsInSF + sfnSf.GetSlot();   
        
        if(Msg3){
                uint8_t delta_delay =  delta_delayVector.at(m_numerology); //extra slot delay for Msg3
                slotnumber=slotnumber +delta_delay +10; 
                while(m_pattern[slotnumber%m_pattern.size()] != LteNrTddSlotType::UL)
                {
                    slotnumber = slotnumber +1;
                }
            }
        //TODO_bad code design
        res = scheduleCompleteSlot(bwpIndex,ue,numRB,slotType,Msg3,res,slotnumber);
        if((res->NumberRB == 0) )
        {   
            res = schedulePartPacket(bwpIndex,ue,numRB,slotType,Msg3,res,slotnumber);
        }

        if(!Msg3 &&bwpIndex<m_numBwp-2)
        {
            if(m_numSym -1 - (res->StartSymbol + res->NumberSymbols) < freeSymbolsMap.at(bwpIndex) )
            {
                //std::cout<<"freeSymbolsMap at "<< uint(bwpIndex) << " reduced to " <<uint(m_numSym -1 - (res->StartSymbol + res->NumberSymbols))<<"from "<<uint(freeSymbolsMap.at(bwpIndex))<<"|"<<sfnSf<<std::endl;
                freeSymbolsMap.at(bwpIndex) = m_numSym -1 - (res->StartSymbol + res->NumberSymbols);
                NS_ASSERT(freeSymbolsMap.at(bwpIndex) >=0&&freeSymbolsMap.at(bwpIndex)<=14);
            }

        }
    

        //std::cout<<slotType<< " ue expected "<< numRB  <<"and got "<< uint(res->NumberSymbols) <<" symbols with "<< res->NumberRB<<" rb ||"<<sfnSf<<std::endl;
        return res;
    }

    RessourceSet* 
    NrMacSchedulerRessourceManager::scheduleCompleteSlot(uint16_t bwpIndex,uint16_t ue,uint32_t numRB, LteNrTddSlotType slotType, bool Msg3,RessourceSet* res,uint64_t slotnumber)
    {
        uint16_t bwInRBG =  bwpRessourceMap.at(bwpIndex).getUpperBorder() - bwpRessourceMap.at(bwpIndex).getLowerBorder() +1;
        //NS_ASSERT(numRB<(m_numSym-coresetMap.at(bwpIndex))*bwInRBG); //we are not able to schedule more rbg
        uint32_t schSymbols =1;
        bool advanced_scheduling = false;
        uint8_t max_symbols = slotType == LteNrTddSlotType::DL ? m_numSym -coresetMap.at(bwpIndex)  : m_numSym-1;
        if(bwpIndex >= uint8_t(m_numBwp-2))
        {
            //bwp for emBB UEs
            advanced_scheduling = true;
            
        }

        uint rbToSchedule;
        if(numRB >bwInRBG)
        {
            if(bwpIndex >= m_numBwp-2) //its an eMBB UE 
            {
                rbToSchedule = bwInRBG;
                schSymbols = ceil(float(numRB)/float(rbToSchedule));
            }
            else{
                
                if(m_use5MHz)
                {
                    uint maxRb = bwInRBG/4;
                    rbToSchedule= maxRb;
                    while(uint(rbToSchedule-1) * max_symbols > numRB && rbToSchedule>1)
                    {
                        rbToSchedule -=1;
                    }
                   
                }
                else{
                    rbToSchedule=bwInRBG;
                }
                

                schSymbols = ceil(float(numRB)/float(rbToSchedule));
                if(schSymbols > max_symbols)
                {
                    schSymbols = max_symbols;
                }
                numRB = schSymbols*rbToSchedule;
            }
        
        }
        else{
            rbToSchedule=numRB;
        }
        uint64_t scheduleCeiling;
        if(Msg3)
        {
            uint8_t slotsInSF = pow(2,m_numerology);
            scheduleCeiling = (slotnumber * m_numSym)+ (slotsInSF*m_numSym*10); //max 10 ms Scheduling
        }
        else{
            scheduleCeiling = (slotnumber * m_numSym)+m_numSym;

        }


        if(advanced_scheduling)
        {   
            std::vector<uint8_t> consideredBwps(m_numBwp-2);
            for(size_t i = 0; i<consideredBwps.size();++i)
            {
                consideredBwps[i]=i;
            }
            findOptimalScheduling(numRB,consideredBwps,res,bwpIndex,slotType);
            if(res->NumberRB>0&&res->NumberSymbols>0)
            {   
                markRessources(res,ue,slotnumber);
                reduceFreeSymbols(res);
                return res;
            }
            
            //std::cout<<"Didnt get any ressources from advanded scheduling. Switching to normal scheduling."<<std::endl;
        }

        //start normal Scheduling
        uint32_t counter = 0;
        //TODO:: scheduleCeiling
        scheduleCeiling =  (slotnumber * m_numSym%m_ressourceWindowElements)+m_numSym;
        for(size_t i = slotnumber * m_numSym%m_ressourceWindowElements; i < scheduleCeiling; i= i +m_numSym , ++slotnumber)
        {
           //start of a new slot
            int8_t usedSymbols=0;
            counter=0;

           if(m_pattern[slotnumber%m_pattern.size()] ==  slotType ||m_pattern[slotnumber%m_pattern.size()] == ns3::F ) 
            {
                uint8_t symNum;
                if(slotType == ns3::DL || slotType==ns3::S || slotType==ns3::F)
                {
                    symNum = coresetMap.at(bwpIndex);
                }
                else{
                    symNum =0;
                }
                for(; symNum <m_numSym; ++symNum)
                {   
                     NS_LOG_DEBUG("slotnumber:"<< slotnumber <<", counter:"<<counter<<", numRB: "<<numRB<<",usedSymbols: "<<usedSymbols);
                    //start of a new symbol
                    if( schSymbols >1){
                        if(counter%bwInRBG==0){
                            ++usedSymbols;
                        }
                        else{
                            //ressources were already occupied
                            //TODO?
                            //counter = 0;   
                        }
                    }
                    else{
                        counter =0;
                    }
                    for(uint16_t rb = bwpRessourceMap.at(bwpIndex).getLowerBorder(); rb<= bwpRessourceMap.at(bwpIndex).getUpperBorder();rb++ )
                    {
                        //NS_LOG_DEBUG(rb);
                        //find free ressources
                        if( m_ressourcen[i+symNum][rb] == 0)
                        {
                            ++counter;
                        }
                        else{
                            //schedule as much rb as possible
                            if( counter> 0 && uint(usedSymbols) == 0)
                            {
                                ++usedSymbols;
                                uint16_t endRB;
                                uint16_t startSym;
                                if(rb == 0)
                                {
                                    // Free RBs wrapped from the previous symbol. symNum==0 must not
                                    // underflow (uint8_t 0-1 -> 255) or we index m_ressourcen OOB.
                                    endRB = bwpRessourceMap.at(bwpIndex).getUpperBorder();
                                    startSym = (symNum == 0) ? 0 : static_cast<uint16_t>(symNum - 1);
                                }
                                else{
                                    endRB=rb-1;
                                    startSym=symNum;
                                }

                                // Guard: counter RBs ending at endRB must stay in-range
                                if (counter == 0 || endRB + 1 < counter)
                                {
                                    counter = 0;
                                    usedSymbols = 0;
                                    continue;
                                }

                                while(startSym + uint(usedSymbols)<m_numSym && usedSymbols*counter < numRB)
                                {
                                    bool SymbolFree = true;
                                    const size_t row = i + startSym + usedSymbols;
                                    if (row >= m_ressourcen.size())
                                    {
                                        SymbolFree = false;
                                    }
                                    else
                                    {
                                        for(uint index = 0; index < counter ; ++index)
                                        {
                                            if(m_ressourcen[row][endRB-index] != 0 )
                                            {
                                                SymbolFree =false;
                                                break;
                                            }
                                        }
                                    }
                                    if(SymbolFree) {usedSymbols++;}
                                    else{
                                        break; //schedule all free ressources. Ressources might be occupied by PBCH
                                    }
                                }
                                //mark ressources
                                for(uint markCounter = 0;markCounter < counter; ++markCounter )
                                {
                                    for( uint8_t i_schedSymbol= 0;i_schedSymbol<usedSymbols; ++i_schedSymbol)
                                    {
                                        const size_t row = i + startSym + i_schedSymbol;
                                        if (row >= m_ressourcen.size())
                                        {
                                            break;
                                        }
                                        m_ressourcen[row][endRB-markCounter] = ue;
                                    }
                                }
                                
                                uint32_t resFramenumber = slotnumber/pow(2,m_numerology)/10;
                                uint8_t resSubframenumber = (int)(slotnumber/pow(2,m_numerology))%10;
                                uint8_t resSlotNumber = slotnumber%(int)(pow(2,m_numerology));

                                std::vector<uint8_t> rbgmask;
                        
                        
                                rbgmask = createRBGmask(bwpIndex,counter,endRB-counter+1,usedSymbols,startSym);
                                res->Framenumber = resFramenumber;
                                res->Subframenumber = resSubframenumber;
                                res->Slotnumber=resSlotNumber;
                                res->StartSymbol = uint8_t(startSym);
                                res->NumberSymbols=static_cast<uint8_t>(usedSymbols);
                                res->StartRB = endRB-counter+1;
                                res->NumberRB = counter;
                                res->rbgmask = rbgmask;
                                return res;

                            }

                            counter=0;
                            usedSymbols=0;

                        }
                        

                        // Rectangular grant completion: when free RBs in this symbol reach
                        // rbToSchedule, extend over symbols until numRB is covered.
                        // Must NOT be gated on m_use5MHz — that flag only sets rbToSchedule to
                        // ~¼ BWP (capability-style). True 5 MHz (use5MHz=false) uses the full
                        // BWP as rbToSchedule and needs this same path (FGCS M3 bugfix).
                        if(counter == rbToSchedule)
                        {
                            //sanity check
                            if((slotType == ns3::DL && symNum == coresetMap.at(bwpIndex ))|| (slotType == ns3::UL && symNum==0))
                            {
                                usedSymbols++;
                                while(uint(symNum) + usedSymbols<m_numSym && usedSymbols*rbToSchedule < numRB)
                                {

                                    bool SymbolFree = true;
                                    
                                    for(uint index = 0; index < rbToSchedule ; ++index)
                                    {
                                        if(m_ressourcen[i+symNum+usedSymbols][rb-index] != 0 )
                                        {
                                            SymbolFree =false;
                                        }

                                    }
                                    if(SymbolFree) {usedSymbols++;}
                                    else{
                                        break; //schedule all free ressources. Ressources might be occupied by PBCH
                                    }
                                }

                                //mark ressources
                                for(uint markCounter = 0;markCounter < rbToSchedule; ++markCounter )
                                {
                                    for( uint8_t i_schedSymbol= 0;i_schedSymbol<usedSymbols; ++i_schedSymbol)
                                    {
                                        m_ressourcen[i+symNum+i_schedSymbol][rb-markCounter] = ue;
                                    }
                                }
                                
                            
                                uint32_t resFramenumber = slotnumber/pow(2,m_numerology)/10;
                                uint8_t resSubframenumber = (int)(slotnumber/pow(2,m_numerology))%10;
                                uint8_t resSlotNumber = slotnumber%(int)(pow(2,m_numerology));

                                std::vector<uint8_t> rbgmask;
                        
                                rbgmask = createRBGmask(bwpIndex,rbToSchedule,rb-rbToSchedule+1,usedSymbols,symNum);
                                res->Framenumber = resFramenumber;
                                res->Subframenumber = resSubframenumber;
                                res->Slotnumber=resSlotNumber;
                                res->StartSymbol = uint8_t(symNum);
                                res->NumberSymbols=static_cast<uint8_t>(usedSymbols);
                                // Must match the RBs actually marked / masked above (not always BWP lower border).
                                res->StartRB = rb - rbToSchedule + 1;
                                res->NumberRB = rbToSchedule;
                                res->rbgmask = rbgmask;
                                return res;
                            }
                            else{
                                    //What to do?
                                }
                        }






                        if(counter == numRB )
                        {
                            //quick fix for a problem with the tracked usedSymbols
                            if(schSymbols > 1)
                            {
                                usedSymbols = numRB/bwInRBG;
                            }
                            NS_LOG_DEBUG("Found free ressources: "<< "slotnumber:"<<slotnumber<<", endSymbol:"<<symNum<<",usedSymbols: "<<usedSymbols+1<<", numRB: "<<numRB);
                            //found ressources
                            if (Msg3)
                                {
                                    msg3SlotMap.insert({slotnumber,ue});
                                }
                            //mark them 
                            if(schSymbols >1)
                            {
                                if(  slotnumber%m_pattern.size() == 7 && usedSymbols == 13)
                                {
                                    usedSymbols = 4;
                                }
                                 for(uint markCounter = 0;markCounter < bwInRBG; ++markCounter )
                                {
                                    for( uint8_t i_schedSymbol= 0;i_schedSymbol<usedSymbols; ++i_schedSymbol)
                                    {
                                        m_ressourcen[i+symNum-i_schedSymbol][bwpRessourceMap.at(bwpIndex).getLowerBorder()+markCounter] = ue;
                                    }
                                }

                            } 
                            else{
                                
                                for(uint markCounter = 0;markCounter < numRB; ++markCounter )
                                {

                                 m_ressourcen[i+symNum][rb-numRB+markCounter+1] = ue;
                                
                                }
                            }
                           
                            uint32_t resFramenumber = slotnumber/pow(2,m_numerology)/10;
                            uint8_t resSubframenumber = (int)(slotnumber/pow(2,m_numerology))%10;
                            uint8_t resSlotNumber = slotnumber%(int)(pow(2,m_numerology));

                            std::vector<uint8_t> rbgmask;
                            if(schSymbols ==1){
                                rbgmask = createRBGmask(bwpIndex,numRB,rb-numRB+1,schSymbols,symNum);
                                NS_ASSERT(rb-numRB+1 >= 0);
                                res->Framenumber = resFramenumber;
                                res->Subframenumber = resSubframenumber;
                                res->Slotnumber=resSlotNumber;
                                res->StartSymbol = symNum;
                                res->NumberSymbols=schSymbols;
                                res->StartRB = uint(rb-numRB+1);
                                res->NumberRB = numRB;
                                res->rbgmask = rbgmask;
                                //res = new RessourceSet{resFramenumber,resSubframenumber,resSlotNumber,symNum,schSymbols,uint(rb-numRB+1),numRB,rbgmask};

                            }
                            else
                            {
                               rbgmask = createRBGmask(bwpIndex,bwInRBG,bwpRessourceMap.at(bwpIndex).getLowerBorder(),usedSymbols,symNum);
                                res->Framenumber = resFramenumber;
                                res->Subframenumber = resSubframenumber;
                                res->Slotnumber=resSlotNumber;
                                res->StartSymbol = uint8_t(symNum-usedSymbols+1);
                                res->NumberSymbols=static_cast<uint8_t>(usedSymbols);
                                res->StartRB = bwpRessourceMap.at(bwpIndex).getLowerBorder();
                                res->NumberRB = bwInRBG;
                                res->rbgmask = rbgmask;

                                
                               //res = new RessourceSet{resFramenumber,resSubframenumber,resSlotNumber,uint8_t(symNum-usedSymbols+1),static_cast<uint8_t>(usedSymbols),0,bwInRBG,rbgmask};
                            }
                            
                            //return ressource set
                            // if(advanced_scheduling)
                            // {
                            //      if(res->NumberRB>0)
                            //     {
                            //         //std::cout<<"numberRb: "<<res->NumberRB<<",NumerSym: "<<uint(res->NumberSymbols)<<std::endl;
                            //         markRessources(res,ue,slotnumber);
                            //         reduceFreeSymbols(res);
                            //         //std::cout<<"UE "<< ue<<" expected "<<uint(numRB)<<" Ressources and got "<< uint(res->NumberRB* res->NumberSymbols)<<" in slotnumber " <<slotnumber<<std::endl;
                            //         return res;
                            //     }
                            // }
                            return res;
                        }

                        
                    }
                    
                } 
            }
             
        }
        return res;

    }

    RessourceSet* 
    NrMacSchedulerRessourceManager::schedulePartPacket(uint16_t bwpIndex,uint16_t ue,uint32_t numRB, LteNrTddSlotType slotType, bool Msg3,RessourceSet* res,uint64_t slotnumber )
    {
        uint16_t bwInRBG =  bwpRessourceMap.at(bwpIndex).getUpperBorder() - bwpRessourceMap.at(bwpIndex).getLowerBorder() +1;
        //NS_ASSERT(numRB<(m_numSym-coresetMap.at(bwpIndex))*bwInRBG); //we are not able to schedule more rbg
        const uint16_t lowerBorder = bwpRessourceMap.at(bwpIndex).getLowerBorder();

        uint8_t schSymbols =1;
        if(numRB >bwInRBG)
        {
            schSymbols = ceil(float(numRB)/float(bwInRBG));
            numRB = schSymbols*bwInRBG;
        }

        // Mark only FREE cells in [startSym, startSym+nSym) × BWP.
        // Shrink NumberRB to the first contiguous FREE run; abort if nothing is FREE.
        // Dense-UL hardening: never overwrite another UE / reserved RB (was fatal at PHY).
        auto tryMarkOnlyFree =
            [&](size_t baseIdx, uint8_t startSym, uint8_t nSym) -> bool {
                if (nSym == 0)
                {
                    return false;
                }
                uint16_t startRB = lowerBorder;
                uint16_t markedRB = 0;
                for (uint16_t markCounter = 0; markCounter < bwInRBG; ++markCounter)
                {
                    bool allFree = true;
                    for (uint8_t s = 0; s < nSym; ++s)
                    {
                        const size_t row = baseIdx + startSym + s;
                        if (row >= m_ressourcen.size() ||
                            m_ressourcen[row][lowerBorder + markCounter] !=
                                RessourceAllocationStatus::FREE)
                        {
                            allFree = false;
                            break;
                        }
                    }
                    if (!allFree)
                    {
                        if (markedRB > 0)
                        {
                            break; // stop at first gap after a FREE run
                        }
                        continue; // skip leading non-FREE RBs
                    }
                    if (markedRB == 0)
                    {
                        startRB = lowerBorder + markCounter;
                    }
                    else if (lowerBorder + markCounter != startRB + markedRB)
                    {
                        break;
                    }
                    for (uint8_t s = 0; s < nSym; ++s)
                    {
                        m_ressourcen[baseIdx + startSym + s][lowerBorder + markCounter] = ue;
                    }
                    ++markedRB;
                }
                if (markedRB == 0)
                {
                    NrCampaignStats::Inc("sched.part_alloc_abort_no_free_rb");
                    NS_LOG_WARN("schedulePartPacket: abort allocation for UE "
                                << ue << " — no FREE RBs in partial rectangle"
                                << " (startSym=" << unsigned(startSym)
                                << ", nSym=" << unsigned(nSym) << ")");
                    return false;
                }
                if (markedRB < bwInRBG)
                {
                    NrCampaignStats::Inc("sched.part_alloc_shrunk");
                    NS_LOG_WARN("schedulePartPacket: shrunk NumberRB from "
                                << bwInRBG << " to " << markedRB << " for UE " << ue
                                << " (skipped non-FREE RBs)");
                }

                const uint32_t resFramenumber = slotnumber / pow(2, m_numerology) / 10;
                const uint8_t resSubframenumber =
                    (int)(slotnumber / pow(2, m_numerology)) % 10;
                const uint8_t resSlotNumber = slotnumber % (int)(pow(2, m_numerology));
                res->Framenumber = resFramenumber;
                res->Subframenumber = resSubframenumber;
                res->Slotnumber = resSlotNumber;
                res->StartSymbol = startSym;
                res->NumberSymbols = nSym;
                res->StartRB = startRB;
                res->NumberRB = markedRB;
                res->rbgmask = createRBGmask(bwpIndex, markedRB, startRB, nSym, startSym);
                return true;
            };

            uint32_t counter = 0;
  
            int8_t usedSymbols=0;
            size_t i =   slotnumber * m_numSym%m_ressourceWindowElements;
           if(m_pattern[slotnumber%m_pattern.size()] ==  slotType ||m_pattern[slotnumber%m_pattern.size()] == ns3::F ) 
            {
                uint8_t symNum;
                if(slotType == ns3::DL || slotType==ns3::S || slotType==ns3::F)
                {
                    symNum = coresetMap.at(bwpIndex);
                }
                else{
                    symNum =0;
                }
                for(; symNum <m_numSym; ++symNum)
                {   
                     NS_LOG_DEBUG("slotnumber:"<< slotnumber <<", counter:"<<counter<<", numRB: "<<numRB<<",usedSymbols: "<<usedSymbols);
                    //start of a new symbol
                    if( schSymbols >1){
                        if(counter%bwInRBG==0){
                            ++usedSymbols;
                        }
                        else{
                            // Previous symbol was only partially free — finalize what we have.
                            // Geometry matches prior StartSymbol/NumberSymbols formulas.
                            if (usedSymbols > 0)
                            {
                                const uint8_t nSym = static_cast<uint8_t>(usedSymbols);
                                const uint8_t startSym =
                                    static_cast<uint8_t>(symNum - usedSymbols + 1);
                                if (tryMarkOnlyFree(i, startSym, nSym))
                                {
                                    return res;
                                }
                            }
                            counter = 0;
                            usedSymbols = 0;
                        }
                    }
                    else{
                        counter =0;
                    }
                    for(uint16_t rb = lowerBorder; rb<= bwpRessourceMap.at(bwpIndex).getUpperBorder();rb++ )
                    {
                        //find free ressources
                        if( m_ressourcen[i+symNum][rb] == RessourceAllocationStatus::FREE)
                        {
                            ++counter;
                        }
                        else{

                            if(rb == lowerBorder && counter >0 && schSymbols >1)
                            {
                                // Hit occupied RB at BWP start after collecting prior symbols.
                                // Finalize prior symbols only (exclude current occupied symbol).
                                if (usedSymbols > 1)
                                {
                                    const uint8_t nSym =
                                        static_cast<uint8_t>(usedSymbols - 1);
                                    const uint8_t startSym =
                                        static_cast<uint8_t>(symNum - (usedSymbols - 1));
                                    if (tryMarkOnlyFree(i, startSym, nSym))
                                    {
                                        return res;
                                    }
                                }
                                counter = 0;
                                usedSymbols = 0;
                            }
                            else
                            {
                                counter=0;
                                usedSymbols=0;
                            }
                        }
                      
                    }
                    
                } 
            }
             
        return res;

    }

    void 
    NrMacSchedulerRessourceManager::findOptimalScheduling(uint32_t numRB, std::vector<uint8_t> consideredBwps, RessourceSet* maxRes, uint16_t bwpIndex, LteNrTddSlotType slotType )
    {
        if(consideredBwps.size()>0)
        {
            //std::cout<<"startMax: "<<uint(maxRes->StartRB)<<","<<uint(maxRes->NumberRB)<<","<<uint(maxRes->StartSymbol)<<","<<uint(maxRes->NumberSymbols)<<std::endl;
            uint8_t maxSym = 0;
            uint8_t maxIndex = 0;

            //get index of bwp with highes number of free symbols
            for(std::vector<uint8_t>::iterator it = consideredBwps.begin(); it != consideredBwps.end(); ++it)
            {
                //std::cout<<"maxSymtest:"<<uint(freeSymbolsMap.at(*it))<<std::endl;
                if(freeSymbolsMap.at(*it)>maxSym)
                {
                    maxSym=freeSymbolsMap.at(*it);
                    maxIndex = std::distance(consideredBwps.begin(),it);
                }
            }

            if(maxSym == 0)
            {
                return;
            }

            //calculate amount of connected ressources. 
            uint8_t index = maxIndex+1;
            uint16_t rb= maxSym*m_rbPerBwp; //length of bwp(51) hardcoded for now 
            while(index<consideredBwps.size() &&freeSymbolsMap.at(consideredBwps[index]) ==maxSym)
            {
                rb +=maxSym*m_rbPerBwp; //length of bwp(51) hardcoded for now 
                ++index;
            }
            //std::cout<<"rb:"<<uint(rb)<<std::endl;
            if(maxRes == nullptr || rb > (maxRes->NumberRB *  maxRes->NumberSymbols) )
            {
                //maxRes
                maxRes->StartRB=bwpRessourceMap.at(consideredBwps[maxIndex]).getLowerBorder();
                maxRes->NumberRB = rb/maxSym; //length of bwp(51) hardcoded for now 
                NS_ASSERT_MSG(m_numSym>=freeSymbolsMap.at(consideredBwps[maxIndex])+uint(1),"Not enough free symbols available. There is something wrong with maxRes.");
                if(slotType == LteNrTddSlotType::DL) 
                {
                    maxRes->StartSymbol=m_numSym -freeSymbolsMap.at(consideredBwps[maxIndex]);
                }
                else{
                    maxRes->StartSymbol=m_numSym -freeSymbolsMap.at(consideredBwps[maxIndex])-1;
                }
                maxRes->NumberSymbols = maxSym;
                maxRes->rbgmask = createRBGmask(bwpIndex, maxRes->NumberRB,maxRes->StartRB,maxRes->NumberSymbols,maxRes->StartSymbol);
                
            }
        
            //use these ressources if they are enough for this transmission
            if(maxRes->NumberRB *  maxRes->NumberSymbols >= numRB)
            {
                //only use as many symbols as needed
                while(uint(maxRes->NumberRB *  (maxRes->NumberSymbols-1))> numRB)
                {
                    --maxRes->NumberSymbols;
                }

                if(maxRes->NumberSymbols ==1)
                {
                    maxRes->NumberRB = numRB;
                    maxRes->rbgmask = createRBGmask(bwpIndex, maxRes->NumberRB,maxRes->StartRB,maxRes->NumberSymbols,maxRes->StartSymbol);
                }
                
            }
            else{
                std::vector<uint8_t> consideredBwpsLeft(consideredBwps.begin(),consideredBwps.begin()+maxIndex);
                if(consideredBwps.begin()+maxIndex !=consideredBwps.end() )
                {
                    std::vector<uint8_t> consideredBwpsRight(consideredBwps.begin()+maxIndex+1,consideredBwps.end());
                    findOptimalScheduling(numRB,consideredBwpsRight,maxRes, bwpIndex,slotType);
                }

                    
                if(maxRes->NumberRB *  maxRes->NumberSymbols < numRB)
                {
                    findOptimalScheduling(numRB,consideredBwpsLeft,maxRes, bwpIndex,slotType);
                }
            }    
        }
    }

    void 
    NrMacSchedulerRessourceManager::markRessources(RessourceSet* res, uint16_t ue, uint64_t slotNumber)
    {
        uint64_t slotIndex = slotNumber*m_numSym%m_ressourceWindowElements;
        for(uint8_t sym = res->StartSymbol; sym <res->NumberSymbols+res->StartSymbol; ++sym)
        {
            for(uint16_t rb = res->StartRB ;rb < res->StartRB+res->NumberRB; ++rb )
            {

                if(m_ressourcen[slotIndex+sym][rb] == 0)
                {
                    m_ressourcen[slotIndex+sym][rb] = ue;
                }
                else{
                    std::cout<<"Something is wrong with the advanced scheduling"<<std::endl;
                    std::cout<<"slotNumber: "<<slotNumber<<std::endl;
                    std::cout<<"sym: "<<uint(sym)<<std::endl;
                    std::cout<<"rb: "<<uint(rb)<<std::endl;
                    std::cout<<"inSlot: "<<uint(m_ressourcen[slotIndex+sym][rb])<<std::endl;
                    //std::cout<<m_ressourcen[slotIndex+sym][rb]<<std::endl;
                }
            }


        }
    }

    void
    NrMacSchedulerRessourceManager::MarkHarqRetxOccupancy(uint16_t bwpIndex,
                                                          const SfnSf& sfn,
                                                          uint8_t symStart,
                                                          uint8_t numSym,
                                                          const std::vector<uint8_t>& rbgBitmask,
                                                          uint16_t rnti)
    {
        NS_LOG_FUNCTION(this);
        if (bwpRessourceMap.find(bwpIndex) == bwpRessourceMap.end())
        {
            return;
        }
        const uint16_t lowerBorder = bwpRessourceMap.at(bwpIndex).getLowerBorder();
        const uint8_t slotsInSf = static_cast<uint8_t>(1u << m_numerology);
        const uint64_t slotNumber =
            sfn.GetFrame() * slotsInSf * 10 + sfn.GetSubframe() * slotsInSf + sfn.GetSlot();
        const uint64_t slotIndex = (slotNumber * m_numSym) % m_ressourceWindowElements;

        for (size_t i = 0; i < rbgBitmask.size(); ++i)
        {
            if (rbgBitmask[i] == 0)
            {
                continue;
            }
            const uint16_t rb = static_cast<uint16_t>(lowerBorder + i);
            for (uint8_t sym = 0; sym < numSym; ++sym)
            {
                const uint8_t absSym = static_cast<uint8_t>(symStart + sym);
                if (absSym >= m_numSym)
                {
                    break;
                }
                const size_t row = slotIndex + absSym;
                if (row >= m_ressourcen.size() || rb >= m_ressourcen[row].size())
                {
                    continue;
                }
                // Occupy free cells only; CORESET / already-used cells stay as-is.
                if (m_ressourcen[row][rb] == 0)
                {
                    m_ressourcen[row][rb] = rnti;
                }
            }
        }
    }

    void
    NrMacSchedulerRessourceManager::reduceFreeSymbols(RessourceSet* res)
    {
        uint8_t startBwp = floor(res->StartRB/m_rbPerBwp);
        uint8_t numBwp  = ceil(float(res->NumberRB)/m_rbPerBwp);
        for(uint8_t index = 0; index < numBwp; ++index)
        {
            freeSymbolsMap.at(startBwp+index)-=res->NumberSymbols;
            NS_ASSERT(freeSymbolsMap.at(startBwp+index) >=0&&freeSymbolsMap.at(startBwp+index)<=14);
        }
    }

    void
    NrMacSchedulerRessourceManager::resetFreeRessources(SfnSf sf)
    {
        //TODO?
       

        uint64_t slotnumber = sf.GetFrame() * sf.GetSlotPerSubframe() * sf.GetSubframesPerFrame() +sf.GetSubframe() * sf.GetSlotPerSubframe()+ sf.GetSlot();

        if(m_pattern[slotnumber%m_pattern.size()] == ns3::DL ||m_pattern[slotnumber%m_pattern.size()] == ns3::F ||m_pattern[slotnumber%m_pattern.size()] == ns3::S)
        {
            for(uint index= 0; index<freeSymbolsMap.size();++index )
            {
                freeSymbolsMap.at(index)=m_numSym-coresetMap.at(index);
                uint symIndex = coresetMap.at(index);
                while(symIndex<uint(m_numSym) && m_ressourcen[(slotnumber*m_numSym+symIndex)%m_ressourceWindowElements][m_rbPerBwp*index] != 0 )
                {
                    --freeSymbolsMap.at(index);
                    ++symIndex;
                }
            }


        }
        else{
            for(uint index= 0; index<freeSymbolsMap.size();++index )
            {
                freeSymbolsMap.at(index)=13;
                uint symIndex = 0;
                while(symIndex<uint(m_numSym-1) && m_ressourcen[(slotnumber*m_numSym+symIndex)%m_ressourceWindowElements][m_rbPerBwp*index] != 0 )
                {
                    --freeSymbolsMap.at(index);
                    ++symIndex;
                }
            }

        }
      
    }


    std::vector<uint8_t>
    NrMacSchedulerRessourceManager::createRBGmask(uint8_t bwpIndex, uint16_t NumberRB,  uint16_t StartRB, uint8_t NumberSymbols, uint8_t StartSymbol)
    {
        NS_LOG_FUNCTION(this);

        uint16_t upperBorder = bwpRessourceMap.at(bwpIndex).getUpperBorder();
        uint16_t lowerBorder = bwpRessourceMap.at(bwpIndex).getLowerBorder();
        const uint16_t bwInRBG = upperBorder - lowerBorder + 1;

        // Clamp instead of fatal: true 5 MHz can see oversized NumberRB requests
        // when AMC still asks for many RBs; keep the grant inside the BWP.
        if (StartRB < lowerBorder)
        {
            StartRB = lowerBorder;
        }
        if (StartRB > upperBorder)
        {
            StartRB = lowerBorder;
            NumberRB = 0;
        }
        if (NumberRB > 0 && StartRB + NumberRB - 1 > upperBorder)
        {
            NumberRB = static_cast<uint16_t>(upperBorder - StartRB + 1);
            NrCampaignStats::SoftFail("SCHED_RBG_MASK_CLAMP@gnb");
            NS_LOG_WARN("createRBGmask: clamped NumberRB to " << NumberRB
                        << " (bwp=" << unsigned(bwpIndex) << " bw=" << bwInRBG << ")");
        }

        std::vector<uint8_t> rbgBitmask = std::vector<uint8_t> (bwInRBG, 0);

         for (int32_t i = StartRB-lowerBorder; i < StartRB-lowerBorder+NumberRB; ++i)
        {
            if (i >= 0 && static_cast<uint16_t>(i) < bwInRBG)
            {
                rbgBitmask[i] = 1;
            }
        }

        return rbgBitmask;

    }

    std::vector<std::vector<int>>
    NrMacSchedulerRessourceManager::getSlotRessources(uint8_t bwpIndex, const SfnSf sfn, LteNrTddSlotType type )
    {
        NS_LOG_FUNCTION(this);
        std::vector<std::vector<int>> slotRes;
        slotRes.resize(m_numSym, std::vector<int>());
        uint16_t rbSize = bwpRessourceMap.at(bwpIndex).getUpperBorder() -  bwpRessourceMap.at(bwpIndex).getLowerBorder() +1;
        for (size_t i = 0; i < slotRes.size (); ++i)
        {
           slotRes[i].resize (rbSize, 0); 
        }
        uint8_t slotsInSf = pow(2,m_numerology); 
        //uint64_t index = (sfn.GetFrame()*slotsInSf*10 + sfn.GetSubframe()* slotsInSf + sfn.GetSlot()) *(m_numSym)%m_ressourceWindowElements;
        uint64_t index = ((sfn.GetFrame()*slotsInSf*10 + sfn.GetSubframe()* slotsInSf + sfn.GetSlot() )* m_numSym)%m_ressourceWindowElements;
        // uint64_t index = Simulator::Now().GetMilliSeconds()%(m_ressourceWindowSize+m_resBufferSize) < m_ressourceWindowSize ?  
        //     (sfn.GetFrame()*slotsInSf*10 + sfn.GetSubframe()* slotsInSf + sfn.GetSlot()) * m_numSym%m_ressourceWindowElements :  
        //     (sfn.GetFrame()*slotsInSf*10 + sfn.GetSubframe()* slotsInSf + sfn.GetSlot()) * m_numSym%m_ressourceWindowElements+m_ressourceWindowElements ;
        for(size_t i = 0; i < slotRes.size (); ++i)
        {
            for(size_t rb = 0; rb <rbSize; ++rb)
            {
                 slotRes[i][rb] = m_ressourcen[index+i][bwpRessourceMap.at(bwpIndex).getLowerBorder()+rb];
            }
        }
        
        //std::cout<<"got slot ressources"<<std::endl;
        return slotRes;
    }


    std::vector<std::shared_ptr<DciInfoElementTdma>>
    NrMacSchedulerRessourceManager::createDCI (std::vector<std::shared_ptr<ns3::NrMacSchedulerUeInfo>> &ueInfoVec,uint8_t bwpIndex, const SfnSf& sfn, Ptr<NrAmc> m_Amc, LteNrTddSlotType type)
    {
        std::list<uint16_t> scheduledRntiList;
        NS_LOG_FUNCTION(this);
        std::vector<std::shared_ptr<DciInfoElementTdma>> dciVector;
        std::shared_ptr<DciInfoElementTdma> dci;
        std::shared_ptr<ns3::NrMacSchedulerUeInfo> ueInfo;
        uint16_t rbSize = bwpRessourceMap.at(bwpIndex).getUpperBorder() -  bwpRessourceMap.at(bwpIndex).getLowerBorder() +1;
        uint8_t slotsInSf = pow(2,m_numerology);
    

        uint64_t index = (sfn.GetFrame()*slotsInSf*10 + sfn.GetSubframe()* slotsInSf + sfn.GetSlot()) *(m_numSym)%m_ressourceWindowElements; 

        // if(Simulator::Now().GetMilliSeconds()%(m_ressourceWindowSize+m_resBufferSize) >  m_ressourceWindowSize )
        // {
        //     index = index+m_ressourceWindowElements;
        //     m_ressourceWindowElements TODO::
        // } 
        int16_t ue = 0;
        size_t bwpLowerBorder = bwpRessourceMap.at(bwpIndex).getLowerBorder(); 
        uint8_t dciSym =1;
        uint16_t startRb =0;
        uint16_t numRB = 0;
        for(uint8_t symbol = 0; symbol < m_numSym; ++symbol)
        {
            for(size_t rb = 0; rb <rbSize; ++rb)
            {
                if((m_ressourcen[index+symbol][bwpLowerBorder+rb] >0 || ue >0)  )
                {
                    std::list<uint16_t>::iterator findIter = std::find(scheduledRntiList.begin(), scheduledRntiList.end(), ue);

                    bool alreadySchedueled = findIter != scheduledRntiList.end();
                    if(m_ressourcen[index+symbol][bwpLowerBorder+rb] == ue&& !alreadySchedueled)
                    {
                        ++numRB;
                        if(uint16_t(rb) == rbSize-1)
                        {
                            //end of bandwitdh. Create DCI
                            bool foundUe = false;
                            for(auto ueElement: ueInfoVec)
                            {
                                if(ueElement->m_rnti == ue)
                                {
                                    ueInfo= ueElement;
                                    foundUe = true;
                                    break;
                                }
                            }
                            NS_LOG_DEBUG("UE "<<ue<<" found:"<<foundUe);

                            dciSym = 1;
                            // count symbols for DCI — require full rectangle, not only the edge RB
                            while (symbol + dciSym < m_numSym)
                            {
                                bool fullRow = true;
                                for (uint16_t iRB = 0; iRB < numRB; ++iRB)
                                {
                                    if (m_ressourcen[index + symbol + dciSym][startRb + iRB] != ue)
                                    {
                                        fullRow = false;
                                        break;
                                    }
                                }
                                if (!fullRow)
                                {
                                    break;
                                }
                                ++dciSym;
                            }

                            if(foundUe)
                            {
                                findIter = std::find(scheduledRntiList.begin(), scheduledRntiList.end(), ue);

                                if(findIter == scheduledRntiList.end())
                                {
                                    scheduledRntiList.emplace_back(ue);
                                }
                                else{
                                    //if(bwpIndex == 5 || bwpIndex == 6)
                                    if(bwpIndex >= m_numBwp-2)
                                    {
                                        //TODO_! bug in scheduling behaviour. When a ue transmits on a bwp != 5 and doesnt use all of its data, it is still listed 
                                        // as active for bwp 5 scheduling and the scheduler tried to create a dci for every symbol. Rewriting of the sceduling process is needed.
                                        // A single dci is still getting created but it shouldnt affect the ue since its on a different bwp.
                                        numRB =0;
                                        continue;
                                        
                                    }
                                    NS_FATAL_ERROR("Scheduled RNTI "<<ue<<" twice in a slot.");
                                }
                                dci = createSingleDCI(ue,symbol,dciSym,startRb,numRB,bwpIndex, m_Amc ,ueInfo, type);
                                //std::cout<<"DcI tbsize: "<<dci->m_tbSize[0] << std::endl;
                               // std::cout<<uint(dci->m_numSym)<<std::endl;
                                //std::cout<<uint(dci->m_symStart)<<std::endl;
                                if(ueInfo->transmitMsg3)
                                {
                                    // Mark only this UE's Msg3 rectangle (was wiping 0..rb and corrupting neighbors).
                                    for (uint8_t iSym = 0; iSym < dciSym; ++iSym)
                                    {
                                        for (uint16_t iRB = 0; iRB < numRB; ++iRB)
                                        {
                                            m_ressourcen[index + symbol + iSym][startRb + iRB] =
                                                RessourceAllocationStatus::SCH_MSG3;
                                        }
                                    }
                                }
                                dciVector.emplace_back(dci);
                                dciSym =1;
                            }
                        }
                    }
                    else{

                        //create DCI
                        if(numRB >0 && ue>0 && !alreadySchedueled)
                        {
                            bool foundUe = false;
                            for(auto ueElement: ueInfoVec)
                            {
                                if(ueElement->m_rnti == ue)
                                {
                                    ueInfo= ueElement;
                                    foundUe = true;
                                    break;
                                }
                            }
                            NS_LOG_DEBUG("UE "<<ue<<" found:"<<foundUe);

                            dciSym = 1;
                            //count symbols for DCI — require full rectangle, not only the edge RB
                            while (symbol + dciSym < m_numSym)
                            {
                                bool fullRow = true;
                                for (uint16_t iRB = 0; iRB < numRB; ++iRB)
                                {
                                    if (m_ressourcen[index + symbol + dciSym][startRb + iRB] != ue)
                                    {
                                        fullRow = false;
                                        break;
                                    }
                                }
                                if (!fullRow)
                                {
                                    break;
                                }
                                ++dciSym;
                            }

                            if(foundUe)
                            {
                                std::list<uint16_t>::iterator findIter = std::find(scheduledRntiList.begin(), scheduledRntiList.end(), ue);

                                if(findIter == scheduledRntiList.end())
                                {
                                    scheduledRntiList.emplace_back(ue);
                                }
                                else{
                                    if(bwpIndex >= m_numBwp-2)
                                    //if(bwpIndex == 5 || bwpIndex == 6)
                                    {
                                        //TODO_! bug in scheduling behaviour. When a ue transmits on a bwp != 5 and doesnt use all of its data, it is still listed 
                                        // as active for bwp 5 scheduling and the scheduler tried to create a dci for every symbol. Rewriting of the sceduling process is needed.
                                        // A single dci is still getting created but it shouldnt affect the ue since its on a different bwp.
                                        numRB =0;
                                        continue;
                                        
                                    }
                                    NS_FATAL_ERROR("Scheduled RNTI "<<ue<<" twice in a slot.");
                                }
                                dci = createSingleDCI(ue,symbol,dciSym,startRb,numRB,bwpIndex, m_Amc ,ueInfo, type);
                                // std::cout<<"DcI tbsize: "<<dci->m_tbSize[0] << std::endl;
                                // std::cout<<uint(dci->m_numSym)<<std::endl;
                                if(ueInfo->transmitMsg3)
                                {
                                    for (uint8_t iSym = 0; iSym < dciSym; ++iSym)
                                    {
                                        for (uint16_t iRB = 0; iRB < numRB; ++iRB)
                                        {
                                            m_ressourcen[index + symbol + iSym][startRb + iRB] =
                                                RessourceAllocationStatus::SCH_MSG3;
                                        }
                                    }
                                }
                                dciVector.emplace_back(dci);
                                dciSym =1;
                                
                            }

                        }
                        ue = m_ressourcen[index+symbol][bwpLowerBorder+rb];
                        startRb = bwpLowerBorder+rb;
                        numRB =1;
                    }
            
                }
            }
        }

        // Dense-UL hardening: per-symbol rbgBitmasks across DCIs must be disjoint.
        // Drop later conflicting DCIs instead of fatal PHY overlap.
        {
            std::vector<std::shared_ptr<DciInfoElementTdma>> filtered;
            filtered.reserve(dciVector.size());
            for (const auto& cand : dciVector)
            {
                bool overlap = false;
                for (uint8_t s = 0; s < cand->m_numSym && !overlap; ++s)
                {
                    const uint8_t sym = cand->m_symStart + s;
                    for (size_t r = 0; r < cand->m_rbgBitmask.size() && !overlap; ++r)
                    {
                        if (cand->m_rbgBitmask[r] == 0)
                        {
                            continue;
                        }
                        for (const auto& kept : filtered)
                        {
                            if (sym < kept->m_symStart ||
                                sym >= kept->m_symStart + kept->m_numSym)
                            {
                                continue;
                            }
                            if (r < kept->m_rbgBitmask.size() && kept->m_rbgBitmask[r] != 0)
                            {
                                overlap = true;
                                break;
                            }
                        }
                    }
                }
                if (overlap)
                {
                    NrCampaignStats::SoftFail("SCHED_DCI_OVERLAP_DROPPED@gnb");
                    NS_LOG_WARN("createDCI: dropping overlapping DCI for RNTI "
                                << cand->m_rnti << " (rbgBitmask conflict with earlier DCI"
                                << " in same slot, bwp=" << unsigned(bwpIndex) << ")");
                    continue;
                }
                filtered.push_back(cand);
            }
            dciVector.swap(filtered);
        }

        return dciVector;
    }




    std::shared_ptr<DciInfoElementTdma> 
    NrMacSchedulerRessourceManager::createSingleDCI(uint16_t rnti,uint8_t StartSymbol, uint8_t symNum,size_t startRb,int NumRB,uint16_t bwpIndex,Ptr<NrAmc> m_Amc,std::shared_ptr<ns3::NrMacSchedulerUeInfo> ueInfo, LteNrTddSlotType type)
    {
        NS_LOG_FUNCTION(this);
        uint32_t tbs;
        std::vector<uint8_t> ndi;
        std::vector<uint8_t> rv;
        std::vector<uint8_t> mcs;
        uint8_t harq =0;
        uint8_t tpc =1;
        std::vector<uint32_t> m_tbs;
        std::vector<uint8_t> rbgmask;
        //std::cout<<"rnti:"<<rnti<<",NumRb: "<<NumRB<<",startRb:"<<startRb<<",symNum:"<<uint(symNum)<<",StartSymbol:"<<uint(StartSymbol)<<std::endl;
        NS_ASSERT(StartSymbol+symNum<=14);

        rbgmask = createRBGmask(bwpIndex,NumRB,startRb,symNum,StartSymbol);

        DciInfoElementTdma::DciFormat dciFormat;
        if ( type == UL)
        {
            dciFormat =  DciInfoElementTdma::DciFormat::UL;
            tbs = m_Amc->CalculateTbSize (ueInfo->m_ulMcs,
                                           ueInfo->m_ulRBG * 1); //TODO 1 -> GetNumRbPerRbg()
            mcs = {ueInfo->m_ulMcs};
            ndi = {1};
            rv = {0};
             m_tbs = {tbs};
        }
        else 
        {
            dciFormat =  DciInfoElementTdma::DciFormat::DL;
            tbs=m_Amc->CalculateTbSize (ueInfo->m_dlMcs.at(0),
                                           (ueInfo->m_dlRBG)* 1);
                                                            
            for( uint tbNum =0 ; tbNum < ueInfo->m_dlTbSize.size() ; ++tbNum )
            {
                mcs.push_back(ueInfo->m_dlMcs.at(0));
                ndi.push_back(1);
                rv.push_back(0);
                m_tbs.push_back(tbs);   
            }
        }
    
    
        std::shared_ptr<DciInfoElementTdma> dci = std::make_shared<DciInfoElementTdma>(rnti,
            dciFormat,
            StartSymbol,
            symNum,
            mcs,
            m_tbs,
            ndi,
            rv,
            DciInfoElementTdma::VarTtiType::DATA,
            bwpIndex,
            harq,
            rbgmask,tpc);
            
        return dci;
    }


    //function copied from NrUePhy::SetPattern
    void
    NrMacSchedulerRessourceManager::SetPattern(const std::string& pattern)
    {
        NS_LOG_FUNCTION(this);

        static std::unordered_map<std::string, LteNrTddSlotType> lookupTable = {
            {"DL", LteNrTddSlotType::DL},
            {"UL", LteNrTddSlotType::UL},
            {"S", LteNrTddSlotType::S},
            {"F", LteNrTddSlotType::F},
        };

        std::vector<LteNrTddSlotType> vector;
        std::stringstream ss(pattern);
        std::string token;
        std::vector<std::string> extracted;

        while (std::getline(ss, token, '|'))
        {
            extracted.push_back(token);
        }

        vector.reserve(extracted.size());
        for (const auto& v : extracted)
        {
            vector.push_back(lookupTable[v]);
        }

        m_pattern = vector;
    }

    uint16_t
    NrMacSchedulerRessourceManager::countFreeCce(uint64_t slotnumber, uint8_t bwpID)
    {
        const uint64_t indexSlot = slotnumber * m_numSym % m_ressourceWindowElements;
        const uint8_t rbPerCce = 6 / coresetMap.at(bwpID);
        uint16_t freeCells = 0;
        for (uint i = bwpRessourceMap.at(bwpID).getLowerBorder(); i < bwpRessourceMap.at(bwpID).getUpperBorder(); ++i)
        {
            if (m_ressourcen[indexSlot][i] == RessourceAllocationStatus::CORESET)
            {
                ++freeCells;
            }
        }
        return rbPerCce > 0 ? freeCells / rbPerCce : 0;
    }

    bool
    NrMacSchedulerRessourceManager::checkPdcchUsage(bool reserve ,uint64_t slotnumber,uint8_t bwpID, uint16_t keepFree)
    {
        // IoTJ R1: DL CCE reservation. An UL DCI is refused if taking a CCE would leave
        // keepFree or fewer CCEs for DL assignments in this CORESET occasion.
        if (keepFree > 0 && countFreeCce(slotnumber, bwpID) <= keepFree)
        {
            if (!reserve)
            {
                NrCampaignStats::Inc("sched.pdcch_dl_reserve_refusals");
            }
            return false;
        }
        /* 
            This function searched free PDCCH-ressources for a given slot and bwp. If the flag reserve is set, then a set of ressources are getting marked as used. Marked ressources cannot be used by other devices and therefore the amount 
            of possible DCI transmissions is limited per slot. The real transmission uses TDMA with unlimited ressources. 
        */
        
        //uint64_t indexSlot = Simulator::Now().GetMilliSeconds()%(m_ressourceWindowSize+m_resBufferSize) < m_ressourceWindowSize ?  slotnumber * m_numSym%m_ressourceWindowElements :  slotnumber * m_numSym%(m_ressourceWindowSize*m_numSlots * m_numSym/10) ;
        uint64_t indexSlot =  slotnumber * m_numSym%m_ressourceWindowElements;

        uint8_t numRB = 6/coresetMap.at(bwpID);
        NS_ASSERT(numRB >0 && numRB <=6);

        for(uint i = bwpRessourceMap.at(bwpID).getLowerBorder(); i<bwpRessourceMap.at(bwpID).getUpperBorder();++i )
        {
            if(m_ressourcen[indexSlot][i]== RessourceAllocationStatus::CORESET)
            {
                //we know that the whole CCE of the Coreset is usable from the implementation
                //mark used Coreset ressource
                if(!reserve)
                {
                    return true;
                }
                NrCampaignStats::Inc("sched.dci_cce_reserved");
                for(uint8_t rb =0; rb<numRB; ++rb )
                {
                    for(uint8_t symNum =0; symNum <coresetMap.at(bwpID);++symNum)
                    {

                        m_ressourcen[indexSlot+symNum][i+rb] = SCH_CORESET;
                    }

                }
                return true;
                
            }
        }
        //std::cout<<"all PDCCH ressources occupied"<<std::endl;
        return false; //all ressources already used
    }

    

    bool
    NrMacSchedulerRessourceManager::markViablePdcchRessource(bool Msg3, SfnSf sfnsf,uint16_t rnti, uint8_t bwpID)
    {

        /* 
            This function is similiar to 'pdcchAvailableInSearchSpace'. The difference is that this one marks ressources by setting the 'reserve' flag for reserve 'checkPdcchUsage'
        */ 
        const uint64_t tgtSlotM = sfnsf.GetFrame() * sfnsf.GetSlotPerSubframe() * sfnsf.GetSubframesPerFrame() + sfnsf.GetSubframe() * sfnsf.GetSlotPerSubframe() + sfnsf.GetSlot();
        const uint16_t keepFreeM = (!Msg3 && m_pattern[tgtSlotM % m_pattern.size()] == ns3::UL) ? m_dlCceReserve : 0;
        uint16_t slotsUntilTransmission = sfnsf.GetFrame() * sfnsf.GetSlotPerSubframe() * sfnsf.GetSubframesPerFrame() +sfnsf.GetSubframe() * sfnsf.GetSlotPerSubframe()+ sfnsf.GetSlot()- Simulator::Now ().GetMicroSeconds()* pow(2,m_numerology)/1000;
        
        if(m_searchSpaces.find(rnti) != m_searchSpaces.end())
        {
            SearchSpaceSet searchSpace = m_searchSpaces.at(rnti);
            for(uint i = 0; i< slotsUntilTransmission; ++i)
            {
                SfnSf dciSlot = sfnsf;
                dciSlot.Subtract(i);
                uint64_t slotnumber= dciSlot.GetFrame() * dciSlot.GetSlotPerSubframe() * dciSlot.GetSubframesPerFrame() +dciSlot.GetSubframe() * dciSlot.GetSlotPerSubframe()+ dciSlot.GetSlot();
                if( (slotnumber- searchSpace.location.offset)%searchSpace.location.slotPeriodicity <= uint(searchSpace.duration-1)) //Nr-in-bullets 3.5.2, with adjusted duration
                {
                    //a possible DL-slot in the seach space is found
                    if(m_pattern[slotnumber%m_pattern.size()] == ns3::DL ||m_pattern[slotnumber%m_pattern.size()] == ns3::F ||m_pattern[slotnumber%m_pattern.size()] == ns3::S)
                    {
                    
                        if(checkPdcchUsage(true,slotnumber,bwpID,keepFreeM))
                        {
                            return true; 
                        }
            
                        //std::cout<<"Pdcch already fully used: "<<  std::endl;
                        
                    }
                }
            }
            return false; 

        }
        else{
            return true; //no searchspace is set. Common Search Space not implemented 
        }
    }

    bool
    NrMacSchedulerRessourceManager::pdcchAvailableInSearchSpace(bool Msg3, SfnSf sfnsf,uint16_t rnti, uint8_t bwpID, bool countStats)
    {
        /*This function checks the possibility to schedule data to the ue.
        First it is checked ehether the ue is already scheduled in the same slot. After that a possible DL slot for scheduling is checked with the configured searchspace. 
        This function returns true if a DL-slot has free PDCCH-capaties. Note: this function does not reserve them. This is dont after scheduling
        */
        const uint64_t tgtSlot = sfnsf.GetFrame() * sfnsf.GetSlotPerSubframe() * sfnsf.GetSubframesPerFrame() + sfnsf.GetSubframe() * sfnsf.GetSlotPerSubframe() + sfnsf.GetSlot();
        const bool tgtIsUl = m_pattern[tgtSlot % m_pattern.size()] == ns3::UL;
        const std::string pdcchKey = std::string("sched.pdcch_check.") + (tgtIsUl ? "ul." : "dl.");
        const uint16_t keepFree = (!Msg3 && tgtIsUl) ? m_dlCceReserve : 0;
        if(!Msg3 && isAlreadyScheduled(rnti,sfnsf,bwpID))
        {
            if (countStats) { NrCampaignStats::Inc(pdcchKey + "already_scheduled"); }
            return false;
        }

        //this function models the useage of search spaces for PDCCH and its ressources. The transmission of dci must not be in that slot. 
        uint16_t slotsUntilTransmission = sfnsf.GetFrame() * sfnsf.GetSlotPerSubframe() * sfnsf.GetSubframesPerFrame() +sfnsf.GetSubframe() * sfnsf.GetSlotPerSubframe()+ sfnsf.GetSlot()- Simulator::Now ().GetMicroSeconds()* pow(2,m_numerology)/1000;
        if(m_searchSpaces.find(rnti) != m_searchSpaces.end())
        {
            SearchSpaceSet searchSpace = m_searchSpaces.at(rnti);
            for(uint i = 0; i< slotsUntilTransmission; ++i)
            {
                SfnSf dciSlot = sfnsf;
                dciSlot.Subtract(i);
                uint64_t slotnumber= dciSlot.GetFrame() * dciSlot.GetSlotPerSubframe() * dciSlot.GetSubframesPerFrame() +dciSlot.GetSubframe() * dciSlot.GetSlotPerSubframe()+ dciSlot.GetSlot();
                if( (slotnumber- searchSpace.location.offset)%searchSpace.location.slotPeriodicity <= uint(searchSpace.duration-1)) //Nr-in-bullets 3.5.2, with adjusted duration
                {
                    //a possible DL-slot in the seach space is found
                    if(m_pattern[slotnumber%m_pattern.size()] == ns3::DL ||m_pattern[slotnumber%m_pattern.size()] == ns3::F ||m_pattern[slotnumber%m_pattern.size()] == ns3::S)
                    {
                        if(checkPdcchUsage(false,slotnumber,bwpID,keepFree))
                        {
                            if (countStats) { NrCampaignStats::Inc(pdcchKey + "ok"); }
                            return true; 
                        }                                 
                    }
                }           
            }
            if (countStats) { NrCampaignStats::Inc(pdcchKey + "no_free_cce"); }
            return false; 
        }
        else{
            if (countStats) { NrCampaignStats::Inc(pdcchKey + "no_search_space"); }
            return true; //no searchspace is set. Common Search Space not implemented 
        }    
    }

    bool
    NrMacSchedulerRessourceManager::isAlreadyScheduled(uint16_t rnti, SfnSf sfnsf, uint8_t bwpID)
    {
        uint64_t slotnumber = sfnsf.GetFrame() * sfnsf.GetSlotPerSubframe() * sfnsf.GetSubframesPerFrame() +sfnsf.GetSubframe() * sfnsf.GetSlotPerSubframe()+ sfnsf.GetSlot();
        //size_t i = Simulator::Now().GetMilliSeconds()%(m_ressourceWindowSize+m_resBufferSize) < m_ressourceWindowSize ?  slotnumber * m_numSym%m_ressourceWindowElements :  slotnumber * m_numSym%(m_ressourceWindowSize*m_numSlots * m_numSym/10) ;
        size_t i =  slotnumber * m_numSym%m_ressourceWindowElements;
        // std::vector<std::vector<int>> slotRessources = getSlotRessources(bwpID,Simulator::Now().GetMicroSeconds(),LteNrTddSlotType::DL);
        for(uint8_t symNum =0; symNum <m_numSym; ++symNum)
        {   
            for(uint16_t rb = bwpRessourceMap.at(bwpID).getLowerBorder(); rb<= bwpRessourceMap.at(bwpID).getUpperBorder();rb++ )
            {
                if( m_ressourcen[i+symNum][rb] == rnti)
                {
                    std::cout<<"RNTI "<<rnti<<" got already ressources in Slot "<<slotnumber<<std::endl;
                    return true;
                }
            }
        }
        return false; 
    }

    
    void
    NrMacSchedulerRessourceManager::CreateSearchSpace(uint16_t rnti,uint16_t periodicity,uint16_t offset,uint16_t duration)
    {
        SearchSpaceSet ssSet;
        ssSet.location.slotPeriodicity = periodicity;
        ssSet.location.offset = offset;
        ssSet.duration = duration;
        m_searchSpaces.insert({rnti,ssSet});
    }

    void
    NrMacSchedulerRessourceManager::UpdateRntiMap(uint64_t imsi, uint16_t rnti)
    {
        m_rntiMap.insert({rnti,imsi}) ;
    }

    void
    NrMacSchedulerRessourceManager::UpdatePrachRessources(uint32_t prachNumber,uint8_t occasion,bool collision)
    {
        // uint8_t slotsInSF = pow(2,m_numerology);
        // uint64_t index = prachNumber*20*m_numSym*slotsInSF;
        //std::cout<<"Occasion "<<uint(occasion)<<" index"<<index<<std::endl;
        
        //uint8_t slotNumber = getSlotNumber(occasion);
        
        if(m_lastPrachNo <  prachNumber)
        {
            m_lastPrachNo = prachNumber;
            if(m_lastPrachNo !=0)
            {
                std::string logfile_path = m_logDir +"_PRACH_Usage.log";
                std::ofstream logfile;
                logfile.open (logfile_path, std::ios_base::app );      
                logfile <<  prachNumber<<"," <<std::to_string(m_PrachUsedCount)<<"," << std::to_string(m_prachCollisionCount)<< "\r\n";
                logfile.close ();

            }
            m_PrachUsedCount = 1;
        }
        else{
            ++m_PrachUsedCount;
            if(collision)
            {
                ++m_prachCollisionCount;
            }
        }
    }

    void 
    NrMacSchedulerRessourceManager::logRessources(uint16_t numUe , int seed)
    {
        NS_LOG_FUNCTION(this);
        //Spectal Ressources
        std::string logfile_path = m_logDir +"_Spectral_Ressources.log";
        std::ofstream logfile;
        logfile.open (logfile_path, std::ios::out | std::ios::trunc);
        uint8_t slotsInSF = pow(2,m_numerology);
        for (uint64_t i = 0; i < (Simulator::Now().GetMilliSeconds ())*m_numSym*slotsInSF; ++i)
        {
            //add slotnumber to log
            if(i%m_numSym ==0 )
            {
                uint64_t slotNumber = i/m_numSym;   
                logfile << "Slotnumber: "<< slotNumber<<" | " << m_pattern[slotNumber%m_pattern.size()];
                logfile << "\r\n" ;
            }
             for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
            {
                logfile << " "<< m_ressourcen[i][rb];     
            }
    
             logfile << "\r\n";

            // add slot delimiter
            if(i%m_numSym ==m_numSym-1 )
            {
                 for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
                {
                    logfile << "-";     
                }
                logfile << "\r\n" ;
            }
        }
       
        logfile.close ();
        //PDCCH capacity
        logfile_path = m_logDir +"_PDCCH_Usage.log";
        logfile.open (logfile_path, std::ios::out | std::ios::trunc);
        for (uint64_t slot = 0; slot < uint64_t((Simulator::Now().GetMilliSeconds ())*slotsInSF); ++slot)
        {
            if(m_pattern[slot%m_pattern.size()] == ns3::DL)
            {
                for( uint8_t bwpId =0; bwpId<m_numBwp-2 ; ++bwpId )
                {
                    uint8_t coreSetSymbols = coresetMap.at(bwpId);
                    uint8_t dciRb = 6/coreSetSymbols;
                    float counterPdcchUsed = 0;
                    float counterPdcchUnused = 0;
                    
                    for (int64_t rb = m_rbPerBwp*bwpId; rb <m_rbPerBwp*(bwpId+1);rb+=dciRb)
                    {
                        switch(m_ressourcen[slot*m_numSym][rb])
                        {
                            case SCH_CORESET: ++counterPdcchUsed;
                                break;
                            case CORESET: ++counterPdcchUnused;
                                break;

                            default:
                                std::cout<<"Is this a DL slot? Something went wrong"<<std::endl;
                        }
                    }

                    logfile << counterPdcchUsed/(counterPdcchUsed+counterPdcchUnused)<<",";                 
                }
                //new line
                logfile << "\r\n";
            }             
        }
    }


    

    RessourceUsageStats
    NrMacSchedulerRessourceManager::calculateCapacityUsage(uint16_t endInitialization,uint16_t followUpTime )
    {
        NS_LOG_FUNCTION(this);
        RessourceUsageStats stats;
        uint8_t slotsInSF = pow(2,m_numerology);
        uint64_t startIndex = endInitialization * m_numSym * slotsInSF;
        uint64_t slotNumber =0;
        for (uint64_t i = startIndex; i < (Simulator::Now().GetMilliSeconds ()-followUpTime)*m_numSym*slotsInSF; ++i)
        {
            slotNumber = i/m_numSym; 
            for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
            {
                switch(m_ressourcen[i][rb])
                {
                    case FREE: if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                        stats.freeRessources_UL++;
                    }
                    else{
                        stats.freeRessources_DL++;
                    }
                        break;
                    case PRACH: stats.PrachRessources++;
                        break;
                    case RESERVED: stats.ControlRessources++;
                        break;
                    case CORESET: stats.PdcchRessources++;
                        break;
                    case SCH_CORESET: stats.PdcchUsed++;
                        break;
                    case PUCCH: stats.PucchRessources++;
                        break;
                    case SCH_MSG3: stats.usedRessources_UL++;
                        break;
                    
                    case PBCH: stats.ControlRessources++;
                        break;
                    
                    default:
                        //no reserved ressources -> schedueled Data
                        if(m_ressourcen[i][rb] > 0) //sanity check
                        {
                            if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
                                stats.usedRessources_UL++;
                            }
                            else{
                                stats.usedRessources_DL++;
                            }
                        }
                }
            }
        }
        return stats;
    }
    


    RessourceUsageStats
    NrMacSchedulerRessourceManager::getCapacityUsage()
    {
        
        return m_ressourceStats;
    }

    UeRessourceUsage
    NrMacSchedulerRessourceManager::calculateUeSpecificCapacityUsage(uint16_t endInitialization,uint16_t followUpTime, uint16_t numDevices )
    {
        NS_LOG_FUNCTION(this);
        // UeRessourceUsage UeStats(numDevices);
        // uint8_t slotsInSF = pow(2,m_numerology);
        // uint64_t startIndex = endInitialization * m_numSym * slotsInSF;
        // uint64_t slotNumber =0;
        // for (uint64_t i = startIndex; i < (Simulator::Now().GetMilliSeconds ()-followUpTime)*m_numSym*slotsInSF; ++i)
        // {
        //     slotNumber = i/m_numSym; 
        //     for (int64_t rb = 0; rb <m_rbPerBwp*(m_numBwp-2);++rb)
        //     {
        //         switch(m_ressourcen[i][rb])
        //         {
        //             case FREE: if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
        //                 UeStats.freeRessources++;
        //             }
        //             else{
        //                 //stats.freeRessources_DL++;
        //             }
        //                 break;
                    
        //             default:
        //                 //no reserved ressources -> schedueled Data
        //                 if(m_ressourcen[i][rb] > 0) //sanity check
        //                 {
        //                     if(m_pattern[slotNumber%m_pattern.size()] == ns3::UL ||m_pattern[slotNumber%m_pattern.size()] == ns3::F){
        //                     UeStats.UlUsageArr[m_rntiMap.at(m_ressourcen[i][rb])]++;
        //                     }
        //                     else{
        //                     //stats.usedRessources_DL++;
        //                     }
        //                 }
        //         }
        //     }
        // }
        // return UeStats;
        return m_ueStats;
    }

    UeRessourceUsage
    NrMacSchedulerRessourceManager::getUeSpecificCapacityUsage()
    {
        NS_LOG_FUNCTION(this);
        return m_ueStats;
    }
    



    BwpBorders::BwpBorders(uint16_t index, uint8_t numBwp, uint16_t bwpInRBG)
    {
        if(index == 0)
        {
            countRB = 0; //initialize countRB
        }
        if (index <numBwp-2) //TODO nicht hardcoden. Bestimmung der Grenzen an den manager übergeben
        {
            upperBorder = countRB+bwpInRBG-1;
            lowerBorder = countRB;
            countRB +=bwpInRBG;
        }
        else{
            upperBorder = countRB-1; //TODO: dynamic
            lowerBorder=0;
        }
        std::cout<<"Borders index "<< index << " low: "<< lowerBorder<< " high: "<<upperBorder<<std::endl;
    } 
    uint16_t
    BwpBorders::getUpperBorder()
    {
        return upperBorder;
    }
    uint16_t
     BwpBorders::getLowerBorder()
    {
        return lowerBorder;
    }
}
    