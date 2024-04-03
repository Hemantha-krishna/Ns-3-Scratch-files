/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

void SetupEchoServer (Ptr<Node> serverNode);
Ptr<Socket> SetupPacketReceive (Ptr<Node> node);

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  // Create nodes
  NodeContainer nodes;
  nodes.Create (5);

  // Set up mobility
  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  positionAlloc->Add (Vector (10.0, 0.0, 0.0));
  positionAlloc->Add (Vector (15.0, 0.0, 0.0));
  positionAlloc->Add (Vector (20.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  // Set up Wi-Fi
  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211a);
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel",
                                  "MaxRange", DoubleValue(100.0));
  wifiPhy.SetChannel (wifiChannel.Create ());
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, nodes);

  // Install Internet stack
  InternetStackHelper internet;
  internet.Install (nodes);

  // Assign IP addresses
  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  // Set up echo server and clients
  SetupEchoServer (nodes.Get (0)); // Node 0 as the server
  SetupEchoClients (NodeContainer (nodes.Get (1), nodes.Get (2), nodes.Get (3), nodes.Get (4)), interfaces);

  // Set up packet receive for monitoring
  Ptr<Socket> recvSink = SetupPacketReceive (nodes.Get (1));

  // Configure animation
  AnimationInterface anim("adhoc.xml");

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}

void SetupEchoServer (Ptr<Node> serverNode)
{
  UdpEchoServerHelper echoServer (9); // Port 9 for echo (discard) service
  ApplicationContainer serverApps = echoServer.Install (serverNode);
  serverApps.Start (Seconds (0.5)); // Start the server at 0.5 seconds
  serverApps.Stop (Seconds (250.0));
}

void SetupEchoClients (NodeContainer clients, Ipv4InterfaceContainer interfaces)
{
  UdpEchoClientHelper echoClient (interfaces.GetAddress (0), 9); // Server's IP and port
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  for (uint32_t i = 0; i < clients.GetN (); ++i)
  {
    ApplicationContainer clientApps = echoClient.Install (clients.Get (i));
    clientApps.Start (Seconds (2.0 + i));
    clientApps.Stop (Seconds (250.0));
  }
}

Ptr<Socket> SetupPacketReceive (Ptr<Node> node)
{
  Ptr<Socket> socket = Socket::CreateSocket (node, UdpSocketFactory::GetTypeId ());
  InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 9);
  socket->Bind (local);
  socket->SetRecvCallback (MakeCallback (&ReceivePacket));
  return socket;
}

void ReceivePacket (Ptr<Socket> socket)
{
  Ptr<Packet> packet;
  Address from;
  while ((packet = socket->RecvFrom (from)))
  {
    NS_LOG_UNCOND("Received one packet!");
  }
}

