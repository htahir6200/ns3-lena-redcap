/*
 * Copyright (c) 2012 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
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
 * Author: Lluis Parcerisa <lparcerisa@cttc.cat>
 * Modified by:
 *          Danilo Abrignani <danilo.abrignani@unibo.it> (Carrier Aggregation - GSoC 2015)
 *          Biljana Bojovic <biljana.bojovic@cttc.es> (Carrier Aggregation)
 */

#include "ns3/nr-rrc-header.h"

#include "ns3/log.h"

#include <sstream>
#include <stdio.h>

#define MAX_DRB 32
#define MAX_DRB_TO_ADD 29   // According to section 6.4 ETSI TS 138.331
#define MAX_NR_ARFCN 3279165 // According to section 6.4 ETSI TS 138.331
#define MAX_EARFCN 262143
#define MAX_RAT_CAPABILITIES 8
#define MAX_SI_MESSAGE 32
#define MAX_SIB 32
#define MAX_BWP_CONFIGURED 4 // According to section 6.4 ETSI TS 138.331
#define MAX_BWP_DEFINED 15 //effictive 16 because 0 is also a viable BWP

#define MAX_REPORT_CONFIG_ID 32
#define MAX_OBJECT_ID 32
#define MAX_MEAS_ID 32
#define MAX_CELL_MEAS 32
#define MAX_CELL_REPORT 8

#define MAX_SCELL_REPORT 5
#define MAX_SCELL_CONF 5

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("NrRrcHeader");

//////////////////// RrcAsn1Header class ///////////////////////////////
RrcAsn1Header::RrcAsn1Header()
{
}

TypeId
RrcAsn1Header::GetTypeId()
{
    static TypeId tid = TypeId("ns3::RrcAsn1Header").SetParent<Header>().SetGroupName("Lte");
    return tid;
}

TypeId
RrcAsn1Header::GetInstanceTypeId() const
{
    return GetTypeId();
}

int
RrcAsn1Header::GetMessageType() const
{
    return m_messageType;
}

int
RrcAsn1Header::BandwidthToEnum(uint16_t bandwidth) const
{
    int n;
    switch (bandwidth)
    {
    case 6:
        n = 0;
        break;
    case 15:
        n = 1;
        break;
    case 25:
        n = 2;
        break;
    case 50:
        n = 3;
        break;
    case 75:
        n = 4;
        break;
    case 100:
        n = 5;
        break;
    case 200:
        n = 6;
        break;
    case 1000:
        n = 7;
        break;
    default:
        NS_FATAL_ERROR("Wrong bandwidth: " << bandwidth);
    }
    return n;
}

uint16_t
RrcAsn1Header::EnumToBandwidth(int n) const
{
    uint16_t bw;
    switch (n)
    {
    case 0:
        bw = 6;
        break;
    case 1:
        bw = 15;
        break;
    case 2:
        bw = 25;
        break;
    case 3:
        bw = 50;
        break;
    case 4:
        bw = 75;
        break;
    case 5:
        bw = 100;
        break;
    case 6:
        bw = 200;
        break;
    case 7:
        bw= 1000;
        break;
    default:
        NS_FATAL_ERROR("Wrong enum value for bandwidth: " << n);
    }
    return bw;
}

void
RrcAsn1Header::SerializeDrbToAddModList(std::list<NrRrcSap::DrbToAddMod> drbToAddModList) const
{
    // Serialize DRB-ToAddModList sequence-of
    SerializeSequenceOf(drbToAddModList.size(), MAX_DRB_TO_ADD, 1);

    // Serialize the elements in the sequence-of list
    std::list<NrRrcSap::DrbToAddMod>::iterator it = drbToAddModList.begin();
    for (; it != drbToAddModList.end(); it++)
    {
        // Serialize DRB-ToAddMod sequence
        // 5 optional fields. Extension marker is present.
        std::bitset<5> drbToAddModListOptionalFieldsPresent = std::bitset<5>();
        drbToAddModListOptionalFieldsPresent.set(4, 1); // eps-BearerIdentity present
        drbToAddModListOptionalFieldsPresent.set(3, 0); // pdcp-Config not present
        drbToAddModListOptionalFieldsPresent.set(2, 1); // rlc-Config present
        drbToAddModListOptionalFieldsPresent.set(1, 1); // logicalChannelIdentity present
        drbToAddModListOptionalFieldsPresent.set(0, 1); // logicalChannelConfig present
        SerializeSequence(drbToAddModListOptionalFieldsPresent, true);

        // Serialize eps-BearerIdentity::=INTEGER (0..15)
        SerializeInteger(it->epsBearerIdentity, 0, 15);

        // Serialize drb-Identity ::= INTEGER (1..32)
        SerializeInteger(it->drbIdentity, 1, 32);

        switch (it->rlcConfig.choice)
        {
        case NrRrcSap::RlcConfig::UM_BI_DIRECTIONAL:
            // Serialize rlc-Config choice
            SerializeChoice(4, 1, true);

            // Serialize UL-UM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(2, 0); // sn-FieldLength

            // Serialize DL-UM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(2, 0);  // sn-FieldLength
            SerializeEnum(32, 0); // t-Reordering
            break;

        case NrRrcSap::RlcConfig::UM_UNI_DIRECTIONAL_UL:
            // Serialize rlc-Config choice
            SerializeChoice(4, 2, true);

            // Serialize UL-UM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(2, 0); // sn-FieldLength
            break;

        case NrRrcSap::RlcConfig::UM_UNI_DIRECTIONAL_DL:
            // Serialize rlc-Config choice
            SerializeChoice(4, 3, true);

            // Serialize DL-UM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(2, 0);  // sn-FieldLength
            SerializeEnum(32, 0); // t-Reordering
            break;

        case NrRrcSap::RlcConfig::AM:
        default:
            // Serialize rlc-Config choice
            SerializeChoice(4, 0, true);

            // Serialize UL-AM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(64, 0); // t-PollRetransmit
            SerializeEnum(8, 0);  // pollPDU
            SerializeEnum(16, 0); // pollByte
            SerializeEnum(8, 0);  // maxRetxThreshold

            // Serialize DL-AM-RLC
            SerializeSequence(std::bitset<0>(), false);
            SerializeEnum(32, 0); // t-Reordering
            SerializeEnum(64, 0); // t-StatusProhibit
            break;
        }

        // Serialize logicalChannelIdentity ::=INTEGER (3..10)
        SerializeInteger(it->logicalChannelIdentity, 3, 10);

        // Serialize logicalChannelConfig
        SerializeLogicalChannelConfig(it->logicalChannelConfig);
    }
}

void
RrcAsn1Header::SerializeSrbToAddModList(std::list<NrRrcSap::SrbToAddMod> srbToAddModList) const
{
    // Serialize SRB-ToAddModList ::= SEQUENCE (SIZE (1..2)) OF SRB-ToAddMod
    SerializeSequenceOf(srbToAddModList.size(), 2, 1);

    // Serialize the elements in the sequence-of list
    std::list<NrRrcSap::SrbToAddMod>::iterator it = srbToAddModList.begin();
    for (; it != srbToAddModList.end(); it++)
    {
        // Serialize SRB-ToAddMod sequence
        // 2 optional fields. Extension marker is present.
        std::bitset<2> srbToAddModListOptionalFieldsPresent = std::bitset<2>();
        srbToAddModListOptionalFieldsPresent.set(1, 0); // rlc-Config not present
        srbToAddModListOptionalFieldsPresent.set(0, 1); // logicalChannelConfig present
        SerializeSequence(srbToAddModListOptionalFieldsPresent, true);

        // Serialize srb-Identity ::= INTEGER (1..2)
        SerializeInteger(it->srbIdentity, 1, 2);

        // Serialize logicalChannelConfig choice
        // 2 options, selected option 0 (var "explicitValue", of type LogicalChannelConfig)
        SerializeChoice(2, 0, false);

        // Serialize LogicalChannelConfig
        SerializeLogicalChannelConfig(it->logicalChannelConfig);
    }
}

void
RrcAsn1Header::SerializeLogicalChannelConfig(
    NrRrcSap::LogicalChannelConfig logicalChannelConfig) const
{
    // Serialize LogicalChannelConfig sequence
    // 1 optional field (ul-SpecificParameters), which is present. Extension marker present.
    SerializeSequence(std::bitset<1>(1), true);

    // Serialize ul-SpecificParameters sequence
    // 1 optional field (logicalChannelGroup), which is present. No extension marker.
    SerializeSequence(std::bitset<1>(1), false);

    // Serialize priority ::= INTEGER (1..16)
    SerializeInteger(logicalChannelConfig.priority, 1, 16);

    // Serialize prioritisedBitRate
    int prioritizedBitRate;
    switch (logicalChannelConfig.prioritizedBitRateKbps)
    {
    case 0:
        prioritizedBitRate = 0;
        break;
    case 8:
        prioritizedBitRate = 1;
        break;
    case 16:
        prioritizedBitRate = 2;
        break;
    case 32:
        prioritizedBitRate = 3;
        break;
    case 64:
        prioritizedBitRate = 4;
        break;
    case 128:
        prioritizedBitRate = 5;
        break;
    case 256:
        prioritizedBitRate = 6;
        break;
    default:
        prioritizedBitRate = 7; // Infinity
    }
    SerializeEnum(16, prioritizedBitRate);

    // Serialize bucketSizeDuration
    int bucketSizeDuration;
    switch (logicalChannelConfig.bucketSizeDurationMs)
    {
    case 50:
        bucketSizeDuration = 0;
        break;
    case 100:
        bucketSizeDuration = 1;
        break;
    case 150:
        bucketSizeDuration = 2;
        break;
    case 300:
        bucketSizeDuration = 3;
        break;
    case 500:
        bucketSizeDuration = 4;
        break;
    case 1000:
        bucketSizeDuration = 5;
        break;
    default:
        bucketSizeDuration = 5;
    }
    SerializeEnum(8, bucketSizeDuration);

    // Serialize logicalChannelGroup ::= INTEGER (0..3)
    SerializeInteger(logicalChannelConfig.logicalChannelGroup, 0, 3);
}

void
RrcAsn1Header::SerializePhysicalConfigDedicated(
    NrRrcSap::PhysicalConfigDedicated physicalConfigDedicated) const
{
    // Serialize PhysicalConfigDedicated Sequence
    std::bitset<10> optionalFieldsPhysicalConfigDedicated;
    optionalFieldsPhysicalConfigDedicated.set(
        9,
        physicalConfigDedicated.havePdschConfigDedicated); // pdsch-ConfigDedicated
    optionalFieldsPhysicalConfigDedicated.set(8, 0);       // pucch-ConfigDedicated not present
    optionalFieldsPhysicalConfigDedicated.set(7, 0);       // pusch-ConfigDedicated not present
    optionalFieldsPhysicalConfigDedicated.set(6, 0); // uplinkPowerControlDedicated not present
    optionalFieldsPhysicalConfigDedicated.set(5, 0); // tpc-PDCCH-ConfigPUCCH not present
    optionalFieldsPhysicalConfigDedicated.set(4, 0); // tpc-PDCCH-ConfigPUSCH not present
    optionalFieldsPhysicalConfigDedicated.set(3, 0); // cqi-ReportConfig not present
    optionalFieldsPhysicalConfigDedicated.set(
        2,
        physicalConfigDedicated.haveSoundingRsUlConfigDedicated); // soundingRS-UL-ConfigDedicated
    optionalFieldsPhysicalConfigDedicated.set(
        1,
        physicalConfigDedicated.haveAntennaInfoDedicated); // antennaInfo
    optionalFieldsPhysicalConfigDedicated.set(0, 0);       // schedulingRequestConfig not present
    SerializeSequence(optionalFieldsPhysicalConfigDedicated, true);

    if (physicalConfigDedicated.havePdschConfigDedicated)
    {
        // Serialize Pdsch-ConfigDedicated Sequence:
        // 0 optional / default fields, no extension marker.
        SerializeSequence(std::bitset<0>(), false);

        // Serialize  p-a
        // Assuming the value in the struct is the enum index
        SerializeEnum(8, physicalConfigDedicated.pdschConfigDedicated.pa);

        // Serialize release
        SerializeNull();
    }

    if (physicalConfigDedicated.haveSoundingRsUlConfigDedicated)
    {
        // Serialize SoundingRS-UL-ConfigDedicated choice:
        switch (physicalConfigDedicated.soundingRsUlConfigDedicated.type)
        {
        case NrRrcSap::SoundingRsUlConfigDedicated::RESET:
            SerializeChoice(2, 0, false);
            SerializeNull();
            break;

        case NrRrcSap::SoundingRsUlConfigDedicated::SETUP:
        default:
            // 2 options, selected: 1 (setup)
            SerializeChoice(2, 1, false);

            // Serialize setup sequence
            // 0 optional / default fields, no extension marker.
            SerializeSequence(std::bitset<0>(), false);

            // Serialize srs-Bandwidth
            SerializeEnum(4, physicalConfigDedicated.soundingRsUlConfigDedicated.srsBandwidth);

            // Serialize  srs-HoppingBandwidth
            SerializeEnum(4, 0);

            // Serialize freqDomainPosition
            SerializeInteger(0, 0, 23);

            // Serialize duration
            SerializeBoolean(false);

            // Serialize srs-ConfigIndex
            SerializeInteger(physicalConfigDedicated.soundingRsUlConfigDedicated.srsConfigIndex,
                             0,
                             1023);

            // Serialize transmissionComb
            SerializeInteger(0, 0, 1);

            // Serialize cyclicShift
            SerializeEnum(8, 0);

            break;
        }
    }

    if (physicalConfigDedicated.haveAntennaInfoDedicated)
    {
        // Serialize antennaInfo choice
        // 2 options. Selected: 0 ("explicitValue" of type "AntennaInfoDedicated")
        SerializeChoice(2, 0, false);

        // Serialize AntennaInfoDedicated sequence
        // 1 optional parameter, not present. No extension marker.
        SerializeSequence(std::bitset<1>(0), false);

        // Serialize transmissionMode
        // Assuming the value in the struct is the enum index
        SerializeEnum(8, physicalConfigDedicated.antennaInfo.transmissionMode);

        // Serialize ue-TransmitAntennaSelection choice
        SerializeChoice(2, 0, false);

        // Serialize release
        SerializeNull();
    }
}

void
RrcAsn1Header::SerializeRadioResourceConfigDedicated(
    NrRrcSap::RadioResourceConfigDedicated radioResourceConfigDedicated) const
{
    bool isSrbToAddModListPresent = !radioResourceConfigDedicated.srbToAddModList.empty();
    bool isDrbToAddModListPresent = !radioResourceConfigDedicated.drbToAddModList.empty();
    bool isDrbToReleaseListPresent = !radioResourceConfigDedicated.drbToReleaseList.empty();

    // 6 optional fields. Extension marker is present.
    std::bitset<6> optionalFieldsPresent = std::bitset<6>();
    optionalFieldsPresent.set(5, isSrbToAddModListPresent);  // srb-ToAddModList present
    optionalFieldsPresent.set(4, isDrbToAddModListPresent);  // drb-ToAddModList present
    optionalFieldsPresent.set(3, isDrbToReleaseListPresent); // drb-ToReleaseList present
    optionalFieldsPresent.set(2, 0);                         // mac-MainConfig not present
    optionalFieldsPresent.set(1, 0);                         // sps-Config not present
    optionalFieldsPresent.set(0,
                              (radioResourceConfigDedicated.havePhysicalConfigDedicated) ? 1 : 0);
    SerializeSequence(optionalFieldsPresent, true);

    // Serialize srbToAddModList
    if (isSrbToAddModListPresent)
    {
        SerializeSrbToAddModList(radioResourceConfigDedicated.srbToAddModList);
    }

    // Serialize drbToAddModList
    if (isDrbToAddModListPresent)
    {
        SerializeDrbToAddModList(radioResourceConfigDedicated.drbToAddModList);
    }

    // Serialize drbToReleaseList
    if (isDrbToReleaseListPresent)
    {
        SerializeSequenceOf(radioResourceConfigDedicated.drbToReleaseList.size(), MAX_DRB, 1);
        std::list<uint8_t>::iterator it = radioResourceConfigDedicated.drbToReleaseList.begin();
        for (; it != radioResourceConfigDedicated.drbToReleaseList.end(); it++)
        {
            // DRB-Identity ::= INTEGER (1..32)
            SerializeInteger(*it, 1, 32);
        }
    }

    if (radioResourceConfigDedicated.havePhysicalConfigDedicated)
    {
        SerializePhysicalConfigDedicated(radioResourceConfigDedicated.physicalConfigDedicated);
    }
}

void 
RrcAsn1Header::SerializeRadioBearerConfig(
      NrRrcSap::RadioBearerConfig radioBearerConfig) const
{
    bool isSrbToAddModListPresent = !radioBearerConfig.srbToAddModList.empty();
    bool isDrbToAddModListPresent = !radioBearerConfig.drbToAddModList.empty();
    bool isDrbToReleaseListPresent = !radioBearerConfig.drbToReleaseList.empty();

    // 9 optional fields. Extension marker is present.
    std::bitset<9> optionalFieldsPresent = std::bitset<9>();
    optionalFieldsPresent.set(8, isSrbToAddModListPresent);  // srb-ToAddModList present
    optionalFieldsPresent.set(7, 0);                         // srb3-ToRelease not present
    optionalFieldsPresent.set(6, isDrbToAddModListPresent);  // drb-ToAddModList not present
    optionalFieldsPresent.set(5, isDrbToReleaseListPresent); // drb-ToReleaseList present
    optionalFieldsPresent.set(4, 0);                         // securityConfig not present
    optionalFieldsPresent.set(3, 0);                         // mrb-ToAddModList-r17 not present
    optionalFieldsPresent.set(2, 0);                         // mrb-ToReleaseList-r17 not present
    optionalFieldsPresent.set(1, 0);                         // srb-ToAddModListExt-r17 not present
    optionalFieldsPresent.set(0, 0);                         // srb4-ToRelease-r17 not present
    SerializeSequence(optionalFieldsPresent, true);


    // Serialize srbToAddModList
    if (isSrbToAddModListPresent)
    {
        SerializeSrbToAddModList(radioBearerConfig.srbToAddModList);
    }

    // Serialize drbToAddModList
    if (isDrbToAddModListPresent)
    {
        SerializeDrbToAddModList(radioBearerConfig.drbToAddModList);
    }

    // Serialize drbToReleaseList
    if (isDrbToReleaseListPresent)
    {
        SerializeSequenceOf(radioBearerConfig.drbToReleaseList.size(), MAX_DRB, 1);
        std::list<uint8_t>::iterator it = radioBearerConfig.drbToReleaseList.begin();
        for (; it != radioBearerConfig.drbToReleaseList.end(); it++)
        {
            // DRB-Identity ::= INTEGER (1..32)
            SerializeInteger(*it, 1, 32);
        }
    }
}

void
RrcAsn1Header::SerializeSpCellConfig(
    NrRrcSap::SpCellConfig spCellConfig) const
{
    SerializeSpCellConfigDedicated(spCellConfig.spCellConfigDedicated);
}

void
RrcAsn1Header::SerializeSpCellConfigDedicated(
    NrRrcSap::ServingCellConfig spCellConfigDedicated) const
{
    bool isDownlinkBWP_ToReleaseListPresent = !spCellConfigDedicated.downlinkBWP_ToReleaseList.empty();
    bool isDownlinkBWP_ToAddModListPresent = !spCellConfigDedicated.downlinkBWP_ToAddModList.empty();

    // 2 optional fields. Extension marker is present.
    std::bitset<2> optionalFieldsPresent = std::bitset<2>();
    optionalFieldsPresent.set(1, isDownlinkBWP_ToReleaseListPresent);
    optionalFieldsPresent.set(0, isDownlinkBWP_ToAddModListPresent);                         

    SerializeSequence(optionalFieldsPresent, true);

    // Serialize downlinkBWP_ToReleaseList
    if (isDownlinkBWP_ToReleaseListPresent)
    {
        SerializeSequenceOf(spCellConfigDedicated.downlinkBWP_ToReleaseList.size(), MAX_BWP_CONFIGURED, 1);
        std::list<uint8_t>::iterator it = spCellConfigDedicated.downlinkBWP_ToReleaseList.begin();
        for (; it != spCellConfigDedicated.downlinkBWP_ToReleaseList.end(); it++)
        {
            //BwpToRelease ::= INTEGER (1..MAX_BWP_CONFIGURED)
            SerializeInteger(*it, 1, MAX_BWP_CONFIGURED);
        }
    }

    // Serialize downlinkBWP_ToAddModList
    if (isDownlinkBWP_ToAddModListPresent)
    {
        SerializeSequenceOf(spCellConfigDedicated.downlinkBWP_ToAddModList.size(), MAX_BWP_CONFIGURED, 1);
        std::list<NrRrcSap::BWP_Downlink>::iterator it = spCellConfigDedicated.downlinkBWP_ToAddModList.begin();
        for (; it != spCellConfigDedicated.downlinkBWP_ToAddModList.end(); it++)
        {
            //BwpToAddMod ::= INTEGER (0..MAX_BWP_DEFINED)
            SerializeInteger(it->bwp_Id, 0, MAX_BWP_DEFINED); //we just add a consisting BWP 

            SerializeBwpDlCommon(it->bwp_dlCommon);

        }
    }

    // Serialize firstActiveDownlinkBWP_Id
    SerializeInteger(spCellConfigDedicated.firstActiveDownlinkBWP_Id,0,MAX_BWP_DEFINED); 

    // Serialize bwp_InactivityTimer
    SerializeBwpInactivityTimer(spCellConfigDedicated.bwp_InactivityTimer);


    // Serialize uplinkConfig
    SerializeUplinkConfig(spCellConfigDedicated.uplinkConfig);
}
void
RrcAsn1Header::SerializeBwpDlCommon(NrRrcSap::Bwp_DownlinkCommon bwp_DownlinkCommon) const
{
    SerializeBWP(bwp_DownlinkCommon.genericParameters);
}

void
RrcAsn1Header::SerializeUplinkConfig(
    NrRrcSap::UplinkConfig uplinkConfig) const
{
    bool isUplinkBWP_ToReleaseListPresent = !uplinkConfig.uplinkBWP_ToReleaseList.empty();
    bool isUplinkBWP_ToAddModListPresent = !uplinkConfig.uplinkBWP_ToAddModList.empty();


    //TODO_what do these extensions do exactly and are they needed?
    // 2 optional fields. Extension marker is present.
    std::bitset<2> optionalFieldsPresent = std::bitset<2>();
    optionalFieldsPresent.set(1, isUplinkBWP_ToReleaseListPresent);
    optionalFieldsPresent.set(0, isUplinkBWP_ToAddModListPresent);                         

    SerializeSequence(optionalFieldsPresent, true);


      // Serialize uplinkBWP_ToReleaseList
    if (isUplinkBWP_ToReleaseListPresent)
    {
        SerializeSequenceOf(uplinkConfig.uplinkBWP_ToReleaseList.size(), 4, 1);
        std::list<uint8_t>::iterator it = uplinkConfig.uplinkBWP_ToReleaseList.begin();
        for (; it != uplinkConfig.uplinkBWP_ToReleaseList.end(); it++)
        {
            //BwpToRelease ::= INTEGER (1..MAX_BWP_CONFIGURED)
            SerializeInteger(*it, 1, MAX_BWP_CONFIGURED);
        }
    }

    // Serialize uplinkBWP_ToAddModList
    if (isUplinkBWP_ToAddModListPresent)
    {
        SerializeSequenceOf(uplinkConfig.uplinkBWP_ToAddModList.size(), 4, 1);
        std::list<NrRrcSap::BWP_Uplink>::iterator it = uplinkConfig.uplinkBWP_ToAddModList.begin();
        for (; it != uplinkConfig.uplinkBWP_ToAddModList.end(); it++)
        {

            //BwpToAddMod ::= INTEGER (0..MAX_BWP_DEFINED)
            SerializeInteger(it->bwp_Id, 0, MAX_BWP_DEFINED);

            SerializeBwpUlCommon(it->bwp_UplinkCommon);

            
        }
    }

    // Serialize firstActiveUplinkBWP_Id
    SerializeInteger(uplinkConfig.firstActiveUplinkBWP_Id, 0, MAX_BWP_DEFINED); 
}

void
RrcAsn1Header::SerializeBwpUlCommon(NrRrcSap::Bwp_UplinkCommon bwp_UplinkCommon) const
{
    SerializeBWP(bwp_UplinkCommon.genericParameters);
}

void
RrcAsn1Header::SerializeBWP(NrRrcSap::BWP bwp) const
{
    SerializeInteger(bwp.locationAndBandwidth,0,37949);
}


void
RrcAsn1Header::SerializeBwpInactivityTimer(NrRrcSap::Bwp_InactivityTimer usedValue) const
{
    switch (usedValue)
    {
    case NrRrcSap::ms2:
        SerializeEnum(31, 0);
        break;
    case NrRrcSap::ms3:
        SerializeEnum(31, 1);
        break;
    case NrRrcSap::ms4:
        SerializeEnum(31, 2);
        break;
    case NrRrcSap::ms5:
        SerializeEnum(31, 3);
        break;
    case NrRrcSap::ms6:
        SerializeEnum(31, 4);
        break;
    case NrRrcSap::ms8:
        SerializeEnum(31, 5);
        break;
    case NrRrcSap::ms10:
        SerializeEnum(31, 6);
        break;
    case NrRrcSap::ms20:
        SerializeEnum(31, 7);
        break;
    case NrRrcSap::ms30:
        SerializeEnum(31, 8);
        break;
    case NrRrcSap::ms40:
        SerializeEnum(31, 9);
        break;
    case NrRrcSap::ms50:
        SerializeEnum(31, 10);
        break;
    case NrRrcSap::ms60:
        SerializeEnum(31, 11);
        break;
    case NrRrcSap::ms80:
        SerializeEnum(31, 12);
        break;
    case NrRrcSap::ms100:
        SerializeEnum(31, 13);
        break;
    case NrRrcSap::ms200:
        SerializeEnum(31, 14);
        break;
    case NrRrcSap::ms300:
        SerializeEnum(31, 15);
        break;
    case NrRrcSap::ms500:
        SerializeEnum(31, 16);
        break;
    case NrRrcSap::ms750:
        SerializeEnum(31, 17);
        break;
    case NrRrcSap::ms1280:
        SerializeEnum(31, 18);
        break;
    case NrRrcSap::ms1920:
        SerializeEnum(31, 19);
        break;
    case NrRrcSap::ms2560:
        SerializeEnum(31, 20);
        break;
    case NrRrcSap::spare10:
        SerializeEnum(31, 21);
        break;
    case NrRrcSap::spare9:
        SerializeEnum(31, 22);
        break;
    case NrRrcSap::spare8:
        SerializeEnum(31, 23);
        break;
    case NrRrcSap::spare7:
        SerializeEnum(31, 24);
        break;
    case NrRrcSap::spare6:
        SerializeEnum(31, 25);
        break;
    case NrRrcSap::spare5:
        SerializeEnum(31, 26);
        break;
    case NrRrcSap::spare4:
        SerializeEnum(31, 27);
        break;
    case NrRrcSap::spare3:
        SerializeEnum(31, 28);
        break;
    case NrRrcSap::spare2:
        SerializeEnum(31, 29);
        break;
    case NrRrcSap::spare1:
        SerializeEnum(31, 30);
        break;
    default:
        SerializeEnum(31, 13); //defaule:: 100ms
    }
}

void
RrcAsn1Header::SerializeSuspendConfig(NrRrcSap::SuspendConfig scfg) const
{

     // Serialize full-rnti
    SerializeBitstring(std::bitset<40>(scfg.fullRnti));

    // Serialize shortI-RNTI 
    SerializeBitstring(std::bitset<24>(scfg.fullRnti)); // we do not distinguish between short and full rnti


    //ran-PagingCycle
    switch (scfg.ran_PagingCycle)
    {
        case NrRrcSap::rf32:
            SerializeEnum(4, 0);
            break;
        case NrRrcSap::rf64:
            SerializeEnum(4, 1);
            break;
        case NrRrcSap::rf128:
            SerializeEnum(4, 2);
            break;
        case NrRrcSap::rf256:
            SerializeEnum(4, 3);
            break;
        default:
            SerializeEnum(4, 1); //default rf64
    }

    std::bitset<2> optionalFieldsPresent1 = std::bitset<2>();
    optionalFieldsPresent1.set(1, 0);  // RAN-NotificationAreaInfo
    optionalFieldsPresent1.set(0, 1); //PeriodicRNAU-TimerValue
    SerializeSequence(optionalFieldsPresent1, false);

    //t380 
    switch (scfg.t380)
    {
        case NrRrcSap::min5:
            SerializeEnum(8, 0);
            break;
        case NrRrcSap::min10:
            SerializeEnum(8, 1);
            break;
        case NrRrcSap::min20:
            SerializeEnum(8, 2);
            break;
        case NrRrcSap::min30:
            SerializeEnum(8, 3);
            break;
        case NrRrcSap::min60:
            SerializeEnum(8, 4);
            break;
        case NrRrcSap::min120:
            SerializeEnum(8, 5);
            break;
        case NrRrcSap::min360:
            SerializeEnum(8, 6);
            break;
        case NrRrcSap::min720:
            SerializeEnum(8, 7);
            break;
        default:
            SerializeEnum(8, 4); //default min60
    }


    // Serialize nextHopChainingCount //unused
    SerializeInteger(0,0,7);

    std::bitset<4> optionalFieldsPresent2 = std::bitset<4>();
    optionalFieldsPresent2.set(3, 0); // SL-ServingCellInfo-r17
    optionalFieldsPresent2.set(2, 1); // sdt-Config-r17
    optionalFieldsPresent2.set(1, 0); // srs-PosRRC-InactiveConfig-r17
    optionalFieldsPresent2.set(0, 1); // ran-ExtendedPagingCycle-r17
    SerializeSequence(optionalFieldsPresent2, false);


    //SDT_Config_r17
    SerializeSdtConfig(scfg.sdt_Config_r17);

    //ran-ExtendedPagingCycle-r17
       switch (scfg.ran_ExtendedPagingCycle_r17)
    {
        case NrRrcSap::e_rf256:
            SerializeEnum(4, 0);
            break;
        case NrRrcSap::e_rf512:
            SerializeEnum(4, 1);
            break;
        case NrRrcSap::e_rf1024:
            SerializeEnum(4, 2);
            break;
        case NrRrcSap::e_spare1:
            SerializeEnum(4, 3);
            break;
        default:
            SerializeEnum(4, 1); //default e_rf512
    }
}

