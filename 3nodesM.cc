/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

/*

 * Copyright (c) 2005,2006,2007 INRIA

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

 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>

 */



#include "ns3/gnuplot.h"

#include "ns3/command-line.h"

#include "ns3/config.h"

#include "ns3/uinteger.h"

#include "ns3/string.h"

#include "ns3/log.h"

#include "ns3/yans-wifi-helper.h"

#include "ns3/mobility-helper.h"

#include "ns3/ipv4-address-helper.h"

#include "ns3/on-off-helper.h"

#include "ns3/yans-wifi-channel.h"

#include "ns3/packet-socket-helper.h"

#include "ns3/packet-socket-address.h"

#include "ns3/netanim-module.h"

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi-Adhoc");

class Experiment

{

public:

  Experiment();

  Experiment(std::string name);

  Gnuplot2dDataset Run(const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,

                      const WifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel);

private:

  void ReceivePacket(Ptr<Socket> socket);

  Ptr<Socket> SetupPacketReceive(Ptr<Node> node);

  uint32_t m_bytesTotal;

  Gnuplot2dDataset m_output;

};

Experiment::Experiment()

{

}

Experiment::Experiment(std::string name)

  : m_output(name)

{

  m_output.SetStyle(Gnuplot2dDataset::LINES);

}

void

Experiment::ReceivePacket(Ptr<Socket> socket)

{

  Ptr<Packet> packet;

  while ((packet = socket->Recv()))

  {

    m_bytesTotal += packet->GetSize();

  }

}

Ptr<Socket>

Experiment::SetupPacketReceive(Ptr<Node> node)

{

  TypeId tid = TypeId::LookupByName("ns3::PacketSocketFactory");

  Ptr<Socket> sink = Socket::CreateSocket(node, tid);

  sink->Bind();

  sink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));

  return sink;

}

Gnuplot2dDataset

Experiment::Run(const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,

                 const WifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel)

{

  m_bytesTotal = 0;

  NodeContainer c;
  c.Create(3); // Create 3 nodes
  PacketSocketHelper packetSocket;
  packetSocket.Install(c);

  YansWifiPhyHelper phy = wifiPhy;
  phy.SetChannel(wifiChannel.Create());

  WifiMacHelper mac = wifiMac;
  NetDeviceContainer devices = wifi.Install(phy, mac, c);

  MobilityHelper mobility;

  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(0.0),
                                "MinY", DoubleValue(0.0),
                                "DeltaX", DoubleValue(50.0),
                                "DeltaY", DoubleValue(0.0),
                                "GridWidth", UintegerValue(3),
                                "LayoutType", StringValue("RowFirst"));

  mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(0, 150, -50, 50)),
                            "Time", TimeValue(Seconds(1.0)));

  mobility.Install(c);

  // Create On/Off applications for each node to communicate with others
  for (uint32_t i = 0; i < c.GetN(); ++i)
  {
    PacketSocketAddress socket;
    socket.SetSingleDevice(devices.Get(i)->GetIfIndex());

    // Set the destination address for the On/Off application (other nodes)
    if (i == 0)
      socket.SetPhysicalAddress(devices.Get(1)->GetAddress());
    else
      socket.SetPhysicalAddress(devices.Get(0)->GetAddress());

    socket.SetProtocol(1);

    OnOffHelper onoff("ns3::PacketSocketFactory", Address(socket));
    onoff.SetConstantRate(DataRate(60000000));
    onoff.SetAttribute("PacketSize", UintegerValue(2000));

    ApplicationContainer apps = onoff.Install(c.Get(i));
    apps.Start(Seconds(0.5));
    apps.Stop(Seconds(250.0));
  }

  Ptr<Socket> recvSink = SetupPacketReceive(c.Get(0));
  AnimationInterface anim("3nodesM.xml");

  Simulator::Run();
  Simulator::Destroy();

  return m_output;
}

int main(int argc, char *argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  Gnuplot gnuplot = Gnuplot("reference-rates.png");

  Experiment experiment;
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211a);
  WifiMacHelper wifiMac;
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  Gnuplot2dDataset dataset;

  wifiMac.SetType("ns3::AdhocWifiMac");

  for (uint32_t i = 0; i < 5; ++i)
  {
    std::string dataMode;
    switch (i)
    {
    case 0:
      dataMode = "OfdmRate54Mbps";
      break;
    case 1:
      dataMode = "OfdmRate48Mbps";
      break;
    case 2:
      dataMode = "OfdmRate36Mbps";
      break;
    case 3:
      dataMode = "OfdmRate24Mbps";
      break;
    case 4:
      dataMode = "OfdmRate18Mbps";
      break;
    }

    std::string rateName = "Rate" + std::to_string(i);
    experiment = Experiment(rateName);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue(dataMode));
    dataset = experiment.Run(wifi, wifiPhy, wifiMac, wifiChannel);
    gnuplot.AddDataset(dataset);
  }

  gnuplot.GenerateOutput(std::cout);

  gnuplot = Gnuplot("rate-control.png");

  for (uint32_t i = 0; i < 5; ++i)
  {
    std::string rateControl;
    switch (i)
    {
    case 0:
      rateControl = "ns3::ArfWifiManager";
      break;
    case 1:
      rateControl = "ns3::AarfWifiManager";
      break;
    case 2:
      rateControl = "ns3::AarfcdWifiManager";
      break;
    case 3:
      rateControl = "ns3::CaraWifiManager";
      break;
    case 4:
      rateControl = "ns3::RraaWifiManager";
      break;
    }

    std::string rateControlName = "RateControl" + std::to_string(i);
    experiment = Experiment(rateControlName);
    wifi.SetRemoteStationManager(rateControl);
    dataset = experiment.Run(wifi, wifiPhy, wifiMac, wifiChannel);
    gnuplot.AddDataset(dataset);
  }

  gnuplot.GenerateOutput(std::cout);

  return 0;
}

