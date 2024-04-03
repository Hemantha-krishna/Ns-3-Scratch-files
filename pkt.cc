#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"

using namespace ns3;

int main (int argc, char *argv[]) {
    // Create two nodes
    NodeContainer nodes;
    nodes.Create(2);

    // Install internet stack on the nodes
    InternetStackHelper stack;
    stack.Install(nodes);

    // Create a mobility model and assign it to the nodes
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::GridPositionAllocator", "MinX", DoubleValue(0.0), "MinY", DoubleValue(0.0), "DeltaX", DoubleValue(5.0), "DeltaY", DoubleValue(10.0), "GridWidth", UintegerValue(3), "LayoutType", StringValue("RowFirst"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    // Declare a NetDeviceContainer
    NetDeviceContainer devices;

    // Get the list of devices from each node and add them to the devices container
    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        devices.Add(nodes.Get(i)->GetDevice(0));
    }

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface = address.Assign(devices);

    // Install PacketSink application
    uint16_t sinkPort = 8080;
    Address sinkLocalAddress (InetSocketAddress (iface.GetAddress (1), sinkPort));
    PacketSinkHelper sink ("ns3::UdpSocketFactory", sinkLocalAddress);
    ApplicationContainer sinkApps = sink.Install (nodes.Get (1));
    sinkApps.Start (Seconds (1.0));
    sinkApps.Stop (Seconds (10.0));

    // Install OnOff application
    OnOffHelper onOff ("ns3::UdpSocketFactory", sinkLocalAddress);
    onOff.SetAttribute ("MaxBytes", UintegerValue (1024));
    onOff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=2]"));
    onOff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=2]"));
    ApplicationContainer onOffApps = onOff.Install (nodes.Get (0));
    onOffApps.Start (Seconds (0.0));
    onOffApps.Stop (Seconds (10.0));

    // Create the animation
    AnimationInterface anim ("wifi-tcp.xml");
    anim.UpdateNodeDescription (nodes.Get (0), "Sender");
    anim.UpdateNodeColor (nodes.Get (0), 255, 0, 0);
    anim.UpdateNodeDescription (nodes.Get (1), "Receiver");
    anim.UpdateNodeColor (nodes.Get (1), 0, 255, 0);

    // Run the simulation
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}