void 
RrcAsn1Header::SerializeSdtConfig(NrRrcSap::SDT_Config_r17 sdtcfg) const
{
    // Serialize sdt-DRB-List-r17
    bool hasDRB;

    // Watchdog: if list has 0 elements, set boolean to false
    if (sdtcfg.sdt_DRB_List_r17.empty())
    {
       hasDRB = false;
    }
    else{
        hasDRB = true;
    }

    std::bitset<2> sdtConfigOptionalFields;
    sdtConfigOptionalFields.set(1, hasDRB); // sdt-DRB-List-r17
    sdtConfigOptionalFields.set(0, 0); // sdt-SRB2-Indication-r17
    SerializeSequence(sdtConfigOptionalFields, false);


    if(hasDRB)
    {
        SerializeSequenceOf(sdtcfg.sdt_DRB_List_r17.size(), 29, 1);
        // serialize sdt-DRB-List-r17 elements in the list
        std::list<uint8_t>::iterator it;
        for (it = sdtcfg.sdt_DRB_List_r17.begin(); it != sdtcfg.sdt_DRB_List_r17.end(); it++)
        {
            SerializeInteger(*it,1,32);
        }
    }
   
    
    

}

void
RrcAsn1Header::SerializeSystemInformationBlockType1(
    NrRrcSap::SystemInformationBlockType1 systemInformationBlockType1) const
{
    // 3 optional fields, no extension marker.
    std::bitset<3> sysInfoBlk1Opts;
    sysInfoBlk1Opts.set(2, 0); // p-Max absent
    sysInfoBlk1Opts.set(1, 0); // tdd-Config absent
    sysInfoBlk1Opts.set(0, 0); // nonCriticalExtension absent
    SerializeSequence(sysInfoBlk1Opts, false);

    // Serialize cellAccessRelatedInfo
    // 1 optional field (csgIdentity) which is present, no extension marker.
    SerializeSequence(std::bitset<1>(1), false);

    // Serialize plmn-IdentityList
    SerializeSequenceOf(1, 6, 1);

    // PLMN-IdentityInfo
    SerializeSequence(std::bitset<0>(), false);

    SerializePlmnIdentity(
        systemInformationBlockType1.cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity);

    // Serialize trackingAreaCode
    SerializeBitstring(std::bitset<16>(0));
    // Serialize cellIdentity
    SerializeBitstring(
        std::bitset<28>(systemInformationBlockType1.cellAccessRelatedInfo.cellIdentity));
    // Serialize cellBarred
    SerializeEnum(2, 0);
    // Serialize intraFreqReselection
    SerializeEnum(2, 0);
    // Serialize csg-Indication
    SerializeBoolean(systemInformationBlockType1.cellAccessRelatedInfo.csgIndication);
    // Serialize csg-Identity
    SerializeBitstring(
        std::bitset<27>(systemInformationBlockType1.cellAccessRelatedInfo.csgIdentity));

    // Serialize cellSelectionInfo
    SerializeSequence(std::bitset<1>(0), false);
    // Serialize q-RxLevMin
    SerializeInteger(-50, -70, -22);

    // Serialize freqBandIndicator
    SerializeInteger(1, 1, 64);

    // Serialize schedulingInfoList
    SerializeSequenceOf(1, MAX_SI_MESSAGE, 1);
    // SchedulingInfo
    SerializeSequence(std::bitset<0>(), false);
    // si-Periodicity
    SerializeEnum(7, 0);
    // sib-MappingInfo
    SerializeSequenceOf(0, MAX_SIB - 1, 0);

    // Serialize si-WindowLength
    SerializeEnum(7, 0);

    // Serialize systemInfoValueTag
    SerializeInteger(0, 0, 31);
}

void
RrcAsn1Header::SerializeRadioResourceConfigCommon(
    NrRrcSap::RadioResourceConfigCommon radioResourceConfigCommon) const
{
    // 9 optional fields. Extension marker yes.
    std::bitset<9> rrCfgCmmOpts;
    rrCfgCmmOpts.set(8, 1); // rach-ConfigCommon is present
    rrCfgCmmOpts.set(7, 0); // pdsch-ConfigCommon not present
    rrCfgCmmOpts.set(6, 0); // phich-Config not present
    rrCfgCmmOpts.set(5, 0); // pucch-ConfigCommon  not present
    rrCfgCmmOpts.set(4, 0); // soundingRS-UL-ConfigCommon not present
    rrCfgCmmOpts.set(3, 0); // uplinkPowerControlCommon not present
    rrCfgCmmOpts.set(2, 0); // antennaInfoCommon not present
    rrCfgCmmOpts.set(1, 0); // p-Max not present
    rrCfgCmmOpts.set(0, 0); // tdd-Config not present

    SerializeSequence(rrCfgCmmOpts, true);

    if (rrCfgCmmOpts[8])
    {
        // Serialize RACH-ConfigCommon
        SerializeRachConfigCommon(radioResourceConfigCommon.rachConfigCommon);
    }

    // Serialize PRACH-Config
    // 1 optional, 0 extension marker.
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize PRACH-Config rootSequenceIndex
    SerializeInteger(0, 0, 1023);

    // Serialize PUSCH-ConfigCommon
    SerializeSequence(std::bitset<0>(), false);

    // Serialize pusch-ConfigBasic
    SerializeSequence(std::bitset<0>(), false);
    SerializeInteger(1, 1, 4);
    SerializeEnum(2, 0);
    SerializeInteger(0, 0, 98);
    SerializeBoolean(false);

    // Serialize UL-ReferenceSignalsPUSCH
    SerializeSequence(std::bitset<0>(), false);
    SerializeBoolean(false);
    SerializeInteger(0, 0, 29);
    SerializeBoolean(false);
    SerializeInteger(4, 0, 7);

    // Serialize UL-CyclicPrefixLength
    SerializeEnum(2, 0);
}

void
RrcAsn1Header::SerializeRadioResourceConfigCommonSib(
    NrRrcSap::RadioResourceConfigCommonSib radioResourceConfigCommonSib) const
{
    SerializeSequence(std::bitset<0>(0), true);

    // rach-ConfigCommon
    SerializeRachConfigCommon(radioResourceConfigCommonSib.rachConfigCommon);

    // bcch-Config
    SerializeSequence(std::bitset<0>(0), false);
    SerializeEnum(4, 0); // modificationPeriodCoeff
    // pcch-Config
    SerializeSequence(std::bitset<0>(0), false);
    SerializeEnum(4, 0); // defaultPagingCycle
    SerializeEnum(8, 0); // nB
    // prach-Config
    SerializeSequence(std::bitset<1>(0), false);
    SerializeInteger(0, 0, 1023); // rootSequenceIndex
    // pdsch-ConfigCommon
    SerializeSequence(std::bitset<0>(0), false);
    SerializeInteger(0, -60, 50); // referenceSignalPower
    SerializeInteger(0, 0, 3);    // p-b
    // pusch-ConfigCommon
    SerializeSequence(std::bitset<0>(0), false);
    SerializeSequence(std::bitset<0>(0), false); // pusch-ConfigBasic
    SerializeInteger(1, 1, 4);                   // n-SB
    SerializeEnum(2, 0);                         // hoppingMode
    SerializeInteger(0, 0, 98);                  // pusch-HoppingOffset
    SerializeBoolean(false);                     // enable64QAM
    SerializeSequence(std::bitset<0>(0), false); // UL-ReferenceSignalsPUSCH
    SerializeBoolean(false);                     // groupHoppingEnabled
    SerializeInteger(0, 0, 29);                  // groupAssignmentPUSCH
    SerializeBoolean(false);                     // sequenceHoppingEnabled
    SerializeInteger(0, 0, 7);                   // cyclicShift
    // pucch-ConfigCommon
    SerializeSequence(std::bitset<0>(0), false);
    SerializeEnum(3, 0);          // deltaPUCCH-Shift
    SerializeInteger(0, 0, 98);   // nRB-CQI
    SerializeInteger(0, 0, 7);    // nCS-AN
    SerializeInteger(0, 0, 2047); // n1PUCCH-AN
    // soundingRS-UL-ConfigCommon
    SerializeChoice(2, 0, false);
    SerializeNull(); // release
    // uplinkPowerControlCommon
    SerializeSequence(std::bitset<0>(0), false);
    SerializeInteger(0, -126, 24);               // p0-NominalPUSCH
    SerializeEnum(8, 0);                         // alpha
    SerializeInteger(-110, -127, -96);           // p0-NominalPUCCH
    SerializeSequence(std::bitset<0>(0), false); // deltaFList-PUCCH
    SerializeEnum(3, 0);                         // deltaF-PUCCH-Format1
    SerializeEnum(3, 0);                         // deltaF-PUCCH-Format1b
    SerializeEnum(4, 0);                         // deltaF-PUCCH-Format2
    SerializeEnum(3, 0);                         // deltaF-PUCCH-Format2a
    SerializeEnum(3, 0);                         // deltaF-PUCCH-Format2b
    SerializeInteger(0, -1, 6);
    // ul-CyclicPrefixLength
    SerializeEnum(2, 0);
}

void
RrcAsn1Header::SerializeSystemInformationBlockType2(
    NrRrcSap::SystemInformationBlockType2 systemInformationBlockType2) const
{
    SerializeSequence(std::bitset<2>(0), true);

    // RadioResourceConfigCommonSib
    SerializeRadioResourceConfigCommonSib(systemInformationBlockType2.radioResourceConfigCommon);

    // ue-TimersAndConstants
    SerializeSequence(std::bitset<0>(0), true);
    SerializeEnum(8, 0); // t300
    SerializeEnum(8, 0); // t301
    SerializeEnum(7, 0); // t310
    SerializeEnum(8, 0); // n310
    SerializeEnum(7, 0); // t311
    SerializeEnum(8, 0); // n311

    // freqInfo
    SerializeSequence(std::bitset<2>(3), false);
    SerializeInteger((int)systemInformationBlockType2.freqInfo.ulCarrierFreq, 0, MAX_NR_ARFCN);
    SerializeEnum(8, BandwidthToEnum(systemInformationBlockType2.freqInfo.ulBandwidth));

    SerializeInteger(29, 1, 32); // additionalSpectrumEmission
    // timeAlignmentTimerCommon
    SerializeEnum(8, 0);
}

void
RrcAsn1Header::SerializeMeasResults(NrRrcSap::MeasResults measResults) const
{
    // Watchdog: if list has 0 elements, set boolean to false
    if (measResults.measResultListEutra.empty())
    {
        measResults.haveMeasResultNeighCells = false;
    }

    std::bitset<4> measResultOptional;
    measResultOptional.set(3, measResults.haveMeasResultServFreqList);
    measResultOptional.set(2, false); // LocationInfo-r10
    measResultOptional.set(1, false); // MeasResultForECID-r9
    measResultOptional.set(0, measResults.haveMeasResultNeighCells);
    SerializeSequence(measResultOptional, true);

    // Serialize measId
    SerializeInteger(measResults.measId, 1, MAX_MEAS_ID);

    // Serialize measResultPCell sequence
    SerializeSequence(std::bitset<0>(0), false);

    // Serialize rsrpResult
    SerializeInteger(measResults.measResultPCell.rsrpResult, 0, 97);

    // Serialize rsrqResult
    SerializeInteger(measResults.measResultPCell.rsrqResult, 0, 34);

    if (measResults.haveMeasResultNeighCells)
    {
        // Serialize Choice = 0 (MeasResultListEUTRA)
        SerializeChoice(4, 0, false);

        // Serialize measResultNeighCells
        SerializeSequenceOf(measResults.measResultListEutra.size(), MAX_CELL_REPORT, 1);

        // serialize MeasResultEutra elements in the list
        std::list<NrRrcSap::MeasResultEutra>::iterator it;
        for (it = measResults.measResultListEutra.begin();
             it != measResults.measResultListEutra.end();
             it++)
        {
            SerializeSequence(std::bitset<1>(it->haveCgiInfo), false);

            // Serialize PhysCellId
            SerializeInteger(it->physCellId, 0, 503);

            // Serialize CgiInfo
            if (it->haveCgiInfo)
            {
                SerializeSequence(std::bitset<1>(it->cgiInfo.plmnIdentityList.size()), false);

                // Serialize cellGlobalId
                SerializeSequence(std::bitset<0>(0), false);
                SerializePlmnIdentity(it->cgiInfo.plmnIdentity);
                SerializeBitstring(std::bitset<28>(it->cgiInfo.cellIdentity));

                // Serialize trackingAreaCode
                SerializeBitstring(std::bitset<16>(it->cgiInfo.trackingAreaCode));

                // Serialize plmn-IdentityList
                if (!it->cgiInfo.plmnIdentityList.empty())
                {
                    SerializeSequenceOf(it->cgiInfo.plmnIdentityList.size(), 5, 1);
                    std::list<uint32_t>::iterator it2;
                    for (it2 = it->cgiInfo.plmnIdentityList.begin();
                         it2 != it->cgiInfo.plmnIdentityList.end();
                         it2++)
                    {
                        SerializePlmnIdentity(*it2);
                    }
                }
            }

            // Serialize measResult
            std::bitset<2> measResultFieldsPresent;
            measResultFieldsPresent[1] = it->haveRsrpResult;
            measResultFieldsPresent[0] = it->haveRsrqResult;
            SerializeSequence(measResultFieldsPresent, true);

            if (it->haveRsrpResult)
            {
                SerializeInteger(it->rsrpResult, 0, 97);
            }

            if (it->haveRsrqResult)
            {
                SerializeInteger(it->rsrqResult, 0, 34);
            }
        }
    }

    // measResultServFreqList-r10 serialization
    if (measResults.haveMeasResultServFreqList)
    {
        // Serialize measResultServFreqList-r10
        SerializeSequenceOf(measResults.measResultServFreqList.size(), MAX_SCELL_REPORT, 1);
        // serialize MeasResultServFreqList-r10 elements in the list
        for (const auto& it : measResults.measResultServFreqList)
        {
            // Serialize MeasResultServFreq-r10
            std::bitset<2> measResultServFreqPresent;
            measResultServFreqPresent[0] = it.haveMeasResultSCell;
            measResultServFreqPresent[1] = it.haveMeasResultBestNeighCell;
            SerializeSequence(measResultServFreqPresent, true);

            // Serialize servFreqId-r10
            SerializeInteger(it.servFreqId, 0, 7);

            if (it.haveMeasResultSCell)
            {
                // Serialize rsrpResultSCell-r10
                SerializeInteger(it.measResultSCell.rsrpResult, 0, 97);

                // Serialize rsrqResultSCell-r10
                SerializeInteger(it.measResultSCell.rsrqResult, 0, 34);
            }

            if (it.haveMeasResultBestNeighCell)
            {
                // Serialize physCellId-r10
                SerializeInteger(it.measResultBestNeighCell.physCellId, 0, 503);

                // Serialize rsrpResultNCell-r10
                SerializeInteger(it.measResultBestNeighCell.rsrpResult, 0, 97);

                // Serialize rsrqResultNCell-r10
                SerializeInteger(it.measResultBestNeighCell.rsrqResult, 0, 34);
            }

            NS_ASSERT(!it.haveMeasResultBestNeighCell); // Not implemented
        }
    }
}

void
RrcAsn1Header::SerializePlmnIdentity(uint32_t plmnId) const
{
    // plmn-Identity sequence, mcc is optional, no extension marker
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize mnc
    int nDig = (plmnId > 99) ? 3 : 2;

    SerializeSequenceOf(nDig, 3, 2);
    for (int i = nDig - 1; i >= 0; i--)
    {
        int n = floor(plmnId / pow(10, i));
        SerializeInteger(n, 0, 9);
        plmnId -= n * pow(10, i);
    }

    // cellReservedForOperatorUse
    SerializeEnum(2, 0);
}

void
RrcAsn1Header::SerializeRachConfigCommon(NrRrcSap::RachConfigCommon rachConfigCommon) const
{
    // rach-ConfigCommon
    SerializeSequence(std::bitset<0>(0), true);

    // preambleInfo
    SerializeSequence(std::bitset<1>(0), false);

    // numberOfRA-Preambles
    switch (rachConfigCommon.preambleInfo.numberOfRaPreambles)
    {
    case 4:
        SerializeEnum(16, 0);
        break;
    case 8:
        SerializeEnum(16, 1);
        break;
    case 12:
        SerializeEnum(16, 2);
        break;
    case 16:
        SerializeEnum(16, 3);
        break;
    case 20:
        SerializeEnum(16, 4);
        break;
    case 24:
        SerializeEnum(16, 5);
        break;
    case 28:
        SerializeEnum(16, 6);
        break;
    case 32:
        SerializeEnum(16, 7);
        break;
    case 36:
        SerializeEnum(16, 8);
        break;
    case 40:
        SerializeEnum(16, 9);
        break;
    case 44:
        SerializeEnum(16, 10);
        break;
    case 48:
        SerializeEnum(16, 11);
        break;
    case 52:
        SerializeEnum(16, 12);
        break;
    case 56:
        SerializeEnum(16, 13);
        break;
    case 60:
        SerializeEnum(16, 14);
        break;
    case 64:
        SerializeEnum(16, 15);
        break;
    default:
        NS_FATAL_ERROR("Wrong numberOfRA-Preambles value");
    }

    SerializeSequence(std::bitset<0>(0), false); // powerRampingParameters
    SerializeEnum(4, 0);                         // powerRampingStep
    SerializeEnum(16, 0);                        // preambleInitialReceivedTargetPower
    SerializeSequence(std::bitset<0>(0), false); // ra-SupervisionInfo

    // preambleTransMax
    switch (rachConfigCommon.raSupervisionInfo.preambleTransMax)
    {
    case 3:
        SerializeEnum(11, 0);
        break;
    case 4:
        SerializeEnum(11, 1);
        break;
    case 5:
        SerializeEnum(11, 2);
        break;
    case 6:
        SerializeEnum(11, 3);
        break;
    case 7:
        SerializeEnum(11, 4);
        break;
    case 8:
        SerializeEnum(11, 5);
        break;
    case 10:
        SerializeEnum(11, 6);
        break;
    case 20:
        SerializeEnum(11, 7);
        break;
    case 50:
        SerializeEnum(11, 8);
        break;
    case 100:
        SerializeEnum(11, 9);
        break;
    case 200:
        SerializeEnum(11, 10);
        break;
    default:
        SerializeEnum(11, 0);
    }

    // ra-ResponseWindowSize
    switch (rachConfigCommon.raSupervisionInfo.raResponseWindowSize)
    {
    case 2:
        SerializeEnum(8, 0);
        break;
    case 3:
        SerializeEnum(8, 1);
        break;
    case 4:
        SerializeEnum(8, 2);
        break;
    case 5:
        SerializeEnum(8, 3);
        break;
    case 6:
        SerializeEnum(8, 4);
        break;
    case 7:
        SerializeEnum(8, 5);
        break;
    case 8:
        SerializeEnum(8, 6);
        break;
    case 10:
        SerializeEnum(8, 7);
        break;
    default:
        SerializeEnum(8, 0);
    }

    SerializeEnum(8, 0);       // mac-ContentionResolutionTimer
    SerializeInteger(1, 1, 8); // maxHARQ-Msg3Tx

    // connEstFailCount
    switch (rachConfigCommon.txFailParam.connEstFailCount)
    {
    case 1:
        SerializeEnum(8, 1);
        break;
    case 2:
        SerializeEnum(8, 2);
        break;
    case 3:
        SerializeEnum(8, 3);
        break;
    case 4:
        SerializeEnum(8, 4);
        break;
    default:
        SerializeEnum(8, 1);
    }
}

void
RrcAsn1Header::SerializeQoffsetRange(int8_t qOffsetRange) const
{
    switch (qOffsetRange)
    {
    case -24:
        SerializeEnum(31, 0);
        break;
    case -22:
        SerializeEnum(31, 1);
        break;
    case -20:
        SerializeEnum(31, 2);
        break;
    case -18:
        SerializeEnum(31, 3);
        break;
    case -16:
        SerializeEnum(31, 4);
        break;
    case -14:
        SerializeEnum(31, 5);
        break;
    case -12:
        SerializeEnum(31, 6);
        break;
    case -10:
        SerializeEnum(31, 7);
        break;
    case -8:
        SerializeEnum(31, 8);
        break;
    case -6:
        SerializeEnum(31, 9);
        break;
    case -5:
        SerializeEnum(31, 10);
        break;
    case -4:
        SerializeEnum(31, 11);
        break;
    case -3:
        SerializeEnum(31, 12);
        break;
    case -2:
        SerializeEnum(31, 13);
        break;
    case -1:
        SerializeEnum(31, 14);
        break;
    case 0:
        SerializeEnum(31, 15);
        break;
    case 1:
        SerializeEnum(31, 16);
        break;
    case 2:
        SerializeEnum(31, 17);
        break;
    case 3:
        SerializeEnum(31, 18);
        break;
    case 4:
        SerializeEnum(31, 19);
        break;
    case 5:
        SerializeEnum(31, 20);
        break;
    case 6:
        SerializeEnum(31, 21);
        break;
    case 8:
        SerializeEnum(31, 22);
        break;
    case 10:
        SerializeEnum(31, 23);
        break;
    case 12:
        SerializeEnum(31, 24);
        break;
    case 14:
        SerializeEnum(31, 25);
        break;
    case 16:
        SerializeEnum(31, 26);
        break;
    case 18:
        SerializeEnum(31, 27);
        break;
    case 20:
        SerializeEnum(31, 28);
        break;
    case 22:
        SerializeEnum(31, 29);
        break;
    case 24:
        SerializeEnum(31, 30);
        break;
    default:
        SerializeEnum(31, 15);
    }
}

void
RrcAsn1Header::SerializeThresholdEutra(NrRrcSap::ThresholdEutra thresholdEutra) const
{
    switch (thresholdEutra.choice)
    {
    case NrRrcSap::ThresholdEutra::THRESHOLD_RSRP:
        SerializeChoice(2, 0, false);
        SerializeInteger(thresholdEutra.range, 0, 97);
        break;
    case NrRrcSap::ThresholdEutra::THRESHOLD_RSRQ:
    default:
        SerializeChoice(2, 1, false);
        SerializeInteger(thresholdEutra.range, 0, 34);
    }
}

