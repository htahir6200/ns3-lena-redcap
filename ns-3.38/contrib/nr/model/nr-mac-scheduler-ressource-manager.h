#include <ns3/object.h>
#include <string> 
#include <functional>
#include <memory>
#include <vector>
#include <ns3/nr-control-messages.h>
#include "nr-mac-scheduler-ns3.h"
#include "nr-phy-sap.h"

#ifndef HEADER_RessourceManagaer
#define HEADER_RessourceManagaer

namespace ns3 {

    const std::vector<uint8_t> k2_delayVector = {1,4,2,3,11,21}; //138.214 Table 6.1.2.1.1-4  
    const std::vector<uint8_t> delta_delayVector = {2,3,4,6,24,48}; //138.214 Table 6.1.2.1.1-5  

    enum RessourceAllocationStatus : int16_t
    {
        FREE = 0, 
        RESERVED = -1,  
        PRACH = -2,  
        PBCH = -3, 
        SCH_MSG3 = -4,
        PUCCH = -5,
        CORESET = -6,
        SCH_CORESET = -7
    };


    struct RessourceSet{
        uint32_t Framenumber;
        uint8_t Subframenumber;
        uint8_t Slotnumber;
        uint8_t StartSymbol;
        uint8_t NumberSymbols;
        uint32_t StartRB;
        uint16_t NumberRB;
        std::vector<uint8_t> rbgmask;

    };

      class BwpBorders : public Object
    {
        public:
        BwpBorders(uint16_t index, uint8_t numBwp, uint16_t numRB );
        uint16_t getUpperBorder();
        uint16_t getLowerBorder();
        protected:
        uint16_t upperBorder;
        uint16_t lowerBorder; 

        static inline uint16_t countRB;
    };

    struct MonitoringSlotPeriodicityAndOfTset
    {
        uint16_t slotPeriodicity;
        uint16_t offset;
        //monitoringSymbolsWithinSlot CORESET location is known
    };
    
    struct SearchSpaceSet
    {
        MonitoringSlotPeriodicityAndOfTset location;
        uint16_t duration;
    };

    struct RessourceUsageStats
    {
        uint64_t PdcchRessources{0};
        uint64_t PdcchUsed{0};
        uint64_t PucchRessources{0};
        uint64_t freeRessources_DL{0};
        uint64_t freeRessources_UL{0};
        uint64_t usedRessources_UL{0};
        uint64_t usedRessources_DL{0};
        uint64_t PrachRessources{0};
        uint64_t ControlRessources{0};
    };

    struct UeRessourceUsage
    {
        uint64_t* UlUsageArr;  // Zeiger auf ein dynamisches Array
        uint64_t ArrSize;    // Größe des Arrays
        uint64_t freeRessources{0};
        std::map<uint32_t,uint64_t> ueResMap;
        std::map<uint32_t,uint64_t> tmpResMap; //used for exceptional cases where the rnti is not assigned to a ue
    

    // Konstruktor zum Initialisieren
    UeRessourceUsage(uint64_t numDevices) : ArrSize(numDevices) {
        UlUsageArr = new uint64_t[ArrSize](); // Dynamisches Array der angegebenen Größe
    }

    UeRessourceUsage() {
        //UlUsageArr = new uint64_t[ArrSize](); // Dynamisches Array der angegebenen Größe
    }

    // // Destruktor zum Freigeben des Speichers
    ~UeRessourceUsage() {
        //delete[] UlUsageArr;
    }
    };

    class NrMacSchedulerRessourceManager : public Object
    {
        
        public:

