/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 *   Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2 as
 *   published by the Free Software Foundation;
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/config-store-module.h"
#include "ns3/config-store.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ideal-beamforming-algorithm.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/network-module.h"
#include "ns3/nr-helper.h"
#include "ns3/nr-access-stats.h"
#include "ns3/nr-campaign-stats.h"
#include "ns3/lte-rlc-am.h"
#include "ns3/lte-radio-bearer-info.h"
#include "ns3/nr-ue-rrc.h"
#include "ns3/nr-ue-net-device.h"
#include "ns3/nr-mac-scheduler-tdma-rr.h"
#include "ns3/nr-module.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/point-to-point-module.h"
#include "ns3/random-variable-stream.h"
#include "ns3/udp-client-5hine.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <math.h>
#include <set>
#include "ns3/nr-energy-model.h"

#include <memory>
#include <vector>

#define M_PI 3.14159265358979323846
using namespace ns3;

static double g_progressSimTimeSeconds = 36.0;

// IoTJ R1: once-per-second time series of offered / delivered bytes and UL backlog.
static std::ofstream g_tsFile;
static Ptr<FlowMonitor> g_tsMonitor;
static NetDeviceContainer g_ueDevs;

struct RlcBacklog
{
    uint64_t txon{0};
    uint64_t retx{0};
    uint64_t txed{0};
    uint64_t uesWithBacklog{0}; // UEs with txon+retx > 0 on any data radio bearer
};

// IoTJ R1: read the UL RLC AM buffers of every UE data radio bearer directly.
RlcBacklog readRlcBacklog()
{
    RlcBacklog b;
    for (uint32_t i = 0; i < g_ueDevs.GetN(); ++i)
    {
        Ptr<NrUeNetDevice> dev = DynamicCast<NrUeNetDevice>(g_ueDevs.Get(i));
        if (!dev || !dev->GetRrc())
        {
            continue;
        }
        ObjectMapValue drbs;
        dev->GetRrc()->GetAttribute("DataRadioBearerMap", drbs);
        uint64_t ueWaiting = 0;
        for (auto it = drbs.Begin(); it != drbs.End(); ++it)
        {
            Ptr<LteDataRadioBearerInfo> drb = DynamicCast<LteDataRadioBearerInfo>(it->second);
            if (!drb)
            {
                continue;
            }
            Ptr<LteRlcAm> am = DynamicCast<LteRlcAm>(drb->m_rlc);
            if (!am)
            {
                continue;
            }
            uint32_t txon = 0;
            uint32_t retx = 0;
            uint32_t txed = 0;
            am->GetBufferBytesForStats(txon, retx, txed);
            b.txon += txon;
            b.retx += retx;
            b.txed += txed;
            ueWaiting += txon + retx;
        }
        b.uesWithBacklog += ueWaiting > 0 ? 1 : 0;
    }
    return b;
}

void timeSeriesSnapshot()
{
    uint64_t tx = 0;
    uint64_t rx = 0;
    if (g_tsMonitor)
    {
        for (const auto& kv : g_tsMonitor->GetFlowStats())
        {
            tx += kv.second.txBytes;
            rx += kv.second.rxBytes;
        }
    }
    const RlcBacklog bl = readRlcBacklog();
    g_tsFile << Simulator::Now().GetSeconds() << "," << tx << "," << rx << "," << (bl.txon + bl.retx)
             << "," << bl.uesWithBacklog << "," << bl.txed << "," << NrCampaignStats::TotalSoftFails() << ","
             << NrCampaignStats::Get("ue.sr_unanswered") << ","
             << NrCampaignStats::Get("sched.ul.not_scheduled.pdcch") << ","
             << NrCampaignStats::Get("sched.ul.grants") << "\n";
    Simulator::Schedule(Seconds(1.0), &timeSeriesSnapshot);
}

// IoTJ R1: one-shot snapshot at traffic stop, so a run with a drain period also yields the
// PDR as measured without drain (original paper definition).
void trafficStopSnapshot()
{
    uint64_t tx = 0;
    uint64_t rx = 0;
    uint64_t txPay = 0;
    uint64_t rxPay = 0;
    if (g_tsMonitor)
    {
        for (const auto& kv : g_tsMonitor->GetFlowStats())
        {
            tx += kv.second.txBytes;
            rx += kv.second.rxBytes;
            if (kv.second.txBytes > 100) // same payload-flow rule as parse_results.py
            {
                txPay += kv.second.txBytes;
                rxPay += kv.second.rxBytes;
            }
        }
    }
    NrCampaignStats::Set("atstop.tx_bytes", tx);
    NrCampaignStats::Set("atstop.rx_bytes", rx);
    NrCampaignStats::Set("atstop.payload_tx_bytes", txPay);
    NrCampaignStats::Set("atstop.payload_rx_bytes", rxPay);
    const RlcBacklog bl = readRlcBacklog();
    NrCampaignStats::Set("atstop.ul_backlog_bytes", bl.txon + bl.retx);
    NrCampaignStats::Set("atstop.ul_retx_bytes", bl.retx);
    NrCampaignStats::Set("atstop.ul_unacked_bytes", bl.txed);
    NrCampaignStats::Set("atstop.ues_with_backlog", bl.uesWithBacklog);
}

void progressPrint()
{
    static auto lastWall = std::chrono::steady_clock::now();
    auto nowWall = std::chrono::steady_clock::now();
    const double wallDelta =
        std::chrono::duration<double>(nowWall - lastWall).count();
    lastWall = nowWall;

    const double simNow = Simulator::Now().GetSeconds();
    const double remainingSim = std::max(0.0, g_progressSimTimeSeconds - simNow);
    // progress callback fires every 0.1s of sim time
    const double wallPerSimSecond = (wallDelta > 0.0) ? (wallDelta / 0.1) : 0.0;
    const double remainingWallMin =
        (wallPerSimSecond > 0.0) ? (remainingSim * wallPerSimSecond / 60.0) : 0.0;

    std::cout << "Current simulation time: " << simNow << "s / " << g_progressSimTimeSeconds
              << "s - ~" << wallPerSimSecond << "s wall per 1s sim - Runtime left: ~"
              << remainingWallMin << " min\n";
    Simulator::Schedule(Seconds(0.1), &progressPrint);
}