void
RrcAsn1Header::SerializeMeasConfig(NrRrcSap::MeasConfig measConfig) const
{
    // Serialize MeasConfig sequence
    // 11 optional fields, extension marker present
    std::bitset<11> measConfigOptional;
    measConfigOptional.set(10, !measConfig.measObjectToRemoveList.empty());
    measConfigOptional.set(9, !measConfig.measObjectToAddModList.empty());
    measConfigOptional.set(8, !measConfig.reportConfigToRemoveList.empty());
    measConfigOptional.set(7, !measConfig.reportConfigToAddModList.empty());
    measConfigOptional.set(6, !measConfig.measIdToRemoveList.empty());
    measConfigOptional.set(5, !measConfig.measIdToAddModList.empty());
    measConfigOptional.set(4, measConfig.haveQuantityConfig);
    measConfigOptional.set(3, measConfig.haveMeasGapConfig);
    measConfigOptional.set(2, measConfig.haveSmeasure);
    measConfigOptional.set(1, false); // preRegistrationInfoHRPD
    measConfigOptional.set(0, measConfig.haveSpeedStatePars);
    SerializeSequence(measConfigOptional, true);

    if (!measConfig.measObjectToRemoveList.empty())
    {
        SerializeSequenceOf(measConfig.measObjectToRemoveList.size(), MAX_OBJECT_ID, 1);
        for (std::list<uint8_t>::iterator it = measConfig.measObjectToRemoveList.begin();
             it != measConfig.measObjectToRemoveList.end();
             it++)
        {
            SerializeInteger(*it, 1, MAX_OBJECT_ID);
        }
    }

    if (!measConfig.measObjectToAddModList.empty())
    {
        SerializeSequenceOf(measConfig.measObjectToAddModList.size(), MAX_OBJECT_ID, 1);
        for (std::list<NrRrcSap::MeasObjectToAddMod>::iterator it =
                 measConfig.measObjectToAddModList.begin();
             it != measConfig.measObjectToAddModList.end();
             it++)
        {
            SerializeSequence(std::bitset<0>(), false);
            SerializeInteger(it->measObjectId, 1, MAX_OBJECT_ID);
            SerializeChoice(4, 0, true); // Select MeasObjectEUTRA

            // Serialize measObjectEutra
            std::bitset<5> measObjOpts;
            measObjOpts.set(4, !it->measObjectEutra.cellsToRemoveList.empty());
            measObjOpts.set(3, !it->measObjectEutra.cellsToAddModList.empty());
            measObjOpts.set(2, !it->measObjectEutra.blackCellsToRemoveList.empty());
            measObjOpts.set(1, !it->measObjectEutra.blackCellsToAddModList.empty());
            measObjOpts.set(0, it->measObjectEutra.haveCellForWhichToReportCGI);
            SerializeSequence(measObjOpts, true);

            // Serialize carrierFreq
            SerializeInteger(it->measObjectEutra.carrierFreq, 0, MAX_NR_ARFCN);

            // Serialize  allowedMeasBandwidth
            SerializeEnum(8, BandwidthToEnum(it->measObjectEutra.allowedMeasBandwidth));

            SerializeBoolean(it->measObjectEutra.presenceAntennaPort1);
            SerializeBitstring(std::bitset<2>(it->measObjectEutra.neighCellConfig));
            SerializeQoffsetRange(it->measObjectEutra.offsetFreq);

            if (!it->measObjectEutra.cellsToRemoveList.empty())
            {
                SerializeSequenceOf(it->measObjectEutra.cellsToRemoveList.size(), MAX_CELL_MEAS, 1);
                for (std::list<uint8_t>::iterator it2 =
                         it->measObjectEutra.cellsToRemoveList.begin();
                     it2 != it->measObjectEutra.cellsToRemoveList.end();
                     it2++)
                {
                    SerializeInteger(*it2, 1, MAX_CELL_MEAS);
                }
            }

            if (!it->measObjectEutra.cellsToAddModList.empty())
            {
                SerializeSequenceOf(it->measObjectEutra.cellsToAddModList.size(), MAX_CELL_MEAS, 1);
                for (std::list<NrRrcSap::CellsToAddMod>::iterator it2 =
                         it->measObjectEutra.cellsToAddModList.begin();
                     it2 != it->measObjectEutra.cellsToAddModList.end();
                     it2++)
                {
                    SerializeSequence(std::bitset<0>(), false);

                    // Serialize cellIndex
                    SerializeInteger(it2->cellIndex, 1, MAX_CELL_MEAS);

                    // Serialize PhysCellId
                    SerializeInteger(it2->physCellId, 0, 503);

                    // Serialize cellIndividualOffset
                    SerializeQoffsetRange(it2->cellIndividualOffset);
                }
            }

            if (!it->measObjectEutra.blackCellsToRemoveList.empty())
            {
                SerializeSequenceOf(it->measObjectEutra.blackCellsToRemoveList.size(),
                                    MAX_CELL_MEAS,
                                    1);
                for (std::list<uint8_t>::iterator it2 =
                         it->measObjectEutra.blackCellsToRemoveList.begin();
                     it2 != it->measObjectEutra.blackCellsToRemoveList.end();
                     it2++)
                {
                    SerializeInteger(*it2, 1, MAX_CELL_MEAS);
                }
            }

            if (!it->measObjectEutra.blackCellsToAddModList.empty())
            {
                SerializeSequenceOf(it->measObjectEutra.blackCellsToAddModList.size(),
                                    MAX_CELL_MEAS,
                                    1);
                for (std::list<NrRrcSap::BlackCellsToAddMod>::iterator it2 =
                         it->measObjectEutra.blackCellsToAddModList.begin();
                     it2 != it->measObjectEutra.blackCellsToAddModList.end();
                     it2++)
                {
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeInteger(it2->cellIndex, 1, MAX_CELL_MEAS);

                    // Serialize PhysCellIdRange
                    // range optional
                    std::bitset<1> rangePresent = std::bitset<1>(it2->physCellIdRange.haveRange);
                    SerializeSequence(rangePresent, false);
                    SerializeInteger(it2->physCellIdRange.start, 0, 503);
                    if (it2->physCellIdRange.haveRange)
                    {
                        switch (it2->physCellIdRange.range)
                        {
                        case 4:
                            SerializeEnum(16, 0);
                            break;
                        case 8:
                            SerializeEnum(16, 1);
                            break;
                        case 12:
                            SerializeEnum(16, 2);
                            break;
                        case 16:
                            SerializeEnum(16, 3);
                            break;
                        case 24:
                            SerializeEnum(16, 4);
                            break;
                        case 32:
                            SerializeEnum(16, 5);
                            break;
                        case 48:
                            SerializeEnum(16, 6);
                            break;
                        case 64:
                            SerializeEnum(16, 7);
                            break;
                        case 84:
                            SerializeEnum(16, 8);
                            break;
                        case 96:
                            SerializeEnum(16, 9);
                            break;
                        case 128:
                            SerializeEnum(16, 10);
                            break;
                        case 168:
                            SerializeEnum(16, 11);
                            break;
                        case 252:
                            SerializeEnum(16, 12);
                            break;
                        case 504:
                            SerializeEnum(16, 13);
                            break;
                        default:
                            SerializeEnum(16, 0);
                        }
                    }
                }
            }

            if (it->measObjectEutra.haveCellForWhichToReportCGI)
            {
                SerializeInteger(it->measObjectEutra.cellForWhichToReportCGI, 0, 503);
            }
        }
    }

    if (!measConfig.reportConfigToRemoveList.empty())
    {
        SerializeSequenceOf(measConfig.reportConfigToRemoveList.size(), MAX_REPORT_CONFIG_ID, 1);
        for (std::list<uint8_t>::iterator it = measConfig.reportConfigToRemoveList.begin();
             it != measConfig.reportConfigToRemoveList.end();
             it++)
        {
            SerializeInteger(*it, 1, MAX_REPORT_CONFIG_ID);
        }
    }

    if (!measConfig.reportConfigToAddModList.empty())
    {
        SerializeSequenceOf(measConfig.reportConfigToAddModList.size(), MAX_REPORT_CONFIG_ID, 1);
        for (std::list<NrRrcSap::ReportConfigToAddMod>::iterator it =
                 measConfig.reportConfigToAddModList.begin();
             it != measConfig.reportConfigToAddModList.end();
             it++)
        {
            SerializeSequence(std::bitset<0>(), false);
            SerializeInteger(it->reportConfigId, 1, MAX_REPORT_CONFIG_ID);
            SerializeChoice(2, 0, false); // reportConfigEUTRA

            // Serialize ReportConfigEUTRA
            SerializeSequence(std::bitset<0>(), true);
            switch (it->reportConfigEutra.triggerType)
            {
            case NrRrcSap::ReportConfigEutra::PERIODICAL:
                SerializeChoice(2, 1, false);
                SerializeSequence(std::bitset<0>(), false);
                switch (it->reportConfigEutra.purpose)
                {
                case NrRrcSap::ReportConfigEutra::REPORT_CGI:
                    SerializeEnum(2, 1);
                    break;
                case NrRrcSap::ReportConfigEutra::REPORT_STRONGEST_CELLS:
                default:
                    SerializeEnum(2, 0);
                }
                break;
            case NrRrcSap::ReportConfigEutra::EVENT:
            default:
                SerializeChoice(2, 0, false);
                SerializeSequence(std::bitset<0>(), false);
                switch (it->reportConfigEutra.eventId)
                {
                case NrRrcSap::ReportConfigEutra::EVENT_A1:
                    SerializeChoice(5, 0, true);
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeThresholdEutra(it->reportConfigEutra.threshold1);
                    break;
                case NrRrcSap::ReportConfigEutra::EVENT_A2:
                    SerializeChoice(5, 1, true);
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeThresholdEutra(it->reportConfigEutra.threshold1);
                    break;
                case NrRrcSap::ReportConfigEutra::EVENT_A3:
                    SerializeChoice(5, 2, true);
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeInteger(it->reportConfigEutra.a3Offset, -30, 30);
                    SerializeBoolean(it->reportConfigEutra.reportOnLeave);
                    break;
                case NrRrcSap::ReportConfigEutra::EVENT_A4:
                    SerializeChoice(5, 3, true);
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeThresholdEutra(it->reportConfigEutra.threshold1);
                    break;
                case NrRrcSap::ReportConfigEutra::EVENT_A5:
                default:
                    SerializeChoice(5, 4, true);
                    SerializeSequence(std::bitset<0>(), false);
                    SerializeThresholdEutra(it->reportConfigEutra.threshold1);
                    SerializeThresholdEutra(it->reportConfigEutra.threshold2);
                }

                SerializeInteger(it->reportConfigEutra.hysteresis, 0, 30);

                switch (it->reportConfigEutra.timeToTrigger)
                {
                case 0:
                    SerializeEnum(16, 0);
                    break;
                case 40:
                    SerializeEnum(16, 1);
                    break;
                case 64:
                    SerializeEnum(16, 2);
                    break;
                case 80:
                    SerializeEnum(16, 3);
                    break;
                case 100:
                    SerializeEnum(16, 4);
                    break;
                case 128:
                    SerializeEnum(16, 5);
                    break;
                case 160:
                    SerializeEnum(16, 6);
                    break;
                case 256:
                    SerializeEnum(16, 7);
                    break;
                case 320:
                    SerializeEnum(16, 8);
                    break;
                case 480:
                    SerializeEnum(16, 9);
                    break;
                case 512:
                    SerializeEnum(16, 10);
                    break;
                case 640:
                    SerializeEnum(16, 11);
                    break;
                case 1024:
                    SerializeEnum(16, 12);
                    break;
                case 1280:
                    SerializeEnum(16, 13);
                    break;
                case 2560:
                    SerializeEnum(16, 14);
                    break;
                case 5120:
                default:
                    SerializeEnum(16, 15);
                }
            } // end trigger type

            // Serialize triggerQuantity
            if (it->reportConfigEutra.triggerQuantity == NrRrcSap::ReportConfigEutra::RSRP)
            {
                SerializeEnum(2, 0);
            }
            else
            {
                SerializeEnum(2, 1);
            }

            // Serialize reportQuantity
            if (it->reportConfigEutra.reportQuantity ==
                NrRrcSap::ReportConfigEutra::SAME_AS_TRIGGER_QUANTITY)
            {
                SerializeEnum(2, 0);
            }
            else
            {
                SerializeEnum(2, 1);
            }

            // Serialize maxReportCells
            SerializeInteger(it->reportConfigEutra.maxReportCells, 1, MAX_CELL_REPORT);

            // Serialize reportInterval
            switch (it->reportConfigEutra.reportInterval)
            {
            case NrRrcSap::ReportConfigEutra::MS120:
                SerializeEnum(16, 0);
                break;
            case NrRrcSap::ReportConfigEutra::MS240:
                SerializeEnum(16, 1);
                break;
            case NrRrcSap::ReportConfigEutra::MS480:
                SerializeEnum(16, 2);
                break;
            case NrRrcSap::ReportConfigEutra::MS640:
                SerializeEnum(16, 3);
                break;
            case NrRrcSap::ReportConfigEutra::MS1024:
                SerializeEnum(16, 4);
                break;
            case NrRrcSap::ReportConfigEutra::MS2048:
                SerializeEnum(16, 5);
                break;
            case NrRrcSap::ReportConfigEutra::MS5120:
                SerializeEnum(16, 6);
                break;
            case NrRrcSap::ReportConfigEutra::MS10240:
                SerializeEnum(16, 7);
                break;
            case NrRrcSap::ReportConfigEutra::MIN1:
                SerializeEnum(16, 8);
                break;
            case NrRrcSap::ReportConfigEutra::MIN6:
                SerializeEnum(16, 9);
                break;
            case NrRrcSap::ReportConfigEutra::MIN12:
                SerializeEnum(16, 10);
                break;
            case NrRrcSap::ReportConfigEutra::MIN30:
                SerializeEnum(16, 11);
                break;
            case NrRrcSap::ReportConfigEutra::MIN60:
                SerializeEnum(16, 12);
                break;
            case NrRrcSap::ReportConfigEutra::SPARE3:
                SerializeEnum(16, 13);
                break;
            case NrRrcSap::ReportConfigEutra::SPARE2:
                SerializeEnum(16, 14);
                break;
            case NrRrcSap::ReportConfigEutra::SPARE1:
            default:
                SerializeEnum(16, 15);
            }

            // Serialize reportAmount
            switch (it->reportConfigEutra.reportAmount)
            {
            case 1:
                SerializeEnum(8, 0);
                break;
            case 2:
                SerializeEnum(8, 1);
                break;
            case 4:
                SerializeEnum(8, 2);
                break;
            case 8:
                SerializeEnum(8, 3);
                break;
            case 16:
                SerializeEnum(8, 4);
                break;
            case 32:
                SerializeEnum(8, 5);
                break;
            case 64:
                SerializeEnum(8, 6);
                break;
            default:
                SerializeEnum(8, 7);
            }
        }
    }

    if (!measConfig.measIdToRemoveList.empty())
    {
        SerializeSequenceOf(measConfig.measIdToRemoveList.size(), MAX_MEAS_ID, 1);
        for (std::list<uint8_t>::iterator it = measConfig.measIdToRemoveList.begin();
             it != measConfig.measIdToRemoveList.end();
             it++)
        {
            SerializeInteger(*it, 1, MAX_MEAS_ID);
        }
    }

    if (!measConfig.measIdToAddModList.empty())
    {
        SerializeSequenceOf(measConfig.measIdToAddModList.size(), MAX_MEAS_ID, 1);
        for (std::list<NrRrcSap::MeasIdToAddMod>::iterator it =
                 measConfig.measIdToAddModList.begin();
             it != measConfig.measIdToAddModList.end();
             it++)
        {
            SerializeInteger(it->measId, 1, MAX_MEAS_ID);
            SerializeInteger(it->measObjectId, 1, MAX_OBJECT_ID);
            SerializeInteger(it->reportConfigId, 1, MAX_REPORT_CONFIG_ID);
        }
    }

    if (measConfig.haveQuantityConfig)
    {
        // QuantityConfig sequence
        // 4 optional fields, only first (EUTRA) present. Extension marker yes.
        std::bitset<4> quantityConfigOpts(0);
        quantityConfigOpts.set(3, 1);
        SerializeSequence(quantityConfigOpts, true);
        SerializeSequence(std::bitset<0>(), false);

        switch (measConfig.quantityConfig.filterCoefficientRSRP)
        {
        case 0:
            SerializeEnum(16, 0);
            break;
        case 1:
            SerializeEnum(16, 1);
            break;
        case 2:
            SerializeEnum(16, 2);
            break;
        case 3:
            SerializeEnum(16, 3);
            break;
        case 4:
            SerializeEnum(16, 4);
            break;
        case 5:
            SerializeEnum(16, 5);
            break;
        case 6:
            SerializeEnum(16, 6);
            break;
        case 7:
            SerializeEnum(16, 7);
            break;
        case 8:
            SerializeEnum(16, 8);
            break;
        case 9:
            SerializeEnum(16, 9);
            break;
        case 11:
            SerializeEnum(16, 10);
            break;
        case 13:
            SerializeEnum(16, 11);
            break;
        case 15:
            SerializeEnum(16, 12);
            break;
        case 17:
            SerializeEnum(16, 13);
            break;
        case 19:
            SerializeEnum(16, 14);
            break;
        default:
            SerializeEnum(16, 4);
        }

        switch (measConfig.quantityConfig.filterCoefficientRSRQ)
        {
        case 0:
            SerializeEnum(16, 0);
            break;
        case 1:
            SerializeEnum(16, 1);
            break;
        case 2:
            SerializeEnum(16, 2);
            break;
        case 3:
            SerializeEnum(16, 3);
            break;
        case 4:
            SerializeEnum(16, 4);
            break;
        case 5:
            SerializeEnum(16, 5);
            break;
        case 6:
            SerializeEnum(16, 6);
            break;
        case 7:
            SerializeEnum(16, 7);
            break;
        case 8:
            SerializeEnum(16, 8);
            break;
        case 9:
            SerializeEnum(16, 9);
            break;
        case 11:
            SerializeEnum(16, 10);
            break;
        case 13:
            SerializeEnum(16, 11);
            break;
        case 15:
            SerializeEnum(16, 12);
            break;
        case 17:
            SerializeEnum(16, 13);
            break;
        case 19:
            SerializeEnum(16, 14);
            break;
        default:
            SerializeEnum(16, 4);
        }
    }

    if (measConfig.haveMeasGapConfig)
    {
        switch (measConfig.measGapConfig.type)
        {
        case NrRrcSap::MeasGapConfig::RESET:
            SerializeChoice(2, 0, false);
            SerializeNull();
            break;
        case NrRrcSap::MeasGapConfig::SETUP:
        default:
            SerializeChoice(2, 1, false);
            SerializeSequence(std::bitset<0>(), false);
            switch (measConfig.measGapConfig.gapOffsetChoice)
            {
            case NrRrcSap::MeasGapConfig::GP0:
                SerializeChoice(2, 0, true);
                SerializeInteger(measConfig.measGapConfig.gapOffsetValue, 0, 39);
                break;
            case NrRrcSap::MeasGapConfig::GP1:
            default:
                SerializeChoice(2, 1, true);
                SerializeInteger(measConfig.measGapConfig.gapOffsetValue, 0, 79);
            }
        }
    }

    if (measConfig.haveSmeasure)
    {
        SerializeInteger(measConfig.sMeasure, 0, 97);
    }

    // ...Here preRegistrationInfoHRPD would be serialized

    if (measConfig.haveSpeedStatePars)
    {
        switch (measConfig.speedStatePars.type)
        {
        case NrRrcSap::SpeedStatePars::RESET:
            SerializeChoice(2, 0, false);
            SerializeNull();
            break;
        case NrRrcSap::SpeedStatePars::SETUP:
        default:
            SerializeChoice(2, 1, false);
            SerializeSequence(std::bitset<0>(), false);
            switch (measConfig.speedStatePars.mobilityStateParameters.tEvaluation)
            {
            case 30:
                SerializeEnum(8, 0);
                break;
            case 60:
                SerializeEnum(8, 1);
                break;
            case 120:
                SerializeEnum(8, 2);
                break;
            case 180:
                SerializeEnum(8, 3);
                break;
            case 240:
                SerializeEnum(8, 4);
                break;
            default:
                SerializeEnum(8, 5);
                break;
            }

            switch (measConfig.speedStatePars.mobilityStateParameters.tHystNormal)
            {
            case 30:
                SerializeEnum(8, 0);
                break;
            case 60:
                SerializeEnum(8, 1);
                break;
            case 120:
                SerializeEnum(8, 2);
                break;
            case 180:
                SerializeEnum(8, 3);
                break;
            case 240:
                SerializeEnum(8, 4);
                break;
            default:
                SerializeEnum(8, 5);
                break;
            }

            SerializeInteger(measConfig.speedStatePars.mobilityStateParameters.nCellChangeMedium,
                             1,
                             16);
            SerializeInteger(measConfig.speedStatePars.mobilityStateParameters.nCellChangeHigh,
                             1,
                             16);

            SerializeSequence(std::bitset<0>(), false);
            switch (measConfig.speedStatePars.timeToTriggerSf.sfMedium)
            {
            case 25:
                SerializeEnum(4, 0);
                break;
            case 50:
                SerializeEnum(4, 1);
                break;
            case 75:
                SerializeEnum(4, 2);
                break;
            case 100:
            default:
                SerializeEnum(4, 3);
            }

            switch (measConfig.speedStatePars.timeToTriggerSf.sfHigh)
            {
            case 25:
                SerializeEnum(4, 0);
                break;
            case 50:
                SerializeEnum(4, 1);
                break;
            case 75:
                SerializeEnum(4, 2);
                break;
            case 100:
            default:
                SerializeEnum(4, 3);
            }
        }
    }
}

void
RrcAsn1Header::SerializeNonCriticalExtensionConfiguration(
    NrRrcSap::NonCriticalExtensionConfiguration nonCriticalExtension) const
{
    // 3 optional fields. Extension marker not present.
    std::bitset<3> noncriticalExtension_v1020;
    noncriticalExtension_v1020.set(
        2,
        !nonCriticalExtension.sCellToReleaseList.empty()); // sCellToReleaseList-r10
    noncriticalExtension_v1020.set(
        1,
        !nonCriticalExtension.sCellToAddModList.empty()); // sCellToAddModList-r10
    noncriticalExtension_v1020.set(
        0,
        0); // No nonCriticalExtension RRCConnectionReconfiguration-v1130-IEs
    SerializeSequence(noncriticalExtension_v1020, false);

    if (!nonCriticalExtension.sCellToReleaseList.empty())
    {
        SerializeSequenceOf(nonCriticalExtension.sCellToReleaseList.size(), MAX_OBJECT_ID, 1);
        for (uint8_t sCellIndex : nonCriticalExtension.sCellToReleaseList)
        {
            SerializeInteger(sCellIndex, 1, 7); // sCellIndex-r10
        }
    }

    if (!nonCriticalExtension.sCellToAddModList.empty())
    {
        SerializeSequenceOf(nonCriticalExtension.sCellToAddModList.size(), MAX_OBJECT_ID, 1);
        for (auto& it : nonCriticalExtension.sCellToAddModList)
        {
            std::bitset<4> sCellToAddMod_r10;
            sCellToAddMod_r10.set(3, 1); // sCellIndex
            sCellToAddMod_r10.set(2, 1); // CellIdentification
            sCellToAddMod_r10.set(1, 1); // RadioResourceConfigCommonSCell
            sCellToAddMod_r10.set(
                0,
                it.haveRadioResourceConfigDedicatedSCell); // No nonCriticalExtension RRC
            SerializeSequence(sCellToAddMod_r10, false);
            SerializeInteger(it.sCellIndex, 1, 7); // sCellIndex-r10

            // Serialize CellIdentification
            std::bitset<2> cellIdentification_r10;
            cellIdentification_r10.set(1, 1); // phyCellId-r10
            cellIdentification_r10.set(0, 1); // dl-CarrierFreq-r10
            SerializeSequence(cellIdentification_r10, false);

            SerializeInteger(it.cellIdentification.physCellId, 1, 65536);
            SerializeInteger(it.cellIdentification.dlCarrierFreq, 1, MAX_NR_ARFCN);

            // Serialize RadioResourceConfigCommonSCell
            SerializeRadioResourceConfigCommonSCell(it.radioResourceConfigCommonSCell);

            if (it.haveRadioResourceConfigDedicatedSCell)
            {
                // Serialize RadioResourceConfigDedicatedSCell
                SerializeRadioResourceDedicatedSCell(it.radioResourceConfigDedicatedSCell);
            }
        }
    }
}

void
RrcAsn1Header::SerializeRadioResourceConfigCommonSCell(
    NrRrcSap::RadioResourceConfigCommonSCell rrccsc) const
{
    // 2 optional fields. Extension marker not present.
    std::bitset<2> radioResourceConfigCommonSCell_r10;
    radioResourceConfigCommonSCell_r10.set(1, rrccsc.haveNonUlConfiguration); // NonUlConfiguration
    radioResourceConfigCommonSCell_r10.set(0, rrccsc.haveUlConfiguration);    // UlConfiguration
    SerializeSequence(radioResourceConfigCommonSCell_r10, false);

    if (rrccsc.haveNonUlConfiguration)
    {
        // 5 optional fields. Extension marker not present.
        std::bitset<5> nonUlConfiguration_r10;
        nonUlConfiguration_r10.set(4, 1); // Dl- bandwidth --> convert in enum
        nonUlConfiguration_r10.set(3, 1); // AntennaInfoCommon-r10
        nonUlConfiguration_r10.set(2, 0); // phich-Config-r10 Not Implemented
        nonUlConfiguration_r10.set(1, 1); // pdschConfigCommon
        nonUlConfiguration_r10.set(0, 0); // Tdd-Config-r10 Not Implemented
        SerializeSequence(nonUlConfiguration_r10, false);

        SerializeInteger(rrccsc.nonUlConfiguration.dlBandwidth, 6, 1000);

        std::bitset<1> antennaInfoCommon_r10;
        antennaInfoCommon_r10.set(0, 1);
        SerializeSequence(antennaInfoCommon_r10, false);
        SerializeInteger(rrccsc.nonUlConfiguration.antennaInfoCommon.antennaPortsCount, 0, 65536);

        std::bitset<2> pdschConfigCommon_r10;
        pdschConfigCommon_r10.set(1, 1);
        pdschConfigCommon_r10.set(0, 1);
        SerializeSequence(pdschConfigCommon_r10, false);

        SerializeInteger(rrccsc.nonUlConfiguration.pdschConfigCommon.referenceSignalPower, -60, 50);
        SerializeInteger(rrccsc.nonUlConfiguration.pdschConfigCommon.pb, 0, 3);
    }
    if (rrccsc.haveUlConfiguration)
    {
        // Serialize Ul Configuration
        //  7 optional fields. Extension marker present.
        std::bitset<7> UlConfiguration_r10;
        UlConfiguration_r10.set(6, 1); // ul-Configuration-r10
        UlConfiguration_r10.set(5, 0); // p-Max-r10 Not Implemented
        UlConfiguration_r10.set(4, 1); // uplinkPowerControlCommonSCell-r10
        UlConfiguration_r10.set(3, 0); // soundingRS-UL-ConfigCommon-r10
        UlConfiguration_r10.set(2, 0); // ul-CyclicPrefixLength-r10
        UlConfiguration_r10.set(1, 1); // prach-ConfigSCell-r10
        UlConfiguration_r10.set(0, 0); // pusch-ConfigCommon-r10 Not Implemented
        SerializeSequence(UlConfiguration_r10, true);

        // Serialize ulFreqInfo
        std::bitset<3> FreqInfo_r10;
        FreqInfo_r10.set(2, 1); // ulCarrierFreq
        FreqInfo_r10.set(1, 1); // UlBandwidth
        FreqInfo_r10.set(0, 0); // additionalSpectrumEmissionSCell-r10 Not Implemented
        SerializeSequence(FreqInfo_r10, false);

        SerializeInteger(rrccsc.ulConfiguration.ulFreqInfo.ulCarrierFreq, 0, MAX_NR_ARFCN);
        SerializeInteger(rrccsc.ulConfiguration.ulFreqInfo.ulBandwidth, 6, 1000);

        // Serialize UlPowerControllCommonSCell
        std::bitset<2> UlPowerControlCommonSCell_r10;
        UlPowerControlCommonSCell_r10.set(1, 0); // p0-NominalPUSCH-r10 Not Implemented
        UlPowerControlCommonSCell_r10.set(0, 1); // alpha
        SerializeSequence(UlPowerControlCommonSCell_r10, false);

        SerializeInteger(rrccsc.ulConfiguration.ulPowerControlCommonSCell.alpha, 0, 65536);

        // Serialize soundingRs-UlConfigCommon
        // Not Implemented

        // Serialize PrachConfigSCell
        std::bitset<1> prachConfigSCell_r10;
        prachConfigSCell_r10.set(0, 1);
        SerializeSequence(prachConfigSCell_r10, false);
        SerializeInteger(rrccsc.ulConfiguration.prachConfigSCell.index, 0, 256);
    }
}

void
RrcAsn1Header::SerializeRadioResourceDedicatedSCell(
    NrRrcSap::RadioResourceConfigDedicatedSCell rrcdsc) const
{
    // Serialize RadioResourceConfigDedicatedSCell
    std::bitset<1> RadioResourceConfigDedicatedSCell_r10;
    RadioResourceConfigDedicatedSCell_r10.set(0, 1);
    SerializeSequence(RadioResourceConfigDedicatedSCell_r10, false);

    NrRrcSap::PhysicalConfigDedicatedSCell pcdsc = rrcdsc.physicalConfigDedicatedSCell;
    SerializePhysicalConfigDedicatedSCell(pcdsc);
}

void
RrcAsn1Header::SerializePhysicalConfigDedicatedSCell(
    NrRrcSap::PhysicalConfigDedicatedSCell pcdsc) const
{
    std::bitset<2> pcdscOpt;
    pcdscOpt.set(1, pcdsc.haveNonUlConfiguration);
    pcdscOpt.set(0, pcdsc.haveUlConfiguration);
    SerializeSequence(pcdscOpt, true);

    if (pcdsc.haveNonUlConfiguration)
    {
        // Serialize NonUl configuration
        std::bitset<4> nulOpt;
        nulOpt.set(3, pcdsc.haveAntennaInfoDedicated);
        nulOpt.set(2, 0); // crossCarrierSchedulingConfig-r10 Not Implemented
        nulOpt.set(1, 0); // csi-RS-Config-r10 Not Implemented
        nulOpt.set(0, pcdsc.havePdschConfigDedicated); // pdsch-ConfigDedicated-r10
        SerializeSequence(nulOpt, false);

        if (pcdsc.haveAntennaInfoDedicated)
        {
            // Serialize antennaInfo choice
            // 2 options. Selected: 0 ("explicitValue" of type "AntennaInfoDedicated")
            SerializeChoice(2, 0, false);

            // Serialize AntennaInfoDedicated sequence
            // 1 optional parameter, not present. No extension marker.
            SerializeSequence(std::bitset<1>(0), false);

            // Serialize transmissionMode
            // Assuming the value in the struct is the enum index
            SerializeEnum(8, pcdsc.antennaInfo.transmissionMode);

            // Serialize ue-TransmitAntennaSelection choice
            SerializeChoice(2, 0, false);

            // Serialize release
            SerializeNull();
        }
        if (pcdsc.havePdschConfigDedicated)
        {
            // Serialize Pdsch-ConfigDedicated Sequence:
            // 0 optional / default fields, no extension marker.
            SerializeSequence(std::bitset<0>(), false);

            // Serialize  p-a
            // Assuming the value in the struct is the enum index
            SerializeEnum(8, pcdsc.pdschConfigDedicated.pa);

            // Serialize release
            SerializeNull();
        }
    }
    if (pcdsc.haveUlConfiguration)
    {
        // Serialize Ul Configuration
        std::bitset<7> ulOpt;
        ulOpt.set(6, pcdsc.haveAntennaInfoUlDedicated); // antennaInfoUL-r10
        ulOpt.set(5, 0); // pusch-ConfigDedicatedSCell-r10 not present
        ulOpt.set(4, 0); // uplinkPowerControlDedicatedSCell-r10 not present
        ulOpt.set(3, 0); // cqi-ReportConfigSCell-r10 not present
        ulOpt.set(2, pcdsc.haveSoundingRsUlConfigDedicated); // soundingRS-UL-ConfigDedicated-r10
        ulOpt.set(1, 0); // soundingRS-UL-ConfigDedicated-v1020 not present
        ulOpt.set(0, 0); // soundingRS-UL-ConfigDedicatedAperiodic-r10 not present
        SerializeSequence(ulOpt, false);

        if (pcdsc.haveAntennaInfoUlDedicated)
        {
            // Serialize antennaInfo choice
            // 2 options. Selected: 0 ("explicitValue" of type "AntennaInfoDedicated")
            SerializeChoice(2, 0, false);

            // Serialize AntennaInfoDedicated sequence
            // 1 optional parameter, not present. No extension marker.
            SerializeSequence(std::bitset<1>(0), false);

            // Serialize transmissionMode
            // Assuming the value in the struct is the enum index
            SerializeEnum(8, pcdsc.antennaInfoUl.transmissionMode);

            // Serialize ue-TransmitAntennaSelection choice
            SerializeChoice(2, 0, false);

            // Serialize release
            SerializeNull();
        }
        if (pcdsc.haveSoundingRsUlConfigDedicated)
        {
            // Serialize SoundingRS-UL-ConfigDedicated choice:
            switch (pcdsc.soundingRsUlConfigDedicated.type)
            {
            case NrRrcSap::SoundingRsUlConfigDedicated::RESET:
                SerializeChoice(2, 0, false);
                SerializeNull();
                break;

            case NrRrcSap::SoundingRsUlConfigDedicated::SETUP:
            default:
                // 2 options, selected: 1 (setup)
                SerializeChoice(2, 1, false);

                // Serialize setup sequence
                // 0 optional / default fields, no extension marker.
                SerializeSequence(std::bitset<0>(), false);

                // Serialize srs-Bandwidth
                SerializeEnum(4, pcdsc.soundingRsUlConfigDedicated.srsBandwidth);

                // Serialize  srs-HoppingBandwidth
                SerializeEnum(4, 0);

                // Serialize freqDomainPosition
                SerializeInteger(0, 0, 23);

                // Serialize duration
                SerializeBoolean(false);

                // Serialize srs-ConfigIndex
                SerializeInteger(pcdsc.soundingRsUlConfigDedicated.srsConfigIndex, 0, 1023);

                // Serialize transmissionComb
                SerializeInteger(0, 0, 1);

                // Serialize cyclicShift
                SerializeEnum(8, 0);

                break;
            }
        }
    }
}

