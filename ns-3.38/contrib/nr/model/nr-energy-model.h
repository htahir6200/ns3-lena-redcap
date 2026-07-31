/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2022 Communication Networks Institute at TU Dortmund University
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
 * Author: Mike Dabrowski based on prior work from Tim Gebauer 
 */


#ifndef NR_ENERGY_H
#define NR_ENERGY_H

#include <ns3/object.h>
#include <ns3/simulator.h>
#include <ns3/li-ion-energy-source.h>
#include <ns3/event-id.h>
#include <cmath>
#include <array>
#include <map>
#include <vector>
#include "ns3/string.h" // Für StringValue
#include "ns3/attribute.h" // Für MakeStringAccessor und MakeStringChecker

namespace ns3 {

struct EnergyParameters{
    double startFrequency;
    double endFrequency;

    double m_psmPower;
    double m_drxPower;
    double m_edrxPower;
    double m_uplinkPower;
    double m_downlinkPower; 
    double m_idlePower ;
    double m_connectedPower; 
    //const std::vector<int8_t> m_uplinkPowerBorders = {-5,0,5,10,15,20,23,INT8_MAX};
    const std::vector<int8_t> m_uplinkPowerBorders = {-28,-27,-26,-25,-24,-23,-22,-21,-20,-19,-18,-17,-16,-15,-14,-13,-12,-11,-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,INT8_MAX};

    std::vector<double> m_uplinkPowerVec;

    EnergyParameters& setStartFrequency(double value) { startFrequency = value; return *this; }
    EnergyParameters& setEndFrequency(double value) { endFrequency = value; return *this; }
    EnergyParameters& setPsmPower(double value) { m_psmPower = value; return *this; }
    EnergyParameters& setDrxPower(double value) { m_drxPower = value; return *this; }
    EnergyParameters& setEdrxPower(double value) { m_edrxPower = value; return *this; }
    EnergyParameters& setUplinkPower(double value) { m_uplinkPower = value; return *this; }
    EnergyParameters& setDownlinkPower(double value) { m_downlinkPower = value; return *this; }
    EnergyParameters& setIdlePower(double value) { m_idlePower = value; return *this; }
    EnergyParameters& setConnectedPower(double value) { m_connectedPower = value; return *this; }

};


class NrChip {
protected:
    double m_psmPower; 
    double m_drxPower;
    double m_edrxPower;
    double m_uplinkPower;
    double m_downlinkPower;
    double m_idlePower;
    double m_connectedPower;
    
    
public:
    NrChip():
        m_psmPower(0),
        m_drxPower(0),
        m_edrxPower(0),
        m_uplinkPower(0),
        m_downlinkPower(0),
        m_idlePower(0){}
    NrChip(double psmPower, double drxPower, double edrxPower, double uplinkPower, double downlinkPower, double idlePower,double connectedPower, bool useEdrx):
        m_psmPower(psmPower),
        m_drxPower(drxPower),
        m_edrxPower(edrxPower),
        m_uplinkPower(uplinkPower),
        m_downlinkPower(downlinkPower),
        m_idlePower(idlePower),
        m_connectedPower(connectedPower),
        m_useEdrx(useEdrx){}
    ~NrChip(){}

    void SetPsmPower(double Power);
    void SetDrxPower(double Power);
    void SetEdrxPower(double Power);
    void SetUplinkPower(double Power);
    void SetDownlinkPower(double Power);
    void SetIdlePower(double Power);

    double GetPsmPower();
    double GetDrxPower();
    double GetEdrxPower();
    double GetUplinkPower();
    double GetDownlinkPower();
    double GetIdlePower();
    double GetConnectedPower();


    void SetEnergyParameterFromCentralFrequency(double centralFrequency,double transmitPower);
    bool m_useEdrx;
    std::vector<EnergyParameters> energyParametersVec;
};



static std::vector<double> m_ulPowerData ={
    0.318510208,
    0.305401305,
    0.318270832,
    0.314280824,
    0.317394371,
    0.313066258,
    0.373759544,
    0.315319837,
    0.312029515,
    0.312849997,
    0.301473836,
    0.308687424,
    0.300186656,
    0.307059309,
    0.304546786,
    0.366621138,
    0.302612829,
    0.295153611,
    0.296417769,
    0.303304451,
    0.303893727,
    0.3102071,
    0.311346158,
    0.312911068,
    0.313958926,
    0.304368534,
    0.305600431,
    0.313254078,
    0.324270292,
    0.330622415,
    0.330417152,
    0.327285911,
    0.494980145,
    0.44716214,
    0.472233284,
    0.470788281,
    0.509648549,
    0.536554061,
    0.561021071,
    0.695108717,
    0.729044055,
    0.812012419,
    1.045590136,
    1.147881135,
    1.210342823,
    1.359629991,
    1.501275159,
    1.732071361,
    2.030911228,
    2.016855211,
    2.00831749,
    2.003081918,
    2.013835034,
    2.008972726,
    1.991845081
};




class RM520N : public NrChip{ //NR
public:

    

