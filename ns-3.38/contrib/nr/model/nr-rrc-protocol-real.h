/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

// Copyright (c) 2019 Centre Tecnologic de Telecomunicacions de Catalunya (CTTC)
//
// SPDX-License-Identifier: GPL-2.0-only

#ifndef NR_RRC_PROTOCOL_Real_H
#define NR_RRC_PROTOCOL_Real_H

#include <ns3/lte-pdcp-sap.h>
#include <ns3/lte-rlc-sap.h>
#include <ns3/nr-rrc-header.h>
#include <ns3/nr-rrc-sap.h>
#include <ns3/object.h>
#include <ns3/ptr.h>

#include <map>
#include <stdint.h>

namespace ns3
{

class NrUeRrcSapProvider;
class NrUeRrcSapUser;
class NrGnbRrcSapProvider;
class NrUeRrc;

/**
 * \ingroup ue
 * \ingroup gnb
 *
 * \brief RRC message passing from the UE to the GNB
 *
 * Models the transmission of RRC messages from the UE to the eNB in
 * a real fashion, by creating real RRC PDUs and transmitting them
 * over Signaling Radio Bearers using radio resources allocated by the
 * NR MAC scheduler.
 *
 */
class nrUeRrcProtocolReal : public Object
{
    friend class MemberNrUeRrcSapUser<nrUeRrcProtocolReal>;
    /// allow LteRlcSpecificLteRlcSapUser<nrUeRrcProtocolReal> class friend access
    friend class LteRlcSpecificLteRlcSapUser<nrUeRrcProtocolReal>;
    /// allow LtePdcpSpecificLtePdcpSapUser<nrUeRrcProtocolReal> class friend access
    friend class LtePdcpSpecificLtePdcpSapUser<nrUeRrcProtocolReal>;

  public:
    /**
     * \brief nrUeRrcProtocolReal constructor
     */
    nrUeRrcProtocolReal();
    /**
     * \brief ~nrUeRrcProtocolReal
     */
    ~nrUeRrcProtocolReal() override;

    // inherited from Object
    void DoDispose() override;
    /**
     * \brief GetTypeId
     * \return the type id of the object
     */
    static TypeId GetTypeId();

    /**
     * \brief SetNrUeRrcSapProvider
     * \param p
     */
    void SetNrUeRrcSapProvider(NrUeRrcSapProvider* p);
    /**
     * \brief GetNrUeRrcSapUser
     * \return
     */
    NrUeRrcSapUser* GetNrUeRrcSapUser();

    /**
     * \brief SetUeRrc
     * \param rrc
     */
    void SetUeRrc(Ptr<NrUeRrc> rrc);

  private:
    // methods forwarded from NrUeRrcSapUser
    void DoSetup(NrUeRrcSapUser::SetupParameters params);
    void DoSendRrcConnectionRequest(NrRrcSap::NrRrcConnectionRequest msg,uint16_t grantBytes);
    void DoSendRrcResumeRequest(bool sdt, NrRrcSap::RrcResumeRequest msg, uint16_t grantedBytes);
    void DoSendRrcConnectionSetupCompleted(NrRrcSap::RrcConnectionSetupCompleted msg);
    void DoSendRrcResumeComplete(NrRrcSap::RrcResumeComplete msg);
    void DoSendRrcConnectionReconfigurationCompleted(
        NrRrcSap::RrcConnectionReconfigurationCompleted msg);
    void DoSendRrcConnectionReestablishmentRequest(
        NrRrcSap::RrcConnectionReestablishmentRequest msg);
    void DoSendRrcConnectionReestablishmentComplete(
        NrRrcSap::RrcConnectionReestablishmentComplete msg);
    void DoSendMeasurementReport(NrRrcSap::MeasurementReport msg);
    /**
     * \brief Send ideal UE context remove request function
     *
     * Notify eNodeB to release UE context once radio link failure
     * or random access failure is detected. It is needed since no
     * RLF detection mechanism at eNodeB is implemented
     *
     * \param rnti the RNTI of the UE
     */
    void DoSendIdealUeContextRemoveRequest(uint16_t rnti);

    void DoUpdateBwp(uint16_t rnti, uint8_t bwpID);

    void SetGnbRrcSapProvider();

    void DoReceivePdcpPdu(Ptr<Packet> p);
    /**
     * Receive PDCP SDU function
     *
     * \param params LtePdcpSapUser::ReceivePdcpSduParameters
     */
    void DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params);

    Ptr<NrUeRrc> m_rrc;
    uint16_t m_rnti;
    NrUeRrcSapProvider* m_ueRrcSapProvider;
    NrUeRrcSapUser* m_ueRrcSapUser;
    NrGnbRrcSapProvider* m_enbRrcSapProvider;


    NrUeRrcSapUser::SetupParameters m_setupParameters; ///< setup parameters
    NrUeRrcSapProvider::CompleteSetupParameters
        m_completeSetupParameters; ///< complete setup parameters
};