Buffer::Iterator
RrcAsn1Header::DeserializeThresholdEutra(NrRrcSap::ThresholdEutra* thresholdEutra,
                                         Buffer::Iterator bIterator)
{
    int thresholdEutraChoice;
    int range;
    bIterator = DeserializeChoice(2, false, &thresholdEutraChoice, bIterator);

    switch (thresholdEutraChoice)
    {
    case 0:
        thresholdEutra->choice = NrRrcSap::ThresholdEutra::THRESHOLD_RSRP;
        bIterator = DeserializeInteger(&range, 0, 97, bIterator);
        thresholdEutra->range = range;
        break;
    case 1:
    default:
        thresholdEutra->choice = NrRrcSap::ThresholdEutra::THRESHOLD_RSRQ;
        bIterator = DeserializeInteger(&range, 0, 34, bIterator);
        thresholdEutra->range = range;
    }

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeQoffsetRange(int8_t* qOffsetRange, Buffer::Iterator bIterator)
{
    int n;
    bIterator = DeserializeEnum(31, &n, bIterator);
    switch (n)
    {
    case 0:
        *qOffsetRange = -24;
        break;
    case 1:
        *qOffsetRange = -22;
        break;
    case 2:
        *qOffsetRange = -20;
        break;
    case 3:
        *qOffsetRange = -18;
        break;
    case 4:
        *qOffsetRange = -16;
        break;
    case 5:
        *qOffsetRange = -14;
        break;
    case 6:
        *qOffsetRange = -12;
        break;
    case 7:
        *qOffsetRange = -10;
        break;
    case 8:
        *qOffsetRange = -8;
        break;
    case 9:
        *qOffsetRange = -6;
        break;
    case 10:
        *qOffsetRange = -5;
        break;
    case 11:
        *qOffsetRange = -4;
        break;
    case 12:
        *qOffsetRange = -3;
        break;
    case 13:
        *qOffsetRange = -2;
        break;
    case 14:
        *qOffsetRange = -1;
        break;
    case 15:
        *qOffsetRange = 0;
        break;
    case 16:
        *qOffsetRange = 1;
        break;
    case 17:
        *qOffsetRange = 2;
        break;
    case 18:
        *qOffsetRange = 3;
        break;
    case 19:
        *qOffsetRange = 4;
        break;
    case 20:
        *qOffsetRange = 5;
        break;
    case 21:
        *qOffsetRange = 6;
        break;
    case 22:
        *qOffsetRange = 8;
        break;
    case 23:
        *qOffsetRange = 10;
        break;
    case 24:
        *qOffsetRange = 12;
        break;
    case 25:
        *qOffsetRange = 14;
        break;
    case 26:
        *qOffsetRange = 16;
        break;
    case 27:
        *qOffsetRange = 18;
        break;
    case 28:
        *qOffsetRange = 20;
        break;
    case 29:
        *qOffsetRange = 22;
        break;
    case 30:
    default:
        *qOffsetRange = 24;
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeBwpInactivityTimer(NrRrcSap::Bwp_InactivityTimer* usedValue, Buffer::Iterator bIterator)
{
    int n;
    bIterator = DeserializeEnum(31, &n, bIterator);
    switch (n)
    {
    case 0:
        *usedValue = NrRrcSap::ms2;
        break;
    case 1:
        *usedValue = NrRrcSap::ms3;
        break;
    case 2:
        *usedValue = NrRrcSap::ms4;
        break;
    case 3:
        *usedValue = NrRrcSap::ms5;
        break;
    case 4:
        *usedValue = NrRrcSap::ms6;
        break;
    case 5:
        *usedValue = NrRrcSap::ms8;
        break;
    case 6:
        *usedValue = NrRrcSap::ms10;
        break;
    case 7:
        *usedValue = NrRrcSap::ms20;
        break;
    case 8:
        *usedValue = NrRrcSap::ms30;
        break;
    case 9:
        *usedValue = NrRrcSap::ms40;
        break;
    case 10:
        *usedValue = NrRrcSap::ms50;
        break;
    case 11:
        *usedValue = NrRrcSap::ms60;
        break;
    case 12:
        *usedValue = NrRrcSap::ms80;
        break;
    case 13:
        *usedValue = NrRrcSap::ms100;
        break;
    case 14:
        *usedValue = NrRrcSap::ms200;
        break;
    case 15:
        *usedValue = NrRrcSap::ms300;
        break;
    case 16:
        *usedValue = NrRrcSap::ms500;
        break;
    case 17:
        *usedValue = NrRrcSap::ms750;
        break;
    case 18:
        *usedValue = NrRrcSap::ms1280;
        break;
    case 19:
        *usedValue = NrRrcSap::ms1920;
        break;
    case 20:
        *usedValue = NrRrcSap::ms2560;
        break;
    case 21:
        *usedValue = NrRrcSap::spare10;
        break;
    case 22:
        *usedValue = NrRrcSap::spare9;
        break;
    case 23:
        *usedValue = NrRrcSap::spare8;
        break;
    case 24:
        *usedValue = NrRrcSap::spare7;
        break;
    case 25:
        *usedValue = NrRrcSap::spare6;
        break;
    case 26:
        *usedValue = NrRrcSap::spare5;
        break;
    case 27:
        *usedValue = NrRrcSap::spare4;
        break;
    case 28:
        *usedValue = NrRrcSap::spare3;
        break;
    case 29:
        *usedValue = NrRrcSap::spare2;
        break;
    case 30:
        *usedValue = NrRrcSap::spare1;
        break;
    default:
        *usedValue = NrRrcSap::ms100;
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeSuspendConfig(NrRrcSap::SuspendConfig* scfg, Buffer::Iterator bIterator)
{
    int n;

    // Serialize full-rnti
    std::bitset<40> fullRnti;
    bIterator = DeserializeBitstring(&fullRnti,bIterator);
    scfg->fullRnti =fullRnti.to_ulong();

    std::bitset<24> shortRnti;
    bIterator =DeserializeBitstring(&shortRnti,bIterator);
    scfg->shortRnti =shortRnti.to_ulong();

    //Deserialize ran-PagingCycle
    bIterator = DeserializeEnum(4, &n, bIterator);
    switch (n)
    {
    case 0:
        scfg->ran_PagingCycle= NrRrcSap::rf32;
        break;
    case 1:
        scfg->ran_PagingCycle= NrRrcSap::rf64;
        break;
    case 2:
        scfg->ran_PagingCycle= NrRrcSap::rf128;
        break;
    case 3:
        scfg->ran_PagingCycle= NrRrcSap::rf256;
        break;
    default:
        scfg->ran_PagingCycle= NrRrcSap::rf64;
    }
   

    // Deserialize optionalFields1 
    std::bitset<2> optionalFieldsPresent1 = std::bitset<2>();
    bIterator = DeserializeSequence(&optionalFieldsPresent1, false, bIterator);

    if(optionalFieldsPresent1[1])
    {
        // Deserialize RAN-NotificationAreaInfo
        //...
    }

    if(optionalFieldsPresent1[0])
    {
        // Deserialize t380 
        bIterator = DeserializeEnum(8, &n, bIterator);
        switch (n)
        {
        case 0:
            scfg->t380= NrRrcSap::min5;
            break;
        case 1:
            scfg->t380= NrRrcSap::min10;
            break;
        case 2:
            scfg->t380= NrRrcSap::min20;
            break;
        case 3:
            scfg->t380= NrRrcSap::min30;
            break;
        case 4:
            scfg->t380= NrRrcSap::min60;
            break;
        case 5:
            scfg->t380= NrRrcSap::min120;
            break;
        case 6:
            scfg->t380= NrRrcSap::min360;
            break;
        case 7:
            scfg->t380= NrRrcSap::min720;
            break;
        default:
            scfg->t380= NrRrcSap::min60;
        }

    }
  
    //NextHopChainingCount
    bIterator = DeserializeInteger(&n,0,7,bIterator); //unused

    // Deserialize optionalFields3
    std::bitset<4> optionalFieldsPresent2 = std::bitset<4>();
    bIterator = DeserializeSequence(&optionalFieldsPresent2, false, bIterator);

      
    if(optionalFieldsPresent2[3])
    {
        // Deserialize SL-ServingCellInfo-r17 
        //...
    }

      if(optionalFieldsPresent2[2])
    {
        // Deserialize sdt-Config-r17
         bIterator =DeserializeSdtConfig(&scfg->sdt_Config_r17,bIterator);
    }

      if(optionalFieldsPresent2[1])
    {
        // Deserialize SRS-PosRRC-InactiveConfig-r17
        //...
    }

      if(optionalFieldsPresent2[0])
    {
        // Deserialize ExtendedPagingCycle-r17
        bIterator = DeserializeEnum(4, &n, bIterator);
        switch (n)
        {
        case 0:
            scfg->ran_ExtendedPagingCycle_r17= NrRrcSap::e_rf256;
            break;
        case 1:
            scfg->ran_ExtendedPagingCycle_r17= NrRrcSap::e_rf512;
            break;
        case 2:
            scfg->ran_ExtendedPagingCycle_r17= NrRrcSap::e_rf1024;
            break;
        case 3:
            scfg->ran_ExtendedPagingCycle_r17= NrRrcSap::e_spare1;
            break;
        default:
            scfg->ran_ExtendedPagingCycle_r17= NrRrcSap::e_rf512;
        }
    }

   


    return bIterator;
    
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioResourceConfigDedicated(
    NrRrcSap::RadioResourceConfigDedicated* radioResourceConfigDedicated,
    Buffer::Iterator bIterator)
{
    // Deserialize RadioResourceConfigDedicated sequence
    std::bitset<6> optionalFieldsPresent = std::bitset<6>();
    bIterator = DeserializeSequence(&optionalFieldsPresent, true, bIterator);

    if (optionalFieldsPresent[5])
    {
        // Deserialize srb-ToAddModList
        bIterator =
            DeserializeSrbToAddModList(&(radioResourceConfigDedicated->srbToAddModList), bIterator);
    }

    if (optionalFieldsPresent[4])
    {
        // Deserialize drb-ToAddModList
        bIterator =
            DeserializeDrbToAddModList(&(radioResourceConfigDedicated->drbToAddModList), bIterator);
    }

    if (optionalFieldsPresent[3])
    {
        // Deserialize drb-ToReleaseList
        int n;
        int val;
        bIterator = DeserializeSequenceOf(&n, MAX_DRB, 1, bIterator);
        for (int i = 0; i < n; i++)
        {
            bIterator = DeserializeInteger(&val, 1, 32, bIterator);
            radioResourceConfigDedicated->drbToReleaseList.push_back(val);
        }
    }

    if (optionalFieldsPresent[2])
    {
        // Deserialize mac-MainConfig
        // ...
    }

    if (optionalFieldsPresent[1])
    {
        // Deserialize sps-Config
        // ...
    }

    radioResourceConfigDedicated->havePhysicalConfigDedicated = optionalFieldsPresent[0];
    if (optionalFieldsPresent[0])
    {
        // Deserialize physicalConfigDedicated
        bIterator = DeserializePhysicalConfigDedicated(
            &radioResourceConfigDedicated->physicalConfigDedicated,
            bIterator);
    }

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioBearerConfig(
    NrRrcSap::RadioBearerConfig* radioBearerConfig,
    Buffer::Iterator bIterator)
{
    // Deserialize RadioResourceConfigDedicated sequence
    std::bitset<9> optionalFieldsPresent = std::bitset<9>();
    bIterator = DeserializeSequence(&optionalFieldsPresent, true, bIterator);

    if (optionalFieldsPresent[8])
    {
        // Deserialize srb-ToAddModList
        bIterator =
            DeserializeSrbToAddModList(&(radioBearerConfig->srbToAddModList), bIterator);
    }
     if (optionalFieldsPresent[7])
    {
        // Deserialize srb3-ToRelease 
        //...
    }
     if (optionalFieldsPresent[6])
    {
       // Deserialize drb-ToAddModList
        bIterator =
            DeserializeDrbToAddModList(&(radioBearerConfig->drbToAddModList), bIterator);
    }
     if (optionalFieldsPresent[5])
    {
         // Deserialize drb-ToReleaseList
        int n;
        int val;
        bIterator = DeserializeSequenceOf(&n, MAX_DRB, 1, bIterator);
        for (int i = 0; i < n; i++)
        {
            bIterator = DeserializeInteger(&val, 1, 32, bIterator);
            radioBearerConfig->drbToReleaseList.push_back(val);
        }
    }
     if (optionalFieldsPresent[4])
    {
        // Deserialize securityConfig
        // ...
    }
    if (optionalFieldsPresent[3])
    {
        // Deserialize mrb-ToAddModList-r17 
        // ...
    }

    if (optionalFieldsPresent[2])
    {
        // Deserialize mrb-ToReleaseList-r17
        // ...
    }

    if (optionalFieldsPresent[1])
    {
        // Deserialize srb-ToAddModListExt-r17
        // ...
    }

    if (optionalFieldsPresent[0])
    {
        // Deserialize srb4-ToRelease-r17 
        // ...
    }


    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeSpCellConfig(
    NrRrcSap::SpCellConfig* spCellConfig,
    Buffer::Iterator bIterator)
{
    bIterator = DeserializeSpCellConfigDedicated(&spCellConfig->spCellConfigDedicated, bIterator);

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeSpCellConfigDedicated(
    NrRrcSap::ServingCellConfig* spCellConfigDedicated,
    Buffer::Iterator bIterator)
{
    // Deserialize SpCellConfigDedicated sequence
    std::bitset<2> optionalFieldsPresent = std::bitset<2>();
    bIterator = DeserializeSequence(&optionalFieldsPresent, true, bIterator);
    if (optionalFieldsPresent[1])
    {
        // Deserialize downlinkBWP_ToReleaseList
        int n;
        int val;
        bIterator = DeserializeSequenceOf(&n, MAX_BWP_CONFIGURED, 1, bIterator);
        for (int i = 0; i < n; i++)
        {
            bIterator = DeserializeInteger(&val, 0, MAX_BWP_DEFINED, bIterator);
            spCellConfigDedicated->downlinkBWP_ToReleaseList.push_back(val);
        }
    }

    if (optionalFieldsPresent[0])
    {
        // Deserialize downlinkBWP_ToAddModList
        bIterator =
            DeserializeDlBwpToAddModList(&(spCellConfigDedicated->downlinkBWP_ToAddModList), bIterator);
    }


    int n;
    // Deserialize firstActiveDownlinkBWP_Id
    bIterator = DeserializeInteger(&n, 0, MAX_BWP_DEFINED,bIterator); 
    spCellConfigDedicated->firstActiveDownlinkBWP_Id = n;

    // Deserialize bwp_inactivityTimer
    bIterator = DeserializeBwpInactivityTimer(&spCellConfigDedicated->bwp_InactivityTimer, bIterator);


    // Deserialize uplinkConfig
    DeserializeUplinkConfig(&spCellConfigDedicated->uplinkConfig, bIterator);
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeDlBwpToAddModList(std::list<NrRrcSap::BWP_Downlink>* dlBwpToAddModList,  Buffer::Iterator bIterator)
{  
    int n;
    int val;
    bIterator = DeserializeSequenceOf(&n, MAX_BWP_CONFIGURED, 1, bIterator);
    dlBwpToAddModList->clear();
    for (int i = 0; i < n; i++)
    {
        NrRrcSap::BWP_Downlink dlBwpToAdd;
        bIterator = DeserializeInteger(&val, 0, MAX_BWP_DEFINED, bIterator);
        dlBwpToAdd.bwp_Id = val;

        bIterator = DeserializeBwpDlCommon(&dlBwpToAdd.bwp_dlCommon, bIterator);

        dlBwpToAddModList->push_back(dlBwpToAdd);
    }
    return bIterator;

}

Buffer::Iterator
RrcAsn1Header::DeserializeUlBwpToAddModList(std::list<NrRrcSap::BWP_Uplink>* ulBwpToAddModList, Buffer::Iterator bIterator)
{  
    int n;
    int val;
    bIterator = DeserializeSequenceOf(&n, MAX_BWP_CONFIGURED, 1, bIterator);
    ulBwpToAddModList->clear();
    for (int i = 0; i < n; i++)
    {
        NrRrcSap::BWP_Uplink ulBwpToAdd;
        bIterator = DeserializeInteger(&val, 0, MAX_BWP_DEFINED, bIterator);
        ulBwpToAdd.bwp_Id = val;

        bIterator = DeserializeBwpUlCommon(&ulBwpToAdd.bwp_UplinkCommon, bIterator);

        ulBwpToAddModList->push_back(ulBwpToAdd);
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeBwpDlCommon(NrRrcSap::Bwp_DownlinkCommon* bwp_DownlinkCommon, Buffer::Iterator bIterator)
{
    bIterator = DeserializeBWP(&bwp_DownlinkCommon->genericParameters, bIterator);
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeBwpUlCommon(NrRrcSap::Bwp_UplinkCommon* bwp_UplinkCommon, Buffer::Iterator bIterator)
{
    bIterator = DeserializeBWP(&bwp_UplinkCommon->genericParameters, bIterator);
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeBWP(NrRrcSap::BWP* bwp,  Buffer::Iterator bIterator)
{
    int val;
    bIterator = DeserializeInteger(&val,0,37949, bIterator);
    bwp->locationAndBandwidth = val;
    return bIterator;
}



Buffer::Iterator
RrcAsn1Header::DeserializeUplinkConfig(
    NrRrcSap::UplinkConfig* uplinkConfig,
    Buffer::Iterator bIterator)
{
    // Deserialize UplinkConfig sequence
    std::bitset<2> optionalFieldsPresent = std::bitset<2>();
    bIterator = DeserializeSequence(&optionalFieldsPresent, true, bIterator);
    if (optionalFieldsPresent[1])
    {
        // Deserialize uplinkBWP_ToReleaseList
        int n;
        int val;
        bIterator = DeserializeSequenceOf(&n, MAX_BWP_CONFIGURED, 1, bIterator);
        for (int i = 0; i < n; i++)
        {
            bIterator = DeserializeInteger(&val, 0, MAX_BWP_DEFINED, bIterator);
            uplinkConfig->uplinkBWP_ToReleaseList.push_back(val);
        }
    }

    if (optionalFieldsPresent[0])
    {
        // Deserialize uplinkBWP_ToAddModList
        bIterator =
            DeserializeUlBwpToAddModList(&(uplinkConfig->uplinkBWP_ToAddModList), bIterator);
    
    }
    int n;
    // DeSerialize firstActiveUplinkBWP_Id
    bIterator = DeserializeInteger(&n, 0, MAX_BWP_DEFINED,bIterator); //a UE can have 4 Bwps configured
    uplinkConfig->firstActiveUplinkBWP_Id = n;

    return bIterator;
}



Buffer::Iterator
RrcAsn1Header::DeserializeSrbToAddModList(std::list<NrRrcSap::SrbToAddMod>* srbToAddModList,
                                          Buffer::Iterator bIterator)
{
    int numElems;
    bIterator = DeserializeSequenceOf(&numElems, 2, 1, bIterator);

    srbToAddModList->clear();

    // Deserialize SRB-ToAddMod elements
    for (int i = 0; i < numElems; i++)
    {
        NrRrcSap::SrbToAddMod srbToAddMod;
        // Deserialize SRB-ToAddMod sequence
        // 2 optional fields, extension marker present
        std::bitset<2> optionalFields;
        bIterator = DeserializeSequence(&optionalFields, true, bIterator);

        // Deserialize srbIdentity
        int n;
        bIterator = DeserializeInteger(&n, 1, 2, bIterator);
        srbToAddMod.srbIdentity = n;

        if (optionalFields[1])
        {
            // Deserialize rlcConfig choice
            // ...
        }

        if (optionalFields[0])
        {
            // Deserialize logicalChannelConfig choice
            int sel;
            bIterator = DeserializeChoice(2, false, &sel, bIterator);

            // Deserialize logicalChannelConfig defaultValue
            if (sel == 1)
            {
                bIterator = DeserializeNull(bIterator);
            }

            // Deserialize logicalChannelConfig explicitValue
            else if (sel == 0)
            {
                bIterator =
                    DeserializeLogicalChannelConfig(&srbToAddMod.logicalChannelConfig, bIterator);
            }
        }
        srbToAddModList->insert(srbToAddModList->end(), srbToAddMod);
    }

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeDrbToAddModList(std::list<NrRrcSap::DrbToAddMod>* drbToAddModList,
                                          Buffer::Iterator bIterator)
{
    int n;
    int val;
    bIterator = DeserializeSequenceOf(&n, MAX_DRB_TO_ADD, 1, bIterator);

    drbToAddModList->clear();

    for (int i = 0; i < n; i++)
    {
        NrRrcSap::DrbToAddMod drbToAddMod;

        std::bitset<5> optionalFields;
        bIterator = DeserializeSequence(&optionalFields, true, bIterator);

        if (optionalFields[4])
        {
            // Deserialize epsBearerIdentity
            bIterator = DeserializeInteger(&val, 0, 15, bIterator);
            drbToAddMod.epsBearerIdentity = val;
        }

        bIterator = DeserializeInteger(&val, 1, 32, bIterator);
        drbToAddMod.drbIdentity = val;

        if (optionalFields[3])
        {
            // Deserialize pdcp-Config
            // ...
        }

        if (optionalFields[2])
        {
            // Deserialize RLC-Config
            int chosen;
            bIterator = DeserializeChoice(4, true, &chosen, bIterator);

            int sel;
            std::bitset<0> bitset0;
            switch (chosen)
            {
            case 0:
                drbToAddMod.rlcConfig.choice = NrRrcSap::RlcConfig::AM;

                // Deserialize UL-AM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(64, &sel, bIterator); // t-PollRetransmit
                bIterator = DeserializeEnum(8, &sel, bIterator);  // pollPDU
                bIterator = DeserializeEnum(16, &sel, bIterator); // pollByte
                bIterator = DeserializeEnum(8, &sel, bIterator);  // maxRetxThreshold

                // Deserialize DL-AM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(32, &sel, bIterator); // t-Reordering
                bIterator = DeserializeEnum(64, &sel, bIterator); // t-StatusProhibit
                break;

            case 1:
                drbToAddMod.rlcConfig.choice = NrRrcSap::RlcConfig::UM_BI_DIRECTIONAL;

                // Deserialize UL-UM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(2, &sel, bIterator); // sn-FieldLength

                // Deserialize DL-UM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(2, &sel, bIterator);  // sn-FieldLength
                bIterator = DeserializeEnum(32, &sel, bIterator); // t-Reordering
                break;

            case 2:
                drbToAddMod.rlcConfig.choice = NrRrcSap::RlcConfig::UM_UNI_DIRECTIONAL_UL;

                // Deserialize UL-UM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(2, &sel, bIterator); // sn-FieldLength
                break;

            case 3:
                drbToAddMod.rlcConfig.choice = NrRrcSap::RlcConfig::UM_UNI_DIRECTIONAL_DL;

                // Deserialize DL-UM-RLC
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(2, &sel, bIterator);  // sn-FieldLength
                bIterator = DeserializeEnum(32, &sel, bIterator); // t-Reordering
                break;
            }
        }

        if (optionalFields[1])
        {
            bIterator = DeserializeInteger(&val, 3, 10, bIterator);
            drbToAddMod.logicalChannelIdentity = val;
        }

        if (optionalFields[0])
        {
            bIterator =
                DeserializeLogicalChannelConfig(&drbToAddMod.logicalChannelConfig, bIterator);
        }

        drbToAddModList->insert(drbToAddModList->end(), drbToAddMod);
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeLogicalChannelConfig(
    NrRrcSap::LogicalChannelConfig* logicalChannelConfig,
    Buffer::Iterator bIterator)
{
    int n;

    // Deserialize LogicalChannelConfig sequence
    // 1 optional field, extension marker is present.
    std::bitset<1> bitset1;
    bIterator = DeserializeSequence(&bitset1, true, bIterator);

    if (bitset1[0])
    {
        // Deserialize ul-SpecificParameters sequence
        bIterator = DeserializeSequence(&bitset1, false, bIterator);

        // Deserialize priority
        bIterator = DeserializeInteger(&n, 1, 16, bIterator);
        logicalChannelConfig->priority = n;

        // Deserialize prioritisedBitRate
        bIterator = DeserializeEnum(16, &n, bIterator);
        uint16_t prioritizedBitRateKbps;

        switch (n)
        {
        case 0:
            prioritizedBitRateKbps = 0;
            break;
        case 1:
            prioritizedBitRateKbps = 8;
            break;
        case 2:
            prioritizedBitRateKbps = 16;
            break;
        case 3:
            prioritizedBitRateKbps = 32;
            break;
        case 4:
            prioritizedBitRateKbps = 64;
            break;
        case 5:
            prioritizedBitRateKbps = 128;
            break;
        case 6:
            prioritizedBitRateKbps = 256;
            break;
        case 7:
            prioritizedBitRateKbps = 10000;
            break;
        default:
            prioritizedBitRateKbps = 10000;
        }
        logicalChannelConfig->prioritizedBitRateKbps = prioritizedBitRateKbps;

        // Deserialize bucketSizeDuration
        bIterator = DeserializeEnum(8, &n, bIterator);
        uint16_t bucketSizeDurationMs;
        switch (n)
        {
        case 0:
            bucketSizeDurationMs = 50;
            break;
        case 1:
            bucketSizeDurationMs = 100;
            break;
        case 2:
            bucketSizeDurationMs = 150;
            break;
        case 3:
            bucketSizeDurationMs = 300;
            break;
        case 4:
            bucketSizeDurationMs = 500;
            break;
        case 5:
            bucketSizeDurationMs = 1000;
            break;
        default:
            bucketSizeDurationMs = 1000;
        }
        logicalChannelConfig->bucketSizeDurationMs = bucketSizeDurationMs;

        if (bitset1[0])
        {
            // Deserialize logicalChannelGroup
            bIterator = DeserializeInteger(&n, 0, 3, bIterator);
            logicalChannelConfig->logicalChannelGroup = n;
        }
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializePhysicalConfigDedicated(
    NrRrcSap::PhysicalConfigDedicated* physicalConfigDedicated,
    Buffer::Iterator bIterator)
{
    std::bitset<10> optionalFieldPresent;
    bIterator = DeserializeSequence(&optionalFieldPresent, true, bIterator);

    physicalConfigDedicated->havePdschConfigDedicated = optionalFieldPresent[9];
    if (optionalFieldPresent[9])
    {
        // Deserialize pdsch-ConfigDedicated
        std::bitset<0> bitset0;
        bIterator = DeserializeSequence(&bitset0, false, bIterator);

        int slct;

        // Deserialize p-a
        bIterator = DeserializeEnum(8, &slct, bIterator);
        physicalConfigDedicated->pdschConfigDedicated.pa = slct;

        bIterator = DeserializeNull(bIterator);
    }
    if (optionalFieldPresent[8])
    {
        // Deserialize pucch-ConfigDedicated
        // ...
    }
    if (optionalFieldPresent[7])
    {
        // Deserialize pusch-ConfigDedicated
        // ...
    }
    if (optionalFieldPresent[6])
    {
        // Deserialize uplinkPowerControlDedicated
        // ...
    }
    if (optionalFieldPresent[5])
    {
        // Deserialize tpc-PDCCH-ConfigPUCCH
        // ...
    }
    if (optionalFieldPresent[4])
    {
        // Deserialize tpc-PDCCH-ConfigPUSCH
        // ...
    }
    if (optionalFieldPresent[3])
    {
        // Deserialize cqi-ReportConfig
        // ...
    }
    physicalConfigDedicated->haveSoundingRsUlConfigDedicated = optionalFieldPresent[2];
    if (optionalFieldPresent[2])
    {
        // Deserialize soundingRS-UL-ConfigDedicated
        int sel;
        bIterator = DeserializeChoice(2, false, &sel, bIterator);

        if (sel == 0)
        {
            physicalConfigDedicated->soundingRsUlConfigDedicated.type =
                NrRrcSap::SoundingRsUlConfigDedicated::RESET;

            bIterator = DeserializeNull(bIterator);
        }

        else if (sel == 1)
        {
            physicalConfigDedicated->soundingRsUlConfigDedicated.type =
                NrRrcSap::SoundingRsUlConfigDedicated::SETUP;

            std::bitset<0> bitset0;
            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            int slct;

            // Deserialize srs-Bandwidth
            bIterator = DeserializeEnum(4, &slct, bIterator);
            physicalConfigDedicated->soundingRsUlConfigDedicated.srsBandwidth = slct;

            // Deserialize srs-HoppingBandwidth
            bIterator = DeserializeEnum(4, &slct, bIterator);

            // Deserialize freqDomainPosition
            bIterator = DeserializeInteger(&slct, 0, 23, bIterator);

            // Deserialize duration
            bool duration;
            bIterator = DeserializeBoolean(&duration, bIterator);

            // Deserialize srs-ConfigIndex
            bIterator = DeserializeInteger(&slct, 0, 1023, bIterator);
            physicalConfigDedicated->soundingRsUlConfigDedicated.srsConfigIndex = slct;

            // Deserialize transmissionComb
            bIterator = DeserializeInteger(&slct, 0, 1, bIterator);

            // Deserialize cyclicShift
            bIterator = DeserializeEnum(8, &slct, bIterator);
        }
    }
    physicalConfigDedicated->haveAntennaInfoDedicated = optionalFieldPresent[1];
    if (optionalFieldPresent[1])
    {
        // Deserialize antennaInfo
        int sel;
        bIterator = DeserializeChoice(2, false, &sel, bIterator);
        if (sel == 1)
        {
            bIterator = DeserializeNull(bIterator);
        }
        else if (sel == 0)
        {
            std::bitset<1> codebookSubsetRestrictionPresent;
            bIterator = DeserializeSequence(&codebookSubsetRestrictionPresent, false, bIterator);

            int txmode;
            bIterator = DeserializeEnum(8, &txmode, bIterator);
            physicalConfigDedicated->antennaInfo.transmissionMode = txmode;

            if (codebookSubsetRestrictionPresent[0])
            {
                // Deserialize codebookSubsetRestriction
                // ...
            }

            int txantennaselchosen;
            bIterator = DeserializeChoice(2, false, &txantennaselchosen, bIterator);
            if (txantennaselchosen == 0)
            {
                // Deserialize ue-TransmitAntennaSelection release
                bIterator = DeserializeNull(bIterator);
            }
            else if (txantennaselchosen == 1)
            {
                // Deserialize ue-TransmitAntennaSelection setup
                // ...
            }
        }
    }
    if (optionalFieldPresent[0])
    {
        // Deserialize schedulingRequestConfig
        // ...
    }
    return bIterator;
}

void
RrcAsn1Header::Print(std::ostream& os) const
{
    NS_LOG_FUNCTION(this << &os);
    NS_FATAL_ERROR("RrcAsn1Header Print() function must also specify "
                   "NrRrcSap::RadioResourceConfigDedicated as a second argument");
}

Buffer::Iterator
RrcAsn1Header::DeserializeNonCriticalExtensionConfig(
    NrRrcSap::NonCriticalExtensionConfiguration* nonCriticalExtension,
    Buffer::Iterator bIterator)
{
    NS_LOG_FUNCTION(this);
    std::bitset<2> nonCriticalExtension_v890;
    bIterator = DeserializeSequence(&nonCriticalExtension_v890, false, bIterator);

    if (nonCriticalExtension_v890[0])
    {
        // Continue to analyze future Release optional fields
        std::bitset<3> nonCriticalExtension_v920;
        bIterator = DeserializeSequence(&nonCriticalExtension_v920, false, bIterator);
        if (nonCriticalExtension_v920[0])
        {
            // Continue to deserialize future Release optional fields
            std::bitset<3> nonCriticalExtension_v1020;
            bIterator = DeserializeSequence(&nonCriticalExtension_v1020, false, bIterator);

            if (nonCriticalExtension_v1020[2])
            {
                // sCellToReleaseList-r10
                int numElems;

                bIterator = DeserializeSequenceOf(&numElems, MAX_OBJECT_ID, 1, bIterator);
                nonCriticalExtension->sCellToReleaseList.clear();

                for (int i = 0; i < numElems; i++)
                {
                    // Deserialize SCellIndex-r10
                    int sCellIndex;
                    bIterator = DeserializeInteger(&sCellIndex, 1, 7, bIterator);
                    nonCriticalExtension->sCellToReleaseList.push_back(sCellIndex);
                }
            }

            if (nonCriticalExtension_v1020[1])
            {
                // sCellToAddModList-r10

                int numElems;
                bIterator = DeserializeSequenceOf(&numElems, MAX_OBJECT_ID, 1, bIterator);
                nonCriticalExtension->sCellToAddModList.clear();
                // Deserialize SCellToAddMod
                for (int i = 0; i < numElems; i++)
                {
                    std::bitset<4> sCellToAddMod_r10;
                    bIterator = DeserializeSequence(&sCellToAddMod_r10, false, bIterator);

                    NrRrcSap::SCellToAddMod sctam;
                    // Deserialize sCellIndex
                    NS_ASSERT(sCellToAddMod_r10[3]); // sCellIndex
                    int n;
                    bIterator = DeserializeInteger(&n, 1, 7, bIterator);
                    sctam.sCellIndex = n;
                    // Deserialize CellIdentification
                    NS_ASSERT(sCellToAddMod_r10[2]); // CellIdentification
                    bIterator = DeserializeCellIdentification(&sctam.cellIdentification, bIterator);

                    // Deserialize RadioResourceConfigCommonSCell
                    NS_ASSERT(sCellToAddMod_r10[1]);
                    bIterator = DeserializeRadioResourceConfigCommonSCell(
                        &sctam.radioResourceConfigCommonSCell,
                        bIterator);
                    if (sCellToAddMod_r10[0])
                    {
                        sctam.haveRadioResourceConfigDedicatedSCell = true;
                        // Deserialize RadioResourceConfigDedicatedSCell
                        bIterator = DeserializeRadioResourceConfigDedicatedSCell(
                            &sctam.radioResourceConfigDedicatedSCell,
                            bIterator);
                    }
                    else
                    {
                        sctam.haveRadioResourceConfigDedicatedSCell = false;
                    }

                    nonCriticalExtension->sCellToAddModList.push_back(sctam);
                }
            }

            NS_ASSERT(!nonCriticalExtension_v1020[0]); // No nonCriticalExtension
                                                       // RRCConnectionReconfiguration-v1130-IEs
        }
    }

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeCellIdentification(NrRrcSap::CellIdentification* ci,
                                             Buffer::Iterator bIterator)
{
    NS_LOG_FUNCTION(this);
    std::bitset<2> cellIdentification_r10;
    bIterator = DeserializeSequence(&cellIdentification_r10, false, bIterator);
    NS_ASSERT(cellIdentification_r10[1]); // phyCellId-r10
    int n1;
    bIterator = DeserializeInteger(&n1, 1, 65536, bIterator);
    ci->physCellId = n1;
    int n2;
    NS_ASSERT(cellIdentification_r10[0]); // dl-CarrierFreq-r10
    bIterator = DeserializeInteger(&n2, 1, MAX_NR_ARFCN, bIterator);
    ci->dlCarrierFreq = n2;

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioResourceConfigCommonSCell(
    NrRrcSap::RadioResourceConfigCommonSCell* rrccsc,
    Buffer::Iterator bIterator)
{
    NS_LOG_FUNCTION(this);
    std::bitset<2> radioResourceConfigCommonSCell_r10;
    bIterator = DeserializeSequence(&radioResourceConfigCommonSCell_r10, false, bIterator);
    rrccsc->haveNonUlConfiguration = radioResourceConfigCommonSCell_r10[1];
    rrccsc->haveUlConfiguration = radioResourceConfigCommonSCell_r10[0];
    if (rrccsc->haveNonUlConfiguration)
    {
        std::bitset<5> nonUlConfiguration_r10;
        bIterator = DeserializeSequence(&nonUlConfiguration_r10, false, bIterator);
        int n;
        bIterator = DeserializeInteger(&n, 6, 1000, bIterator);
        rrccsc->nonUlConfiguration.dlBandwidth = n;

        std::bitset<1> antennaInfoCommon_r10;
        bIterator = DeserializeSequence(&antennaInfoCommon_r10, false, bIterator);
        bIterator = DeserializeInteger(&n, 0, 65536, bIterator);
        rrccsc->nonUlConfiguration.antennaInfoCommon.antennaPortsCount = n;

        std::bitset<2> pdschConfigCommon_r10;
        bIterator = DeserializeSequence(&pdschConfigCommon_r10, false, bIterator);
        bIterator = DeserializeInteger(&n, -60, 50, bIterator);
        rrccsc->nonUlConfiguration.pdschConfigCommon.referenceSignalPower = n;
        bIterator = DeserializeInteger(&n, 0, 3, bIterator);
        rrccsc->nonUlConfiguration.pdschConfigCommon.pb = n;
    }
    if (rrccsc->haveUlConfiguration)
    {
        std::bitset<7> UlConfiguration_r10;
        bIterator = DeserializeSequence(&UlConfiguration_r10, true, bIterator);

        std::bitset<3> FreqInfo_r10;
        bIterator = DeserializeSequence(&FreqInfo_r10, false, bIterator);
        int n;
        bIterator = DeserializeInteger(&n, 0, MAX_NR_ARFCN, bIterator);
        rrccsc->ulConfiguration.ulFreqInfo.ulCarrierFreq = n;
        bIterator = DeserializeInteger(&n, 6, 1000, bIterator);
        rrccsc->ulConfiguration.ulFreqInfo.ulBandwidth = n;

        std::bitset<2> UlPowerControlCommonSCell_r10;
        bIterator = DeserializeSequence(&UlPowerControlCommonSCell_r10, false, bIterator);
        bIterator = DeserializeInteger(&n, 0, 65536, bIterator);
        rrccsc->ulConfiguration.ulPowerControlCommonSCell.alpha = n;

        std::bitset<1> prachConfigSCell_r10;
        bIterator = DeserializeSequence(&prachConfigSCell_r10, false, bIterator);
        bIterator = DeserializeInteger(&n, 0, 256, bIterator);
        rrccsc->ulConfiguration.prachConfigSCell.index = n;
    }

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioResourceConfigDedicatedSCell(
    NrRrcSap::RadioResourceConfigDedicatedSCell* rrcdsc,
    Buffer::Iterator bIterator)
{
    NS_LOG_FUNCTION(this);
    std::bitset<1> RadioResourceConfigDedicatedSCell_r10;
    bIterator = DeserializeSequence(&RadioResourceConfigDedicatedSCell_r10, false, bIterator);
    bIterator =
        DeserializePhysicalConfigDedicatedSCell(&rrcdsc->physicalConfigDedicatedSCell, bIterator);

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializePhysicalConfigDedicatedSCell(
    NrRrcSap::PhysicalConfigDedicatedSCell* pcdsc,
    Buffer::Iterator bIterator)
{
    NS_LOG_FUNCTION(this);
    std::bitset<2> pcdscOpt;
    bIterator = DeserializeSequence(&pcdscOpt, true, bIterator);
    pcdsc->haveNonUlConfiguration = pcdscOpt[1];
    pcdsc->haveUlConfiguration = pcdscOpt[0];
    if (pcdsc->haveNonUlConfiguration)
    {
        std::bitset<4> nulOpt;
        bIterator = DeserializeSequence(&nulOpt, false, bIterator);
        pcdsc->haveAntennaInfoDedicated = nulOpt[3];
        NS_ASSERT(!nulOpt[2]); // crossCarrierSchedulingConfig-r10 Not Implemented
        NS_ASSERT(!nulOpt[1]); // csi-RS-Config-r10 Not Implemented
        pcdsc->havePdschConfigDedicated = nulOpt[0];

        if (pcdsc->haveAntennaInfoDedicated)
        {
            // Deserialize antennaInfo
            int sel;
            bIterator = DeserializeChoice(2, false, &sel, bIterator);
            if (sel == 1)
            {
                bIterator = DeserializeNull(bIterator);
            }
            else if (sel == 0)
            {
                std::bitset<1> codebookSubsetRestrictionPresent;
                bIterator =
                    DeserializeSequence(&codebookSubsetRestrictionPresent, false, bIterator);

                int txmode;
                bIterator = DeserializeEnum(8, &txmode, bIterator);
                pcdsc->antennaInfo.transmissionMode = txmode;

                if (codebookSubsetRestrictionPresent[0])
                {
                    // Deserialize codebookSubsetRestriction
                    NS_FATAL_ERROR("Not implemented yet");
                    // ...
                }

                int txantennaselchosen;
                bIterator = DeserializeChoice(2, false, &txantennaselchosen, bIterator);
                if (txantennaselchosen == 0)
                {
                    // Deserialize ue-TransmitAntennaSelection release
                    bIterator = DeserializeNull(bIterator);
                }
                else if (txantennaselchosen == 1)
                {
                    // Deserialize ue-TransmitAntennaSelection setup
                    NS_FATAL_ERROR("Not implemented yet");
                    // ...
                }
            }
        }
        if (pcdsc->havePdschConfigDedicated)
        {
            // Deserialize pdsch-ConfigDedicated
            std::bitset<0> bitset0;
            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            int slct;

            // Deserialize p-a
            bIterator = DeserializeEnum(8, &slct, bIterator);
            pcdsc->pdschConfigDedicated.pa = slct;

            bIterator = DeserializeNull(bIterator);
        }
    }
    if (pcdsc->haveUlConfiguration)
    {
        std::bitset<7> ulOpt;
        bIterator = DeserializeSequence(&ulOpt, false, bIterator);
        pcdsc->haveAntennaInfoUlDedicated = ulOpt[6];
        NS_ASSERT(!ulOpt[5]); // pusch-ConfigDedicatedSCell-r10 not present
        NS_ASSERT(!ulOpt[4]); // uplinkPowerControlDedicatedSCell-r10 not present
        NS_ASSERT(!ulOpt[3]); // cqi-ReportConfigSCell-r10 not present
        pcdsc->haveSoundingRsUlConfigDedicated = ulOpt[2];
        NS_ASSERT(!ulOpt[1]); // soundingRS-UL-ConfigDedicated-v1020 not present
        NS_ASSERT(!ulOpt[0]); // soundingRS-UL-ConfigDedicatedAperiodic-r10 not present

        if (pcdsc->haveAntennaInfoUlDedicated)
        {
            // Deserialize antennaInfo
            int sel;
            bIterator = DeserializeChoice(2, false, &sel, bIterator);
            if (sel == 1)
            {
                bIterator = DeserializeNull(bIterator);
            }
            else if (sel == 0)
            {
                std::bitset<1> codebookSubsetRestrictionPresent;
                bIterator =
                    DeserializeSequence(&codebookSubsetRestrictionPresent, false, bIterator);

                int txmode;
                bIterator = DeserializeEnum(8, &txmode, bIterator);
                pcdsc->antennaInfoUl.transmissionMode = txmode;

                if (codebookSubsetRestrictionPresent[0])
                {
                    // Deserialize codebookSubsetRestriction
                    NS_FATAL_ERROR("Not implemented yet");
                    // ...
                }

                int txantennaselchosen;
                bIterator = DeserializeChoice(2, false, &txantennaselchosen, bIterator);
                if (txantennaselchosen == 0)
                {
                    // Deserialize ue-TransmitAntennaSelection release
                    bIterator = DeserializeNull(bIterator);
                }
                else if (txantennaselchosen == 1)
                {
                    // Deserialize ue-TransmitAntennaSelection setup
                    NS_FATAL_ERROR("Not implemented yet");
                    // ...
                }
            }
        }
        if (pcdsc->haveSoundingRsUlConfigDedicated)
        {
            // Deserialize soundingRS-UL-ConfigDedicated
            int sel;
            bIterator = DeserializeChoice(2, false, &sel, bIterator);

            if (sel == 0)
            {
                pcdsc->soundingRsUlConfigDedicated.type =
                    NrRrcSap::SoundingRsUlConfigDedicated::RESET;

                bIterator = DeserializeNull(bIterator);
            }

            else if (sel == 1)
            {
                pcdsc->soundingRsUlConfigDedicated.type =
                    NrRrcSap::SoundingRsUlConfigDedicated::SETUP;

                std::bitset<0> bitset0;
                bIterator = DeserializeSequence(&bitset0, false, bIterator);

                int slct;

                // Deserialize srs-Bandwidth
                bIterator = DeserializeEnum(4, &slct, bIterator);
                pcdsc->soundingRsUlConfigDedicated.srsBandwidth = slct;

                // Deserialize srs-HoppingBandwidth
                bIterator = DeserializeEnum(4, &slct, bIterator);

                // Deserialize freqDomainPosition
                bIterator = DeserializeInteger(&slct, 0, 23, bIterator);

                // Deserialize duration
                bool duration;
                bIterator = DeserializeBoolean(&duration, bIterator);

                // Deserialize srs-ConfigIndex
                bIterator = DeserializeInteger(&slct, 0, 1023, bIterator);
                pcdsc->soundingRsUlConfigDedicated.srsConfigIndex = slct;

                // Deserialize transmissionComb
                bIterator = DeserializeInteger(&slct, 0, 1, bIterator);

                // Deserialize cyclicShift
                bIterator = DeserializeEnum(8, &slct, bIterator);
            }
        }
    }

    return bIterator;
}

void
RrcAsn1Header::Print(std::ostream& os,
                     NrRrcSap::RadioResourceConfigDedicated radioResourceConfigDedicated) const
{
    os << "   srbToAddModList: " << std::endl;
    std::list<NrRrcSap::SrbToAddMod>::iterator it =
        radioResourceConfigDedicated.srbToAddModList.begin();
    for (; it != radioResourceConfigDedicated.srbToAddModList.end(); it++)
    {
        os << "      srbIdentity: " << (int)it->srbIdentity << std::endl;
        os << "      logicalChannelConfig: " << std::endl;
        os << "         priority: " << (int)it->logicalChannelConfig.priority << std::endl;
        os << "         prioritizedBitRateKbps: "
           << (int)it->logicalChannelConfig.prioritizedBitRateKbps << std::endl;
        os << "         bucketSizeDurationMs: "
           << (int)it->logicalChannelConfig.bucketSizeDurationMs << std::endl;
        os << "         logicalChannelGroup: " << (int)it->logicalChannelConfig.logicalChannelGroup
           << std::endl;
    }
    os << std::endl;

    os << "   drbToAddModList: " << std::endl;
    std::list<NrRrcSap::DrbToAddMod>::iterator it2 =
        radioResourceConfigDedicated.drbToAddModList.begin();
    for (; it2 != radioResourceConfigDedicated.drbToAddModList.end(); it2++)
    {
        os << "      epsBearerIdentity: " << (int)it2->epsBearerIdentity << std::endl;
        os << "      drbIdentity: " << (int)it2->drbIdentity << std::endl;
        os << "      rlcConfig: " << it2->rlcConfig.choice << std::endl;
        os << "      logicalChannelIdentity: " << (int)it2->logicalChannelIdentity << std::endl;
        os << "      logicalChannelConfig: " << std::endl;
        os << "         priority: " << (int)it2->logicalChannelConfig.priority << std::endl;
        os << "         prioritizedBitRateKbps: "
           << (int)it2->logicalChannelConfig.prioritizedBitRateKbps << std::endl;
        os << "         bucketSizeDurationMs: "
           << (int)it2->logicalChannelConfig.bucketSizeDurationMs << std::endl;
        os << "         logicalChannelGroup: " << (int)it2->logicalChannelConfig.logicalChannelGroup
           << std::endl;
    }
    os << std::endl;

    os << "   drbToReleaseList: ";
    std::list<uint8_t>::iterator it3 = radioResourceConfigDedicated.drbToReleaseList.begin();
    for (; it3 != radioResourceConfigDedicated.drbToReleaseList.end(); it3++)
    {
        os << (int)*it3 << ", ";
    }
    os << std::endl;

    os << "   havePhysicalConfigDedicated: "
       << radioResourceConfigDedicated.havePhysicalConfigDedicated << std::endl;

    if (radioResourceConfigDedicated.havePhysicalConfigDedicated)
    {
        os << "   physicalConfigDedicated: " << std::endl;

        os << "      haveSoundingRsUlConfigDedicated: "
           << radioResourceConfigDedicated.physicalConfigDedicated.haveSoundingRsUlConfigDedicated
           << std::endl;
        if (radioResourceConfigDedicated.physicalConfigDedicated.haveSoundingRsUlConfigDedicated)
        {
            os << "      soundingRsUlConfigDedicated: " << std::endl;
            os << "         type: "
               << radioResourceConfigDedicated.physicalConfigDedicated.soundingRsUlConfigDedicated
                      .type
               << std::endl;
            os << "         srsBandwidth: "
               << (int)radioResourceConfigDedicated.physicalConfigDedicated
                      .soundingRsUlConfigDedicated.srsBandwidth
               << std::endl;
            os << "         srsConfigIndex: "
               << (int)radioResourceConfigDedicated.physicalConfigDedicated
                      .soundingRsUlConfigDedicated.srsConfigIndex
               << std::endl;
        }

        os << "      haveAntennaInfoDedicated: "
           << radioResourceConfigDedicated.physicalConfigDedicated.haveAntennaInfoDedicated
           << std::endl;
        if (radioResourceConfigDedicated.physicalConfigDedicated.haveAntennaInfoDedicated)
        {
            os << "      antennaInfo Tx mode: "
               << (int)radioResourceConfigDedicated.physicalConfigDedicated.antennaInfo
                      .transmissionMode
               << std::endl;
        }
    }
}
Buffer::Iterator
RrcAsn1Header::DeserializeSdtConfig(NrRrcSap::SDT_Config_r17* sdtcfg, Buffer::Iterator bIterator)
{
    int n;
    //optionalFields
     std::bitset<2> optionalFields;
    bIterator = DeserializeSequence(&optionalFields, false, bIterator);

    if(optionalFields[1])
    {
        //Deserialize sdt-DRB-List-r17
        int numDrbList;
        bIterator = DeserializeSequenceOf(&numDrbList,29,1,bIterator); 
        for (int i = 0; i < numDrbList; i++)
        {
            // DRB-Identity
            bIterator = DeserializeInteger(&n,1,MAX_DRB,bIterator);
            sdtcfg->sdt_DRB_List_r17.emplace_back(n);
        }

    }
   

    if(optionalFields[0])
    {
        //Deserialize sdt-SRB2-Indication-r17
        //...
    }


    return bIterator;


}

Buffer::Iterator
RrcAsn1Header::DeserializeSystemInformationBlockType1(
    NrRrcSap::SystemInformationBlockType1* systemInformationBlockType1,
    Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    std::bitset<3> sysInfoBlkT1Opts;
    bIterator = DeserializeSequence(&sysInfoBlkT1Opts, false, bIterator);

    // Deserialize cellAccessRelatedInfo
    std::bitset<1> cellAccessRelatedInfoOpts;
    bIterator = DeserializeSequence(&cellAccessRelatedInfoOpts, false, bIterator);

    // Deserialize plmn-IdentityList
    int numPlmnIdentityInfoElements;
    bIterator = DeserializeSequenceOf(&numPlmnIdentityInfoElements, 6, 1, bIterator);
    for (int i = 0; i < numPlmnIdentityInfoElements; i++)
    {
        bIterator = DeserializeSequence(&bitset0, false, bIterator);

        // plmn-Identity
        bIterator = DeserializePlmnIdentity(
            &systemInformationBlockType1->cellAccessRelatedInfo.plmnIdentityInfo.plmnIdentity,
            bIterator);
    }

    // Deserialize trackingAreaCode
    std::bitset<16> trackingAreaCode;
    bIterator = DeserializeBitstring(&trackingAreaCode, bIterator);

    // Deserialize cellIdentity
    std::bitset<28> cellIdentity;
    bIterator = DeserializeBitstring(&cellIdentity, bIterator);
    systemInformationBlockType1->cellAccessRelatedInfo.cellIdentity = cellIdentity.to_ulong();

    // Deserialize cellBarred
    bIterator = DeserializeEnum(2, &n, bIterator);

    // Deserialize intraFreqReselection
    bIterator = DeserializeEnum(2, &n, bIterator);

    // Deserialize csg-Indication
    bIterator =
        DeserializeBoolean(&systemInformationBlockType1->cellAccessRelatedInfo.csgIndication,
                           bIterator);

    if (cellAccessRelatedInfoOpts[0])
    {
        // Deserialize csg-Identity
        std::bitset<27> csgIdentity;
        bIterator = DeserializeBitstring(&csgIdentity, bIterator);
        systemInformationBlockType1->cellAccessRelatedInfo.csgIdentity = csgIdentity.to_ulong();
    }

    // Deserialize cellSelectionInfo
    std::bitset<1> qRxLevMinOffsetPresent;
    bIterator = DeserializeSequence(&qRxLevMinOffsetPresent, false, bIterator);
    bIterator = DeserializeInteger(&n, -70, -22, bIterator); // q-RxLevMin
    if (qRxLevMinOffsetPresent[0])
    {
        // Deserialize qRxLevMinOffset
        // ...
    }

    if (sysInfoBlkT1Opts[2])
    {
        // Deserialize p-Max
        // ...
    }

    // freqBandIndicator
    bIterator = DeserializeInteger(&n, 1, 64, bIterator);

    // schedulingInfoList
    int numSchedulingInfo;
    bIterator = DeserializeSequenceOf(&numSchedulingInfo, MAX_SI_MESSAGE, 1, bIterator);
    for (int i = 0; i < numSchedulingInfo; i++)
    {
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
        bIterator = DeserializeEnum(7, &n, bIterator); // si-Periodicity
        int numSibType;
        bIterator =
            DeserializeSequenceOf(&numSibType, MAX_SIB - 1, 0, bIterator); // sib-MappingInfo
        for (int j = 0; j < numSibType; j++)
        {
            bIterator = DeserializeEnum(16, &n, bIterator); // SIB-Type
        }
    }

    if (sysInfoBlkT1Opts[1])
    {
        // tdd-Config
        // ...
    }

    // si-WindowLength
    bIterator = DeserializeEnum(7, &n, bIterator);

    // systemInfoValueTag
    bIterator = DeserializeInteger(&n, 0, 31, bIterator);

    if (sysInfoBlkT1Opts[0])
    {
        // Deserialize nonCriticalExtension
        // ...
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeSystemInformationBlockType2(
    NrRrcSap::SystemInformationBlockType2* systemInformationBlockType2,
    Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    std::bitset<2> sysInfoBlkT2Opts;
    bIterator = DeserializeSequence(&sysInfoBlkT2Opts, true, bIterator);
    if (sysInfoBlkT2Opts[1])
    {
        // Deserialize ac-BarringInfo
        // ...
    }

    // Deserialize radioResourceConfigCommon
    bIterator = DeserializeRadioResourceConfigCommonSib(
        &systemInformationBlockType2->radioResourceConfigCommon,
        bIterator);

    // Deserialize ue-TimersAndConstants
    bIterator = DeserializeSequence(&bitset0, true, bIterator);
    bIterator = DeserializeEnum(8, &n, bIterator); // t300
    bIterator = DeserializeEnum(8, &n, bIterator); // t301
    bIterator = DeserializeEnum(7, &n, bIterator); // t310
    bIterator = DeserializeEnum(8, &n, bIterator); // n310
    bIterator = DeserializeEnum(7, &n, bIterator); // t311
    bIterator = DeserializeEnum(8, &n, bIterator); // n311

    // Deserialize freqInfo
    std::bitset<2> freqInfoOpts;
    bIterator = DeserializeSequence(&freqInfoOpts, false, bIterator);
    if (freqInfoOpts[1])
    {
        // Deserialize ul-CarrierFreq
        bIterator = DeserializeInteger(&n, 0, MAX_NR_ARFCN, bIterator);
        systemInformationBlockType2->freqInfo.ulCarrierFreq = n;
    }
    if (freqInfoOpts[0])
    {
        // Deserialize ul-Bandwidth
        bIterator = DeserializeEnum(8, &n, bIterator);
        systemInformationBlockType2->freqInfo.ulBandwidth = EnumToBandwidth(n);
    }

    // additionalSpectrumEmission
    bIterator = DeserializeInteger(&n, 1, 32, bIterator);

    if (sysInfoBlkT2Opts[0])
    {
        // Deserialize mbsfn-SubframeConfigList
        // ...
    }

    // Deserialize timeAlignmentTimerCommon
    bIterator = DeserializeEnum(8, &n, bIterator);

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioResourceConfigCommon(
    NrRrcSap::RadioResourceConfigCommon* radioResourceConfigCommon,
    Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    std::bitset<9> rrCfgCommOptions;
    bIterator = DeserializeSequence(&rrCfgCommOptions, true, bIterator);

    // rach-ConfigCommon
    if (rrCfgCommOptions[8])
    {
        bIterator =
            DeserializeRachConfigCommon(&radioResourceConfigCommon->rachConfigCommon, bIterator);
    }

    // prach-Config
    std::bitset<1> prachConfigInfoPresent;
    bIterator = DeserializeSequence(&prachConfigInfoPresent, false, bIterator);

    // prach-Config -> rootSequenceIndex
    bIterator = DeserializeInteger(&n, 0, 1023, bIterator);

    // prach-Config -> prach-ConfigInfo
    if (prachConfigInfoPresent[0])
    {
        // ...
    }

    // pdsch-ConfigCommon
    if (rrCfgCommOptions[7])
    {
        // ...
    }

    // pusch-ConfigCommon
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> n-SB
    bIterator = DeserializeInteger(&n, 1, 4, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> hoppingMode
    bIterator = DeserializeEnum(2, &n, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> pusch-HoppingOffset
    bIterator = DeserializeInteger(&n, 0, 98, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> enable64QAM
    bool enable64QAM;
    bIterator = DeserializeBoolean(&enable64QAM, bIterator);

    // ul-ReferenceSignalsPUSCH
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // groupHoppingEnabled
    bool dummyBool;
    bIterator = DeserializeBoolean(&dummyBool, bIterator);

    // groupAssignmentPUSCH
    bIterator = DeserializeInteger(&n, 0, 29, bIterator);

    // sequenceHoppingEnabled
    bIterator = DeserializeBoolean(&dummyBool, bIterator);

    // cyclicShift
    bIterator = DeserializeInteger(&n, 0, 7, bIterator);

    // phich-Config
    if (rrCfgCommOptions[6])
    {
        // ...
    }

    // pucch-ConfigCommon
    if (rrCfgCommOptions[5])
    {
        // ...
    }

    // soundingRS-UL-ConfigCommon
    if (rrCfgCommOptions[4])
    {
        // ...
    }

    // uplinkPowerControlCommon
    if (rrCfgCommOptions[3])
    {
        // ...
    }

    // antennaInfoCommon
    if (rrCfgCommOptions[2])
    {
        // ...
    }

    // p-Max
    if (rrCfgCommOptions[1])
    {
        // ...
    }

    // tdd-Config
    if (rrCfgCommOptions[0])
    {
        // ...
    }

    // ul-CyclicPrefixLength
    bIterator = DeserializeEnum(2, &n, bIterator);

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRachConfigCommon(NrRrcSap::RachConfigCommon* rachConfigCommon,
                                           Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, true, bIterator);

    // preambleInfo
    std::bitset<1> preamblesGroupAConfigPresent;
    bIterator = DeserializeSequence(&preamblesGroupAConfigPresent, false, bIterator);

    // numberOfRA-Preambles
    bIterator = DeserializeEnum(16, &n, bIterator);
    switch (n)
    {
    case 0:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 4;
        break;
    case 1:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 8;
        break;
    case 2:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 12;
        break;
    case 3:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 16;
        break;
    case 4:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 20;
        break;
    case 5:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 24;
        break;
    case 6:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 28;
        break;
    case 7:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 32;
        break;
    case 8:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 36;
        break;
    case 9:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 40;
        break;
    case 10:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 44;
        break;
    case 11:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 48;
        break;
    case 12:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 52;
        break;
    case 13:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 56;
        break;
    case 14:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 60;
        break;
    case 15:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 64;
        break;
    default:
        rachConfigCommon->preambleInfo.numberOfRaPreambles = 4;
    }

    if (preamblesGroupAConfigPresent[0])
    {
        // Deserialize preamblesGroupAConfig
        // ...
    }

    // powerRampingParameters
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeEnum(4, &n, bIterator);  // powerRampingStep
    bIterator = DeserializeEnum(16, &n, bIterator); // preambleInitialReceivedTargetPower

    // ra-SupervisionInfo
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeEnum(11, &n, bIterator); // preambleTransMax
    switch (n)
    {
    case 0:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 3;
        break;
    case 1:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 4;
        break;
    case 2:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 5;
        break;
    case 3:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 6;
        break;
    case 4:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 7;
        break;
    case 5:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 8;
        break;
    case 6:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 10;
        break;
    case 7:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 20;
        break;
    case 8:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 50;
        break;
    case 9:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 100;
        break;
    case 10:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 200;
        break;
    default:
        rachConfigCommon->raSupervisionInfo.preambleTransMax = 0;
    }

    // ra-ResponseWindowSize
    bIterator = DeserializeEnum(8, &n, bIterator);
    switch (n)
    {
    case 0:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 2;
        break;
    case 1:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 3;
        break;
    case 2:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 4;
        break;
    case 3:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 5;
        break;
    case 4:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 6;
        break;
    case 5:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 7;
        break;
    case 6:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 8;
        break;
    case 7:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 10;
        break;
    default:
        rachConfigCommon->raSupervisionInfo.raResponseWindowSize = 0;
    }

    bIterator = DeserializeEnum(8, &n, bIterator);       // mac-ContentionResolutionTimer
    bIterator = DeserializeInteger(&n, 1, 8, bIterator); // maxHARQ-Msg3Tx

    // connEstFailCount
    bIterator = DeserializeEnum(8, &n, bIterator);
    switch (n)
    {
    case 1:
        rachConfigCommon->txFailParam.connEstFailCount = 1;
        break;
    case 2:
        rachConfigCommon->txFailParam.connEstFailCount = 2;
        break;
    case 3:
        rachConfigCommon->txFailParam.connEstFailCount = 3;
        break;
    case 4:
        rachConfigCommon->txFailParam.connEstFailCount = 4;
        break;
    default:
        rachConfigCommon->txFailParam.connEstFailCount = 1;
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeRadioResourceConfigCommonSib(
    NrRrcSap::RadioResourceConfigCommonSib* radioResourceConfigCommonSib,
    Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, true, bIterator);

    // rach-ConfigCommon
    bIterator =
        DeserializeRachConfigCommon(&radioResourceConfigCommonSib->rachConfigCommon, bIterator);

    // bcch-Config
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeEnum(4, &n, bIterator); // modificationPeriodCoeff

    // pcch-Config
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeEnum(4, &n, bIterator); // defaultPagingCycle
    bIterator = DeserializeEnum(8, &n, bIterator); // nB

    // prach-Config
    std::bitset<1> prachConfigInfoPresent;
    bIterator = DeserializeSequence(&prachConfigInfoPresent, false, bIterator);
    // prach-Config -> rootSequenceIndex
    bIterator = DeserializeInteger(&n, 0, 1023, bIterator);
    // prach-Config -> prach-ConfigInfo
    if (prachConfigInfoPresent[0])
    {
        // ...
    }

    // pdsch-ConfigCommon
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeInteger(&n, -60, 50, bIterator); // referenceSignalPower
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);    // p-b

    // pusch-ConfigCommon
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> n-SB
    bIterator = DeserializeInteger(&n, 1, 4, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> hoppingMode
    bIterator = DeserializeEnum(2, &n, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> pusch-HoppingOffset
    bIterator = DeserializeInteger(&n, 0, 98, bIterator);

    // pusch-ConfigCommon -> pusch-ConfigBasic -> enable64QAM
    bool dummyBoolean;
    bIterator = DeserializeBoolean(&dummyBoolean, bIterator);

    // ul-ReferenceSignalsPUSCH
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // groupHoppingEnabled
    bIterator = DeserializeBoolean(&dummyBoolean, bIterator);

    // groupAssignmentPUSCH
    bIterator = DeserializeInteger(&n, 0, 29, bIterator);

    // sequenceHoppingEnabled
    bIterator = DeserializeBoolean(&dummyBoolean, bIterator);

    // cyclicShift
    bIterator = DeserializeInteger(&n, 0, 7, bIterator);

    // pucch-ConfigCommon
    bIterator = DeserializeEnum(3, &n, bIterator);          // deltaPUCCH-Shift
    bIterator = DeserializeInteger(&n, 0, 98, bIterator);   // nRB-CQI
    bIterator = DeserializeInteger(&n, 0, 7, bIterator);    // nCS-AN
    bIterator = DeserializeInteger(&n, 0, 2047, bIterator); // n1PUCCH-AN

    // soundingRS-UL-ConfigCommon
    int choice;
    bIterator = DeserializeChoice(2, false, &choice, bIterator);
    if (choice == 0)
    {
        bIterator = DeserializeNull(bIterator); // release
    }
    if (choice == 1)
    {
        // setup
        // ...
    }

    // uplinkPowerControlCommon
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeInteger(&n, -126, 24, bIterator);  // p0-NominalPUSCH
    bIterator = DeserializeEnum(8, &n, bIterator);            // alpha
    bIterator = DeserializeInteger(&n, -127, -96, bIterator); // p0-NominalPUCCH
    // deltaFList-PUCCH
    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeEnum(3, &n, bIterator);        // deltaF-PUCCH-Format1
    bIterator = DeserializeEnum(3, &n, bIterator);        // deltaF-PUCCH-Format1b
    bIterator = DeserializeEnum(4, &n, bIterator);        // deltaF-PUCCH-Format2
    bIterator = DeserializeEnum(3, &n, bIterator);        // deltaF-PUCCH-Format2a
    bIterator = DeserializeEnum(3, &n, bIterator);        // deltaF-PUCCH-Format2b
    bIterator = DeserializeInteger(&n, -1, 6, bIterator); // deltaPreambleMsg3

    // ul-CyclicPrefixLength
    bIterator = DeserializeEnum(2, &n, bIterator);

    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeMeasResults(NrRrcSap::MeasResults* measResults,
                                      Buffer::Iterator bIterator)
{
    int n;
    std::bitset<0> b0;
    std::bitset<4> measResultOptionalPresent;
    //    bIterator = DeserializeSequence (&measResultNeighCellsPresent,true,bIterator);
    bIterator = DeserializeSequence(&measResultOptionalPresent, true, bIterator);

    // Deserialize measId
    bIterator = DeserializeInteger(&n, 1, MAX_MEAS_ID, bIterator);
    measResults->measId = n;

    // Deserialize measResultServCell
    bIterator = DeserializeSequence(&b0, false, bIterator);

    // Deserialize rsrpResult
    bIterator = DeserializeInteger(&n, 0, 97, bIterator);
    measResults->measResultPCell.rsrpResult = n;

    // Deserialize rsrqResult
    bIterator = DeserializeInteger(&n, 0, 34, bIterator);
    measResults->measResultPCell.rsrqResult = n;

    measResults->haveMeasResultNeighCells = measResultOptionalPresent[0];
    measResults->haveMeasResultServFreqList = measResultOptionalPresent[3];
    if (measResults->haveMeasResultNeighCells)
    {
        int measResultNeighCellsChoice;

        // Deserialize measResultNeighCells
        bIterator = DeserializeChoice(4, false, &measResultNeighCellsChoice, bIterator);

        if (measResultNeighCellsChoice == 0)
        {
            // Deserialize measResultListEUTRA
            int numElems;
            bIterator = DeserializeSequenceOf(&numElems, MAX_CELL_REPORT, 1, bIterator);

            for (int i = 0; i < numElems; i++)
            {
                NrRrcSap::MeasResultEutra measResultEutra;

                std::bitset<1> isCgiInfoPresent;
                bIterator = DeserializeSequence(&isCgiInfoPresent, false, bIterator);

                // PhysCellId
                bIterator = DeserializeInteger(&n, 0, 503, bIterator);
                measResultEutra.physCellId = n;

                measResultEutra.haveCgiInfo = isCgiInfoPresent[0];
                if (isCgiInfoPresent[0])
                {
                    std::bitset<1> havePlmnIdentityList;
                    bIterator = DeserializeSequence(&havePlmnIdentityList, false, bIterator);

                    // Deserialize cellGlobalId
                    bIterator = DeserializeSequence(&b0, false, bIterator);

                    // Deserialize plmn-Identity
                    bIterator =
                        DeserializePlmnIdentity(&measResultEutra.cgiInfo.plmnIdentity, bIterator);

                    // Deserialize CellIdentity
                    std::bitset<28> cellId;
                    bIterator = DeserializeBitstring(&cellId, bIterator);
                    measResultEutra.cgiInfo.cellIdentity = cellId.to_ulong();

                    // Deserialize trackingAreaCode
                    std::bitset<16> trArCo;
                    bIterator = DeserializeBitstring(&trArCo, bIterator);
                    measResultEutra.cgiInfo.trackingAreaCode = trArCo.to_ulong();

                    // Deserialize plmn-IdentityList
                    if (havePlmnIdentityList[0])
                    {
                        int numPlmnElems;
                        bIterator = DeserializeSequenceOf(&numPlmnElems, 5, 1, bIterator);

                        for (int j = 0; j < numPlmnElems; j++)
                        {
                            uint32_t plmnId;
                            bIterator = DeserializePlmnIdentity(&plmnId, bIterator);
                            measResultEutra.cgiInfo.plmnIdentityList.push_back(plmnId);
                        }
                    }
                }

                // Deserialize measResult
                std::bitset<2> measResultOpts;
                bIterator = DeserializeSequence(&measResultOpts, true, bIterator);

                measResultEutra.haveRsrpResult = measResultOpts[1];
                if (measResultOpts[1])
                {
                    // Deserialize rsrpResult
                    bIterator = DeserializeInteger(&n, 0, 97, bIterator);
                    measResultEutra.rsrpResult = n;
                }

                measResultEutra.haveRsrqResult = measResultOpts[0];
                if (measResultOpts[0])
                {
                    // Deserialize rsrqResult
                    bIterator = DeserializeInteger(&n, 0, 34, bIterator);
                    measResultEutra.rsrqResult = n;
                }

                measResults->measResultListEutra.push_back(measResultEutra);
            }
        }

        if (measResultNeighCellsChoice == 1)
        {
            // Deserialize measResultListUTRA
            // ...
        }

        if (measResultNeighCellsChoice == 2)
        {
            // Deserialize measResultListGERAN
            // ...
        }
        if (measResultNeighCellsChoice == 3)
        {
            // Deserialize measResultsCDMA2000
            // ...
        }
    }
    if (measResults->haveMeasResultServFreqList)
    {
        int numElems;
        bIterator = DeserializeSequenceOf(&numElems, MAX_SCELL_REPORT, 1, bIterator);
        for (int i = 0; i < numElems; i++)
        {
            NrRrcSap::MeasResultServFreq measResultServFreq;

            // Deserialize MeasResultServFreq-r10
            std::bitset<2> measResultScellPresent;
            bIterator = DeserializeSequence(&measResultScellPresent, true, bIterator);
            measResultServFreq.haveMeasResultSCell = measResultScellPresent[0];
            measResultServFreq.haveMeasResultBestNeighCell = measResultScellPresent[1];

            // Deserialize servFreqId-r10
            int servFreqId;
            bIterator = DeserializeInteger(&servFreqId, 0, 7, bIterator);
            measResultServFreq.servFreqId = servFreqId;

            if (measResultServFreq.haveMeasResultSCell)
            {
                // Deserialize rsrpResult
                bIterator = DeserializeInteger(&n, 0, 97, bIterator);
                measResultServFreq.measResultSCell.rsrpResult = n;

                // Deserialize rsrqResult
                bIterator = DeserializeInteger(&n, 0, 34, bIterator);
                measResultServFreq.measResultSCell.rsrqResult = n;
            }

            if (measResultServFreq.haveMeasResultBestNeighCell)
            {
                // Deserialize physCellId-r10
                bIterator = DeserializeInteger(&n, 0, 503, bIterator);
                measResultServFreq.measResultBestNeighCell.physCellId = n;

                // Deserialize rsrpResultNCell-r10
                bIterator = DeserializeInteger(&n, 0, 97, bIterator);
                measResultServFreq.measResultBestNeighCell.rsrpResult = n;

                // Deserialize rsrqResultNCell-r10
                bIterator = DeserializeInteger(&n, 0, 34, bIterator);
                measResultServFreq.measResultBestNeighCell.rsrqResult = n;
            }
            measResults->measResultServFreqList.push_back(measResultServFreq);
        }
    }
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializePlmnIdentity(uint32_t* plmnId, Buffer::Iterator bIterator)
{
    int n;
    std::bitset<1> isMccPresent;
    bIterator = DeserializeSequence(&isMccPresent, false, bIterator);

    if (isMccPresent[0])
    {
        // Deserialize mcc
        // ...
    }

    // Deserialize mnc
    int mncDigits;
    int mnc = 0;
    bIterator = DeserializeSequenceOf(&mncDigits, 3, 2, bIterator);

    for (int j = mncDigits - 1; j >= 0; j--)
    {
        bIterator = DeserializeInteger(&n, 0, 9, bIterator);
        mnc += n * pow(10, j);
    }

    *plmnId = mnc;

    // cellReservedForOperatorUse
    bIterator = DeserializeEnum(2, &n, bIterator);
    return bIterator;
}

Buffer::Iterator
RrcAsn1Header::DeserializeMeasConfig(NrRrcSap::MeasConfig* measConfig, Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    std::bitset<2> bitset2;
    std::bitset<11> bitset11;
    int n;

    // measConfig
    bIterator = DeserializeSequence(&bitset11, true, bIterator);

    if (bitset11[10])
    {
        // measObjectToRemoveList
        int measObjectToRemoveListElems;
        bIterator =
            DeserializeSequenceOf(&measObjectToRemoveListElems, MAX_OBJECT_ID, 1, bIterator);

        for (int i = 0; i < measObjectToRemoveListElems; i++)
        {
            bIterator = DeserializeInteger(&n, 1, MAX_OBJECT_ID, bIterator);
            measConfig->measObjectToRemoveList.push_back(n);
        }
    }

    if (bitset11[9])
    {
        // measObjectToAddModList
        int measObjectToAddModListElems;
        bIterator =
            DeserializeSequenceOf(&measObjectToAddModListElems, MAX_OBJECT_ID, 1, bIterator);

        for (int i = 0; i < measObjectToAddModListElems; i++)
        {
            NrRrcSap::MeasObjectToAddMod elem;

            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            bIterator = DeserializeInteger(&n, 1, MAX_OBJECT_ID, bIterator);
            elem.measObjectId = n;

            int measObjectChoice;
            bIterator = DeserializeChoice(4, true, &measObjectChoice, bIterator);

            switch (measObjectChoice)
            {
            case 1:
                // Deserialize measObjectUTRA
                // ...
                break;

            case 2:
                // Deserialize measObjectGERAN
                // ...
                break;

            case 3:
                // Deserialize measObjectCDMA2000
                // ...
                break;

            case 0:
            default:
                // Deserialize measObjectEUTRA
                std::bitset<5> measObjectEutraOpts;
                bIterator = DeserializeSequence(&measObjectEutraOpts, true, bIterator);

                // carrierFreq
                bIterator = DeserializeInteger(&n, 0, MAX_NR_ARFCN, bIterator);
                elem.measObjectEutra.carrierFreq = n;

                // allowedMeasBandwidth
                bIterator = DeserializeEnum(8, &n, bIterator);
                elem.measObjectEutra.allowedMeasBandwidth = EnumToBandwidth(n);

                // presenceAntennaPort1
                bIterator =
                    DeserializeBoolean(&elem.measObjectEutra.presenceAntennaPort1, bIterator);

                // neighCellConfig
                bIterator = DeserializeBitstring(&bitset2, bIterator);
                elem.measObjectEutra.neighCellConfig = bitset2.to_ulong();

                // offsetFreq
                bIterator = DeserializeQoffsetRange(&elem.measObjectEutra.offsetFreq, bIterator);

                if (measObjectEutraOpts[4])
                {
                    // cellsToRemoveList
                    int numElems;
                    bIterator = DeserializeSequenceOf(&numElems, MAX_CELL_MEAS, 1, bIterator);

                    for (int i = 0; i < numElems; i++)
                    {
                        bIterator = DeserializeInteger(&n, 1, MAX_CELL_MEAS, bIterator);
                        elem.measObjectEutra.cellsToRemoveList.push_back(n);
                    }
                }

                if (measObjectEutraOpts[3])
                {
                    // cellsToAddModList
                    int numElems;
                    bIterator = DeserializeSequenceOf(&numElems, MAX_CELL_MEAS, 1, bIterator);

                    for (int i = 0; i < numElems; i++)
                    {
                        NrRrcSap::CellsToAddMod cellsToAddMod;

                        bIterator = DeserializeSequence(&bitset0, false, bIterator);

                        // cellIndex
                        bIterator = DeserializeInteger(&n, 1, MAX_CELL_MEAS, bIterator);
                        cellsToAddMod.cellIndex = n;

                        // PhysCellId
                        bIterator = DeserializeInteger(&n, 0, 503, bIterator);
                        cellsToAddMod.physCellId = n;

                        // cellIndividualOffset
                        bIterator =
                            DeserializeQoffsetRange(&cellsToAddMod.cellIndividualOffset, bIterator);

                        elem.measObjectEutra.cellsToAddModList.push_back(cellsToAddMod);
                    }
                }

                if (measObjectEutraOpts[2])
                {
                    // blackCellsToRemoveList
                    int numElems;
                    bIterator = DeserializeSequenceOf(&numElems, MAX_CELL_MEAS, 1, bIterator);

                    for (int i = 0; i < numElems; i++)
                    {
                        bIterator = DeserializeInteger(&n, 1, MAX_CELL_MEAS, bIterator);
                        elem.measObjectEutra.blackCellsToRemoveList.push_back(n);
                    }
                }

                if (measObjectEutraOpts[1])
                {
                    // blackCellsToAddModList
                    int numElems;
                    bIterator = DeserializeSequenceOf(&numElems, MAX_CELL_MEAS, 1, bIterator);

                    for (int i = 0; i < numElems; i++)
                    {
                        NrRrcSap::BlackCellsToAddMod blackCellsToAddMod;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);

                        bIterator = DeserializeInteger(&n, 1, MAX_CELL_MEAS, bIterator);
                        blackCellsToAddMod.cellIndex = n;

                        // PhysCellIdRange
                        std::bitset<1> isRangePresent;
                        bIterator = DeserializeSequence(&isRangePresent, false, bIterator);

                        // start
                        bIterator = DeserializeInteger(&n, 0, 503, bIterator);
                        blackCellsToAddMod.physCellIdRange.start = n;

                        blackCellsToAddMod.physCellIdRange.haveRange = isRangePresent[0];
                        // initialize range to silence compiler warning
                        blackCellsToAddMod.physCellIdRange.range = 0;
                        if (blackCellsToAddMod.physCellIdRange.haveRange)
                        {
                            // range
                            bIterator = DeserializeEnum(16, &n, bIterator);
                            switch (n)
                            {
                            case 0:
                                blackCellsToAddMod.physCellIdRange.range = 4;
                                break;
                            case 1:
                                blackCellsToAddMod.physCellIdRange.range = 8;
                                break;
                            case 2:
                                blackCellsToAddMod.physCellIdRange.range = 12;
                                break;
                            case 3:
                                blackCellsToAddMod.physCellIdRange.range = 16;
                                break;
                            case 4:
                                blackCellsToAddMod.physCellIdRange.range = 24;
                                break;
                            case 5:
                                blackCellsToAddMod.physCellIdRange.range = 32;
                                break;
                            case 6:
                                blackCellsToAddMod.physCellIdRange.range = 48;
                                break;
                            case 7:
                                blackCellsToAddMod.physCellIdRange.range = 64;
                                break;
                            case 8:
                                blackCellsToAddMod.physCellIdRange.range = 84;
                                break;
                            case 9:
                                blackCellsToAddMod.physCellIdRange.range = 96;
                                break;
                            case 10:
                                blackCellsToAddMod.physCellIdRange.range = 128;
                                break;
                            case 11:
                                blackCellsToAddMod.physCellIdRange.range = 168;
                                break;
                            case 12:
                                blackCellsToAddMod.physCellIdRange.range = 252;
                                break;
                            case 13:
                                blackCellsToAddMod.physCellIdRange.range = 504;
                                break;
                            default:
                                blackCellsToAddMod.physCellIdRange.range = 0;
                            }
                        }

                        elem.measObjectEutra.blackCellsToAddModList.push_back(blackCellsToAddMod);
                    }
                }

                elem.measObjectEutra.haveCellForWhichToReportCGI = measObjectEutraOpts[0];
                if (measObjectEutraOpts[0])
                {
                    // cellForWhichToReportCGI
                    bIterator = DeserializeInteger(&n, 0, 503, bIterator);
                    elem.measObjectEutra.cellForWhichToReportCGI = n;
                }
            }
            measConfig->measObjectToAddModList.push_back(elem);
        }
    }

    if (bitset11[8])
    {
        // reportConfigToRemoveList
        int reportConfigToRemoveListElems;
        bIterator = DeserializeSequenceOf(&reportConfigToRemoveListElems,
                                          MAX_REPORT_CONFIG_ID,
                                          1,
                                          bIterator);

        for (int i = 0; i < reportConfigToRemoveListElems; i++)
        {
            bIterator = DeserializeInteger(&n, 1, MAX_REPORT_CONFIG_ID, bIterator);
            measConfig->reportConfigToRemoveList.push_back(n);
        }
    }

    if (bitset11[7])
    {
        // reportConfigToAddModList
        int reportConfigToAddModListElems;
        bIterator = DeserializeSequenceOf(&reportConfigToAddModListElems,
                                          MAX_REPORT_CONFIG_ID,
                                          1,
                                          bIterator);

        for (int i = 0; i < reportConfigToAddModListElems; i++)
        {
            NrRrcSap::ReportConfigToAddMod elem;

            bIterator = DeserializeSequence(&bitset0, false, bIterator);
            bIterator = DeserializeInteger(&n, 1, MAX_REPORT_CONFIG_ID, bIterator);
            elem.reportConfigId = n;

            // Deserialize reportConfig
            int reportConfigChoice;
            bIterator = DeserializeChoice(2, false, &reportConfigChoice, bIterator);

            if (reportConfigChoice == 0)
            {
                // reportConfigEUTRA
                bIterator = DeserializeSequence(&bitset0, true, bIterator);

                // triggerType
                int triggerTypeChoice;
                bIterator = DeserializeChoice(2, false, &triggerTypeChoice, bIterator);

                if (triggerTypeChoice == 0)
                {
                    // event
                    elem.reportConfigEutra.triggerType = NrRrcSap::ReportConfigEutra::EVENT;
                    bIterator = DeserializeSequence(&bitset0, false, bIterator);

                    // eventId
                    int eventIdChoice;
                    bIterator = DeserializeChoice(5, true, &eventIdChoice, bIterator);

                    switch (eventIdChoice)
                    {
                    case 0:
                        elem.reportConfigEutra.eventId = NrRrcSap::ReportConfigEutra::EVENT_A1;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);
                        bIterator = DeserializeThresholdEutra(&elem.reportConfigEutra.threshold1,
                                                              bIterator);
                        break;

                    case 1:
                        elem.reportConfigEutra.eventId = NrRrcSap::ReportConfigEutra::EVENT_A2;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);
                        bIterator = DeserializeThresholdEutra(&elem.reportConfigEutra.threshold1,
                                                              bIterator);
                        break;

                    case 2:
                        elem.reportConfigEutra.eventId = NrRrcSap::ReportConfigEutra::EVENT_A3;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);
                        bIterator = DeserializeInteger(&n, -30, 30, bIterator);
                        elem.reportConfigEutra.a3Offset = n;
                        bIterator =
                            DeserializeBoolean(&elem.reportConfigEutra.reportOnLeave, bIterator);
                        break;

                    case 3:
                        elem.reportConfigEutra.eventId = NrRrcSap::ReportConfigEutra::EVENT_A4;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);
                        bIterator = DeserializeThresholdEutra(&elem.reportConfigEutra.threshold1,
                                                              bIterator);
                        break;

                    case 4:
                    default:
                        elem.reportConfigEutra.eventId = NrRrcSap::ReportConfigEutra::EVENT_A5;
                        bIterator = DeserializeSequence(&bitset0, false, bIterator);
                        bIterator = DeserializeThresholdEutra(&elem.reportConfigEutra.threshold1,
                                                              bIterator);
                        bIterator = DeserializeThresholdEutra(&elem.reportConfigEutra.threshold2,
                                                              bIterator);
                    }

                    bIterator = DeserializeInteger(&n, 0, 30, bIterator);
                    elem.reportConfigEutra.hysteresis = n;

                    bIterator = DeserializeEnum(16, &n, bIterator);
                    switch (n)
                    {
                    case 0:
                        elem.reportConfigEutra.timeToTrigger = 0;
                        break;
                    case 1:
                        elem.reportConfigEutra.timeToTrigger = 40;
                        break;
                    case 2:
                        elem.reportConfigEutra.timeToTrigger = 64;
                        break;
                    case 3:
                        elem.reportConfigEutra.timeToTrigger = 80;
                        break;
                    case 4:
                        elem.reportConfigEutra.timeToTrigger = 100;
                        break;
                    case 5:
                        elem.reportConfigEutra.timeToTrigger = 128;
                        break;
                    case 6:
                        elem.reportConfigEutra.timeToTrigger = 160;
                        break;
                    case 7:
                        elem.reportConfigEutra.timeToTrigger = 256;
                        break;
                    case 8:
                        elem.reportConfigEutra.timeToTrigger = 320;
                        break;
                    case 9:
                        elem.reportConfigEutra.timeToTrigger = 480;
                        break;
                    case 10:
                        elem.reportConfigEutra.timeToTrigger = 512;
                        break;
                    case 11:
                        elem.reportConfigEutra.timeToTrigger = 640;
                        break;
                    case 12:
                        elem.reportConfigEutra.timeToTrigger = 1024;
                        break;
                    case 13:
                        elem.reportConfigEutra.timeToTrigger = 1280;
                        break;
                    case 14:
                        elem.reportConfigEutra.timeToTrigger = 2560;
                        break;
                    case 15:
                    default:
                        elem.reportConfigEutra.timeToTrigger = 5120;
                        break;
                    }
                }

                if (triggerTypeChoice == 1)
                {
                    // periodical
                    elem.reportConfigEutra.triggerType = NrRrcSap::ReportConfigEutra::PERIODICAL;

                    bIterator = DeserializeSequence(&bitset0, false, bIterator);
                    bIterator = DeserializeEnum(2, &n, bIterator);
                    if (n == 0)
                    {
                        elem.reportConfigEutra.purpose =
                            NrRrcSap::ReportConfigEutra::REPORT_STRONGEST_CELLS;
                    }
                    else
                    {
                        elem.reportConfigEutra.purpose = NrRrcSap::ReportConfigEutra::REPORT_CGI;
                    }
                }

                // triggerQuantity
                bIterator = DeserializeEnum(2, &n, bIterator);
                if (n == 0)
                {
                    elem.reportConfigEutra.triggerQuantity = NrRrcSap::ReportConfigEutra::RSRP;
                }
                else
                {
                    elem.reportConfigEutra.triggerQuantity = NrRrcSap::ReportConfigEutra::RSRQ;
                }

                // reportQuantity
                bIterator = DeserializeEnum(2, &n, bIterator);
                if (n == 0)
                {
                    elem.reportConfigEutra.reportQuantity =
                        NrRrcSap::ReportConfigEutra::SAME_AS_TRIGGER_QUANTITY;
                }
                else
                {
                    elem.reportConfigEutra.reportQuantity = NrRrcSap::ReportConfigEutra::BOTH;
                }

                // maxReportCells
                bIterator = DeserializeInteger(&n, 1, MAX_CELL_REPORT, bIterator);
                elem.reportConfigEutra.maxReportCells = n;

                // reportInterval
                bIterator = DeserializeEnum(16, &n, bIterator);
                switch (n)
                {
                case 0:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS120;
                    break;
                case 1:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS240;
                    break;
                case 2:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS480;
                    break;
                case 3:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS640;
                    break;
                case 4:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS1024;
                    break;
                case 5:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS2048;
                    break;
                case 6:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS5120;
                    break;
                case 7:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MS10240;
                    break;
                case 8:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MIN1;
                    break;
                case 9:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MIN6;
                    break;
                case 10:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MIN12;
                    break;
                case 11:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MIN30;
                    break;
                case 12:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::MIN60;
                    break;
                case 13:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::SPARE3;
                    break;
                case 14:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::SPARE2;
                    break;
                case 15:
                default:
                    elem.reportConfigEutra.reportInterval = NrRrcSap::ReportConfigEutra::SPARE1;
                }

                // reportAmount
                bIterator = DeserializeEnum(8, &n, bIterator);
                switch (n)
                {
                case 0:
                    elem.reportConfigEutra.reportAmount = 1;
                    break;
                case 1:
                    elem.reportConfigEutra.reportAmount = 2;
                    break;
                case 2:
                    elem.reportConfigEutra.reportAmount = 4;
                    break;
                case 3:
                    elem.reportConfigEutra.reportAmount = 8;
                    break;
                case 4:
                    elem.reportConfigEutra.reportAmount = 16;
                    break;
                case 5:
                    elem.reportConfigEutra.reportAmount = 32;
                    break;
                case 6:
                    elem.reportConfigEutra.reportAmount = 64;
                    break;
                default:
                    elem.reportConfigEutra.reportAmount = 0;
                }
            }

            if (reportConfigChoice == 1)
            {
                // ReportConfigInterRAT
                // ...
            }

            measConfig->reportConfigToAddModList.push_back(elem);
        }
    }

    if (bitset11[6])
    {
        // measIdToRemoveList
        int measIdToRemoveListElems;
        bIterator = DeserializeSequenceOf(&measIdToRemoveListElems, MAX_MEAS_ID, 1, bIterator);

        for (int i = 0; i < measIdToRemoveListElems; i++)
        {
            bIterator = DeserializeInteger(&n, 1, MAX_MEAS_ID, bIterator);
            measConfig->measIdToRemoveList.push_back(n);
        }
    }

    if (bitset11[5])
    {
        // measIdToAddModList
        int measIdToAddModListElems;
        bIterator = DeserializeSequenceOf(&measIdToAddModListElems, MAX_MEAS_ID, 1, bIterator);

        for (int i = 0; i < measIdToAddModListElems; i++)
        {
            NrRrcSap::MeasIdToAddMod elem;

            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            bIterator = DeserializeInteger(&n, 1, MAX_MEAS_ID, bIterator);
            elem.measId = n;

            bIterator = DeserializeInteger(&n, 1, MAX_OBJECT_ID, bIterator);
            elem.measObjectId = n;

            bIterator = DeserializeInteger(&n, 1, MAX_REPORT_CONFIG_ID, bIterator);
            elem.reportConfigId = n;

            measConfig->measIdToAddModList.push_back(elem);
        }
    }

    measConfig->haveQuantityConfig = bitset11[4];
    if (measConfig->haveQuantityConfig)
    {
        // quantityConfig
        std::bitset<4> quantityConfigOpts;
        bIterator = DeserializeSequence(&quantityConfigOpts, true, bIterator);

        if (quantityConfigOpts[3])
        {
            // quantityConfigEUTRA
            bIterator = DeserializeSequence(&bitset0, false, bIterator);
            bIterator = DeserializeEnum(16, &n, bIterator);
            switch (n)
            {
            case 0:
                measConfig->quantityConfig.filterCoefficientRSRP = 0;
                break;
            case 1:
                measConfig->quantityConfig.filterCoefficientRSRP = 1;
                break;
            case 2:
                measConfig->quantityConfig.filterCoefficientRSRP = 2;
                break;
            case 3:
                measConfig->quantityConfig.filterCoefficientRSRP = 3;
                break;
            case 4:
                measConfig->quantityConfig.filterCoefficientRSRP = 4;
                break;
            case 5:
                measConfig->quantityConfig.filterCoefficientRSRP = 5;
                break;
            case 6:
                measConfig->quantityConfig.filterCoefficientRSRP = 6;
                break;
            case 7:
                measConfig->quantityConfig.filterCoefficientRSRP = 7;
                break;
            case 8:
                measConfig->quantityConfig.filterCoefficientRSRP = 8;
                break;
            case 9:
                measConfig->quantityConfig.filterCoefficientRSRP = 9;
                break;
            case 10:
                measConfig->quantityConfig.filterCoefficientRSRP = 11;
                break;
            case 11:
                measConfig->quantityConfig.filterCoefficientRSRP = 13;
                break;
            case 12:
                measConfig->quantityConfig.filterCoefficientRSRP = 15;
                break;
            case 13:
                measConfig->quantityConfig.filterCoefficientRSRP = 17;
                break;
            case 14:
                measConfig->quantityConfig.filterCoefficientRSRP = 19;
                break;
            case 15:
                measConfig->quantityConfig.filterCoefficientRSRP = 0;
                break;
            default:
                measConfig->quantityConfig.filterCoefficientRSRP = 4;
            }
            bIterator = DeserializeEnum(16, &n, bIterator);
            switch (n)
            {
            case 0:
                measConfig->quantityConfig.filterCoefficientRSRQ = 0;
                break;
            case 1:
                measConfig->quantityConfig.filterCoefficientRSRQ = 1;
                break;
            case 2:
                measConfig->quantityConfig.filterCoefficientRSRQ = 2;
                break;
            case 3:
                measConfig->quantityConfig.filterCoefficientRSRQ = 3;
                break;
            case 4:
                measConfig->quantityConfig.filterCoefficientRSRQ = 4;
                break;
            case 5:
                measConfig->quantityConfig.filterCoefficientRSRQ = 5;
                break;
            case 6:
                measConfig->quantityConfig.filterCoefficientRSRQ = 6;
                break;
            case 7:
                measConfig->quantityConfig.filterCoefficientRSRQ = 7;
                break;
            case 8:
                measConfig->quantityConfig.filterCoefficientRSRQ = 8;
                break;
            case 9:
                measConfig->quantityConfig.filterCoefficientRSRQ = 9;
                break;
            case 10:
                measConfig->quantityConfig.filterCoefficientRSRQ = 11;
                break;
            case 11:
                measConfig->quantityConfig.filterCoefficientRSRQ = 13;
                break;
            case 12:
                measConfig->quantityConfig.filterCoefficientRSRQ = 15;
                break;
            case 13:
                measConfig->quantityConfig.filterCoefficientRSRQ = 17;
                break;
            case 14:
                measConfig->quantityConfig.filterCoefficientRSRQ = 19;
                break;
            case 15:
                measConfig->quantityConfig.filterCoefficientRSRQ = 0;
                break;
            default:
                measConfig->quantityConfig.filterCoefficientRSRQ = 4;
            }
        }
        if (quantityConfigOpts[2])
        {
            // quantityConfigUTRA
            // ...
        }
        if (quantityConfigOpts[1])
        {
            // quantityConfigGERAN
            // ...
        }
        if (quantityConfigOpts[0])
        {
            // quantityConfigCDMA2000
            // ...
        }
    }

    measConfig->haveMeasGapConfig = bitset11[3];
    if (measConfig->haveMeasGapConfig)
    {
        // measGapConfig
        int measGapConfigChoice;
        bIterator = DeserializeChoice(2, false, &measGapConfigChoice, bIterator);
        switch (measGapConfigChoice)
        {
        case 0:
            measConfig->measGapConfig.type = NrRrcSap::MeasGapConfig::RESET;
            bIterator = DeserializeNull(bIterator);
            break;
        case 1:
        default:
            measConfig->measGapConfig.type = NrRrcSap::MeasGapConfig::SETUP;
            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            int gapOffsetChoice;
            bIterator = DeserializeChoice(2, true, &gapOffsetChoice, bIterator);
            switch (gapOffsetChoice)
            {
            case 0:
                measConfig->measGapConfig.gapOffsetChoice = NrRrcSap::MeasGapConfig::GP0;
                bIterator = DeserializeInteger(&n, 0, 39, bIterator);
                measConfig->measGapConfig.gapOffsetValue = n;
                break;
            case 1:
            default:
                measConfig->measGapConfig.gapOffsetChoice = NrRrcSap::MeasGapConfig::GP1;
                bIterator = DeserializeInteger(&n, 0, 79, bIterator);
                measConfig->measGapConfig.gapOffsetValue = n;
            }
        }
    }

    measConfig->haveSmeasure = bitset11[2];
    if (measConfig->haveSmeasure)
    {
        // s-Measure
        bIterator = DeserializeInteger(&n, 0, 97, bIterator);
        measConfig->sMeasure = n;
    }

    if (bitset11[1])
    {
        // preRegistrationInfoHRPD
        // ...
    }

    measConfig->haveSpeedStatePars = bitset11[0];
    if (measConfig->haveSpeedStatePars)
    {
        // speedStatePars
        int speedStateParsChoice;
        bIterator = DeserializeChoice(2, false, &speedStateParsChoice, bIterator);
        switch (speedStateParsChoice)
        {
        case 0:
            measConfig->speedStatePars.type = NrRrcSap::SpeedStatePars::RESET;
            bIterator = DeserializeNull(bIterator);
            break;
        case 1:
        default:
            measConfig->speedStatePars.type = NrRrcSap::SpeedStatePars::SETUP;
            bIterator = DeserializeSequence(&bitset0, false, bIterator);

            // Deserialize mobilityStateParameters
            // Deserialize t-Evaluation
            bIterator = DeserializeEnum(8, &n, bIterator);
            switch (n)
            {
            case 0:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 30;
                break;
            case 1:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 60;
                break;
            case 2:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 120;
                break;
            case 3:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 180;
                break;
            case 4:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 240;
                break;
            default:
                measConfig->speedStatePars.mobilityStateParameters.tEvaluation = 0;
            }
            // Deserialize t-HystNormal
            bIterator = DeserializeEnum(8, &n, bIterator);
            switch (n)
            {
            case 0:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 30;
                break;
            case 1:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 60;
                break;
            case 2:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 120;
                break;
            case 3:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 180;
                break;
            case 4:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 240;
                break;
            default:
                measConfig->speedStatePars.mobilityStateParameters.tHystNormal = 0;
            }

            bIterator = DeserializeInteger(&n, 1, 16, bIterator);
            measConfig->speedStatePars.mobilityStateParameters.nCellChangeMedium = n;

            bIterator = DeserializeInteger(&n, 1, 16, bIterator);
            measConfig->speedStatePars.mobilityStateParameters.nCellChangeHigh = n;

            // Deserialize timeToTriggerSf
            bIterator = DeserializeEnum(4, &n, bIterator);
            measConfig->speedStatePars.timeToTriggerSf.sfMedium = (n + 1) * 25;
            bIterator = DeserializeEnum(4, &n, bIterator);
            measConfig->speedStatePars.timeToTriggerSf.sfHigh = (n + 1) * 25;
        }
    }
    return bIterator;
}

//////////////////// RrcConnectionRequest class ////////////////////////

// Constructor
RrcConnectionRequestHeader::RrcConnectionRequestHeader()
    : RrcUlCcchMessage()
{
    m_mmec = std::bitset<8>(0UL);
    m_mTmsi = std::bitset<32>(0UL);
    m_establishmentCause = MO_SIGNALLING;
    m_spare = std::bitset<1>(0UL);
}

// Destructor
RrcConnectionRequestHeader::~RrcConnectionRequestHeader()
{
}

TypeId
RrcConnectionRequestHeader::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::RrcConnectionRequestHeader").SetParent<Header>().SetGroupName("Lte");
    return tid;
}

void
RrcConnectionRequestHeader::Print(std::ostream& os) const
{
    os << "MMEC:" << m_mmec << std::endl;
    os << "MTMSI:" << m_mTmsi << std::endl;
    os << "EstablishmentCause:" << m_establishmentCause << std::endl;
    os << "Spare: " << m_spare << std::endl;
}

void
RrcConnectionRequestHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeUlCcchMessage(1);

    // Serialize RRCConnectionRequest sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice:
    // 2 options, selected: 0 (option: rrcConnectionRequest-r8)
    SerializeChoice(2, 0, false);

    // Serialize RRCConnectionRequest-r8-IEs sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize InitialUE-Identity choice:
    // 2 options, selected: 0 (option: s-TMSI)
    SerializeChoice(2, 0, false);

    // Serialize S-TMSI sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize mmec : MMEC ::= BIT STRING (SIZE (8))
    SerializeBitstring(m_mmec);

    // Serialize m-TMSI ::= BIT STRING (SIZE (32))
    SerializeBitstring(m_mTmsi);

    // Serialize establishmentCause : EstablishmentCause ::= ENUMERATED
    SerializeEnum(8, m_establishmentCause);

    // Serialize spare : BIT STRING (SIZE (1))
    SerializeBitstring(std::bitset<1>());

    // Serialize bool redcap
    SerializeBoolean(m_redCap);  //Information is normally inside the identity. //TODO?

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionRequestHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<1> dummy;
    std::bitset<0> optionalOrDefaultMask;
    int selectedOption;

    bIterator = DeserializeUlCcchMessage(bIterator);

    // Deserialize RCConnectionRequest sequence
    bIterator = DeserializeSequence(&optionalOrDefaultMask, false, bIterator);

    // Deserialize criticalExtensions choice:
    bIterator = DeserializeChoice(2, false, &selectedOption, bIterator);

    // Deserialize RRCConnectionRequest-r8-IEs sequence
    bIterator = DeserializeSequence(&optionalOrDefaultMask, false, bIterator);

    // Deserialize InitialUE-Identity choice
    bIterator = DeserializeChoice(2, false, &selectedOption, bIterator);

    // Deserialize S-TMSI sequence
    bIterator = DeserializeSequence(&optionalOrDefaultMask, false, bIterator);

    // Deserialize mmec
    bIterator = DeserializeBitstring(&m_mmec, bIterator);

    // Deserialize m-TMSI
    bIterator = DeserializeBitstring(&m_mTmsi, bIterator);

    // Deserialize establishmentCause
    bIterator = DeserializeEnum(8, &selectedOption, bIterator);

    // Deserialize spare
    bIterator = DeserializeBitstring(&dummy, bIterator);

    //Deerialize bool redcap
    bIterator = DeserializeBoolean(&m_redCap,bIterator);

    return GetSerializedSize();
}

void
RrcConnectionRequestHeader::SetMessage(NrRrcSap::NrRrcConnectionRequest msg)
{
    m_mTmsi = std::bitset<32>((uint32_t)msg.ueIdentity);
    m_mmec = std::bitset<8>((uint32_t)(msg.ueIdentity >> 32));
    m_redCap = msg.redcap;
    m_isDataSerialized = false;
}

NrRrcSap::NrRrcConnectionRequest
RrcConnectionRequestHeader::GetMessage() const
{
    NrRrcSap::NrRrcConnectionRequest msg;
    msg.ueIdentity = (((uint64_t)m_mmec.to_ulong()) << 32) | (m_mTmsi.to_ulong());
    msg.redcap = m_redCap;

    return msg;
}

std::bitset<8>
RrcConnectionRequestHeader::GetMmec() const
{
    return m_mmec;
}

std::bitset<32>
RrcConnectionRequestHeader::GetMtmsi() const
{
    return m_mTmsi;
}

//////////////////// RrcSetup class ////////////////////////
RrcSetupHeader::RrcSetupHeader()
{
}

RrcSetupHeader::~RrcSetupHeader()
{
}

void
RrcSetupHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
    os << "radioResourceConfigDedicated:" << std::endl;
    RrcAsn1Header::Print(os, m_radioResourceConfigDedicated);
}

void
RrcSetupHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeDlCcchMessage(3);

    SerializeInteger(15, 0, 15);

    // Serialize RrcSetup sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier ::=INTEGER (0..3)
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice:
    // 2 options, selected: 0 (option: c1)
    SerializeChoice(2, 0, false);

    // Serialize c1 choice:
    // 8 options, selected: 0 (option: RrcSetup-r8)
    SerializeChoice(8, 0, false);

    // Serialize RrcSetup-r8 sequence
    // 1 optional fields (not present). Extension marker not present.
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize RadioResourceConfigDedicated sequence
    SerializeRadioResourceConfigDedicated(m_radioResourceConfigDedicated);

    // Serialize SpCellConfig sequence
    SerializeSpCellConfig(m_spCellConfig);

    // Serialize nonCriticalExtension sequence
    // 2 optional fields, none present. No extension marker.
    SerializeSequence(std::bitset<2>(0), false);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcSetupHeader::Deserialize(Buffer::Iterator bIterator)
{
    int n;

    std::bitset<0> bitset0;
    std::bitset<1> bitset1;
    std::bitset<2> bitset2;

    bIterator = DeserializeDlCcchMessage(bIterator);

    bIterator = DeserializeInteger(&n, 0, 15, bIterator);

    // Deserialize RrcSetup sequence
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize rrc-TransactionIdentifier ::=INTEGER (0..3)
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionChoice, bIterator);
    if (criticalExtensionChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionChoice == 0)
    {
        // Deserialize c1
        int c1;
        bIterator = DeserializeChoice(8, false, &c1, bIterator);

        if (c1 > 0)
        {
            // Deserialize spareX , X:=7..1
            bIterator = DeserializeNull(bIterator);
        }
        else if (c1 == 0)
        {
            // Deserialize RrcSetup-r8
            // 1 optional fields, no extension marker.
            bIterator = DeserializeSequence(&bitset1, false, bIterator);

            // Deserialize radioResourceConfigDedicated
            bIterator =
                DeserializeRadioResourceConfigDedicated(&m_radioResourceConfigDedicated, bIterator);
   
            // Deserialize spCellConfig
            bIterator = DeserializeSpCellConfig(&m_spCellConfig, bIterator);

            if (bitset1[0])
            {
                // Deserialize nonCriticalExtension
                // 2 optional fields, no extension marker.
                bIterator = DeserializeSequence(&bitset2, false, bIterator);

                // Deserialization of lateR8NonCriticalExtension and nonCriticalExtension
                // ...
            }

        }
    }
    return GetSerializedSize();
}

void
RrcSetupHeader::SetMessage(NrRrcSap::RrcSetup msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_radioResourceConfigDedicated = msg.radioResourceConfigDedicated;
    m_spCellConfig = msg.spCellconfig;
    m_isDataSerialized = false;
}

NrRrcSap::RrcSetup
RrcSetupHeader::GetMessage() const
{
    NrRrcSap::RrcSetup msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.radioResourceConfigDedicated = m_radioResourceConfigDedicated;
    msg.spCellconfig = m_spCellConfig;
    return msg;
}

uint8_t
RrcSetupHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

bool
RrcSetupHeader::HavePhysicalConfigDedicated() const
{
    return m_radioResourceConfigDedicated.havePhysicalConfigDedicated;
}

std::list<NrRrcSap::SrbToAddMod>
RrcSetupHeader::GetSrbToAddModList() const
{
    return m_radioResourceConfigDedicated.srbToAddModList;
}

std::list<NrRrcSap::DrbToAddMod>
RrcSetupHeader::GetDrbToAddModList() const
{
    return m_radioResourceConfigDedicated.drbToAddModList;
}

std::list<uint8_t>
RrcSetupHeader::GetDrbToReleaseList() const
{
    return m_radioResourceConfigDedicated.drbToReleaseList;
}

NrRrcSap::PhysicalConfigDedicated
RrcSetupHeader::GetPhysicalConfigDedicated() const
{
    return m_radioResourceConfigDedicated.physicalConfigDedicated;
}

NrRrcSap::RadioResourceConfigDedicated
RrcSetupHeader::GetRadioResourceConfigDedicated() const
{
    return m_radioResourceConfigDedicated;
}

//////////////////// RrcConnectionSetupCompleteHeader class ////////////////////////

RrcConnectionSetupCompleteHeader::RrcConnectionSetupCompleteHeader()
{
}

RrcConnectionSetupCompleteHeader::~RrcConnectionSetupCompleteHeader()
{
}

void
RrcConnectionSetupCompleteHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize DCCH message
    SerializeUlDcchMessage(4);

    // Serialize RRCConnectionSetupComplete sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice
    // 2 options, selected 0 (c1)
    SerializeChoice(2, 0, false);

    // Choose spare3 NULL
    SerializeChoice(4, 1, false);

    // Serialize spare3 NULL
    SerializeNull();

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionSetupCompleteHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;

    bIterator = DeserializeUlDcchMessage(bIterator);

    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    int n;
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    bIterator = DeserializeChoice(2, false, &n, bIterator);

    if (n == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (n == 0)
    {
        // Deserialize c1
        int c1Chosen;
        bIterator = DeserializeChoice(4, false, &c1Chosen, bIterator);

        if (c1Chosen == 0)
        {
            // Deserialize rrcConnectionSetupComplete-r8
            // ...
        }
        else
        {
            bIterator = DeserializeNull(bIterator);
        }
    }

    return GetSerializedSize();
}

void
RrcConnectionSetupCompleteHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
}

void
RrcConnectionSetupCompleteHeader::SetMessage(NrRrcSap::RrcConnectionSetupCompleted msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_isDataSerialized = false;
}

uint8_t
RrcConnectionSetupCompleteHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

NrRrcSap::RrcConnectionSetupCompleted
RrcConnectionSetupCompleteHeader::GetMessage() const
{
    NrRrcSap::RrcConnectionSetupCompleted msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    return msg;
}

//////////////////// RrcConnectionReconfigurationCompleteHeader class ////////////////////////

RrcConnectionReconfigurationCompleteHeader::RrcConnectionReconfigurationCompleteHeader()
{
}

RrcConnectionReconfigurationCompleteHeader::~RrcConnectionReconfigurationCompleteHeader()
{
}

void
RrcConnectionReconfigurationCompleteHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize DCCH message
    SerializeUlDcchMessage(2);

    // Serialize RRCConnectionSetupComplete sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice
    // 2 options, selected 1 (criticalExtensionsFuture)
    SerializeChoice(2, 1, false);

    // Choose criticalExtensionsFuture
    SerializeSequence(std::bitset<0>(), false);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionReconfigurationCompleteHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeUlDcchMessage(bIterator);
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    bIterator = DeserializeChoice(2, false, &n, bIterator);

    if (n == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (n == 0)
    {
        // Deserialize rrcConnectionReconfigurationComplete-r8
        // ...
    }

    return GetSerializedSize();
}

void
RrcConnectionReconfigurationCompleteHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
}

void
RrcConnectionReconfigurationCompleteHeader::SetMessage(
    NrRrcSap::RrcConnectionReconfigurationCompleted msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReconfigurationCompleted
RrcConnectionReconfigurationCompleteHeader::GetMessage() const
{
    NrRrcSap::RrcConnectionReconfigurationCompleted msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    return msg;
}

uint8_t
RrcConnectionReconfigurationCompleteHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

//////////////////// RrcReconfigurationHeader class ////////////////////////

RrcReconfigurationHeader::RrcReconfigurationHeader()
{
}

RrcReconfigurationHeader::~RrcReconfigurationHeader()
{
}

void
RrcReconfigurationHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeDlDcchMessage(4);


    // Serialize RRCConnectionSetupComplete sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice
    // 2 options, selected 0 (c1)
    SerializeChoice(2, 0, false);

    // Serialize RRCReconfiguration-IEs sequence:
    // 5 optional fields. Extension marker not present.
    std::bitset<5> options;
    options.set(4, m_haveRadioBearerConfig);
    options.set(3, 0); // No secondaryCellGroup
    options.set(2, 0); // No measConfig
    options.set(1, 0); // No lateNonCriticalExtension
    options.set(0, 0); // No nonCriticalExtension
                                                
    SerializeSequence(options, false);

    if (m_haveRadioBearerConfig)
    {
        // Serialize RadioResourceConfigDedicated
        SerializeRadioBearerConfig(m_radioBearerConfig);
    }


    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcReconfigurationHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;

    bIterator = DeserializeDlDcchMessage(bIterator);

    // RRCConnectionReconfiguration sequence
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // rrc-TransactionIdentifier
    int n;
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // criticalExtensions
    int sel;
    bIterator = DeserializeChoice(2, false, &sel, bIterator);
    if (sel == 1)
    {
        // criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (sel == 0)
    {
        
        // rrcReconfiguration-ies
        std::bitset<5> rrcConnRecOpts;
        bIterator = DeserializeSequence(&rrcConnRecOpts, false, bIterator);

        m_haveRadioBearerConfig = rrcConnRecOpts[4];
        if (m_haveRadioBearerConfig)
        {
            DeserializeRadioBearerConfig(&m_radioBearerConfig,bIterator);
        }

        // secondaryCellGroup
        if (rrcConnRecOpts[3])
        {
            // ...
        }

        // measConfig
       if (rrcConnRecOpts[2])
        {
            // ...
        }

        // lateNonCriticalExtension
        if (rrcConnRecOpts[1])
        {
            // ...
        }

        // nonCriticalExtension
        if (rrcConnRecOpts[0])
        {
            // ...
        }
        
    }

    return GetSerializedSize();
}

void
RrcReconfigurationHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
    os << "haveRadioBearerConfig: " << m_haveRadioBearerConfig << std::endl;
}

void
RrcReconfigurationHeader::SetMessage(NrRrcSap::RrcReconfiguration msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_haveRadioBearerConfig = msg.rrcReconfiguration.haveRadioBearerConfig;
    m_radioBearerConfig = msg.rrcReconfiguration.radioBearerConfig;

    m_isDataSerialized = false;
}

NrRrcSap::RrcReconfiguration
RrcReconfigurationHeader::GetMessage() const
{
    NrRrcSap::RrcReconfiguration msg;

    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.rrcReconfiguration.haveRadioBearerConfig = m_haveRadioBearerConfig;
    msg.rrcReconfiguration.radioBearerConfig = m_radioBearerConfig;


    return msg;
}

uint8_t
RrcReconfigurationHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

bool
RrcReconfigurationHeader::GetHaveRadioBearerConfig() const
{
    return m_haveRadioBearerConfig;
}

NrRrcSap::RadioBearerConfig
RrcReconfigurationHeader::GetRadioBearerConfig()
{
    return m_radioBearerConfig;
}

std::list<NrRrcSap::SrbToAddMod>
RrcReconfigurationHeader::GetSrbToAddModList() const
{
    return m_radioBearerConfig.srbToAddModList;
}

std::list<NrRrcSap::DrbToAddMod>
RrcReconfigurationHeader::GetDrbToAddModList() const
{
    return m_radioBearerConfig.drbToAddModList;
}

std::list<uint8_t>
RrcReconfigurationHeader::GetDrbToReleaseList() const
{
    return m_radioBearerConfig.drbToReleaseList;
}


//////////////////// HandoverPreparationInfoHeader class ////////////////////////

HandoverPreparationInfoHeader::HandoverPreparationInfoHeader()
{
}

void
HandoverPreparationInfoHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize HandoverPreparationInformation sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice
    // 2 options, selected 0 (c1)
    SerializeChoice(2, 0, false);

    // Serialize c1 choice
    // 8 options, selected 0 (handoverPreparationInformation-r8)
    SerializeChoice(8, 0, false);

    // Serialize HandoverPreparationInformation-r8-IEs sequence
    // 4 optional fields, no extension marker.
    std::bitset<4> handoverPrepInfoOpts;
    handoverPrepInfoOpts.set(3, 1); // as-Config present
    handoverPrepInfoOpts.set(2, 0); // rrm-Config not present
    handoverPrepInfoOpts.set(1, 0); // as-Context not present
    handoverPrepInfoOpts.set(0, 0); // nonCriticalExtension not present
    SerializeSequence(handoverPrepInfoOpts, false);

    // Serialize ue-RadioAccessCapabilityInfo
    SerializeSequenceOf(0, MAX_RAT_CAPABILITIES, 0);

    // Serialize as-Config
    SerializeSequence(std::bitset<0>(), true);

    // Serialize sourceMeasConfig
    SerializeMeasConfig(m_asConfig.sourceMeasConfig);

    // Serialize sourceRadioResourceConfig
    SerializeRadioResourceConfigDedicated(m_asConfig.sourceRadioResourceConfig);

    // Serialize sourceSecurityAlgorithmConfig
    SerializeSequence(std::bitset<0>(), false);
    // cipheringAlgorithm
    SerializeEnum(8, 0);
    // integrityProtAlgorithm
    SerializeEnum(8, 0);

    // Serialize sourceUE-Identity
    SerializeBitstring(std::bitset<16>(m_asConfig.sourceUeIdentity));

    // Serialize sourceMasterInformationBlock
    SerializeSequence(std::bitset<0>(), false);
    SerializeEnum(
        8,
        BandwidthToEnum(m_asConfig.sourceMasterInformationBlock.dlBandwidth)); // dl-Bandwidth
    SerializeSequence(std::bitset<0>(), false); // phich-Config sequence
    SerializeEnum(2, 0);                        // phich-Duration
    SerializeEnum(4, 0);                        // phich-Resource
    SerializeBitstring(std::bitset<8>(
        m_asConfig.sourceMasterInformationBlock.systemFrameNumber)); // systemFrameNumber
    SerializeBitstring(std::bitset<10>(321));                        // spare

    // Serialize sourceSystemInformationBlockType1 sequence
    SerializeSystemInformationBlockType1(m_asConfig.sourceSystemInformationBlockType1);

    // Serialize sourceSystemInformationBlockType2
    SerializeSystemInformationBlockType2(m_asConfig.sourceSystemInformationBlockType2);

    // Serialize AntennaInfoCommon
    SerializeSequence(std::bitset<0>(0), false);
    SerializeEnum(4, 0); // antennaPortsCount

    // Serialize sourceDlCarrierFreq
    SerializeInteger(m_asConfig.sourceDlCarrierFreq, 0, MAX_NR_ARFCN);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
HandoverPreparationInfoHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    // Deserialize HandoverPreparationInformation sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize criticalExtensions choice
    int criticalExtensionsChosen;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChosen, bIterator);

    if (criticalExtensionsChosen == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChosen == 0)
    {
        // Deserialize c1 choice
        int c1Chosen;
        bIterator = DeserializeChoice(8, false, &c1Chosen, bIterator);
        if (c1Chosen > 0)
        {
            bIterator = DeserializeNull(bIterator);
        }
        else if (c1Chosen == 0)
        {
            // Deserialize handoverPreparationInformation-r8
            std::bitset<4> handoverPrepInfoOpts;
            bIterator = DeserializeSequence(&handoverPrepInfoOpts, false, bIterator);

            // Deserialize ue-RadioAccessCapabilityInfo
            bIterator = DeserializeSequenceOf(&n, MAX_RAT_CAPABILITIES, 0, bIterator);
            for (int i = 0; i < n; i++)
            {
                // Deserialize UE-CapabilityRAT-Container
                // ...
            }

            if (handoverPrepInfoOpts[3])
            {
                // Deserialize as-Config sequence
                bIterator = DeserializeSequence(&bitset0, true, bIterator);

                // Deserialize sourceMeasConfig
                bIterator = DeserializeMeasConfig(&m_asConfig.sourceMeasConfig, bIterator);

                // Deserialize sourceRadioResourceConfig
                bIterator =
                    DeserializeRadioResourceConfigDedicated(&m_asConfig.sourceRadioResourceConfig,
                                                            bIterator);

                // Deserialize sourceSecurityAlgorithmConfig
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(8, &n, bIterator); // cipheringAlgorithm
                bIterator = DeserializeEnum(8, &n, bIterator); // integrityProtAlgorithm

                // Deserialize sourceUE-Identity
                std::bitset<16> cRnti;
                bIterator = DeserializeBitstring(&cRnti, bIterator);
                m_asConfig.sourceUeIdentity = cRnti.to_ulong();

                // Deserialize sourceMasterInformationBlock
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(8, &n, bIterator); // dl-Bandwidth
                m_asConfig.sourceMasterInformationBlock.dlBandwidth = EnumToBandwidth(n);

                // phich-Config
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(2, &n, bIterator); // phich-Duration
                bIterator = DeserializeEnum(4, &n, bIterator); // phich-Resource

                // systemFrameNumber
                std::bitset<8> systemFrameNumber;
                bIterator = DeserializeBitstring(&systemFrameNumber, bIterator);
                m_asConfig.sourceMasterInformationBlock.systemFrameNumber =
                    systemFrameNumber.to_ulong();
                // spare
                std::bitset<10> spare;
                bIterator = DeserializeBitstring(&spare, bIterator);

                // Deserialize sourceSystemInformationBlockType1
                bIterator = DeserializeSystemInformationBlockType1(
                    &m_asConfig.sourceSystemInformationBlockType1,
                    bIterator);

                // Deserialize sourceSystemInformationBlockType2
                bIterator = DeserializeSystemInformationBlockType2(
                    &m_asConfig.sourceSystemInformationBlockType2,
                    bIterator);

                // Deserialize antennaInfoCommon
                bIterator = DeserializeSequence(&bitset0, false, bIterator);
                bIterator = DeserializeEnum(4, &n, bIterator); // antennaPortsCount

                // Deserialize sourceDl-CarrierFreq
                bIterator = DeserializeInteger(&n, 0, MAX_NR_ARFCN, bIterator);
                m_asConfig.sourceDlCarrierFreq = n;
            }
            if (handoverPrepInfoOpts[2])
            {
                // Deserialize rrm-Config
                // ...
            }
            if (handoverPrepInfoOpts[1])
            {
                // Deserialize as-Context
                // ...
            }
            if (handoverPrepInfoOpts[0])
            {
                // Deserialize nonCriticalExtension
                // ...
            }
        }
    }

    return GetSerializedSize();
}

void
HandoverPreparationInfoHeader::Print(std::ostream& os) const
{
    RrcAsn1Header::Print(os, m_asConfig.sourceRadioResourceConfig);
    os << "sourceUeIdentity: " << m_asConfig.sourceUeIdentity << std::endl;
    os << "dlBandwidth: " << (int)m_asConfig.sourceMasterInformationBlock.dlBandwidth << std::endl;
    os << "systemFrameNumber: " << (int)m_asConfig.sourceMasterInformationBlock.systemFrameNumber
       << std::endl;
    os << "plmnIdentityInfo.plmnIdentity: "
       << (int)m_asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.plmnIdentityInfo
              .plmnIdentity
       << std::endl;
    os << "cellAccessRelatedInfo.cellIdentity "
       << (int)m_asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.cellIdentity
       << std::endl;
    os << "cellAccessRelatedInfo.csgIndication: "
       << m_asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIndication
       << std::endl;
    os << "cellAccessRelatedInfo.csgIdentity: "
       << (int)m_asConfig.sourceSystemInformationBlockType1.cellAccessRelatedInfo.csgIdentity
       << std::endl;
    os << "sourceDlCarrierFreq: " << m_asConfig.sourceDlCarrierFreq << std::endl;
}

void
HandoverPreparationInfoHeader::SetMessage(NrRrcSap::HandoverPreparationInfo msg)
{
    m_asConfig = msg.asConfig;
    m_isDataSerialized = false;
}

NrRrcSap::HandoverPreparationInfo
HandoverPreparationInfoHeader::GetMessage() const
{
    NrRrcSap::HandoverPreparationInfo msg;
    msg.asConfig = m_asConfig;

    return msg;
}

NrRrcSap::AsConfig
HandoverPreparationInfoHeader::GetAsConfig() const
{
    return m_asConfig;
}

//////////////////// RrcResumeRequestHeader class ////////////////////////

RrcResumeRequestHeader::RrcResumeRequestHeader()
{
}

RrcResumeRequestHeader::~RrcResumeRequestHeader()
{
}

void
RrcResumeRequestHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeUlCcchMessage(2);

    // Serialize RRCResumeRequest sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize resumeIdentity
    SerializeBitstring(std::bitset<24>(m_resumeIdentity));
   
    // Serialize resumeMac-I
    SerializeBitstring(std::bitset<16>(0));

    // Serialize resumeCause
    switch(m_resumeCause)
    {
        case NrRrcSap::emergency:
            SerializeEnum(11, 0);
            break;
        case NrRrcSap::highPriorityAccess:
            SerializeEnum(11, 1);
            break;
        case NrRrcSap::mt_Access:
            SerializeEnum(11, 2);
            break;
        case NrRrcSap::mo_Signalling:
            SerializeEnum(11, 3);
            break;
        case NrRrcSap::mo_Data:
            SerializeEnum(11, 4);
            break;
        case NrRrcSap::mo_VoiceCall:
            SerializeEnum(11, 5);
            break;
        case NrRrcSap::mo_VideoCall:
            SerializeEnum(11, 6);
            break;
        case NrRrcSap::mo_SMS:
            SerializeEnum(11, 7);
            break;
        case NrRrcSap::rna_Update:
            SerializeEnum(11, 8);
            break;
        case NrRrcSap::mps_PriorityAccess:
            SerializeEnum(11, 9);
            break;
        case NrRrcSap::mcs_PriorityAccess:
            SerializeEnum(11, 10);
            break;
        default:
            SerializeEnum(11, 0);
    }

    // Serialize spare
    SerializeBitstring(std::bitset<1>(0));

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcResumeRequestHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeUlCcchMessage(bIterator);

    // Deserialize RRCResumeRequest sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize resumeIdentity
    std::bitset<24> resumeIdentity;
    bIterator = DeserializeBitstring(&resumeIdentity, bIterator);
    m_resumeIdentity = resumeIdentity.to_ulong();

    // Deserialize resumeMAC-I
    std::bitset<16> shortMacI;
    bIterator = DeserializeBitstring(&shortMacI, bIterator);

    // Deserialize resumeCause
    int resCs;
    bIterator = DeserializeEnum(11 , &resCs, bIterator);
    switch (resCs)
    {
    case 0:
        m_resumeCause = NrRrcSap::emergency;
        break;
    case 1:
        m_resumeCause = NrRrcSap::highPriorityAccess;
        break;
    case 2:
        m_resumeCause = NrRrcSap::mt_Access;
        break;
    case 3:
        m_resumeCause = NrRrcSap::mo_Signalling;
        break;
    case 4:
        m_resumeCause = NrRrcSap::mo_Data;
        break;
    case 5:
        m_resumeCause = NrRrcSap::mo_VoiceCall;
        break;
    case 6:
        m_resumeCause = NrRrcSap::mo_VideoCall;
        break;
    case 7:
        m_resumeCause = NrRrcSap::mo_SMS;
        break;
    case 8:
        m_resumeCause = NrRrcSap::rna_Update;
        break;
    case 9:
        m_resumeCause = NrRrcSap::mps_PriorityAccess;
        break;
    case 10:
        m_resumeCause = NrRrcSap::mcs_PriorityAccess;
        break;
    }

    // Deserialize spare
    std::bitset<1> spare;
    bIterator = DeserializeBitstring(&spare, bIterator);
    

    return GetSerializedSize();
}

void
RrcResumeRequestHeader::Print(std::ostream& os) const
{
}

void
RrcResumeRequestHeader::SetMessage(
    NrRrcSap::RrcResumeRequest msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_resumeIdentity = msg.rrcResumeRequest.resumeIdentity;
    m_resumeCause = msg.rrcResumeRequest.resumeCause;
    m_isDataSerialized = false;
}

NrRrcSap::RrcResumeRequest
RrcResumeRequestHeader::GetMessage() const
{
    NrRrcSap::RrcResumeRequest msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.rrcResumeRequest.resumeIdentity = m_resumeIdentity;
    msg.rrcResumeRequest.resumeCause = m_resumeCause;

    return msg;
}

uint16_t 
RrcResumeRequestHeader::GetResumeIdentity() const
{
    return m_resumeIdentity;
}

NrRrcSap::ResumeCause
RrcResumeRequestHeader::GetResumeCause() const
{
    return m_resumeCause;
}


//////////////////// RrcResumeHeader class ////////////////////////

RrcResumeHeader::RrcResumeHeader()
{
}

RrcResumeHeader::~RrcResumeHeader()
{
}

void
RrcResumeHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeDlDcchMessage(6);

    // Serialize RRCResume sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);


    // Serialize criticalExtensions choice
    // chosen: rrcResume-IEs
    SerializeChoice(2, 0, false);

     //Serialize RRCResume-IEs 
    // 6 optional fields. Extension marker is not present.
    std::bitset<6> optionalFieldsPresent = std::bitset<6>();
    optionalFieldsPresent.set(5, 0);  // RadioBearerConfig
    optionalFieldsPresent.set(4, 1); //masterCellGroup
    optionalFieldsPresent.set(3, 0); //MeasConfig
    optionalFieldsPresent.set(2, 0); // fullConfig
    optionalFieldsPresent.set(1, 0); // lateNonCriticalExtension
    optionalFieldsPresent.set(0, 0); //nonCriticalExtension
    SerializeSequence(optionalFieldsPresent, false);
    
    SerializeSpCellConfig(m_rrcResume.spCellconfig);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcResumeHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeDlDcchMessage(bIterator);

    // Deserialize RRCResumeRequest sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

     // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize RRCResume-IEs 
        std::bitset<6> opts;
        bIterator = DeserializeSequence(&opts, false, bIterator);
        if (opts[5])
        {
            // Deserialize RadioBearerConfig
            // ...
        }

        if (opts[4])
        {
            // Deserialize masterCellGroup
            // Deserialize spCellConfig
            bIterator = DeserializeSpCellConfig(&m_rrcResume.spCellconfig, bIterator);
        }

        if (opts[3])
        {
            // Deserialize MeasConfig
            // ...
        }

        if (opts[2])
        {
            // Deserialize fullConfig
            // ...
        }
        if (opts[1])
        {
            // Deserialize lateNonCriticalExtension
            // ...
        }
        if (opts[0])
        {
            // Deserialize nonCriticalExtension 
            // ...
        }
    }

    else
    {
        bIterator = DeserializeNull(bIterator);
    }
    

    

    return GetSerializedSize();
}

void
RrcResumeHeader::Print(std::ostream& os) const
{
}

void
RrcResumeHeader::SetMessage(
    NrRrcSap::RrcResume msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_rrcResume = msg.rrcResume;
    m_isDataSerialized = false;
}

NrRrcSap::RrcResume
RrcResumeHeader::GetMessage() const
{
    NrRrcSap::RrcResume msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.rrcResume = m_rrcResume;

    return msg;
}



//////////////////// RrcResumeCompleteHeader class ////////////////////////

RrcResumeCompleteHeader::RrcResumeCompleteHeader()
{
}

RrcResumeCompleteHeader::~RrcResumeCompleteHeader()
{
}

void
RrcResumeCompleteHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeUlDcchMessage(5);

    // Serialize RRCResumeComplete sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);


    // Serialize criticalExtensions choice
    // chosen: rrcResumeComplete-IEs
    SerializeChoice(2, 0, false);

     //Serialize RRCResumeComplete-IEs 
    // 6 optional fields. Extension marker is not present.
    std::bitset<5> optionalFieldsPresent = std::bitset<5>();
    optionalFieldsPresent.set(4, 0); //DedicatedNAS-Message
    optionalFieldsPresent.set(3, 0); //selectedPLMN-Identity
    optionalFieldsPresent.set(2, 0); // UplinkTxDirectCurrentList
    optionalFieldsPresent.set(1, 0); // lateNonCriticalExtension
    optionalFieldsPresent.set(0, 0); //nonCriticalExtension
    SerializeSequence(optionalFieldsPresent, false);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcResumeCompleteHeader::Deserialize(Buffer::Iterator bIterator)
{
     std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeUlDcchMessage(bIterator);

    // Deserialize RRCResumeComplete sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

     // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize RRCResumeComplete-IEs 
        std::bitset<5> opts;
        bIterator = DeserializeSequence(&opts, false, bIterator);

        if (opts[4])
        {
            // Deserialize DedicatedNAS-Message
            // ...
        }

        if (opts[3])
        {
            // Deserialize selectedPLMN-Identity
            // ...
        }

        if (opts[2])
        {
            // Deserialize UplinkTxDirectCurrentList
            // ...
        }
        if (opts[1])
        {
            // Deserialize lateNonCriticalExtension
            // ...
        }
        if (opts[0])
        {
            // Deserialize nonCriticalExtension 
            // ...
        }
    }

    else
    {
        bIterator = DeserializeNull(bIterator);
    }
    

    

    return GetSerializedSize();
    

    return GetSerializedSize();
}

void
RrcResumeCompleteHeader::Print(std::ostream& os) const
{
}

void
RrcResumeCompleteHeader::SetMessage(
    NrRrcSap::RrcResumeComplete msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_rrcResumeComplete = msg.rrcResumeComplete;
    m_isDataSerialized = false;
}

NrRrcSap::RrcResumeComplete
RrcResumeCompleteHeader::GetMessage() const
{
    NrRrcSap::RrcResumeComplete msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.rrcResumeComplete = m_rrcResumeComplete;

    return msg;
}




//////////////////// RrcConnectionReestablishmentRequestHeader class ////////////////////////

RrcConnectionReestablishmentRequestHeader::RrcConnectionReestablishmentRequestHeader()
{
}

RrcConnectionReestablishmentRequestHeader::~RrcConnectionReestablishmentRequestHeader()
{
}

void
RrcConnectionReestablishmentRequestHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeUlCcchMessage(0);

    // Serialize RrcConnectionReestablishmentReques sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice
    // chosen: rrcConnectionReestablishmentRequest-r8
    SerializeChoice(2, 0, false);

    // Serialize RRCConnectionReestablishmentRequest-r8-IEs sequence
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize ue-Identity
    SerializeSequence(std::bitset<0>(), false);
    // Serialize c-RNTI
    SerializeBitstring(std::bitset<16>(m_ueIdentity.cRnti));
    // Serialize physCellId
    SerializeInteger(m_ueIdentity.physCellId, 0, 503);
    // Serialize shortMAC-I
    SerializeBitstring(std::bitset<16>(0));

    // Serialize ReestablishmentCause
    switch (m_reestablishmentCause)
    {
    case NrRrcSap::RECONFIGURATION_FAILURE:
        SerializeEnum(4, 0);
        break;
    case NrRrcSap::HANDOVER_FAILURE:
        SerializeEnum(4, 1);
        break;
    case NrRrcSap::OTHER_FAILURE:
        SerializeEnum(4, 2);
        break;
    default:
        SerializeEnum(4, 3);
    }

    // Serialize spare
    SerializeBitstring(std::bitset<2>(0));

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionReestablishmentRequestHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeUlCcchMessage(bIterator);

    // Deserialize RrcConnectionReestablishmentRequest sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize criticalExtensions choice
    bIterator = DeserializeChoice(2, false, &n, bIterator);
    if (n == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (n == 0)
    {
        // Deserialize RRCConnectionReestablishmentRequest-r8-IEs
        bIterator = DeserializeSequence(&bitset0, false, bIterator);

        // Deserialize ReestabUE-Identity sequence
        bIterator = DeserializeSequence(&bitset0, false, bIterator);

        // Deserialize c-RNTI
        std::bitset<16> cRnti;
        bIterator = DeserializeBitstring(&cRnti, bIterator);
        m_ueIdentity.cRnti = cRnti.to_ulong();

        // Deserialize physCellId
        int physCellId;
        bIterator = DeserializeInteger(&physCellId, 0, 503, bIterator);
        m_ueIdentity.physCellId = physCellId;

        // Deserialize shortMAC-I
        std::bitset<16> shortMacI;
        bIterator = DeserializeBitstring(&shortMacI, bIterator);

        // Deserialize ReestablishmentCause
        int reestCs;
        bIterator = DeserializeEnum(4, &reestCs, bIterator);
        switch (reestCs)
        {
        case 0:
            m_reestablishmentCause = NrRrcSap::RECONFIGURATION_FAILURE;
            break;
        case 1:
            m_reestablishmentCause = NrRrcSap::HANDOVER_FAILURE;
            break;
        case 2:
            m_reestablishmentCause = NrRrcSap::OTHER_FAILURE;
            break;
        case 3:
            break;
        }

        // Deserialize spare
        std::bitset<2> spare;
        bIterator = DeserializeBitstring(&spare, bIterator);
    }

    return GetSerializedSize();
}

void
RrcConnectionReestablishmentRequestHeader::Print(std::ostream& os) const
{
    os << "ueIdentity.cRnti: " << (int)m_ueIdentity.cRnti << std::endl;
    os << "ueIdentity.physCellId: " << (int)m_ueIdentity.physCellId << std::endl;
    os << "m_reestablishmentCause: " << m_reestablishmentCause << std::endl;
}

void
RrcConnectionReestablishmentRequestHeader::SetMessage(
    NrRrcSap::RrcConnectionReestablishmentRequest msg)
{
    m_ueIdentity = msg.ueIdentity;
    m_reestablishmentCause = msg.reestablishmentCause;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReestablishmentRequest
RrcConnectionReestablishmentRequestHeader::GetMessage() const
{
    NrRrcSap::RrcConnectionReestablishmentRequest msg;
    msg.ueIdentity = m_ueIdentity;
    msg.reestablishmentCause = m_reestablishmentCause;

    return msg;
}

NrRrcSap::ReestabUeIdentity
RrcConnectionReestablishmentRequestHeader::GetUeIdentity() const
{
    return m_ueIdentity;
}

NrRrcSap::ReestablishmentCause
RrcConnectionReestablishmentRequestHeader::GetReestablishmentCause() const
{
    return m_reestablishmentCause;
}

//////////////////// RrcConnectionReestablishmentHeader class ////////////////////////

RrcConnectionReestablishmentHeader::RrcConnectionReestablishmentHeader()
{
}

RrcConnectionReestablishmentHeader::~RrcConnectionReestablishmentHeader()
{
}

void
RrcConnectionReestablishmentHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    SerializeDlCcchMessage(0);

    // Serialize RrcConnectionReestablishment sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice
    SerializeChoice(2, 0, false);

    // Serialize c1 choice
    SerializeChoice(8, 0, false);

    // Serialize RRCConnectionReestablishment-r8-IEs sequence
    // 1 optional field, no extension marker
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize radioResourceConfigDedicated
    SerializeRadioResourceConfigDedicated(m_radioResourceConfigDedicated);

    // Serialize nextHopChainingCount
    SerializeInteger(0, 0, 7);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionReestablishmentHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeDlCcchMessage(bIterator);

    // Deserialize RrcConnectionReestablishment sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize c1
        int c1;
        bIterator = DeserializeChoice(8, false, &c1, bIterator);
        if (c1 > 0)
        {
            bIterator = DeserializeNull(bIterator);
        }
        else if (c1 == 0)
        {
            // Deserialize rrcConnectionReestablishment-r8
            // 1 optional field
            std::bitset<1> nonCriticalExtensionPresent;
            bIterator = DeserializeSequence(&nonCriticalExtensionPresent, false, bIterator);

            // Deserialize RadioResourceConfigDedicated
            bIterator =
                DeserializeRadioResourceConfigDedicated(&m_radioResourceConfigDedicated, bIterator);

            // Deserialize nextHopChainingCount
            bIterator = DeserializeInteger(&n, 0, 7, bIterator);
        }
    }

    return GetSerializedSize();
}

void
RrcConnectionReestablishmentHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
    os << "RadioResourceConfigDedicated: " << std::endl;
    RrcAsn1Header::Print(os, m_radioResourceConfigDedicated);
}

void
RrcConnectionReestablishmentHeader::SetMessage(NrRrcSap::RrcConnectionReestablishment msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_radioResourceConfigDedicated = msg.radioResourceConfigDedicated;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReestablishment
RrcConnectionReestablishmentHeader::GetMessage() const
{
    NrRrcSap::RrcConnectionReestablishment msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    msg.radioResourceConfigDedicated = m_radioResourceConfigDedicated;
    return msg;
}

uint8_t
RrcConnectionReestablishmentHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

NrRrcSap::RadioResourceConfigDedicated
RrcConnectionReestablishmentHeader::GetRadioResourceConfigDedicated() const
{
    return m_radioResourceConfigDedicated;
}

//////////////////// RrcConnectionReestablishmentCompleteHeader class ////////////////////////

RrcConnectionReestablishmentCompleteHeader::RrcConnectionReestablishmentCompleteHeader()
{
}

void
RrcConnectionReestablishmentCompleteHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize DCCH message
    SerializeUlDcchMessage(3);

    // Serialize RrcConnectionReestablishmentComplete sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);

    // Serialize criticalExtensions choice
    SerializeChoice(2, 0, false);

    // Serialize rrcConnectionReestablishmentComplete-r8 sequence
    // 1 optional field (not present), no extension marker.
    SerializeSequence(std::bitset<1>(0), false);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionReestablishmentCompleteHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeUlDcchMessage(bIterator);

    // Deserialize RrcConnectionReestablishmentComplete sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize rrcConnectionReestablishmentComplete-r8
        std::bitset<1> opts;
        bIterator = DeserializeSequence(&opts, false, bIterator);
        if (opts[0])
        {
            // Deserialize RRCConnectionReestablishmentComplete-v920-IEs
            // ...
        }
    }

    return GetSerializedSize();
}

void
RrcConnectionReestablishmentCompleteHeader::Print(std::ostream& os) const
{
    os << "rrcTransactionIdentifier: " << (int)m_rrcTransactionIdentifier << std::endl;
}

void
RrcConnectionReestablishmentCompleteHeader::SetMessage(
    NrRrcSap::RrcConnectionReestablishmentComplete msg)
{
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReestablishmentComplete
RrcConnectionReestablishmentCompleteHeader::GetMessage() const
{
    NrRrcSap::RrcConnectionReestablishmentComplete msg;
    msg.rrcTransactionIdentifier = m_rrcTransactionIdentifier;
    return msg;
}

uint8_t
RrcConnectionReestablishmentCompleteHeader::GetRrcTransactionIdentifier() const
{
    return m_rrcTransactionIdentifier;
}

//////////////////// RrcConnectionReestablishmentRejectHeader class ////////////////////////

RrcConnectionReestablishmentRejectHeader::RrcConnectionReestablishmentRejectHeader()
{
}

RrcConnectionReestablishmentRejectHeader::~RrcConnectionReestablishmentRejectHeader()
{
}

void
RrcConnectionReestablishmentRejectHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize CCCH message
    SerializeDlCcchMessage(1);

    // Serialize RrcConnectionReestablishmentReject sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice
    SerializeChoice(2, 0, false);

    // Serialize RRCConnectionReestablishmentReject-r8-IEs sequence
    // 1 optional field (not present), no extension marker.
    SerializeSequence(std::bitset<1>(0), false);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionReestablishmentRejectHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;

    bIterator = DeserializeDlCcchMessage(bIterator);

    // Deserialize RrcConnectionReestablishmentReject sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize rrcConnectionReestablishmentReject-r8
        std::bitset<1> opts;
        bIterator = DeserializeSequence(&opts, false, bIterator);
        if (opts[0])
        {
            // Deserialize RRCConnectionReestablishmentReject-v8a0-IEs
            // ...
        }
    }

    return GetSerializedSize();
}

void
RrcConnectionReestablishmentRejectHeader::Print(std::ostream& os) const
{
}

void
RrcConnectionReestablishmentRejectHeader::SetMessage(
    NrRrcSap::RrcConnectionReestablishmentReject msg)
{
    m_rrcConnectionReestablishmentReject = msg;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReestablishmentReject
RrcConnectionReestablishmentRejectHeader::GetMessage() const
{
    return m_rrcConnectionReestablishmentReject;
}

//////////////////// RrcReleaseHeader class ////////////////////////

RrcReleaseHeader::RrcReleaseHeader()
{
}

RrcReleaseHeader::~RrcReleaseHeader()
{
}

void
RrcReleaseHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize DCCH message
    SerializeDlDcchMessage(5);

    // Serialize RrcRelease sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);


    // Serialize rrc-TransactionIdentifier
    SerializeInteger(m_rrcTransactionIdentifier, 0, 3);


    // Serialize criticalExtensions choice
    SerializeChoice(2, 0, false);

    //Serialize RRCRelease-IEs (v17)
    // 6 optional fields. Extension marker is not present.
    std::bitset<6> optionalFieldsPresent = std::bitset<6>();
    optionalFieldsPresent.set(5, 0);  // redirectedCarrierInfo
    optionalFieldsPresent.set(4, 0); //cellReselectionPriorities
    optionalFieldsPresent.set(3, (m_hasSuspendConfig)? 1 : 0); //SuspendConfig
    optionalFieldsPresent.set(2, 0); // deprioritisationReq
    optionalFieldsPresent.set(1, 0); // lateNonCriticalExtension
    optionalFieldsPresent.set(0, 0); //nonCriticalExtension
    SerializeSequence(optionalFieldsPresent, false);

    if(m_hasSuspendConfig)
    {
        SerializeSuspendConfig(m_rrcRelease.suspendConfig);

    }   

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcReleaseHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeDlDcchMessage(bIterator);

    // Deserialize RrcRelease sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize rrc-TransactionIdentifier
    bIterator = DeserializeInteger(&n, 0, 3, bIterator);
    m_rrcTransactionIdentifier = n;

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize RRCRelease-r17-IEs
        std::bitset<6> opts;
        bIterator = DeserializeSequence(&opts, false, bIterator);
        if (opts[5])
        {
            // Deserialize redirectedCarrierInfo
            // ...
        }

        if (opts[4])
        {
            // Deserialize cellReselectionPriorities
            // ...
        }

        if (opts[3])
        {
            // Deserialize suspendConfig
            m_hasSuspendConfig = true;
            bIterator = DeserializeSuspendConfig(&m_rrcRelease.suspendConfig,bIterator);
        }

        if (opts[2])
        {
            // Deserialize deprioritisationReq
            // ...
        }
        if (opts[1])
        {
            // Deserialize lateNonCriticalExtension
            // ...
        }
        if (opts[0])
        {
            // Deserialize nonCriticalExtension 
            // ...
        }
    }

    return GetSerializedSize();
}

void
RrcReleaseHeader::Print(std::ostream& os) const
{
}

void
RrcReleaseHeader::SetMessage(NrRrcSap::RrcRelease msg)
{
    m_rrcRelease = msg.rrcRelease;
    m_rrcTransactionIdentifier = msg.rrcTransactionIdentifier;
    m_hasSuspendConfig = msg.rrcRelease.hasSuspendConfig;
    m_isDataSerialized = false;
}

NrRrcSap::RrcRelease
RrcReleaseHeader::GetMessage() const
{
    NrRrcSap::RrcRelease msg;
    msg.rrcTransactionIdentifier =m_rrcTransactionIdentifier;
    msg.rrcRelease = m_rrcRelease;
    msg.rrcRelease.hasSuspendConfig = m_hasSuspendConfig;
    return msg;
}

//////////////////// RrcConnectionRejectHeader class ////////////////////////

RrcConnectionRejectHeader::RrcConnectionRejectHeader()
{
}

RrcConnectionRejectHeader::~RrcConnectionRejectHeader()
{
}

void
RrcConnectionRejectHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize CCCH message
    SerializeDlCcchMessage(2);

    // Serialize RrcConnectionReject sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice
    SerializeChoice(2, 0, false);

    // Serialize c1 choice
    SerializeChoice(4, 0, false);

    // Serialize rrcConnectionReject-r8 sequence
    // 1 optional field (not present), no extension marker.
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize waitTime
    SerializeInteger(m_rrcConnectionReject.waitTime, 1, 16);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
RrcConnectionRejectHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeDlCcchMessage(bIterator);

    // Deserialize RrcConnectionReject sequence
    // 0 optional fields, no extension marker
    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    // Deserialize criticalExtensions choice
    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);
    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize c1 choice
        int c1Choice;
        bIterator = DeserializeChoice(4, false, &c1Choice, bIterator);

        if (c1Choice > 0)
        {
            bIterator = DeserializeNull(bIterator);
        }
        else if (c1Choice == 0)
        {
            // Deserialize rrcConnectionReject-r8
            std::bitset<1> opts;
            bIterator = DeserializeSequence(&opts, false, bIterator);

            bIterator = DeserializeInteger(&n, 1, 16, bIterator);
            m_rrcConnectionReject.waitTime = n;

            if (opts[0])
            {
                // Deserialize RRCConnectionReject-v8a0-IEs
                // ...
            }
        }
    }

    return GetSerializedSize();
}

void
RrcConnectionRejectHeader::Print(std::ostream& os) const
{
    os << "wait time: " << (int)m_rrcConnectionReject.waitTime << std::endl;
}

void
RrcConnectionRejectHeader::SetMessage(NrRrcSap::RrcConnectionReject msg)
{
    m_rrcConnectionReject = msg;
    m_isDataSerialized = false;
}

NrRrcSap::RrcConnectionReject
RrcConnectionRejectHeader::GetMessage() const
{
    return m_rrcConnectionReject;
}

//////////////////// MeasurementReportHeader class ////////////////////////

MeasurementReportHeader::MeasurementReportHeader()
{
}

MeasurementReportHeader::~MeasurementReportHeader()
{
}

void
MeasurementReportHeader::PreSerialize() const
{
    m_serializationResult = Buffer();

    // Serialize DCCH message
    SerializeUlDcchMessage(1);

    // Serialize MeasurementReport sequence:
    // no default or optional fields. Extension marker not present.
    SerializeSequence(std::bitset<0>(), false);

    // Serialize criticalExtensions choice:
    // c1 chosen
    SerializeChoice(2, 0, false);

    // Serialize c1 choice
    // measurementReport-r8 chosen
    SerializeChoice(8, 0, false);

    // Serialize MeasurementReport-r8-IEs sequence:
    // 1 optional fields, not present. Extension marker not present.
    SerializeSequence(std::bitset<1>(0), false);

    // Serialize measResults
    SerializeMeasResults(m_measurementReport.measResults);

    // Finish serialization
    FinalizeSerialization();
}

uint32_t
MeasurementReportHeader::Deserialize(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;

    bIterator = DeserializeSequence(&bitset0, false, bIterator);

    bIterator = DeserializeUlDcchMessage(bIterator);

    int criticalExtensionsChoice;
    bIterator = DeserializeChoice(2, false, &criticalExtensionsChoice, bIterator);

    if (criticalExtensionsChoice == 1)
    {
        // Deserialize criticalExtensionsFuture
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
    }
    else if (criticalExtensionsChoice == 0)
    {
        // Deserialize c1
        int c1Choice;
        bIterator = DeserializeChoice(8, false, &c1Choice, bIterator);

        if (c1Choice > 0)
        {
            bIterator = DeserializeNull(bIterator);
        }
        else
        {
            // Deserialize measurementReport-r8
            std::bitset<1> isNonCriticalExtensionPresent;
            bIterator = DeserializeSequence(&isNonCriticalExtensionPresent, false, bIterator);

            // Deserialize measResults
            bIterator = DeserializeMeasResults(&m_measurementReport.measResults, bIterator);

            if (isNonCriticalExtensionPresent[0])
            {
                // Deserialize nonCriticalExtension MeasurementReport-v8a0-IEs
                // ...
            }
        }
    }

    return GetSerializedSize();
}

void
MeasurementReportHeader::Print(std::ostream& os) const
{
    os << "measId = " << (int)m_measurementReport.measResults.measId << std::endl;
    os << "rsrpResult = " << (int)m_measurementReport.measResults.measResultPCell.rsrpResult
       << std::endl;
    os << "rsrqResult = " << (int)m_measurementReport.measResults.measResultPCell.rsrqResult
       << std::endl;
    os << "haveMeasResultNeighCells = "
       << (int)m_measurementReport.measResults.haveMeasResultNeighCells << std::endl;

    if (m_measurementReport.measResults.haveMeasResultNeighCells)
    {
        std::list<NrRrcSap::MeasResultEutra> measResultListEutra =
            m_measurementReport.measResults.measResultListEutra;
        std::list<NrRrcSap::MeasResultEutra>::iterator it = measResultListEutra.begin();
        for (; it != measResultListEutra.end(); it++)
        {
            os << "   physCellId =" << (int)it->physCellId << std::endl;
            os << "   haveCgiInfo =" << it->haveCgiInfo << std::endl;
            if (it->haveCgiInfo)
            {
                os << "      plmnIdentity = " << (int)it->cgiInfo.plmnIdentity << std::endl;
                os << "      cellIdentity = " << (int)it->cgiInfo.cellIdentity << std::endl;
                os << "      trackingAreaCode = " << (int)it->cgiInfo.trackingAreaCode << std::endl;
                os << "      havePlmnIdentityList = " << !it->cgiInfo.plmnIdentityList.empty()
                   << std::endl;
                if (!it->cgiInfo.plmnIdentityList.empty())
                {
                    for (std::list<uint32_t>::iterator it2 = it->cgiInfo.plmnIdentityList.begin();
                         it2 != it->cgiInfo.plmnIdentityList.end();
                         it2++)
                    {
                        os << "         plmnId : " << *it2 << std::endl;
                    }
                }
            }

            os << "   haveRsrpResult =" << it->haveRsrpResult << std::endl;
            if (it->haveRsrpResult)
            {
                os << "   rsrpResult =" << (int)it->rsrpResult << std::endl;
            }

            os << "   haveRsrqResult =" << it->haveRsrqResult << std::endl;
            if (it->haveRsrqResult)
            {
                os << "   rsrqResult =" << (int)it->rsrqResult << std::endl;
            }
        }
    }
}

void
MeasurementReportHeader::SetMessage(NrRrcSap::MeasurementReport msg)
{
    m_measurementReport = msg;
    m_isDataSerialized = false;
}

NrRrcSap::MeasurementReport
MeasurementReportHeader::GetMessage() const
{
    NrRrcSap::MeasurementReport msg;
    msg = m_measurementReport;
    return msg;
}

///////////////////  RrcUlDcchMessage //////////////////////////////////
RrcUlDcchMessage::RrcUlDcchMessage()
    : RrcAsn1Header()
{
}

RrcUlDcchMessage::~RrcUlDcchMessage()
{
}

uint32_t
RrcUlDcchMessage::Deserialize(Buffer::Iterator bIterator)
{
    DeserializeUlDcchMessage(bIterator);
    return 1;
}

void
RrcUlDcchMessage::Print(std::ostream& os) const
{
    std::cout << "UL DCCH MSG TYPE: " << m_messageType << std::endl;
}

void
RrcUlDcchMessage::PreSerialize() const
{
    SerializeUlDcchMessage(m_messageType);
}

Buffer::Iterator
RrcUlDcchMessage::DeserializeUlDcchMessage(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeChoice(2, false, &n, bIterator);
    if (n == 1)
    {
        // Deserialize messageClassExtension
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
        m_messageType = -1;
    }
    else if (n == 0)
    {
        // Deserialize c1
        bIterator = DeserializeChoice(16, false, &m_messageType, bIterator);
    }

    return bIterator;
}

void
RrcUlDcchMessage::SerializeUlDcchMessage(int messageType) const
{
    SerializeSequence(std::bitset<0>(), false);
    // Choose c1
    SerializeChoice(2, 0, false);
    // Choose message type
    SerializeChoice(16, messageType, false);
}

///////////////////  RrcDlDcchMessage //////////////////////////////////
RrcDlDcchMessage::RrcDlDcchMessage()
    : RrcAsn1Header()
{
}

RrcDlDcchMessage::~RrcDlDcchMessage()
{
}

uint32_t
RrcDlDcchMessage::Deserialize(Buffer::Iterator bIterator)
{
    DeserializeDlDcchMessage(bIterator);
    return 1;
}

void
RrcDlDcchMessage::Print(std::ostream& os) const
{
    std::cout << "DL DCCH MSG TYPE: " << m_messageType << std::endl;
}

void
RrcDlDcchMessage::PreSerialize() const
{
    SerializeDlDcchMessage(m_messageType);
}

Buffer::Iterator
RrcDlDcchMessage::DeserializeDlDcchMessage(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeChoice(2, false, &n, bIterator);
    if (n == 1)
    {
        // Deserialize messageClassExtension
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
        m_messageType = -1;
    }
    else if (n == 0)
    {
        // Deserialize c1
        bIterator = DeserializeChoice(16, false, &m_messageType, bIterator);
    }

    return bIterator;
}

void
RrcDlDcchMessage::SerializeDlDcchMessage(int messageType) const
{
    SerializeSequence(std::bitset<0>(), false);
    // Choose c1
    SerializeChoice(2, 0, false);
    // Choose message type
    SerializeChoice(16, messageType, false);
}

///////////////////  RrcUlCcchMessage //////////////////////////////////
RrcUlCcchMessage::RrcUlCcchMessage()
    : RrcAsn1Header()
{
}

RrcUlCcchMessage::~RrcUlCcchMessage()
{
}

uint32_t
RrcUlCcchMessage::Deserialize(Buffer::Iterator bIterator)
{
    DeserializeUlCcchMessage(bIterator);
    return 1;
}

void
RrcUlCcchMessage::Print(std::ostream& os) const
{
    std::cout << "UL CCCH MSG TYPE: " << m_messageType << std::endl;
}

void
RrcUlCcchMessage::PreSerialize() const
{
    SerializeUlCcchMessage(m_messageType);
}

Buffer::Iterator
RrcUlCcchMessage::DeserializeUlCcchMessage(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeChoice(2, false, &n, bIterator);
    if (n == 1)
    {
        // Deserialize messageClassExtension
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
        m_messageType = -1;
    }
    else if (n == 0)
    {
        // Deserialize c1
        bIterator = DeserializeChoice(4, false, &m_messageType, bIterator);
    }

    return bIterator;
}

void
RrcUlCcchMessage::SerializeUlCcchMessage(int messageType) const
{
    SerializeSequence(std::bitset<0>(), false);
    // Choose c1
    SerializeChoice(2, 0, false);
    // Choose message type
    SerializeChoice(4, messageType, false);
}

///////////////////  RrcDlCcchMessage //////////////////////////////////
RrcDlCcchMessage::RrcDlCcchMessage()
    : RrcAsn1Header()
{
}

RrcDlCcchMessage::~RrcDlCcchMessage()
{
}

uint32_t
RrcDlCcchMessage::Deserialize(Buffer::Iterator bIterator)
{
    DeserializeDlCcchMessage(bIterator);
    return 1;
}

void
RrcDlCcchMessage::Print(std::ostream& os) const
{
    std::cout << "DL CCCH MSG TYPE: " << m_messageType << std::endl;
}

void
RrcDlCcchMessage::PreSerialize() const
{
    SerializeDlCcchMessage(m_messageType);
}

Buffer::Iterator
RrcDlCcchMessage::DeserializeDlCcchMessage(Buffer::Iterator bIterator)
{
    std::bitset<0> bitset0;
    int n;

    bIterator = DeserializeSequence(&bitset0, false, bIterator);
    bIterator = DeserializeChoice(2, false, &n, bIterator);
    if (n == 1)
    {
        // Deserialize messageClassExtension
        bIterator = DeserializeSequence(&bitset0, false, bIterator);
        m_messageType = -1;
    }
    else if (n == 0)
    {
        // Deserialize c1
        bIterator = DeserializeChoice(4, false, &m_messageType, bIterator);
    }

    return bIterator;
}

void
RrcDlCcchMessage::SerializeDlCcchMessage(int messageType) const
{
    SerializeSequence(std::bitset<0>(), false);
    // Choose c1
    SerializeChoice(2, 0, false);
    // Choose message type
    SerializeChoice(4, messageType, false);
}

} // namespace ns3
