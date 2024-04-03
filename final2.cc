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
#include "ns3/mobility-model.h"
#include "ns3/packet-socket-helper.h"
#include "ns3/packet-socket-address.h"
#include "ns3/netanim-module.h"
#include "ns3/aodv-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/internet-stack-helper.h"


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
    void SetPosition(Ptr<Node> node, Vector position);
    Vector GetPosition(Ptr<Node> node);
    void AdvancePosition(Ptr<Node> node);
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

void Experiment::SetPosition(Ptr<Node> node, Vector position)
{
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    mobility->SetPosition(position);
}

Vector Experiment::GetPosition(Ptr<Node> node)
{
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    return mobility->GetPosition();
}

void Experiment::AdvancePosition(Ptr<Node> node)
{
    Vector pos = GetPosition(node);
    Vector posBefore = GetPosition(node);
    double mbs = ((m_bytesTotal * 8.0) / 1000000); // Throughput calculation

    NS_LOG_INFO("Throughput: " << mbs << " Mbps");
    NS_LOG_INFO("Total bytes received: " << m_bytesTotal);
    m_bytesTotal = 0; // Reset for the next interval
    m_output.Add(pos.x, mbs);
    pos.x += 1.0; // Increase this value to make the node move faster

    if (pos.x >= 210.0)
    {
        return;
    }

    SetPosition(node, pos);
    Vector posAfter = GetPosition(node);
    NS_LOG_INFO("Node Position before updating: " << posBefore.x << ", " << posBefore.y << ", " << posBefore.z);
    NS_LOG_INFO("Node Position after updating: " << posAfter.x << ", " << posAfter.y << ", " << posAfter.z);

    Simulator::Schedule(Seconds(1.0), &Experiment::AdvancePosition, this, node);
}

void Experiment::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    while ((packet = socket->Recv()))
    {
        m_bytesTotal += packet->GetSize();
        NS_LOG_INFO("Received packet of size: " << packet->GetSize());
        NS_LOG_INFO("Total bytes received: " << m_bytesTotal);

        // Add more information about the source and destination addresses if needed
        Address sourceAddress;
        socket->GetPeerName(sourceAddress);
        NS_LOG_INFO("Source Address: " << sourceAddress);

        Address destinationAddress;
        socket->GetSockName(destinationAddress);
        NS_LOG_INFO("Destination Address: " << destinationAddress);

        // Print node position for debugging
        Vector nodePosition = GetPosition(socket->GetNode());
        NS_LOG_INFO("Node Position after updating: " << nodePosition.x << ", " << nodePosition.y << ", " << nodePosition.z);

        // Print throughput calculation for debugging
        double mbs = ((m_bytesTotal * 8.0) / 1000000);
        NS_LOG_INFO("Throughput: " << mbs << " Mbps");
    }
}


Ptr<Socket> Experiment::SetupPacketReceive(Ptr<Node> node)
{
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sink = Socket::CreateSocket(node, tid);
    InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
    sink->Bind(local);
    sink->SetRecvCallback(MakeCallback(&Experiment::ReceivePacket, this));

    return sink;
}

Gnuplot2dDataset Experiment::Run(const WifiHelper &wifi, const YansWifiPhyHelper &wifiPhy,
                                 const WifiMacHelper &wifiMac, const YansWifiChannelHelper &wifiChannel)
{
    m_bytesTotal = 0;

    NodeContainer c;
    c.Create(5);

    PacketSocketHelper packetSocket;
    packetSocket.Install(c);

    YansWifiPhyHelper phy = wifiPhy;
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac = wifiMac;
    NetDeviceContainer devices = wifi.Install(phy, mac, c);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(1.0, 0.0, 0.0));
    positionAlloc->Add(Vector(2.0, 0.0, 0.0));
    positionAlloc->Add(Vector(0.0, 1.0, 0.0));
    positionAlloc->Add(Vector(1.0, 1.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Set up communication between all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        for (uint32_t j = 0; j < c.GetN(); ++j)
        {
            if (i != j)
            {
                PacketSocketAddress socket;
                socket.SetSingleDevice(devices.Get(i)->GetIfIndex());
                socket.SetPhysicalAddress(devices.Get(j)->GetAddress());
                socket.SetProtocol(4);

                OnOffHelper onoff("ns3::PacketSocketFactory", Address(socket));
                onoff.SetConstantRate(DataRate(60000000));
                onoff.SetAttribute("PacketSize", UintegerValue(2000));

                ApplicationContainer apps = onoff.Install(c.Get(i));
                apps.Start(Seconds(0.5));
                apps.Stop(Seconds(250.0));
            }
        }
    }

    // Schedule position advancement for all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Simulator::Schedule(Seconds(0.5), &Experiment::AdvancePosition, this, c.Get(i));
    }

    // Set up packet reception for all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Ptr<Socket> recvSink = SetupPacketReceive(c.Get(i));
    }

    AnimationInterface anim("line4.xml");
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        anim.UpdateNodeSize(c.Get(i)->GetId(), 0.5, 0.5);
    }

    Simulator::Run();
    Simulator::Destroy();

    return m_output;
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    LogComponentEnable("Wifi-Adhoc", LOG_LEVEL_INFO);

    NodeContainer c;
    c.Create(5); // Create 5 nodes

    Gnuplot gnuplot("reference-rates.png");

    Experiment experiment;
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211a);
    WifiMacHelper wifiMac;
    YansWifiPhyHelper wifiPhy;
    YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
    Gnuplot2dDataset dataset;

    wifiMac.SetType("ns3::AdhocWifiMac");

    // Set the AODV routing protocol
    AodvHelper aodv;
    InternetStackHelper stack;
    stack.SetRoutingHelper(aodv);
    stack.Install(c);  // Use the NodeContainer 'c' here

    PacketSocketHelper packetSocket;
    packetSocket.Install(c);

    YansWifiPhyHelper phy = wifiPhy;
    phy.SetChannel(wifiChannel.Create());

    WifiMacHelper mac = wifiMac;
    NetDeviceContainer devices = wifi.Install(phy, mac, c);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(1.0, 0.0, 0.0));
    positionAlloc->Add(Vector(2.0, 0.0, 0.0));
    positionAlloc->Add(Vector(0.0, 1.0, 0.0));
    positionAlloc->Add(Vector(1.0, 1.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(c);

    // Set up communication between all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        for (uint32_t j = 0; j < c.GetN(); ++j)
        {
            if (i != j)
            {
                PacketSocketAddress socket;
                socket.SetSingleDevice(devices.Get(i)->GetIfIndex());
                socket.SetPhysicalAddress(devices.Get(j)->GetAddress());
                socket.SetProtocol(4);

                OnOffHelper onoff("ns3::PacketSocketFactory", Address(socket));
                onoff.SetConstantRate(DataRate(60000000));
                onoff.SetAttribute("PacketSize", UintegerValue(2000));

                ApplicationContainer apps = onoff.Install(c.Get(i));
                apps.Start(Seconds(0.5));
                apps.Stop(Seconds(250.0));
            }
        }
    }

    // Schedule position advancement for all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Simulator::Schedule(Seconds(0.5), &Experiment::AdvancePosition, &experiment, c.Get(i));
    }

    // Set up packet reception for all nodes
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        Ptr<Socket> recvSink = experiment.SetupPacketReceive(c.Get(i));
    }

    AnimationInterface anim("line4.xml");
    for (uint32_t i = 0; i < c.GetN(); ++i)
    {
        anim.UpdateNodeSize(c.Get(i)->GetId(), 0.5, 0.5);
    }

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}