    RM520N(double centralFrequency, double transmitPower){
        EnergyParameters energyParameters1;
        energyParameters1.setStartFrequency(0)
                        .setEndFrequency(1e9)
                        .setPsmPower(NAN)
                        .setDrxPower(0)
                        .setEdrxPower(NAN)
                        .setUplinkPower(0)
                        .setDownlinkPower(0)
                        .setIdlePower(NAN)
                        .setConnectedPower(0);
        energyParametersVec.push_back(energyParameters1);

        EnergyParameters energyParameters2;
        energyParameters2.setStartFrequency(1e9)
                        .setEndFrequency(0)
                        .setPsmPower(NAN)
                        .setDrxPower(0)
                        .setEdrxPower(NAN)
                        .setUplinkPower(0)
                        .setDownlinkPower(0)
                        .setIdlePower(NAN)
                        .setConnectedPower(0);
        energyParametersVec.push_back(energyParameters2);

        EnergyParameters energyParameters3;
        energyParameters3.setStartFrequency(2e9)
                        .setEndFrequency(5e9)
                        .setPsmPower(NAN)
                        .setDrxPower(34.8*std::pow(10,-3))
                        .setEdrxPower(NAN)
                        //.setUplinkPower(1104*std::pow(10,-3))
                        .setDownlinkPower(1104*std::pow(10,-3))
                        .setIdlePower(NAN)
                        .setConnectedPower(588.2*std::pow(10,-3));
        //energyParameters3.m_uplinkPowerVec.insert(energyParameters3.m_uplinkPowerVec.end(),{(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3)),(1104*std::pow(10,-3))});
        energyParameters3.m_uplinkPowerVec.insert(energyParameters3.m_uplinkPowerVec.end(),m_ulPowerData.begin(),m_ulPowerData.end());
        energyParametersVec.push_back(energyParameters3);

        SetEnergyParameterFromCentralFrequency(centralFrequency,transmitPower);
    
        
        m_useEdrx = false;
    };


  
};

class RG255C : public NrChip{ //RedCap
public:
    RG255C(double centralFrequency, double transmitPower){
        EnergyParameters energyParameters1;
        energyParameters1.setStartFrequency(0)
                        .setEndFrequency(1e9)
                        .setPsmPower(NAN)
                        .setDrxPower(NAN)
                        .setEdrxPower(0)
                        .setUplinkPower(0)
                        .setDownlinkPower(0)
                        .setIdlePower(NAN)
                        .setConnectedPower(0);
        energyParametersVec.push_back(energyParameters1);

        EnergyParameters energyParameters2;
        energyParameters2.setStartFrequency(1e9)
                        .setEndFrequency(0)
                        .setPsmPower(NAN)
                        .setDrxPower(NAN)
                        .setEdrxPower(0)
                        .setUplinkPower(0)
                        .setDownlinkPower(0)
                        .setIdlePower(NAN)
                        .setConnectedPower(0);
        energyParametersVec.push_back(energyParameters2);

        EnergyParameters energyParameters3;
        energyParameters3.setStartFrequency(2e9)
                        .setEndFrequency(5e9)
                        .setPsmPower(NAN)
                        // PTW listen power used when NrEnergyModel::EnableEdrxPtw=true
                        .setDrxPower(100.0*std::pow(10,-3))
                        // Flat eDRX average (Phase A/B). Deep sleep between PTW uses
                        // NrEnergyModel::DeepEdrxPowerW (~9.86 mW) when PTW is enabled.
                        .setEdrxPower(21.8*std::pow(10,-3))
                        .setDownlinkPower(366.3*std::pow(10,-3))
                        .setIdlePower(NAN)
                        .setConnectedPower(294.6*std::pow(10,-3));
        //energyParameters3.m_uplinkPowerVec.insert(energyParameters3.m_uplinkPowerVec.end(),{(20.6036*std::pow(10,-3)),(21.1063*std::pow(10,-3)),(24.5477*std::pow(10,-3)),(27.9953*std::pow(10,-3)),(33.3901*std::pow(10,-3)),(57.5434*std::pow(10,-3)),(98.5977*std::pow(10,-3)),(96.832*std::pow(10,-3))});
        energyParameters3.m_uplinkPowerVec.insert(energyParameters3.m_uplinkPowerVec.end(),m_ulPowerData.begin(),m_ulPowerData.end());
        energyParametersVec.push_back(energyParameters3);

        SetEnergyParameterFromCentralFrequency(centralFrequency,transmitPower);
        m_useEdrx = true;
    };

private:
    EnergyParameters energyParameters1;
    EnergyParameters energyParameters2;
    EnergyParameters energyParameters3;
};


/**
 * \brief Per-UE chip energy accounting with optional eDRX PTW duty cycle.
 *
 * Phase C research model (TS 38.304): when EnableEdrxPtw is true, Inactive/Idle
 * eDRX is billed as PTW listen (RRC_IDLE_EDRX_PTW) plus deep sleep (RRC_IDLE_EDRX).
 * Not a full idle paging PHY — PTW is a periodic duty cycle from ConfigureEdrxPtw().
 */
class NrEnergyModel : public Object
{

public: 
    enum class PowerState{
        RRC_IDLE,
        RRC_IDLE_DRX, 
        RRC_IDLE_EDRX,      //!< deep eDRX (PTW on) or flat eDRX (PTW off)
        RRC_IDLE_EDRX_PTW,  //!< PTW listen / paging monitor
        RRC_Connected,
        RRC_SENDING_PRACH,
        RRC_SENDING_PUSCH, 
        RRC_RECEIVING_PDSCH, 
        OFF,
        NUM_STATES
    }powerState;