        NrMacSchedulerRessourceManager( std::string pattern, uint8_t numerology, uint16_t simTime,uint32_t initTime, std::string logDir,uint8_t  bwpCount, bool use5Mhz, uint16_t rbPerBwp = 51);
        void reserveSystemInformations(uint8_t  bwpCount);
        void reservePrachRessources(NrPhySapProvider::PrachConfig);
        void changeRessourceWindow();
        void resetRessourceGrid();
        void collectResUsage();
        void collectFinalResUsage();
        void moveRessourceBufferValues();
        void configureBwp(uint16_t bwpIndex, uint16_t bwInRBG, uint8_t coresetSymbols);
        void SetPattern(const std::string& pattern);
        bool checkPdcchUsage(bool reserve, uint64_t slotnumber,uint8_t bwpID, uint16_t keepFree = 0);
        /// IoTJ R1: number of still-free CCEs of the CORESET in this slot/BWP.
        uint16_t countFreeCce(uint64_t slotnumber, uint8_t bwpID);
        /// IoTJ R1: an UL DCI may only take a CCE if more than n CCEs stay free for DL DCIs
        /// (protects DL assignments, e.g. RLC STATUS, from UL-grant starvation). 0 = off.
        void SetDlCceReservation(uint16_t n) { m_dlCceReserve = n; }
        uint16_t GetDlCceReservation() const { return m_dlCceReserve; }
        bool markViablePdcchRessource(bool Msg3, SfnSf sfnsf,uint16_t rnti, uint8_t bwpID);
        bool pdcchAvailableInSearchSpace(bool Msg3, SfnSf sfnsf,uint16_t rnti, uint8_t bwpID, bool countStats = true);
        /// IoTJ R1: if true, the DL active-UE pre-filter uses a non-reserving PDCCH check
        /// (the original code reserved a CCE per active DL UE before scheduling).
        void SetFixDlPdcchPrefilter(bool fix) { m_fixDlPdcchPrefilter = fix; }
        /// IoTJ R1: refuse to overwrite reserved/other-UE cells when marking allocations.
        void SetGuardMarks(bool g) { m_guardMarks = g; }
        /// IoTJ R1: build a DCI from the row where its RB run started (fixes UL grants placed in
        /// the PUCCH symbol when an allocation occupied only the last RB of a row).
        void SetFixDciRowSymbol(bool f) { m_fixDciRowSymbol = f; }
        bool GetFixDlPdcchPrefilter() const { return m_fixDlPdcchPrefilter; }
        bool isAlreadyScheduled(uint16_t rnti, SfnSf sfnsf, uint8_t bwpID);
        void CreateSearchSpace(uint16_t rnti,uint16_t periodicity,uint16_t offset,uint16_t duration);  
        void UpdateRntiMap(uint64_t imsi, uint16_t rnti);
        void UpdatePrachRessources(uint32_t prachNumber, uint8_t occasion,bool collision);
        RessourceSet* scheduleData(uint16_t, uint16_t, uint32_t,LteNrTddSlotType,bool , const SfnSf& sfnSf, RessourceSet* res );
        RessourceSet* scheduleCompleteSlot(uint16_t bwpIndex,uint16_t ue,uint32_t numRB, LteNrTddSlotType slotType, bool Msg3,RessourceSet* res,uint64_t slotnumber);
        RessourceSet* schedulePartPacket(uint16_t bwpIndex,uint16_t ue,uint32_t numRB, LteNrTddSlotType slotType, bool Msg3,RessourceSet* res,uint64_t slotnumber);
        void findOptimalScheduling(uint32_t numRB, std::vector<uint8_t> consideredBwps, RessourceSet* maxRes , uint16_t bwpIndex, LteNrTddSlotType slotType );
        void markRessources(RessourceSet* res, uint16_t ue,uint64_t slotNumber);
        /**
         * Mark RBs occupied by a classic HARQ retx DCI so ResManager new-data
         * scheduling does not overlap them in the same slot.
         * rbgBitmask indices are relative to the BWP (0 = lowerBorder).
         */
        void MarkHarqRetxOccupancy(uint16_t bwpIndex,
                                   const SfnSf& sfn,
                                   uint8_t symStart,
                                   uint8_t numSym,
                                   const std::vector<uint8_t>& rbgBitmask,
                                   uint16_t rnti);
        void reduceFreeSymbols(RessourceSet* res);
        void resetFreeRessources(SfnSf sf);
        std::vector<uint8_t> createRBGmask(uint8_t bwpIndex, uint16_t NumberRB,  uint16_t StartRB, uint8_t NumberSymbols, uint8_t StartSymbol);
        std::vector<std::vector<int>> getSlotRessources(uint8_t bwpIndex,const SfnSf sfn, LteNrTddSlotType type);
        std::vector<std::shared_ptr<DciInfoElementTdma>> createDCI(std::vector<std::shared_ptr<ns3::NrMacSchedulerUeInfo>> &ueInfoVec,
                                                        uint8_t bwpIndex, const SfnSf& sfnSf, Ptr<NrAmc> m_Amc, LteNrTddSlotType type );
        std::shared_ptr<DciInfoElementTdma> createSingleDCI(uint16_t rnti,uint8_t startSymbol,uint8_t symNum,size_t rb,int counter,uint16_t bwpIndex, ns3::Ptr<NrAmc> m_Amc,std::shared_ptr<ns3::NrMacSchedulerUeInfo> ueInfo, LteNrTddSlotType type);
        void logRessources(uint16_t numUe , int seed);
        RessourceUsageStats calculateCapacityUsage(uint16_t endInitialization,uint16_t followUpTime);
        RessourceUsageStats getCapacityUsage();
        UeRessourceUsage getUeSpecificCapacityUsage();
        UeRessourceUsage calculateUeSpecificCapacityUsage(uint16_t endInitialization,uint16_t followUpTime, uint16_t numUe );
        uint8_t  m_numBwp;
        bool m_use5MHz;
        uint16_t m_dlCceReserve{0};
        bool m_fixDlPdcchPrefilter{false};
        bool m_guardMarks{false};
        bool m_fixDciRowSymbol{false};
        void guardedMark(size_t row, size_t col, int ue);
        uint16_t m_rbPerBwp{51}; //!< RBs (RBGs) per RedCap BWP; 51@20 MHz μ=1, 12@5 MHz μ=1

        protected:
        std::vector<std::vector<int>> m_ressourcen;
        std::vector<LteNrTddSlotType>  m_pattern;
        uint64_t m_numSlots;
        uint64_t m_numSym;
        uint8_t m_numerology;
        uint16_t m_ressourceWindowSize; 
        uint16_t m_resBufferSize;
        uint32_t m_ressourceWindowElements;
        std::map<int,BwpBorders> bwpRessourceMap;
        std::map<int,uint8_t> coresetMap;
        std::map<uint16_t,int8_t> msg3SlotMap;
        std::map<uint8_t,uint8_t> freeSymbolsMap;
        bool doPrintRessourcen{true};
        std::map<uint16_t, SearchSpaceSet> m_searchSpaces;
        bool m_prachConfigured{false};
        NrPhySapProvider::PrachConfig m_prachConfig;
        std::string m_logDir;
        uint32_t m_lastPrachNo;
        uint16_t m_PrachUsedCount;
        uint16_t m_prachCollisionCount;
        std::map<uint16_t,uint64_t> m_rntiMap;
        UeRessourceUsage m_ueStats;
        RessourceUsageStats m_ressourceStats;
        

    };

 
}

#endif // HEADER_RessourceManagaer

