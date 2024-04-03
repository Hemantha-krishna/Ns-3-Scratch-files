#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/aodv-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

// Define data structures to collect performance metrics
uint32_t totalPacketsSent = 0;
uint32_t totalPacketsReceived = 0;
uint32_t totalPacketDrops = 0;
Time totalTimeDelay = Seconds(0);
uint32_t totalRoutingOverhead = 0;

void PacketSend(Ptr<const Packet> packet)
{
    totalPacketsSent++;
}

void PacketReceive(Ptr<const Packet> packet, const Ipv4Header &header, Ptr<NetDevice> input, Ptr<NetDevice> ocb, Ptr<const Packet> oPacket, const Address &source)
{
    totalPacketsReceived++;
    // Calculate delay
    Time now = Simulator::Now();
    Time packetDelay = now - Time(header.GetTimestamp().GetMicroSeconds() * 1e-6); // Convert microseconds to seconds
    totalTimeDelay += packetDelay;
}

void PacketDrop(Ptr<const Packet> packet, const Ipv4Header &header)
{
    totalPacketDrops++;
}

int main(int argc, char *argv[])
{
    // Step 1: Initialize nodes and containers
    NodeContainer nodes;
    nodes.Create(100); // Create 100 vehicle nodes

    // Step 2: Define the Mobility Model
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(5.0),
                                  "GridWidth", UintegerValue(10),
                                  "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel",
                              "Speed", StringValue("ns3::ConstantRandomVariable[Constant=20]"),
                              "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));

    mobility.Install(nodes);

    // Step 3: Install the Internet Stack
    InternetStackHelper internet;
    internet.Install(nodes);

    // Step 4: Implement the AODV Routing Protocol
    AodvHelper aodv;
    Ipv4ListRoutingHelper list;
    list.Add(aodv, 100);

    InternetStackHelper internetStack;
    internetStack.SetRoutingHelper(list);
    internetStack.Install(nodes);

    // Step 5: Define Traffic and Applications
    UdpEchoClientHelper echoClient(Ipv4Address("1.2.3.4"), 9);
    echoClient.SetAttribute("MaxPackets", UintegerValue(1));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(1.0)));
    echoClient.SetAttribute("PacketSize", UintegerValue(512));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    UdpEchoServerHelper echoServer(9);
    ApplicationContainer serverApps = echoServer.Install(nodes.Get(9));
    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(11.0));

    // Install callbacks for packet send, receive, and drop events
    Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Tx", MakeCallback(&PacketSend));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Rx", MakeCallback(&PacketReceive));
    Config::ConnectWithoutContext("/NodeList/*/$ns3::Ipv4L3Protocol/Drop", MakeCallback(&PacketDrop));

    // Step 8: Run the Simulation
    Simulator::Stop(Seconds(12.0));
    Simulator::Run();

    // Step 9: Calculate and print the performance metrics
    double packetDropRatio = static_cast<double>(totalPacketDrops) / totalPacketsSent;
    double averageDelay = totalTimeDelay.GetSeconds() / totalPacketsReceived;
    double normalizedRoutingLoad = static_cast<double>(totalRoutingOverhead) / totalPacketsSent;

    NS_LOG_UNCOND("Packet Drop Ratio: " << packetDropRatio);
    NS_LOG_UNCOND("Average Delay: " << averageDelay << " seconds");
    NS_LOG_UNCOND("Normalized Routing Load: " << normalizedRoutingLoad);

    Simulator::Destroy();

    return 0;
}

