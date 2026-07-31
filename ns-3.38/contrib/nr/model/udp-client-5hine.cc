/*
 * Copyright (c) 2007,2008,2009 INRIA, UDCAST
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
 * Author: Amine Ismail <amine.ismail@sophia.inria.fr>
 *                      <amine.ismail@udcast.com>
 */
#include "ns3/udp-client-5hine.h"

#include "ns3/seq-ts-header.h"

#include "ns3/inet-socket-address.h"
#include "ns3/inet6-socket-address.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/socket.h"
#include "ns3/uinteger.h"
#include "ns3/object-vector.h"

#include <cstdio>
#include <cstdlib>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("UdpClient5hine");

NS_OBJECT_ENSURE_REGISTERED(UdpClient5hine);

TypeId
UdpClient5hine::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UdpClient5hine")
            .SetParent<Application>()
            .SetGroupName("Applications")
            .AddConstructor<UdpClient5hine>()
            .AddAttribute("MaxPackets",
                          "The maximum number of packets the application will send",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpClient5hine::m_count),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Interval",
                          "The time to wait between packets",
                          TimeValue(Seconds(1.0)),
                          MakeTimeAccessor(&UdpClient5hine::m_interval),
                          MakeTimeChecker())
            .AddAttribute("RemoteAddress",
                          "The destination Address of the outbound packets",
                          AddressValue(),
                          MakeAddressAccessor(&UdpClient5hine::m_peerAddress),
                          MakeAddressChecker())
            .AddAttribute("RemotePort",
                          "The destination port of the outbound packets",
                          UintegerValue(100),
                          MakeUintegerAccessor(&UdpClient5hine::m_peerPort),
                          MakeUintegerChecker<uint16_t>())
            .AddAttribute("PacketSize",
                          "Size of packets generated. The minimum packet size is 12 bytes which is "
                          "the size of the header carrying the sequence number and the time stamp.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&UdpClient5hine::m_size),
                          MakeUintegerChecker<uint32_t>(12, 65507));
    return tid;
}

UdpClient5hine::UdpClient5hine()
{
    NS_LOG_FUNCTION(this);
    m_sent = 0;
    m_totalTx = 0;
    m_socket = nullptr;
    m_sendEvent = EventId();
}

UdpClient5hine::~UdpClient5hine()
{
    NS_LOG_FUNCTION(this);
}

void
UdpClient5hine::SetRemote(Address ip, uint16_t port)
{
    NS_LOG_FUNCTION(this << ip << port);
    m_peerAddress = ip;
    m_peerPort = port;
}

void
UdpClient5hine::SetRemote(Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    m_peerAddress = addr;
}

void
UdpClient5hine::DoDispose()
{
    NS_LOG_FUNCTION(this);
    Application::DoDispose();
}

void
UdpClient5hine::StartApplication()
{
    NS_LOG_FUNCTION(this);

    if (!m_socket)
    {
        TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
        m_socket = Socket::CreateSocket(GetNode(), tid);
        if (Ipv4Address::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                InetSocketAddress(Ipv4Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (Ipv6Address::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(
                Inet6SocketAddress(Ipv6Address::ConvertFrom(m_peerAddress), m_peerPort));
        }
        else if (InetSocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else if (Inet6SocketAddress::IsMatchingType(m_peerAddress) == true)
        {
            if (m_socket->Bind6() == -1)
            {
                NS_FATAL_ERROR("Failed to bind socket");
            }
            m_socket->Connect(m_peerAddress);
        }
        else
        {
            NS_ASSERT_MSG(false, "Incompatible address type: " << m_peerAddress);
        }
    }

#ifdef NS3_LOG_ENABLE
    std::stringstream peerAddressStringStream;
    if (Ipv4Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv4Address::ConvertFrom(m_peerAddress);
    }
    else if (Ipv6Address::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Ipv6Address::ConvertFrom(m_peerAddress);
    }
    else if (InetSocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << InetSocketAddress::ConvertFrom(m_peerAddress).GetIpv4();
    }
    else if (Inet6SocketAddress::IsMatchingType(m_peerAddress))
    {
        peerAddressStringStream << Inet6SocketAddress::ConvertFrom(m_peerAddress).GetIpv6();
    }
    m_peerAddressString = peerAddressStringStream.str();
#endif // NS3_LOG_ENABLE

    m_socket->SetRecvCallback(MakeNullCallback<void, Ptr<Socket>>());
    m_socket->SetAllowBroadcast(true);
    for (auto v = m_transmitTimestamps.begin(); v != m_transmitTimestamps.end(); ++v)
    {
        m_sendEvent = Simulator::Schedule((*v).Get(), &UdpClient5hine::SendOnce, this);
    }
}

void
UdpClient5hine::StopApplication()
{
    NS_LOG_FUNCTION(this);
    Simulator::Cancel(m_sendEvent);
    appStopped = true;
}

void
UdpClient5hine::SendOnce()
{

    NS_LOG_FUNCTION(this);
    if(appStopped)
    {
        return;
    }
    SeqTsHeader seqTs;
    seqTs.SetSeq(m_sent);
    Ptr<Packet> p = Create<Packet>(m_size - (8 + 4)); // 8+4 : the size of the seqTs header
    p->AddHeader(seqTs);

    if ((m_socket->Send(p)) >= 0)
    {
        //std::cout << Simulator::Now().GetMilliSeconds() <<  ", UdpClient5hine::SendOnce: Data sent" << std::endl;
        ++m_sent;
        m_totalTx += p->GetSize();
#ifdef NS3_LOG_ENABLE
        NS_LOG_INFO("TraceDelay TX " << m_size << " bytes to " << m_peerAddressString << " Uid: "
                                     << p->GetUid() << " Time: " << (Simulator::Now()).As(Time::S));
#endif // NS3_LOG_ENABLE
    }
#ifdef NS3_LOG_ENABLE
    else
    {
        NS_LOG_INFO("Error while sending " << m_size << " bytes to " << m_peerAddressString);
    }
#endif // NS3_LOG_ENABLE
}

uint64_t
UdpClient5hine::GetTotalTx() const
{
    return m_totalTx;
}

void
UdpClient5hine::ScheduleTransmit(std::vector<TimeValue> dt)
{
    m_transmitTimestamps = dt;
}

} // Namespace ns3