    static constexpr std::array<const char*, uint(PowerState::NUM_STATES)> PowerStateNames = {
        "RRC_IDLE",
        "RRC_IDLE_DRX",
        "RRC_IDLE_EDRX",
        "RRC_IDLE_EDRX_PTW",
        "RRC_Connected",
        "RRC_SENDING_PRACH",
        "RRC_SENDING_PUSCH",
        "RRC_RECEIVING_PDSCH",
        "OFF"};

    static TypeId GetTypeId();

    NrEnergyModel();
    NrEnergyModel(NrChip module,uint32_t startTimeMs);
   
    ~NrEnergyModel() override;
        
    void DoNotifyStateChange(PowerState newState,uint64_t imsi);
    void DoNotifyStateChange(PowerState newState);
    PowerState DoGetState();
    void ActivateEnergyModel();
    double GetEnergyRemaining();
    double GetEnergyRemainingFraction();

    /** RAN eDRX cycle in radio frames (1 RF = 10 ms). Starts PTW if enabled. */
    void ConfigureEdrxPtw(uint32_t edrxRadioFrames);
    void StopEdrxPtw();

    /**
     * \brief Set process-wide PTW defaults before UE energy models are created.
     * unique_ptr construction does not apply Config::SetDefault via CreateObject.
     */
    static void SetGlobalPtwConfig(bool enable,
                                   uint32_t ptwLengthMs,
                                   double deepEdrxPowerW = 9.8627e-3,
                                   double ptwListenPowerW = 0.10);

    std::string m_outputDir;

private:
    void BillTransition(PowerState newState);
    void EnterPtwWindow();
    void LeavePtwWindow();
    void ScheduleNextPtwCycle();
    bool IsEdrxFamily(PowerState s) const;
    double PowerWattsForState(PowerState s);

    Ptr<LiIonEnergySource> m_battery; // Battery model
    NrChip m_module; // Current Nr Module
    PowerState m_lastState; // Current Powerstate
    PowerState m_savedInactiveState; // Current Powerstate
    Time m_lastStateChange;
    std::map<PowerState, uint64_t> m_timeSpendInState; // Statistics
    std::map<PowerState, double> m_energySpendInState; // Statistics
    std::vector<std::pair<PowerState,uint32_t>> m_states;
    uint64_t m_imsi;
    bool m_isActive {false};

    bool m_enableEdrxPtw{false};
    uint32_t m_ptwLengthMs{1280};
    double m_deepEdrxPowerW{9.8627e-3};
    double m_ptwListenPowerW{0.10};
    uint32_t m_edrxCycleMs{2560};
    bool m_ptwScheduleRunning{false};
    EventId m_ptwEndEvent;
    EventId m_ptwCycleEvent;

    static bool g_cfgEnableEdrxPtw;
    static uint32_t g_cfgPtwLengthMs;
    static double g_cfgDeepEdrxPowerW;
    static double g_cfgPtwListenPowerW;
};

}
#endif 