/**
 * Models the transmission of RRC messages from the UE to the gNB in
 * an Real fashion,  by creating real RRC PDUs and transmitting them
 * over Signaling Radio Bearers using radio resources allocated by the
 * NR MAC scheduler.
 *
 */
class NrGnbRrcProtocolReal : public Object
{
    friend class MemberNrGnbRrcSapUser<NrGnbRrcProtocolReal>;
    /// allow LtePdcpSpecificLtePdcpSapUser<NrGnbRrcProtocolReal> class friend access
    friend class LtePdcpSpecificLtePdcpSapUser<NrGnbRrcProtocolReal>;
    /// allow LteRlcSpecificLteRlcSapUser<NrGnbRrcProtocolReal> class friend access
    friend class LteRlcSpecificLteRlcSapUser<NrGnbRrcProtocolReal>;
    /// allow NrRealProtocolRlcSapUser class friend access
    friend class NrRealProtocolRlcSapUser;
  public:
    NrGnbRrcProtocolReal();
    ~NrGnbRrcProtocolReal() override;

    // inherited from Object
    void DoDispose() override;
    static TypeId GetTypeId();

    void SetNrGnbRrcSapProvider(NrGnbRrcSapProvider* p);
    NrGnbRrcSapUser* GetNrGnbRrcSapUser();

    NrUeRrcSapProvider* GetUeRrcSapProvider(uint16_t rnti);
    void SetUeRrcSapProvider(uint16_t rnti, NrUeRrcSapProvider* p);

  private:
    // methods forwarded from NrGnbRrcSapUser
    void DoSetupUe(uint16_t rnti, NrGnbRrcSapUser::SetupUeParameters params);
    void DoRemoveUe(uint16_t rnti);
    void DoSendSystemInformation(uint16_t cellId, NrRrcSap::SystemInformation msg);
    void SendSystemInformation(uint16_t cellId, NrRrcSap::SystemInformation msg);
    void DoSendRrcSetup(uint16_t rnti, NrRrcSap::RrcSetup msg);
    void DoSendRrcResume(uint16_t rnti, NrRrcSap::RrcResume msg);
    void DoSendRrcReconfiguration(uint16_t rnti,
                                            NrRrcSap::RrcReconfiguration msg);
    void DoSendRrcConnectionReestablishment(uint16_t rnti,
                                            NrRrcSap::RrcConnectionReestablishment msg);
    void DoSendRrcConnectionReestablishmentReject(
        uint16_t rnti,
        NrRrcSap::RrcConnectionReestablishmentReject msg);
    void DoSendRrcRelease(uint16_t rnti, NrRrcSap::RrcRelease msg);
    void DoSendRrcConnectionReject(uint16_t rnti, NrRrcSap::RrcConnectionReject msg);
    Ptr<Packet> DoEncodeHandoverPreparationInformation(NrRrcSap::HandoverPreparationInfo msg);
    NrRrcSap::HandoverPreparationInfo DoDecodeHandoverPreparationInformation(Ptr<Packet> p);
    Ptr<Packet> DoEncodeHandoverCommand(NrRrcSap::RrcConnectionReconfiguration msg);
    NrRrcSap::RrcConnectionReconfiguration DoDecodeHandoverCommand(Ptr<Packet> p);

    void DoReceivePdcpSdu(LtePdcpSapUser::ReceivePdcpSduParameters params);
    /**
     * Receive PDCP PDU function
     *
     * \param rnti the RNTI
     * \param p the packet
     */
    void DoReceivePdcpPdu(uint16_t rnti, Ptr<Packet> p);

    uint16_t m_rnti;
    NrGnbRrcSapProvider* m_enbRrcSapProvider;
    NrGnbRrcSapUser* m_enbRrcSapUser;
    std::map<uint16_t, NrUeRrcSapProvider*> m_enbRrcSapProviderMap;
    std::map<uint16_t, NrGnbRrcSapUser::SetupUeParameters>
        m_setupUeParametersMap; ///< setup UE parameters map
    std::map<uint16_t, NrGnbRrcSapProvider::CompleteSetupUeParameters>
        m_completeSetupUeParametersMap; ///< complete setup UE parameters map
};

// RealProtocolRlcSapUser class
class NrRealProtocolRlcSapUser : public LteRlcSapUser
{
  public:
    /**
     * Real protocol RC SAP user
     *
     * \param pdcp NrGnbRrcProtocolReal *
     * \param rnti the RNTI
     */
    NrRealProtocolRlcSapUser(NrGnbRrcProtocolReal* pdcp, uint16_t rnti);

    // Interface implemented from LteRlcSapUser
    void ReceivePdcpPdu(Ptr<Packet> p) override;

  private:
    NrRealProtocolRlcSapUser();
    NrGnbRrcProtocolReal* m_pdcp; ///< PDCP
    uint16_t m_rnti;               ///< RNTI
};

} // namespace ns3

#endif // NR_RRC_PROTOCOL_Real_H
