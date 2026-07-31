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
 * Phase C: timed eDRX PTW energy duty cycle
 */

#include "nr-energy-model.h"

#include <ns3/boolean.h>
#include <ns3/double.h>
#include <ns3/log.h>
#include <ns3/uinteger.h>

#include <fstream>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrEnergyModel");
NS_OBJECT_ENSURE_REGISTERED(NrEnergyModel);

bool NrEnergyModel::g_cfgEnableEdrxPtw = false;
uint32_t NrEnergyModel::g_cfgPtwLengthMs = 1280;
double NrEnergyModel::g_cfgDeepEdrxPowerW = 9.8627e-3;
double NrEnergyModel::g_cfgPtwListenPowerW = 0.10;

void
NrChip::SetDownlinkPower(double power)
{
    m_downlinkPower = power;
}
void
NrChip::SetUplinkPower(double power)
{
    m_uplinkPower = power;
}
void
NrChip::SetEdrxPower(double power)
{
    m_edrxPower = power;
}
void
NrChip::SetDrxPower(double power)
{
    m_drxPower = power;
}
void
NrChip::SetPsmPower(double power)
{
    m_psmPower = power;
}
void
NrChip::SetIdlePower(double power)
{
    m_idlePower = power;
}
double
NrChip::GetDownlinkPower()
{
    return m_downlinkPower;
}
double
NrChip::GetUplinkPower()
{
    return m_uplinkPower;
}
double
NrChip::GetEdrxPower()
{
    return m_edrxPower;
}
double
NrChip::GetDrxPower()
{
    return m_drxPower;
}
double
NrChip::GetPsmPower()
{
    return m_psmPower;
}
double
NrChip::GetIdlePower()
{
    return m_idlePower;
}
double
NrChip::GetConnectedPower()
{
    return m_connectedPower;
}

void
NrChip::SetEnergyParameterFromCentralFrequency(double centralFrequency, double transmitPower)
{
    for (auto& energyParameter : energyParametersVec)
    {
        if (centralFrequency >= energyParameter.startFrequency &&
            centralFrequency < energyParameter.endFrequency)
        {
            m_psmPower = energyParameter.m_psmPower;
            m_drxPower = energyParameter.m_drxPower;
            m_edrxPower = energyParameter.m_edrxPower;
            m_downlinkPower = energyParameter.m_downlinkPower;
            m_idlePower = energyParameter.m_idlePower;
            m_connectedPower = energyParameter.m_connectedPower;

            uint index = 0;
            while (transmitPower > energyParameter.m_uplinkPowerBorders.at(index))
            {
                index = index + 1;
            }
            m_uplinkPower = energyParameter.m_uplinkPowerVec.at(index);
        }
    }
    std::cout << m_uplinkPower << std::endl;
}