int
main(int argc, char* argv[])
{

    //fixed values
    int16_t gNbOffsetX = 0; //-50
    int16_t gNbOffsetY = 0; //3
    uint16_t numerology = 1; // numerology for contiguous case (see https://arxiv.org/pdf/1911.05534.pdf Table 1)
    //uint32_t followUpTime = 1000;
    uint8_t numBands = 1;
    double centralFrequencyBand = 3750e6;

    double enddeviceHeight = 2.5;


    //data traffic
    
    //uint mimoScalingFactor = 2; // UL does not use MIMO. Packet sizes are reduced to consider MIMO effects
   

    //used chips 
    int16_t transmitPower = 23;

    //-------------------------------------------------------------------------------------------------------------------------------------------------
    //simulation parameters 
    uint32_t seed = 1;
    
    bool enddeviceDownlinkstream = true;
    uint32_t packetSize = 5000;
    double transmissionIntervall =5 ; // in seconds

    uint16_t enddevicesNumPergNb = 1;
    uint16_t gNbNum = 1;          // 1 = IoTJ/M2/M3; 2+ = FGCS multi-cell
    double gNbIsd = 500.0;        // inter-site distance (m), RMa layout
    uint16_t embbUes = 0;
    uint32_t embbPacketSize = 1500;
    double embbTransmissionIntervall = 0.1; // seconds — heavier than RedCap IoT by default
    double bandwidthBand = 20e6;
    uint16_t m_dataInactivityTimer = 60000;
    uint8_t redCapConfig = 1; // 1: minimal config; 2: maximum config; 3: eRedCap
 

    // non-contiguous case
    double centralFrequencyCc0 = 3750e6;
    double bandwidthCc0 = bandwidthBand;
    uint16_t numerologyCc0Bwp0 = 1;
    uint16_t numerologyCc0Bwp1 = 1;

    //std::string pattern ="F|F|F|F|F|F|F|F|F|F|";
    std::string pattern ="DL|DL|S|UL|UL|UL|UL|UL|UL|UL|"; //start with 2 DL slots to ensure the BCH to be in DL slots (doesnt interfere with UL scheduling)
    double totalTxPower = 27; // 250mW
    bool cellScan = false;
    double beamSearchAngleStep = 10.0;

    bool udpFullBuffer = false;

    bool m_useSdt = false;

    bool m_useErrorModel = false;
    // Default false keeps IoTJ campaigns bit-compatible (no HARQ retx).
    bool enableHarqReTx = false;
    // IoTJ R1 revision options (defaults keep the 2ab3dda behaviour bit-for-bit).
    bool strictPhy = false;
    double drainS = 0.0;
    int16_t mcsOverride = -1;
    int16_t eredcapPowerOffsetDb = 6;
    uint16_t dlCceReserve = 0;
    bool fixDlPdcchPrefilter = false;
    uint32_t retxBsrTimerMs = 0;
    bool guardSchedMarks = false;
    bool fixDciRowSymbol = false;
    bool rlcRetxAlwaysFits = false;
    uint32_t rlcTxBufferBytes = 999999999;
    uint32_t arrivalModel = 0;
    double onMeanS = 0.0;
    double offMeanS = 0.0;
    // For redCapConfig=3: true = real 5 MHz BWP (FGCS); false = IoTJ capability-style (20 MHz + ¼-RB).
    bool true5MHz = true;
    // Phase C energy fidelity
    bool enableEdrxPtw = false;
    uint32_t ptwLengthMs = 1280;
    bool enableRai = false;
    uint16_t raiGuardMs = 20;
    // RAN eDRX cycle in radio frames (256/512/1024/2048 → 2.56/5.12/10.24/20.48 s)
    uint16_t edrxCycleFrames = 256;

    bool useFixedMcs = true;
    std::string simTag = "default";
    std::string outputDir = "./";

    double simTime = 36;  // seconds 
    
    CommandLine cmd(__FILE__);

    cmd.AddValue("simTime", "Simulation time", simTime);
    cmd.AddValue("enddevicesNumPergNb", "The number of RedCap UE per gNb", enddevicesNumPergNb);
    cmd.AddValue("gNbNum", "Number of gNBs (static multi-cell; default 1)", gNbNum);
    cmd.AddValue("gNbIsd", "Inter-site distance in meters between gNBs (default 500)", gNbIsd);
    cmd.AddValue("embbUes", "Number of eMBB UEs sharing the cell (0 = RedCap-only)", embbUes);
    cmd.AddValue("embbPacketSize", "Packet size for eMBB traffic (bytes)", embbPacketSize);
    cmd.AddValue("embbIntervall", "eMBB packet interval in seconds", embbTransmissionIntervall);
    cmd.AddValue("numBands",
                 "Number of operation bands. More than one implies non-contiguous CC",
                 numBands);
    cmd.AddValue("centralFrequencyBand",
                 "The system frequency to be used in band 1",
                 centralFrequencyBand);
    cmd.AddValue("bandwidthBand", "The system bandwidth to be used in band 1", bandwidthBand);
    cmd.AddValue("numerology", "Numerlogy to be used in contiguous case", numerology);
    cmd.AddValue("centralFrequencyCc0",
                 "The system frequency to be used in CC 0",
                 centralFrequencyCc0);
    cmd.AddValue("bandwidthBand", "The system bandwidth to be used in CC 0", bandwidthCc0);

    cmd.AddValue("numerologyCc0Bwp0", "Numerlogy to be used in CC 0, BWP 0", numerologyCc0Bwp0);
    cmd.AddValue("numerologyCc0Bwp1", "Numerlogy to be used in CC 0, BWP 1", numerologyCc0Bwp1);
    cmd.AddValue("tddPattern",
                 "LTE TDD pattern to use (e.g. --tddPattern=DL|S|UL|UL|UL|DL|S|UL|UL|UL|)",
                 pattern);
    cmd.AddValue("totalTxPower",
                 "total tx power that will be proportionally assigned to"
                 " bandwidth parts depending on each BWP bandwidth ",
                 totalTxPower);
    cmd.AddValue("cellScan",
                 "Use beam search method to determine beamforming vector,"
                 "true to use cell scanning method",
                 cellScan);
    cmd.AddValue("beamSearchAngleStep",
                 "Beam search angle step for beam search method",
                 beamSearchAngleStep);
    cmd.AddValue("udpFullBuffer",
                 "Whether to set the full buffer traffic; if this parameter is "
                 "set then the udpInterval parameter will be neglected.",
                 udpFullBuffer);
    cmd.AddValue("sdt", "Whether SDT should be used or not.",m_useSdt);
    cmd.AddValue("seed", "Seed for RNG", seed);
    cmd.AddValue("simTag",
                 "tag to be appended to output filenames to distinguish simulation campaigns",
                 simTag);
    cmd.AddValue("outputDir", "directory where to store simulation results", outputDir);
    cmd.AddValue("DataErrorModelEnabled","Usage of the Error Model",m_useErrorModel);
    cmd.AddValue("EnableHarqReTx",
                 "Enable HARQ retransmissions on the RedCap resource-manager path (default: false)",
                 enableHarqReTx);
    cmd.AddValue("true5MHz",
                 "For redCapConfig=3: true=5 MHz BWP (no ¼-RB); false=capability-style 20 MHz+¼RB (IoTJ)",
                 true5MHz);
    cmd.AddValue("strictPhy",
                 "IoTJ R1: abort at every dense-hardening soft-fail site (original fatal behaviour)",
                 strictPhy);
    cmd.AddValue("drainS",
                 "IoTJ R1: extra simulated seconds after traffic stops so queues can drain (default 0)",
                 drainS);
    cmd.AddValue("mcsOverride",
                 "IoTJ R1: if >=0, use this fixed UL/DL MCS for every RedCap class",
                 mcsOverride);
    cmd.AddValue("eredcapPowerOffsetDb",
                 "IoTJ R1: eRedCap UE TX power reduction in dB (default 6)",
                 eredcapPowerOffsetDb);
    cmd.AddValue("arrivalModel",
                 "IoTJ R1: RedCap packet arrivals: 0 = periodic (default), 1 = Poisson with mean intervall",
                 arrivalModel);
    cmd.AddValue("onMeanS",
                 "IoTJ R1: mean ON time [s] of per-UE on/off activity (0 = always active); mean load preserved",
                 onMeanS);
    cmd.AddValue("offMeanS", "IoTJ R1: mean OFF time [s] of per-UE on/off activity", offMeanS);
    cmd.AddValue("rlcRetxAlwaysFits",
                 "IoTJ R1 diagnostic: LteRlcAm::TxOpportunityForRetxAlwaysBigEnough (not realistic)",
                 rlcRetxAlwaysFits);
    cmd.AddValue("rlcTxBufferBytes",
                 "IoTJ R1: RLC TX buffer per bearer in bytes (default 999999999 = effectively unlimited)",
                 rlcTxBufferBytes);
    cmd.AddValue("fixDciRowSymbol",
                 "IoTJ R1: build DCIs from the row where the RB run started (fixes UL grants in the PUCCH symbol)",
                 fixDciRowSymbol);
    cmd.AddValue("guardSchedMarks",
                 "IoTJ R1: never overwrite reserved or other-UE grid cells when marking allocations",
                 guardSchedMarks);
    cmd.AddValue("retxBsrTimerMs",
                 "IoTJ R1: UE retxBSR-Timer in ms (TS 38.321); 0 = off (original)",
                 retxBsrTimerMs);
    cmd.AddValue("fixDlPdcchPrefilter",
                 "IoTJ R1: non-reserving DL active-UE PDCCH pre-filter (fixes RLC STATUS starvation)",
                 fixDlPdcchPrefilter);
    cmd.AddValue("dlCceReserve",
                 "IoTJ R1: CCEs per CORESET occasion that UL DCIs may not take (kept for DL); 0 = off",
                 dlCceReserve);
    cmd.AddValue("intervall","used intervall for transmitting data", transmissionIntervall);
    cmd.AddValue("packetSize","used packetSize for datastream", packetSize);
    cmd.AddValue("inactivityTimer", "Timer to release the connection after it´s expiration",m_dataInactivityTimer);
    cmd.AddValue("transmitpower", "Ul tandmitower of user equipments",transmitPower);
    cmd.AddValue("redCapConfig",
                 "UE category: 1=RedCap minimal, 2=RedCap maximum, 3=eRedCap (5 MHz)",
                 redCapConfig);
    cmd.AddValue("downlink",
                 "If true, main traffic is downlink (remote->UE); if false, uplink (UE->remote)",
                 enddeviceDownlinkstream);
    cmd.AddValue("enableEdrxPtw",
                 "Phase C: bill eDRX as PTW listen + deep sleep (default false = Phase B flat eDRX)",
                 enableEdrxPtw);
    cmd.AddValue("ptwLengthMs", "Phase C: PTW length in ms when enableEdrxPtw=true", ptwLengthMs);
    cmd.AddValue("enableRai",
                 "Phase C research AS RAI: early RRCRelease after last UL (short guard)",
                 enableRai);
    cmd.AddValue("raiGuardMs", "Phase C: RAI guard ms after last UL activity", raiGuardMs);
    cmd.AddValue("edrxCycleFrames",
                 "RAN eDRX cycle in radio frames (256/512/1024/2048)",
                 edrxCycleFrames);

    cmd.Parse(argc, argv);
    NS_ABORT_MSG_IF(drainS < 0, "drainS must be >= 0");
    NS_ABORT_MSG_IF(arrivalModel > 1, "arrivalModel must be 0 or 1");
    NS_ABORT_MSG_IF(onMeanS < 0 || offMeanS < 0 || (onMeanS > 0) != (offMeanS > 0), "onMeanS/offMeanS must both be 0 or both > 0");
    NS_ABORT_MSG_IF(mcsOverride > 28, "mcsOverride must be -1 or 0..28");

    NS_ABORT_MSG_IF(redCapConfig < 1 || redCapConfig > 3,
                    "redCapConfig must be 1 (min), 2 (max), or 3 (eRedCap)");
    g_progressSimTimeSeconds = simTime;
    
    uint8_t mimo_layer = 1;
    uint8_t McsToBeUsed =19;
    bool use5MHz = false;          // capability-style ¼-RB cap (IoTJ eRedCap)
    uint16_t rbPerBwp = 51;        // ResManager RB grid per RedCap BWP
    double redCapBwpHz = 20e6;     // dedicated RedCap BWP channel bandwidth

    switch (redCapConfig)
    {
    //minimal configuration
    case 1:
        mimo_layer=1;
        McsToBeUsed = 19;
        use5MHz = false;
        break;
    //maximum configuration
    case 2:
        mimo_layer=2;
        McsToBeUsed=27;
        use5MHz = false;
        break;
    //eRedCap: true 5 MHz BWP (FGCS) or capability-style (IoTJ)
    case 3:
        mimo_layer=1;
        McsToBeUsed = 19;
        transmitPower -= eredcapPowerOffsetDb;
        if (true5MHz)
        {
            // Real 5 MHz RF BWP @ μ=1, RbOverhead=0.08 → 12 RB (38.104 lists 11).
            use5MHz = false; // do NOT also apply ¼-RB (double-penalty)
            redCapBwpHz = 5e6;
            bandwidthBand = 5e6;
            rbPerBwp = 12;
        }
        else
        {
            // IoTJ capability-style: keep 20 MHz grid, cap grants to ~¼ RB.
            use5MHz = true;
            redCapBwpHz = 20e6;
            rbPerBwp = 51;
        }
        break;
    }

    if (mcsOverride >= 0)
    {
        McsToBeUsed = static_cast<uint8_t>(mcsOverride);
    }

    {
        const char* cfgName =
            (redCapConfig == 1) ? "RedCap-min" : (redCapConfig == 2) ? "RedCap-max" : "eRedCap-5MHz";
        const char* eredMode =
            (redCapConfig != 3) ? "-" : (true5MHz ? "true-5MHz-BWP" : "capability-style");
        std::cout << "Config: " << cfgName
                  << " | eRedCapMode=" << eredMode
                  << " | gNbNum=" << gNbNum
                  << " | gNbIsd=" << gNbIsd << " m"
                  << " | RedCap UEs/gNB=" << enddevicesNumPergNb
                  << " | eMBB UEs=" << embbUes
                  << " | traffic=" << (enddeviceDownlinkstream ? "DL" : "UL")
                  << " | packetSize=" << packetSize
                  << " | interval=" << transmissionIntervall << "s"
                  << " | band=" << (bandwidthBand / 1e6) << " MHz"
                  << " | redCapBWP=" << (redCapBwpHz / 1e6) << " MHz"
                  << " | rbPerBwp=" << rbPerBwp
                  << " | simTime=" << simTime << "s"
                  << " | seed=" << seed
                  << " | simTag=" << simTag << "\n";
    }

    NS_ABORT_MSG_IF(gNbNum < 1, "gNbNum must be >= 1");
    const uint32_t totalRedCapUes =
        static_cast<uint32_t>(enddevicesNumPergNb) * static_cast<uint32_t>(gNbNum);
    
    NrChip chip_RedCap = RG255C(centralFrequencyBand,transmitPower);
    NrChip chip_NR = RM520N(centralFrequencyBand,transmitPower);

    uint initTime = 100 + (totalRedCapUes + embbUes) * 10;
    // True 5 MHz cell: one RedCap BWP. Legacy: one 20 MHz slice per 20 MHz of band.
    uint8_t numBwp_nonOverlapping =
        (redCapConfig == 3 && true5MHz) ? 1 : static_cast<uint8_t>(bandwidthBand / 20e6);
    NS_ABORT_MSG_IF(numBwp_nonOverlapping == 0, "numBwp_nonOverlapping computed as 0");
    
    uint8_t numBwp_Overlapping = 2;
    bandwidthCc0 = bandwidthBand;
    std::string redCapBwp = "";
    std::string embbBwp= "";
    for(uint i = 0; i<numBwp_nonOverlapping; ++i)
    {
        redCapBwp.append(std::to_string(i));
    }
    for(uint i = numBwp_nonOverlapping; i<numBwp_nonOverlapping+numBwp_Overlapping; ++i)
    {
        embbBwp.append(std::to_string(i));
    }

    NS_ASSERT_MSG(redCapBwp!="","No Bwp assigned for RedCap");
    NS_ASSERT_MSG(embbBwp!="","No Bwp assigned for eMBB");
    Config::SetDefault("ns3::NrGnbRrc::BwpForRedCap", StringValue(redCapBwp));
    Config::SetDefault("ns3::NrGnbRrc::BwpForEmBB", StringValue(embbBwp));
    Config::SetDefault("ns3::UeManagerNr::dataInactivityTimer",UintegerValue(m_dataInactivityTimer));
    Config::SetDefault("ns3::UeManagerNr::EnableRai", BooleanValue(enableRai));
    Config::SetDefault("ns3::UeManagerNr::RaiGuardMs", UintegerValue(raiGuardMs));
    Config::SetDefault("ns3::UeManagerNr::edrxCycleFrames", UintegerValue(edrxCycleFrames));
    Config::SetDefault("ns3::NrEnergyModel::EnableEdrxPtw", BooleanValue(enableEdrxPtw));
    Config::SetDefault("ns3::NrEnergyModel::PtwLengthMs", UintegerValue(ptwLengthMs));
    // unique_ptr energy models read this static config (CreateObject path not used).
    NrEnergyModel::SetGlobalPtwConfig(enableEdrxPtw, ptwLengthMs);
    // Warm-up exclusion: do not bill energy during attach/setup (align with app start).
    Config::SetDefault("ns3::NrUeNetDevice::EnergyModelStartTime", UintegerValue(initTime));

    Config::SetDefault("ns3::NrGnbPhy::RbOverhead", DoubleValue(0.08)); //adjusted to 0.08 to align allocated rb with table Table 5.3.2-1 from 3GPP 38.104. Now 51 RB are set for 20MHz BW
    // RIV N_RB for RedCap BWPs (legacy IoTJ used 53; true 5 MHz @ μ=1 oh=0.08 uses 12).
    Config::SetDefault("ns3::NrGnbRrc::RedCapNRbs", UintegerValue(rbPerBwp == 12 ? 12 : 53));
 
    std::string ResultDir = "Results/Cni-Szenario/"+simTag+"/";
    std::string usedRedCapConfig;

    NrAccessStats::Reset();
    NrCampaignStats::Reset();
    NrCampaignStats::SetStrict(strictPhy);
    Config::SetDefault("ns3::NrUeMac::RetxBsrTimer", TimeValue(MilliSeconds(retxBsrTimerMs)));
    std::cout << "R1Config: redCapConfig=" << +redCapConfig << " true5MHz=" << true5MHz
              << " mcs=" << +McsToBeUsed << " mimoLayerArg=" << +mimo_layer
              << " ueTxPowerDbm=" << transmitPower << " errorModel=" << m_useErrorModel
              << " harqReTx=" << enableHarqReTx << " strictPhy=" << strictPhy
              << " drainS=" << drainS << " dlCceReserve=" << dlCceReserve << " fixDlPdcchPrefilter=" << fixDlPdcchPrefilter << " retxBsrTimerMs=" << retxBsrTimerMs << " guardSchedMarks=" << guardSchedMarks << " fixDciRowSymbol=" << fixDciRowSymbol << " rlcRetxAlwaysFits=" << rlcRetxAlwaysFits << " rlcTxBufferBytes=" << rlcTxBufferBytes << " arrivalModel=" << arrivalModel << " onMeanS=" << onMeanS << " offMeanS=" << offMeanS << " simTime=" << simTime << " seed=" << seed
              << " interval=" << transmissionIntervall << " packetSize=" << packetSize
              << " ues=" << enddevicesNumPergNb << " embbUes=" << embbUes << std::endl;

    switch (redCapConfig)
    {
    case 1:
        usedRedCapConfig = "minimal";
        break;
    case 2:
        usedRedCapConfig = "maximum";
        break;
    case 3:
        usedRedCapConfig = true5MHz ? "5MHz_true" : "5MHz_capability";
        break;
    
    default:
        break;
    }

       
    Config::SetDefault("ns3::NrNetDevice::outputDir", StringValue(ResultDir+"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+"/Seed_"+std::to_string(seed)+"/"));
 
    try {
        // create_directories: parents may not exist (macOS / clean checkouts)
        std::filesystem::create_directories(ResultDir +"Delay/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower));
        std::filesystem::create_directories(ResultDir +"Ressourcen/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower));
        std::filesystem::create_directories(ResultDir +"PDCCH/"+std::to_string(packetSize));
        std::filesystem::create_directories(ResultDir +"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+"/Seed_"+std::to_string(seed));
        
    }
    catch (const std::exception& ex) {
        std::cerr << "Failed to create directory: " << ex.what() << std::endl;
    }

    RngSeedManager::SetSeed (seed);
    Ptr<UniformRandomVariable> RaUeUniformVariable = CreateObject<UniformRandomVariable> ();

    //set attribute for gnb
    Config::SetDefault("ns3::NrGnbRrc::SdtUsage", BooleanValue(m_useSdt));
    Config::SetDefault("ns3::NrSpectrumPhy::DataErrorModelEnabled", BooleanValue(m_useErrorModel));

    Config::SetDefault("ns3::NrGnbRrc::PrachConfigurationIndex", UintegerValue(199)); //see https://www.sharetechnote.com/html/5G/5G_RACH.html or 38.211 v15.3.0-Table 6.3.3.2-3 for details to the configuration. Note only short configurations are possible


    double udpAppStartTime = 400; // milliseconds
    double udpAppStopTime = simTime * 1000 + initTime; 

    //simTime = simTime + 1; // add 1 seconds for the last transmissions to finish

    NS_ABORT_IF(numBands < 1);

    Config::SetDefault("ns3::LteRlcUm::MaxTxBufferSize", UintegerValue(rlcTxBufferBytes));
    Config::SetDefault("ns3::LteRlcAm::MaxTxBufferSize", UintegerValue(rlcTxBufferBytes));
    Config::SetDefault("ns3::LteRlcAm::TxOpportunityForRetxAlwaysBigEnough", BooleanValue(rlcRetxAlwaysFits));
    Config::SetDefault ("ns3::NrGnbRrc::EpsBearerToRlcMapping", EnumValue(NrGnbRrc::RLC_AM_ALWAYS));
    Config::SetDefault("ns3::LteRlcAm::PollRetransmitTimer", TimeValue(MilliSeconds(200)));
    Config::SetDefault("ns3::LteRlcAm::ReorderingTimer", TimeValue(MilliSeconds(80))); //maybe has no impact on simulations
    Config::SetDefault("ns3::LteRlcAm::StatusProhibitTimer", TimeValue(MilliSeconds(0))); // needed to be able to send small packets with low interarrival times

    Config::SetDefault("ns3::LteRlcAm::ReportBufferStatusTimer", TimeValue(MilliSeconds(5)));
    
    //Config::SetDefault("ns3::LteRlcAm::TxOpportunityForRetxAlwaysBigEnough", BooleanValue(true));


    //Create the scenario
    BandwidthPartInfo::Scenario scen;
    BandwidthPartInfo::Scenario scen2;
    scen = BandwidthPartInfo::RMa_nLoS; //Adjusted for CNI-lossModel
    scen2 = BandwidthPartInfo::RMa_LoS; //Adjusted for Freespace model

    // create base stations and mobile terminals
    NodeContainer gNbNodes;
    NodeContainer embbUeNodes;
    NodeContainer redCapUeNodes;
    MobilityHelper mobility;

    gNbNodes.Create(gNbNum);

    redCapUeNodes.Create(totalRedCapUes);
    if (embbUes > 0)
    {
        embbUeNodes.Create(embbUes);
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //----- Setting positions in enddevice field -----------------------------------------------------------------//
    Ptr<ListPositionAllocator> apPositionAlloc = CreateObject<ListPositionAllocator>();
    Ptr<ListPositionAllocator> staPositionAlloc = CreateObject<ListPositionAllocator>();

    for (uint16_t g = 0; g < gNbNum; ++g)
    {
        const double gx = gNbOffsetX + g * gNbIsd;
        apPositionAlloc->Add(Vector(gx, gNbOffsetY, 10));
    }
    
    std::ofstream outFile_position;
        std::string fileposition = ResultDir+"Ausgaben/"+std::to_string(packetSize) + "/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+"/position_"+std::to_string(seed)+".txt" ;
    	outFile_position.open(fileposition.c_str(), std::ofstream::out | std::ofstream::trunc);
    	if (!outFile_position.is_open())
    	{
        		std::cerr << "Can't open file " << fileposition<< std::endl;
        		return 1;
    	}

    outFile_position.setf(std::ios_base::fixed);

    // Cluster RedCap UEs near their serving gNB so AttachToClosestEnb splits load.
    for (uint16_t g = 0; g < gNbNum; ++g)
    {
        const double gx = gNbOffsetX + g * gNbIsd;
        for (uint16_t u = 0; u < enddevicesNumPergNb; ++u)
        {
            staPositionAlloc->Add(Vector(gx + 10.0, gNbOffsetY + 10.0 + u, enddeviceHeight));
            outFile_position << "redcap g=" << g << " u=" << u << " x=" << (gx + 10.0)
                             << " y=" << (gNbOffsetY + 10.0 + u) << "\n";
        }
    }
    for (uint32_t e = 0; e < embbUes; ++e)
    {
        // Place eMBB near gNB0 so positions stay unique vs RedCap clusters.
        staPositionAlloc->Add(Vector(gNbOffsetX + 20.0, gNbOffsetY + 10.0 + e, enddeviceHeight));
    }
                   
    outFile_position.close();
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(apPositionAlloc);
    mobility.Install(gNbNodes);
    mobility.SetPositionAllocator(staPositionAlloc);
    mobility.Install (embbUeNodes);
    mobility.Install (redCapUeNodes);
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //----- Setting 5G NR network ---------------------------------------------------------------------------------//

    // setup the nr simulation
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    // Disable SRS
    nrHelper->SetSchedulerAttribute("SrsSymbols", UintegerValue(0));
    nrHelper->SetSchedulerAttribute("EnableSrsInFSlots", BooleanValue(false));
    nrHelper->SetSchedulerAttribute("EnableSrsInUlSlots", BooleanValue(false));

    nrHelper->SetSchedulerAttribute("FixedMcsDl", BooleanValue(useFixedMcs));
    nrHelper->SetSchedulerAttribute("FixedMcsUl", BooleanValue(useFixedMcs));
    if(useFixedMcs)
    {
        nrHelper->SetSchedulerAttribute("StartingMcsDl", UintegerValue(McsToBeUsed));
        nrHelper->SetSchedulerAttribute("StartingMcsUl", UintegerValue(McsToBeUsed));
    }
    

    //set UE transmit power
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(transmitPower));

    // enable or disable HARQ retransmissions
    // Pair EnableHarqReTx=true with DataErrorModelEnabled=true for meaningful NACKs.
    nrHelper->SetSchedulerAttribute("EnableHarqReTx", BooleanValue(enableHarqReTx));
    //Config::SetDefault ("ns3::NrHelper::HarqEnabled", BooleanValue (false)); //disabling this doesnt change anything and disabling it in the nr-helper lets the program crash

    nrHelper->SetSchedulerTypeId (NrMacSchedulerOfdmaRR::GetTypeId ());

    nrHelper->SetSchedulerAttribute ("NumNonOverlappingBwp", UintegerValue(numBwp_nonOverlapping)); //TODO_ enable and schedule harq over manager

    std::string errorModel = "ns3::NrEesmIrT2";  // Do not change this table when RedCap devices are used
    Config::SetDefault("ns3::NrAmc::ErrorModelType", TypeIdValue(TypeId::LookupByName("ns3::NrEesmIrT2"))); // Do not change this table when RedCap devices are used

    // Both DL and UL AMC will have the same model behind.
    nrHelper->SetGnbDlAmcAttribute ("AmcModel", EnumValue (NrAmc::ErrorModel)); // NrAmc::ShannonModel or NrAmc::ErrorModel
    nrHelper->SetGnbUlAmcAttribute ("AmcModel", EnumValue (NrAmc::ErrorModel)); // NrAmc::ShannonModel or NrAmc::ErrorModel

    /*
     * Setup the configuration of the spectrum. There is a contiguous and a non-contiguous
     * example:
     * 1) One operation band is deployed with 4 contiguous component carriers
     *    (CC)s, which are automatically generated by the ccBwpManager
     * 2) One operation bands non-contiguous case. CCs and BWPs are manually created
     */

    BandwidthPartInfoPtrVector allBwps;
    CcBwpCreator ccBwpCreator;

    OperationBandInfo band;

     // For the case of manual configuration of CCs and BWPs
    std::unique_ptr<ComponentCarrierInfo> cc0(new ComponentCarrierInfo());


    std::vector< std::unique_ptr<BandwidthPartInfo>>bwp_vec;
    for(uint i = 0; i< numBwp_nonOverlapping+numBwp_Overlapping; ++i)
    {
        bwp_vec.emplace_back(std::unique_ptr<BandwidthPartInfo>(new BandwidthPartInfo()));
    }

    /*
        * 100MHz band divides into 5 BWP with 20MHz and one BWP with 100MHz
        * The configured spectrum division is:
        * ----------------------------- Band ----------------------------------
        * ----------------------------- CC0------------------------------------
        * ----BWP0-----|----BWP1-----|----BWP2-----|----BWP3-----|----BWP4-----|
        * ------------------------------BWP5-----------------------------------|
        */ 
    uint8_t coresetSymbols = 2; //3
    //For now coresetSymbols should equal for every bandwitdh part. 

    band.m_centralFrequency = centralFrequencyBand;
    band.m_channelBandwidth = bandwidthBand;
    band.m_lowerFrequency = band.m_centralFrequency - band.m_channelBandwidth / 2;
    band.m_higherFrequency = band.m_centralFrequency + band.m_channelBandwidth / 2;
    uint8_t bwpCount = 0;

    // Component Carrier 0
    cc0->m_ccId = 0;
    cc0->m_centralFrequency = centralFrequencyBand;
    cc0->m_channelBandwidth = bandwidthBand;
    cc0->m_lowerFrequency = cc0->m_centralFrequency - cc0->m_channelBandwidth / 2;
    cc0->m_higherFrequency = cc0->m_centralFrequency + cc0->m_channelBandwidth / 2;

    
    for(uint bwpID = 0; bwpID < numBwp_nonOverlapping; ++bwpID)
    {
        bwp_vec.at(bwpID)->m_bwpId = bwpCount;
        bwp_vec.at(bwpID)->m_scenario = scen;
        bwp_vec.at(bwpID)->m_centralFrequency =
            cc0->m_lowerFrequency + redCapBwpHz / 2.0 + bwpID * redCapBwpHz;
        bwp_vec.at(bwpID)->m_channelBandwidth = redCapBwpHz;
        bwp_vec.at(bwpID)->m_lowerFrequency = bwp_vec.at(bwpID)->m_centralFrequency - bwp_vec.at(bwpID)->m_channelBandwidth / 2;
        bwp_vec.at(bwpID)->m_higherFrequency = bwp_vec.at(bwpID)->m_centralFrequency + bwp_vec.at(bwpID)->m_channelBandwidth / 2;
        bwp_vec.at(bwpID)->m_coresetSymbols = coresetSymbols;

        cc0->AddBwp(std::move(bwp_vec.at(bwpID)));
        ++bwpCount;
    }

    for(uint bwpID = numBwp_nonOverlapping; bwpID < numBwp_nonOverlapping+numBwp_Overlapping; ++bwpID)
    {
        bwp_vec.at(bwpID)->m_bwpId = bwpCount;
        if(bwpID == uint(numBwp_nonOverlapping+numBwp_Overlapping-2))
        {
             bwp_vec.at(bwpID)->m_scenario = scen2;
        }
        else{
             bwp_vec.at(bwpID)->m_scenario = scen;
        }
        bwp_vec.at(bwpID)->m_centralFrequency = cc0->m_lowerFrequency + cc0->m_channelBandwidth/2;
        bwp_vec.at(bwpID)->m_channelBandwidth = bandwidthBand;
        bwp_vec.at(bwpID)->m_lowerFrequency = bwp_vec.at(bwpID)->m_centralFrequency - bwp_vec.at(bwpID)->m_channelBandwidth / 2;
        bwp_vec.at(bwpID)->m_higherFrequency = bwp_vec.at(bwpID)->m_centralFrequency + bwp_vec.at(bwpID)->m_channelBandwidth / 2;
        bwp_vec.at(bwpID)->m_coresetSymbols = coresetSymbols;

        cc0->AddBwp(std::move(bwp_vec.at(bwpID)));
        ++bwpCount;
    }
    // Add CC to the corresponding operation band.
    band.AddCc(std::move(cc0));

    nrHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));
    nrHelper->SetSchedulerTypeId(TypeId::LookupByName("ns3::NrMacSchedulerOfdmaRR"));
    // Beamforming method
    if (cellScan)
    {
        idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                             TypeIdValue(CellScanBeamforming::GetTypeId()));
        idealBeamformingHelper->SetBeamformingAlgorithmAttribute("BeamSearchAngleStep",
                                                                 DoubleValue(beamSearchAngleStep));
    }
    else
    {
        idealBeamformingHelper->SetAttribute("BeamformingMethod",
                                             TypeIdValue(DirectPathBeamforming::GetTypeId()));
    }

    nrHelper->InitializeOperationBand(&band);
    allBwps = CcBwpCreator::GetAllBwps({band});

    double x = pow(10, totalTxPower / 10); // convert from dBm to mW

    // Antennas for all the UEs
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2)); 
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4)); // 1 and 1 makes no difference for UL? //4
    nrHelper->SetUeAntennaAttribute("AntennaElement",
                                    PointerValue(CreateObject<IsotropicAntennaModel>()));

    nrHelper->SetUeRedCapAntennaAttribute ("NumRows", UintegerValue (mimo_layer));
    nrHelper->SetUeRedCapAntennaAttribute ("NumColumns", UintegerValue (mimo_layer));
    nrHelper->SetUeRedCapAntennaAttribute ("AntennaElement", PointerValue (CreateObject<IsotropicAntennaModel> ()));



    //implementeation of Ri not compatible with fixedMCS. Set to 2 but doesnt do anything due to an necessary workaround
    nrHelper->SetUePhyAttribute("UseFixedRi", BooleanValue(true));
    nrHelper->SetUePhyAttribute("FixedRankIndicator", UintegerValue(2));


    // Antennas for all the gNbs
    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4)); 
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8)); 
    nrHelper->SetGnbAntennaAttribute("AntennaElement",
                                     PointerValue(CreateObject<IsotropicAntennaModel>()));

    // One ResManager per gNB (must not share across cells). Lifetime: outlive Simulator::Run.
    std::string logDir = ResultDir+"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+"/Seed_"+std::to_string(seed)+"/";

    std::vector<std::unique_ptr<NrMacSchedulerRessourceManager>> schedManagers;
    schedManagers.reserve(gNbNum);
    NetDeviceContainer enbNetDev;
    for (uint16_t g = 0; g < gNbNum; ++g)
    {
        std::string cellLogDir = logDir + "gNb" + std::to_string(g) + "/";
        std::filesystem::create_directories(cellLogDir);
        auto mgr = std::make_unique<NrMacSchedulerRessourceManager>(
            pattern, 1, simTime, initTime / 1000, cellLogDir, bwpCount, use5MHz, rbPerBwp);
        mgr->SetDlCceReservation(dlCceReserve);
        mgr->SetFixDlPdcchPrefilter(fixDlPdcchPrefilter);
        mgr->SetGuardMarks(guardSchedMarks);
        mgr->SetFixDciRowSymbol(fixDciRowSymbol);
        NodeContainer oneGnb;
        oneGnb.Add(gNbNodes.Get(g));
        NetDeviceContainer d =
            nrHelper->InstallGnbDevice(oneGnb, allBwps, mimo_layer, mgr.get());
        enbNetDev.Add(d);
        schedManagers.push_back(std::move(mgr));
    }

    // Install and get the pointers to the NetDevices
    bool SdtUsable = false;
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice (embbUeNodes, allBwps,SdtUsable, chip_NR);
    NetDeviceContainer redCapUeNetDev = nrHelper->InstallRedCapUeDevice (redCapUeNodes, allBwps, SdtUsable, chip_RedCap,mimo_layer);
    ueNetDev.Add(redCapUeNetDev);
    g_ueDevs = ueNetDev;
    
    double polarizationFirstSubArray = (0.0 * M_PI) / 180.0;  // converting to radians
    double polarizationSecondSubArray = (90.0 * M_PI) / 180.0; // converting to radians
    for (uint32_t gi = 0; gi < enbNetDev.GetN(); ++gi)
    {
        ObjectVectorValue gnbSpectrumPhys;
        Ptr<NrSpectrumPhy> nrSpectrumPhy;
        nrHelper->GetGnbPhy(enbNetDev.Get(gi), 0)->GetAttribute("NrSpectrumPhyList", gnbSpectrumPhys);
        nrSpectrumPhy = gnbSpectrumPhys.Get(0)->GetObject<NrSpectrumPhy>();
        nrSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>()->SetAttribute(
            "PolSlantAngle",
            DoubleValue(polarizationFirstSubArray));
        if (gnbSpectrumPhys.GetN() == 2)
        {
            nrSpectrumPhy = gnbSpectrumPhys.Get(1)->GetObject<NrSpectrumPhy>();
            nrSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>()->SetAttribute(
                "PolSlantAngle",
                DoubleValue(polarizationSecondSubArray));
        }
    }


    // for UE (first device — eMBB if present, else RedCap)
    if (ueNetDev.GetN() > 0)
    {
        ObjectVectorValue ueSpectrumPhys;
        Ptr<NrSpectrumPhy> nrSpectrumPhy;
        nrHelper->GetUePhy(ueNetDev.Get(0), 0)->GetAttribute("NrSpectrumPhyList", ueSpectrumPhys);
        nrSpectrumPhy = ueSpectrumPhys.Get(0)->GetObject<NrSpectrumPhy>();
        nrSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>()->SetAttribute(
            "PolSlantAngle",
            DoubleValue(polarizationFirstSubArray));
        if (ueSpectrumPhys.GetN() == 2)
        {
            nrSpectrumPhy = ueSpectrumPhys.Get(1)->GetObject<NrSpectrumPhy>();
            nrSpectrumPhy->GetAntenna()->GetObject<UniformPlanarArray>()->SetAttribute(
                "PolSlantAngle",
                DoubleValue(polarizationSecondSubArray));
        }
    }



    int64_t randomStream = 2;
    randomStream += nrHelper->AssignStreams(enbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueNetDev, randomStream);


    // Manually set the attribute of each gNB netdevice and bandwidth part
    for (uint32_t gi = 0; gi < enbNetDev.GetN(); ++gi)
    {
        for(uint i = 0; i< bwpCount; ++i)
        {
            if(i < uint(numBwp_nonOverlapping))
            {
                nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)
                ->SetAttribute("Numerology", UintegerValue(numerology));
                nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)
                    ->SetAttribute("TxPower", DoubleValue(10 * log10(x/(bwpCount-2))));
                nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)->SetAttribute("Pattern", StringValue(pattern));
            }
            else{
                 nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)
                    ->SetAttribute("Numerology", UintegerValue(numerology));
                nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)
                    ->SetAttribute("TxPower", DoubleValue(10 * log10(x)));
                nrHelper->GetGnbPhy(enbNetDev.Get(gi), i)->SetAttribute("Pattern", StringValue(pattern));

            }
        }
    }

    for (auto it = enbNetDev.Begin(); it != enbNetDev.End(); ++it)
    {
        DynamicCast<NrGnbNetDevice>(*it)->UpdateConfig();
    }

    for (auto it = ueNetDev.Begin(); it != ueNetDev.End(); ++it)
    {
        DynamicCast<NrUeNetDevice>(*it)->UpdateConfig();
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    //----- Setting traffic models --------------------------------------------------------------------------------//

    // create the internet and install the IP stack on the UEs
    // get SGW/PGW and create a single RemoteHost
    Ptr<Node> pgw = epcHelper->GetPgwNode();
    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // connect a remoteHost to pgw. Setup routing too
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(Seconds(0.000)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);
    internet.Install(redCapUeNodes);
    internet.Install(embbUeNodes);
    Ipv4InterfaceContainer ueIpIface;
    ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));

    Ipv4Address remoteHostAddr = internetIpIfaces.GetAddress(1);

     // Set the default gateway for the UEs
    for (uint32_t j = 0; j < redCapUeNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(redCapUeNodes.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }
     for (uint32_t j = 0; j < embbUeNodes.GetN(); ++j)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(embbUeNodes.Get(j)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // attach UEs to the closest eNB before creating the dedicated flows
    nrHelper->AttachToClosestEnb(ueNetDev, enbNetDev);

    // install UDP applications
    uint16_t dlPort = 1;
    uint16_t ulPort = 1;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;

    uint64_t imsi;
    std::ofstream outFile_ports;
    std::string filename = ResultDir+"Ausgaben/"+std::to_string(packetSize) +"/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+ "/_ports_Seed"+std::to_string(seed)+".txt" ;
    outFile_ports.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile_ports.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }

    outFile_ports.setf(std::ios_base::fixed);


    uint accessbreak =20;
    const uint32_t redCapDevOffset = embbUeNodes.GetN(); // eMBB devices are installed first in ueNetDev

    // Install UDP applications for RedCap enddevices
    if(totalRedCapUes > 0){
        for (uint32_t u = 0; u < totalRedCapUes; ++u)
        {
            const uint32_t devIdx = redCapDevOffset + u;
            uint16_t port;
            int access = RaUeUniformVariable->GetValue (8000, 12000); // 
            enum EpsBearer::Qci q;
            if(enddeviceDownlinkstream)
            {
                port = dlPort;
                UdpClientHelper udpDlClient(ueIpIface.GetAddress(devIdx), dlPort);
                udpDlClient.SetAttribute("PacketSize", UintegerValue(packetSize));
                udpDlClient.SetAttribute("Interval", TimeValue(Seconds(transmissionIntervall)));
                udpDlClient.SetAttribute("ArrivalModel", UintegerValue(arrivalModel));
                udpDlClient.SetAttribute("OnMean", TimeValue(Seconds(onMeanS)));
                udpDlClient.SetAttribute("OffMean", TimeValue(Seconds(offMeanS)));
                udpDlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                udpDlClient.SetAttribute("StopApp", UintegerValue(udpAppStopTime));
                clientApps.Add(udpDlClient.Install(remoteHost));
                PacketSinkHelper psh(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), port));
                    serverApps.Add(psh.Install(redCapUeNodes.Get(u)));
            }
            else{
                port = ulPort;
                UdpClientHelper udpUlClient(remoteHostAddr, ulPort);
                udpUlClient.SetAttribute("PacketSize", UintegerValue(packetSize));
                udpUlClient.SetAttribute("Interval", TimeValue(Seconds(transmissionIntervall)));
                udpUlClient.SetAttribute("ArrivalModel", UintegerValue(arrivalModel));
                udpUlClient.SetAttribute("OnMean", TimeValue(Seconds(onMeanS)));
                udpUlClient.SetAttribute("OffMean", TimeValue(Seconds(offMeanS)));
                udpUlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                udpUlClient.SetAttribute("StopApp", UintegerValue(udpAppStopTime));
                clientApps.Add(udpUlClient.Install(redCapUeNodes.Get(u)));
                PacketSinkHelper psh(
                    "ns3::UdpSocketFactory",
                    InetSocketAddress(Ipv4Address::GetAny(), port));
                serverApps.Add(psh.Install(remoteHost));
            }

            serverApps.Get(serverApps.GetN()-1)->SetStartTime (MilliSeconds (access));
            clientApps.Get(serverApps.GetN()-1)->SetStartTime (MilliSeconds (access));

            imsi = nrHelper->GetUePhy(ueNetDev.Get(devIdx),0)->GetImsi();
            outFile_ports << "enddevice,datastream," << port << "," << access << "," << imsi << "\n";

            Ptr<EpcTft> tft = Create<EpcTft>();
            EpcTft::PacketFilter pfD;
            pfD.localPortStart = port;
            pfD.localPortEnd = port;
            ++port;
            if(enddeviceDownlinkstream){
                ++dlPort;
            }
            else{
                ++ulPort;
            }
            tft->Add(pfD);

            q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
            EpsBearer bearerD(q);
            nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(devIdx), bearerD, tft);



            //add initialization ul-transmission add the start of the simulation
            PacketSinkHelper ulInitPacketSinkHelper(
                "ns3::UdpSocketFactory",
                InetSocketAddress(Ipv4Address::GetAny(), ulPort));
            serverApps.Add(ulInitPacketSinkHelper.Install(remoteHost));
            UdpClientHelper ulInitClient(remoteHostAddr, ulPort);
            ulInitClient.SetAttribute("PacketSize", UintegerValue(20));
            ulInitClient.SetAttribute("Interval", TimeValue(Seconds(1)));
            ulInitClient.SetAttribute("MaxPackets", UintegerValue(1));
            access = 10+ (u * accessbreak);
            ulInitClient.SetAttribute("StopApp", UintegerValue(access));
            clientApps.Add(ulInitClient.Install(redCapUeNodes.Get(u)));
            serverApps.Get(serverApps.GetN()-1)->SetStartTime (MilliSeconds (access));
            clientApps.Get(serverApps.GetN()-1)->SetStartTime (MilliSeconds (access));
            Ptr<EpcTft> tftInit = Create<EpcTft>();
            EpcTft::PacketFilter ulpfInit;
            ulpfInit.remotePortStart = ulPort;
            ulpfInit.remotePortEnd = ulPort;
            ++ulPort;
            tftInit->Add(ulpfInit);
            q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
            EpsBearer bearerZ(q);
            nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(devIdx), bearerZ, tftInit);


        }
    }

    // eMBB coexistence traffic (same UL/DL direction as RedCap; distinct packet size)
    if (embbUes > 0)
    {
        for (uint32_t u = 0; u < embbUes; ++u)
        {
            const uint32_t devIdx = u; // eMBB first in ueNetDev
            uint16_t port;
            int access = RaUeUniformVariable->GetValue(6000, 9000);
            enum EpsBearer::Qci q;
            if (enddeviceDownlinkstream)
            {
                port = dlPort;
                UdpClientHelper udpDlClient(ueIpIface.GetAddress(devIdx), dlPort);
                udpDlClient.SetAttribute("PacketSize", UintegerValue(embbPacketSize));
                udpDlClient.SetAttribute("Interval", TimeValue(Seconds(embbTransmissionIntervall)));
                udpDlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                udpDlClient.SetAttribute("StopApp", UintegerValue(udpAppStopTime));
                clientApps.Add(udpDlClient.Install(remoteHost));
                PacketSinkHelper psh("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port));
                serverApps.Add(psh.Install(embbUeNodes.Get(u)));
            }
            else
            {
                port = ulPort;
                UdpClientHelper udpUlClient(remoteHostAddr, ulPort);
                udpUlClient.SetAttribute("PacketSize", UintegerValue(embbPacketSize));
                udpUlClient.SetAttribute("Interval", TimeValue(Seconds(embbTransmissionIntervall)));
                udpUlClient.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
                udpUlClient.SetAttribute("StopApp", UintegerValue(udpAppStopTime));
                clientApps.Add(udpUlClient.Install(embbUeNodes.Get(u)));
                PacketSinkHelper psh("ns3::UdpSocketFactory",
                                     InetSocketAddress(Ipv4Address::GetAny(), port));
                serverApps.Add(psh.Install(remoteHost));
            }

            serverApps.Get(serverApps.GetN() - 1)->SetStartTime(MilliSeconds(access));
            clientApps.Get(serverApps.GetN() - 1)->SetStartTime(MilliSeconds(access));

            imsi = nrHelper->GetUePhy(ueNetDev.Get(devIdx), 0)->GetImsi();
            outFile_ports << "embb,datastream," << port << "," << access << "," << imsi << "\n";

            Ptr<EpcTft> tft = Create<EpcTft>();
            EpcTft::PacketFilter pfD;
            pfD.localPortStart = port;
            pfD.localPortEnd = port;
            if (enddeviceDownlinkstream)
            {
                ++dlPort;
            }
            else
            {
                ++ulPort;
            }
            tft->Add(pfD);
            q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
            EpsBearer bearerD(q);
            nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(devIdx), bearerD, tft);

            PacketSinkHelper ulInitPacketSinkHelper(
                "ns3::UdpSocketFactory",
                InetSocketAddress(Ipv4Address::GetAny(), ulPort));
            serverApps.Add(ulInitPacketSinkHelper.Install(remoteHost));
            UdpClientHelper ulInitClient(remoteHostAddr, ulPort);
            ulInitClient.SetAttribute("PacketSize", UintegerValue(20));
            ulInitClient.SetAttribute("Interval", TimeValue(Seconds(1)));
            ulInitClient.SetAttribute("MaxPackets", UintegerValue(1));
            access = 5 + (u * accessbreak);
            ulInitClient.SetAttribute("StopApp", UintegerValue(access));
            clientApps.Add(ulInitClient.Install(embbUeNodes.Get(u)));
            serverApps.Get(serverApps.GetN() - 1)->SetStartTime(MilliSeconds(access));
            clientApps.Get(serverApps.GetN() - 1)->SetStartTime(MilliSeconds(access));
            Ptr<EpcTft> tftInit = Create<EpcTft>();
            EpcTft::PacketFilter ulpfInit;
            ulpfInit.remotePortStart = ulPort;
            ulpfInit.remotePortEnd = ulPort;
            ++ulPort;
            tftInit->Add(ulpfInit);
            q = EpsBearer::NGBR_VIDEO_TCP_DEFAULT;
            EpsBearer bearerZ(q);
            nrHelper->ActivateDedicatedEpsBearer(ueNetDev.Get(devIdx), bearerZ, tftInit);
        }
    }
       
    outFile_ports.close();

    // IoTJ R1: servers (receivers) stay up during the drain period; clients stop generating at
    // the original time (udpAppStopTime / clientApps.Stop below).
    serverApps.Stop(Seconds(simTime+initTime/1000 + drainS));
    clientApps.Stop(Seconds(simTime+initTime/1000));

    // enable the traces provided by the nr module
     //nrHelper->EnableTraces();
     //nrHelper->GetPhyRxTrace()->SetResultsFolder(logDir);

    FlowMonitorHelper flowmonHelper;
    NodeContainer endpointNodes;
    endpointNodes.Add(remoteHost);
    endpointNodes.Add(redCapUeNodes);
    endpointNodes.Add(embbUeNodes);
    Ptr<ns3::FlowMonitor> monitor = flowmonHelper.Install(endpointNodes);
    monitor->SetAttribute("DelayBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("JitterBinWidth", DoubleValue(0.001));
    monitor->SetAttribute("PacketSizeBinWidth", DoubleValue(20));
    Simulator::Schedule(Seconds(0.1), &progressPrint);  
    g_tsMonitor = monitor;
    g_tsFile.open(ResultDir + "timeseries_Seed" + std::to_string(seed) + ".csv",
                  std::ofstream::out | std::ofstream::trunc);
    g_tsFile << "t_s,tx_bytes,rx_bytes,ul_backlog_bytes,ues_with_backlog,ul_unacked_bytes,softfails,sr_unanswered,ul_not_sched_pdcch,ul_grants\n";
    Simulator::Schedule(Seconds(1.0), &timeSeriesSnapshot);
    // Snapshot after the last client send event at the traffic-stop instant.
    Simulator::Schedule(Seconds(simTime + initTime / 1000) - NanoSeconds(1), &trafficStopSnapshot);
    std::cout << "R1Timing: trafficStartMs=" << udpAppStartTime << "(+per-UE access offset)"
              << " trafficStopS=" << (simTime + initTime / 1000) << " simEndS="
              << (simTime + initTime / 1000 + drainS) << std::endl;
    Simulator::Stop(Seconds(simTime+initTime/1000 + drainS));
    Simulator::Run();

    /*
     * To check what was installed in the memory, i.e., BWPs of eNb Device, and its configuration.
     * Example is: Node 1 -> Device 0 -> BandwidthPartMap -> {0,1} BWPs -> NrGnbPhy ->
    NrPhyMacCommong-> Numerology, Bandwidth, ... GtkConfigStore config; config.ConfigureAttributes
    ();
    */


   //log spectral ressources
    //schedmanager.logRessources(enddevicesNumPergNb,seed);

    RessourceUsageStats resStats{};
    UeRessourceUsage ueStats{};
    for (auto& mgr : schedManagers)
    {
        mgr->collectFinalResUsage();
        auto cellRes = mgr->getCapacityUsage();
        resStats.ControlRessources += cellRes.ControlRessources;
        resStats.freeRessources_DL += cellRes.freeRessources_DL;
        resStats.freeRessources_UL += cellRes.freeRessources_UL;
        resStats.PdcchRessources += cellRes.PdcchRessources;
        resStats.PdcchUsed += cellRes.PdcchUsed;
        resStats.PrachRessources += cellRes.PrachRessources;
        resStats.PucchRessources += cellRes.PucchRessources;
        resStats.usedRessources_DL += cellRes.usedRessources_DL;
        resStats.usedRessources_UL += cellRes.usedRessources_UL;

        auto cellUe = mgr->getUeSpecificCapacityUsage();
        for (const auto& kv : cellUe.ueResMap)
        {
            ueStats.ueResMap[kv.first] += kv.second;
        }
    }

    double capacityUsage_DL = double(resStats.usedRessources_DL)/(resStats.usedRessources_DL+resStats.freeRessources_DL);
    double capacityUsage_UL = double(resStats.usedRessources_UL)/(resStats.usedRessources_UL+resStats.freeRessources_UL);

    std::cout << "Capacity usage DL (sum cells): "<<capacityUsage_DL << std::endl;
    std::cout << "Capacity usage UL (sum cells): "<<capacityUsage_UL << std::endl;

    // Print per-flow statistics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    double averageFlowThroughput = 0.0;
    double averageFlowDelay = 0.0;

    std::ofstream outFile;
    //filename = outputDir + "/" + simTag + ".txt";
    filename = ResultDir+"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig  +"/"+std::to_string(transmitPower)+"/outFile_Seed"+std::to_string(seed)+"_"+".txt" ;
    outFile.open(filename.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!outFile.is_open())
    {
        std::cerr << "Can't open file " << filename << std::endl;
        return 1;
    }
    outFile.setf(std::ios_base::fixed);
    uint64_t totalTxPackets = 0;
    uint64_t totalTxBytes = 0;
    uint64_t totalRxPackets = 0;
    uint64_t totalRxBytes = 0;

    outFile << "Ressource stats" << "\n";
    outFile << "ControlRessources: "<<resStats.ControlRessources << "\n";
    outFile << "freeRessources_DL: "<<resStats.freeRessources_DL << "\n";
    outFile << "freeRessources_UL: "<<resStats.freeRessources_UL << "\n";
    outFile << "PdcchRessources: "<<resStats.PdcchRessources << "\n";
    outFile << "PdcchUsed: "<<resStats.PdcchUsed << "\n";
    outFile << "PrachRessources: "<<resStats.PrachRessources << "\n";
    outFile << "PucchRessources: "<<resStats.PucchRessources << "\n";
    outFile << "usedRessources_DL:"<<resStats.usedRessources_DL << "\n";
    outFile << "usedRessources_UL:"<<resStats.usedRessources_UL << "\n";

    outFile<< "Capacity usage DL: " << capacityUsage_DL << "\n";
    outFile<< "Capacity usage UL: " << capacityUsage_UL << "\n";

    outFile<< "Device specific ressource usage:"<<"\n";
    for(uint i=1;i<=totalRedCapUes;++i)
    {
        outFile<<"Ressource usage for device "<<i<<": "<<ueStats.ueResMap[i]<<" ressource blocks (enddevice)"<<"\n";
    }


    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::stringstream protoStream;
        protoStream << (uint16_t)t.protocol;
        if (t.protocol == 6)
        {
            protoStream.str("TCP");
        }
        if (t.protocol == 17)
        {
            protoStream.str("UDP");
        }
        outFile << "Flow " << i->first << " (" << t.sourceAddress << ":" << t.sourcePort << " -> "
                << t.destinationAddress << ":" << t.destinationPort << ") proto "
                << protoStream.str() << "\n";
        totalTxPackets = totalTxPackets + i->second.txPackets;
        totalTxBytes = totalTxBytes + i->second.txBytes;
        outFile << "  Tx Packets: " << i->second.txPackets << "\n";
        outFile << "  Tx Bytes:   " << i->second.txBytes << "\n";
        outFile << "  TxOffered:  "
                << i->second.txBytes * 8.0 / (simTime - udpAppStartTime/1000.0) / 1000 / 1000 << " Mbps\n";
        outFile << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        if (i->second.rxPackets > 0)
        {
            // Measure the duration of the flow from receiver's perspective
            double rxDuration = i->second.timeLastRxPacket.GetSeconds () -
             i->second.timeFirstTxPacket.GetSeconds ();
            //double rxDuration = (simTime - udpAppStartTime/1000.0);

            averageFlowThroughput += i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000;
            averageFlowDelay += 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;

            outFile << "  Throughput: " << i->second.rxBytes * 8.0 / rxDuration / 1000 / 1000
                    << " Mbps\n";
            outFile << "  Mean delay:  "
                    << 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets << " ms\n";
            // outFile << "  Mean upt:  " << i->second.uptSum / i->second.rxPackets / 1000/1000 << "
            // Mbps \n";
            outFile << "  Mean jitter:  "
                    << 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets << " ms\n";
        }
        else
        {
            outFile << "  Throughput:  0 Mbps\n";
            outFile << "  Mean delay:  0 ms\n";
            outFile << "  Mean jitter: 0 ms\n";
        }
        totalRxPackets = totalRxPackets + i->second.rxPackets;
        totalRxBytes = totalRxBytes + i->second.rxBytes;
        outFile << "  Rx Packets: " << i->second.rxPackets << "\n";
        if(i->second.txPackets == 0){
            outFile << "  PDR: 100%" << "%\n";
        }
        else{
            outFile << "  PDR: " << (i->second.rxPackets*1.0/(i->second.txPackets*1.0))*100 << "%\n";
        }
    }

    outFile << "\n\n  Flow time: " << (simTime - udpAppStartTime/1000.0) << "\n";
    outFile << "  Mean flow throughput: " << averageFlowThroughput / stats.size() << "\n";
    outFile << "  Mean flow delay: " << averageFlowDelay / stats.size() << "\n";
    outFile << "  Total Tx Packets: " << totalTxPackets << "\n";
    outFile << "  Total Rx Packets: " << totalRxPackets << "\n";
    outFile << "  Total Tx Bytes: " << totalTxBytes << "\n";
    outFile << "  Total Rx Bytes: " << totalRxBytes << "\n";
    if (totalTxPackets > 0){
        outFile << "  Total PDR: " << totalRxPackets*100.0 / totalTxPackets << "%\n";
    }
    else{
        outFile << "  Total PDR: " << -1 << "%\n";
    }

    outFile.close();

    std::ifstream f(filename.c_str());

    if (f.is_open())
    {
        std::cout << f.rdbuf();
    }

    //Create a files for Data processing
    std::ofstream dataFile;
    std::string delayFileName = ResultDir+"Delay/"+std::to_string(packetSize) +"/"+usedRedCapConfig+"/"+std::to_string(transmitPower)+"/Seed"+std::to_string(seed)+ ".txt";
    dataFile.open(delayFileName.c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!dataFile.is_open())
    {
        std::cerr << "Can't open file " << delayFileName << std::endl;
        return 1;
    }

    dataFile.setf(std::ios_base::fixed);
    
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
         i != stats.end();
         ++i)
    {
      for(uint32_t indexDelay = 0; indexDelay <i->second.allDelays.size(); ++indexDelay)
      {
        if(indexDelay>0)
        {
          dataFile<<":";
        }
        dataFile<<i->second.allDelays.at(indexDelay);

      }
      dataFile<<"\n";
    }

    
    dataFile.close();


    std::string resUsage =  ResultDir+"Ressourcen/"+std::to_string(packetSize)+"/"+usedRedCapConfig +"/"+std::to_string(transmitPower)+ "/Seed"+std::to_string(seed)+".txt";
     dataFile.open(resUsage .c_str(), std::ofstream::out | std::ofstream::trunc);
    if (!dataFile.is_open())
    {
        std::cerr << "Can't open file " << resUsage << std::endl;
        return 1;
    }

    dataFile.setf(std::ios_base::fixed);
    dataFile << "freeRessources_DL:freeRessources_UL:usedRessources_DL:usedRessources_UL:PrachRessources:PdcchRessources:PdcchUsed:PucchRessources:ControlRessources"<<"\n";
    
    dataFile<<resStats.freeRessources_DL<<":"<<resStats.freeRessources_UL<<":"<<resStats.usedRessources_DL<<":"<<resStats.usedRessources_UL<<":"<<
        resStats.PrachRessources<<":"<<resStats.PdcchRessources<<":"<<resStats.PdcchUsed<<":"<<resStats.PucchRessources<<":"<<resStats.ControlRessources<<"\n";

    
    dataFile.close();


    std::string flowOutput = ResultDir+"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower) + "/flowOutput" + "_Seed"+std::to_string(seed)+".txt";
    dataFile.open(flowOutput .c_str(), std::ofstream::out | std::ofstream::trunc);
    dataFile.setf(std::ios_base::fixed);

    // Map UE IPv4 addresses to RedCap vs eMBB for coexistence parsing
    std::set<uint32_t> redCapAddrs;
    std::set<uint32_t> embbAddrs;
    for (uint32_t u = 0; u < redCapUeNodes.GetN(); ++u)
    {
        redCapAddrs.insert(ueIpIface.GetAddress(redCapDevOffset + u).Get());
    }
    for (uint32_t u = 0; u < embbUeNodes.GetN(); ++u)
    {
        embbAddrs.insert(ueIpIface.GetAddress(u).Get());
    }

    dataFile << "Tx Packets" <<":"<< "Rx Packets"<<":"<<"Mean delay"<<":"<<"Mean jitter"<<":"<<"Tx Bytes"<<":"<<"Rx Bytes"<<":"<<"UeClass"<<"\n";
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin();
        i != stats.end();
        ++i)
    {
        double meanDelayMs = 0.0;
        double meanJitterMs = 0.0;
        if (i->second.rxPackets > 0)
        {
            meanDelayMs = 1000 * i->second.delaySum.GetSeconds() / i->second.rxPackets;
            meanJitterMs = 1000 * i->second.jitterSum.GetSeconds() / i->second.rxPackets;
        }
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);
        std::string ueClass = "other";
        if (redCapAddrs.count(t.sourceAddress.Get()) || redCapAddrs.count(t.destinationAddress.Get()))
        {
            ueClass = "redcap";
        }
        else if (embbAddrs.count(t.sourceAddress.Get()) || embbAddrs.count(t.destinationAddress.Get()))
        {
            ueClass = "embb";
        }
        dataFile << i->second.txPackets << ":" << i->second.rxPackets << ":" << meanDelayMs << ":"
                 << meanJitterMs << ":" << i->second.txBytes << ":" << i->second.rxBytes << ":"
                 << ueClass << "\n\n";
    }
    dataFile.close();

    std::string accessStatsPath = ResultDir+"Ausgaben/"+std::to_string(packetSize)+"/"+usedRedCapConfig+"/"+std::to_string(transmitPower) + "/accessStats_Seed"+std::to_string(seed)+".txt";
    NrAccessStats::Dump(accessStatsPath);
    std::cout << "AccessStats: preamble_tx=" << NrAccessStats::GetPreambleTx()
              << " rar_timeout=" << NrAccessStats::GetRarTimeout()
              << " ra_success=" << NrAccessStats::GetRaSuccess()
              << " ra_failed=" << NrAccessStats::GetRaFailed()
              << " rrc_connected=" << NrAccessStats::GetRrcConnected()
              << " rrc_timeout=" << NrAccessStats::GetRrcTimeout() << std::endl;

    {
        // IoTJ R1 revision statistics.
        timeSeriesSnapshot();
        g_tsFile.close();
        const RlcBacklog bl = readRlcBacklog();
        const uint64_t queued = bl.txon + bl.retx;
        const uint64_t queuedUes = bl.uesWithBacklog;
        NrCampaignStats::Set("queue.ul_backlog_bytes_at_end", queued);
        NrCampaignStats::Set("queue.ul_retx_bytes_at_end", bl.retx);
        NrCampaignStats::Set("queue.ul_unacked_bytes_at_end", bl.txed);
        NrCampaignStats::Set("queue.ues_with_backlog_at_end", queuedUes);
        NrCampaignStats::Set("res.pdcch_coreset_cells_used", resStats.PdcchUsed);
        NrCampaignStats::Set("res.pdcch_coreset_cells_free", resStats.PdcchRessources);
        std::string revPath = ResultDir + "revisionStats_Seed" + std::to_string(seed) + ".txt";
        std::ofstream rev(revPath, std::ofstream::out | std::ofstream::trunc);
        NrCampaignStats::Dump(rev);
        rev.close();
        std::cout << "R1Stats: softfails=" << NrCampaignStats::TotalSoftFails()
                  << " ul_backlog_bytes_at_end=" << queued << " ues_with_backlog=" << queuedUes
                  << " sr_sent=" << NrCampaignStats::Get("ue.sr_sent")
                  << " sr_unanswered=" << NrCampaignStats::Get("ue.sr_unanswered")
                  << " ul_grants=" << NrCampaignStats::Get("sched.ul.grants")
                  << " ul_not_sched_pdcch=" << NrCampaignStats::Get("sched.ul.not_scheduled.pdcch")
                  << " file=" << revPath << std::endl;
    }
    g_tsMonitor = nullptr;
    g_ueDevs = NetDeviceContainer();
    Simulator::Destroy();
    return 0;
}
