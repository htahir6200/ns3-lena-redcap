/*
 * Copyright (c) 2008 INRIA
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
 * Author: Mohamed Amine Ismail <amine.ismail@sophia.inria.fr>
 */
#include "ns3/udp-client-server-helper-5hine.h"

#include "ns3/string.h"
#include "ns3/udp-client-5hine.h"
#include "ns3/udp-server-5hine.h"
#include "ns3/udp-trace-client.h"
#include "ns3/uinteger.h"

namespace ns3
{

UdpServerHelper5hine::UdpServerHelper5hine()
{
    m_factory.SetTypeId(UdpServer5hine::GetTypeId());
}

UdpServerHelper5hine::UdpServerHelper5hine(uint16_t port)
{
    m_factory.SetTypeId(UdpServer5hine::GetTypeId());
    SetAttribute("Port", UintegerValue(port));
}

void
UdpServerHelper5hine::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
UdpServerHelper5hine::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;

        m_server = m_factory.Create<UdpServer5hine>();
        node->AddApplication(m_server);
        apps.Add(m_server);
    }
    return apps;
}

Ptr<UdpServer5hine>
UdpServerHelper5hine::GetServer()
{
    return m_server;
}

UdpClientHelper5hine::UdpClientHelper5hine()
{
    m_factory.SetTypeId(UdpClient5hine::GetTypeId());
}

UdpClientHelper5hine::UdpClientHelper5hine(Address address, uint16_t port)
{
    m_factory.SetTypeId(UdpClient5hine::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
}

UdpClientHelper5hine::UdpClientHelper5hine(Address address)
{
    m_factory.SetTypeId(UdpClient5hine::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
}

void
UdpClientHelper5hine::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
UdpClientHelper5hine::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<UdpClient5hine> client = m_factory.Create<UdpClient5hine>();
        node->AddApplication(client);
        apps.Add(client);
    }
    return apps;
}

UdpTraceClientHelper5hine::UdpTraceClientHelper5hine()
{
    m_factory.SetTypeId(UdpTraceClient::GetTypeId());
}

UdpTraceClientHelper5hine::UdpTraceClientHelper5hine(Address address, uint16_t port, std::string filename)
{
    m_factory.SetTypeId(UdpTraceClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
    SetAttribute("TraceFilename", StringValue(filename));
}

UdpTraceClientHelper5hine::UdpTraceClientHelper5hine(Address address, std::string filename)
{
    m_factory.SetTypeId(UdpTraceClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("TraceFilename", StringValue(filename));
}

void
UdpTraceClientHelper5hine::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

ApplicationContainer
UdpTraceClientHelper5hine::Install(NodeContainer c)
{
    ApplicationContainer apps;
    for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
    {
        Ptr<Node> node = *i;
        Ptr<UdpTraceClient> client = m_factory.Create<UdpTraceClient>();
        node->AddApplication(client);
        apps.Add(client);
    }
    return apps;
}

} // namespace ns3