TypeId
NrEnergyModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::NrEnergyModel")
            .SetParent<Object>()
            .SetGroupName("nr")
            .AddAttribute("EnableEdrxPtw",
                          "Bill eDRX as PTW listen + deep sleep duty cycle (Phase C). "
                          "false keeps Phase A/B flat RRC_IDLE_EDRX power.",
                          BooleanValue(false),
                          MakeBooleanAccessor(&NrEnergyModel::m_enableEdrxPtw),
                          MakeBooleanChecker())
            .AddAttribute("PtwLengthMs",
                          "Paging Time Window length in milliseconds (research default 1280).",
                          UintegerValue(1280),
                          MakeUintegerAccessor(&NrEnergyModel::m_ptwLengthMs),
                          MakeUintegerChecker<uint32_t>(1, 10240))
            .AddAttribute("DeepEdrxPowerW",
                          "Deep eDRX sleep power in Watts between PTW windows.",
                          DoubleValue(9.8627e-3),
                          MakeDoubleAccessor(&NrEnergyModel::m_deepEdrxPowerW),
                          MakeDoubleChecker<double>(0.0))
            .AddAttribute("PtwListenPowerW",
                          "PTW listen / paging-monitor power in Watts.",
                          DoubleValue(0.10),
                          MakeDoubleAccessor(&NrEnergyModel::m_ptwListenPowerW),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

void
NrEnergyModel::SetGlobalPtwConfig(bool enable,
                                 uint32_t ptwLengthMs,
                                 double deepEdrxPowerW,
                                 double ptwListenPowerW)
{
    g_cfgEnableEdrxPtw = enable;
    g_cfgPtwLengthMs = ptwLengthMs;
    g_cfgDeepEdrxPowerW = deepEdrxPowerW;
    g_cfgPtwListenPowerW = ptwListenPowerW;
}

NrEnergyModel::NrEnergyModel()
    : m_lastState(PowerState::OFF),
      m_savedInactiveState(PowerState::OFF),
      m_lastStateChange(Time(0)),
      m_imsi(0)
{
    m_enableEdrxPtw = g_cfgEnableEdrxPtw;
    m_ptwLengthMs = g_cfgPtwLengthMs;
    m_deepEdrxPowerW = g_cfgDeepEdrxPowerW;
    m_ptwListenPowerW = g_cfgPtwListenPowerW;
}

NrEnergyModel::NrEnergyModel(NrChip module, uint32_t startTime)
    : m_module(module),
      m_lastState(PowerState::OFF),
      m_savedInactiveState(PowerState::OFF),
      m_lastStateChange(Time(0)),
      m_imsi(0)
{
    m_enableEdrxPtw = g_cfgEnableEdrxPtw;
    m_ptwLengthMs = g_cfgPtwLengthMs;
    m_deepEdrxPowerW = g_cfgDeepEdrxPowerW;
    m_ptwListenPowerW = g_cfgPtwListenPowerW;
    m_battery = CreateObject<LiIonEnergySource>();
    m_battery->SetInitialEnergy(37000.0);
    Simulator::Schedule(MilliSeconds(startTime), &NrEnergyModel::ActivateEnergyModel, this);
}

NrEnergyModel::~NrEnergyModel()
{
    StopEdrxPtw();
    DoNotifyStateChange(PowerState::OFF);
    std::string logfile_path = m_outputDir + "Energystates.log";
    std::ofstream logfile;
    logfile.open(logfile_path, std::ios_base::app);
    for (auto const& state : m_timeSpendInState)
    {
        logfile << Simulator::Now().GetNanoSeconds() << " imsi; " << m_imsi << ", Time in state "
                << PowerStateNames[uint(state.first)] << ":" << (state.second / 1e6) << "ms"
                << "\n";
    }
    for (auto const& state : m_energySpendInState)
    {
        logfile << Simulator::Now().GetNanoSeconds() << " imsi; " << m_imsi << ", Energy in state "
                << PowerStateNames[uint(state.first)] << ":" << state.second << "\n";
    }
    logfile.close();
}

NrEnergyModel::PowerState
NrEnergyModel::DoGetState()
{
    return m_lastState;
}

void
NrEnergyModel::ActivateEnergyModel()
{
    m_isActive = true;
    m_lastStateChange = Simulator::Now();
    m_lastState = m_savedInactiveState;
}

bool
NrEnergyModel::IsEdrxFamily(PowerState s) const
{
    return s == PowerState::RRC_IDLE_EDRX || s == PowerState::RRC_IDLE_EDRX_PTW ||
           s == PowerState::RRC_IDLE_DRX || s == PowerState::RRC_IDLE;
}

double
NrEnergyModel::PowerWattsForState(PowerState s)
{
    switch (s)
    {
    case PowerState::RRC_Connected:
        return m_module.GetConnectedPower();
    case PowerState::RRC_RECEIVING_PDSCH:
        return m_module.GetDownlinkPower();
    case PowerState::RRC_SENDING_PRACH:
    case PowerState::RRC_SENDING_PUSCH:
        return m_module.GetUplinkPower();
    case PowerState::RRC_IDLE_EDRX_PTW:
        return m_ptwListenPowerW;
    case PowerState::RRC_IDLE_EDRX:
        if (m_enableEdrxPtw && m_ptwScheduleRunning)
        {
            return m_deepEdrxPowerW;
        }
        return m_module.GetEdrxPower();
    case PowerState::RRC_IDLE_DRX:
        return m_module.GetDrxPower();
    case PowerState::RRC_IDLE:
        return m_module.m_useEdrx ? m_module.GetEdrxPower() : m_module.GetDrxPower();
    default:
        return 0.0;
    }
}

void
NrEnergyModel::StopEdrxPtw()
{
    m_ptwScheduleRunning = false;
    if (m_ptwEndEvent.IsRunning())
    {
        m_ptwEndEvent.Cancel();
    }
    if (m_ptwCycleEvent.IsRunning())
    {
        m_ptwCycleEvent.Cancel();
    }
}

void
NrEnergyModel::ConfigureEdrxPtw(uint32_t edrxRadioFrames)
{
    // 1 radio frame = 10 ms (TS 38.331 / 38.304 cycle mapping used in g_eDrxCycle).
    m_edrxCycleMs = std::max<uint32_t>(edrxRadioFrames * 10, m_ptwLengthMs + 1);
    if (!m_enableEdrxPtw)
    {
        return;
    }
    StopEdrxPtw();
    m_ptwScheduleRunning = true;
    // Start a PTW immediately after release so resume paging is possible soon.
    EnterPtwWindow();
}

void
NrEnergyModel::EnterPtwWindow()
{
    if (!m_enableEdrxPtw || !m_ptwScheduleRunning)
    {
        return;
    }
    if (!IsEdrxFamily(m_lastState) && m_lastState != PowerState::OFF &&
        m_lastState != PowerState::RRC_IDLE_EDRX_PTW)
    {
        // Left idle (e.g. reconnect) — do not force PTW.
        return;
    }
    BillTransition(PowerState::RRC_IDLE_EDRX_PTW);
    uint32_t ptwMs = std::min(m_ptwLengthMs, m_edrxCycleMs - 1);
    m_ptwEndEvent = Simulator::Schedule(MilliSeconds(ptwMs), &NrEnergyModel::LeavePtwWindow, this);
    uint32_t remain = m_edrxCycleMs > ptwMs ? m_edrxCycleMs - ptwMs : 1;
    m_ptwCycleEvent =
        Simulator::Schedule(MilliSeconds(ptwMs + remain), &NrEnergyModel::EnterPtwWindow, this);
}

void
NrEnergyModel::LeavePtwWindow()
{
    if (!m_ptwScheduleRunning)
    {
        return;
    }
    if (m_lastState == PowerState::RRC_IDLE_EDRX_PTW)
    {
        BillTransition(PowerState::RRC_IDLE_EDRX);
    }
}

void
NrEnergyModel::ScheduleNextPtwCycle()
{
    // unused — cycle rescheduled from EnterPtwWindow
}

void
NrEnergyModel::BillTransition(PowerState newState)
{
    Time stateTime = (Simulator::Now() - m_lastStateChange);
    double lostEnergy = PowerWattsForState(m_lastState) * stateTime.GetMicroSeconds() / 1e6;
    m_states.push_back(
        std::pair<PowerState, uint32_t>(newState, Simulator::Now().GetMilliSeconds()));
    m_lastStateChange = Simulator::Now();
    if (m_battery)
    {
        m_battery->DecreaseRemainingEnergy(lostEnergy);
    }
    m_timeSpendInState[m_lastState] += stateTime.GetNanoSeconds();
    m_energySpendInState[m_lastState] += lostEnergy;
    m_lastState = newState;
}

void
NrEnergyModel::DoNotifyStateChange(PowerState newState, uint64_t imsi)
{
    if (m_isActive)
    {
        DoNotifyStateChange(newState);
        m_imsi = imsi;
    }
    else
    {
        if (newState == PowerState::RRC_IDLE)
        {
            if (m_module.m_useEdrx)
            {
                newState = PowerState::RRC_IDLE_EDRX;
            }
            else
            {
                newState = PowerState::RRC_IDLE_DRX;
            }
        }
        m_savedInactiveState = newState;
    }
}

void
NrEnergyModel::DoNotifyStateChange(PowerState newState)
{
    if (newState == PowerState::RRC_IDLE)
    {
        if (m_module.m_useEdrx)
        {
            newState = PowerState::RRC_IDLE_EDRX;
        }
        else
        {
            newState = PowerState::RRC_IDLE_DRX;
        }
    }

    // Leaving eDRX family cancels PTW timers (reconnect / PRACH / Connected).
    if (!IsEdrxFamily(newState) && newState != PowerState::OFF)
    {
        StopEdrxPtw();
    }

    BillTransition(newState);
}

double
NrEnergyModel::GetEnergyRemaining()
{
    DoNotifyStateChange(m_lastState);
    return m_battery->GetRemainingEnergy();
}

double
NrEnergyModel::GetEnergyRemainingFraction()
{
    DoNotifyStateChange(m_lastState);
    return m_battery->GetEnergyFraction();
}

} // namespace ns3
