
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/olsr6-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/animation-interface.h"
#include "ns3/qos-wifi-mac-helper.h"
#include "ns3/on-off-helper.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("WifiSimpleAdhocGrid");

void ReceivePacket (Ptr<Socket> socket)
{
  while (socket->Recv ())
    {
      NS_LOG_UNCOND ("Received one packet!");
    }
}

static void GenerateTraffic (Ptr<Socket> socket, uint32_t pktSize, 
                             uint32_t pktCount, Time pktInterval )
{ 
  if (pktCount > 0)
    {
      socket->Send (Create<Packet> (pktSize));
      Simulator::Schedule (pktInterval, &GenerateTraffic, 
                           socket, pktSize,pktCount-1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}


int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double distance = 500;  // m
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 1;
  uint32_t numNodes = 25;  // by default, 5x5
  uint32_t sinkNode = 0;
  uint32_t sourceNode = 24;
  double interval = 1.0; // seconds
  bool verbose = false;
  bool tracing = true;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("distance", "distance (m)", distance);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("tracing", "turn on ascii and pcap tracing", tracing);
  cmd.AddValue ("numNodes", "number of nodes", numNodes);
  cmd.AddValue ("sinkNode", "Receiver node number", sinkNode);
  cmd.AddValue ("sourceNode", "Sender node number", sourceNode);

  cmd.Parse (argc, argv);
  // Convert to time object
  Time interPacketInterval = Seconds (interval);

  // disable fragmentation for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::FragmentationThreshold", StringValue ("2200"));
  // turn off RTS/CTS for frames below 2200 bytes
  Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("2200"));
  // Fix non-unicast data rate to be the same as that of unicast
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode", 
                      StringValue (phyMode));

  NodeContainer c;
  c.Create (numNodes);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }

  //Capa fisica
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (-10) ); 
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO); 


  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::FriisPropagationLossModel");
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Wifi mac con QoS
  QosWifiMacHelper qosWifiMac = QosWifiMacHelper::Default ();
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  
  // Activar modo adhoc
  qosWifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices = wifi.Install (wifiPhy, qosWifiMac, c);
   
  //Movilidad
  MobilityHelper mobility;
  ObjectFactory position;

  position.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  position.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=20|Max=1400]"));
  position.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=20|Max=1400]"));
  Ptr<PositionAllocator> PositionAlloc = position.Create ()->GetObject<PositionAllocator> ();
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                      "Speed", StringValue ("ns3::ExponentialRandomVariable[Mean=50]"),
                                      "Pause",StringValue ("ns3::ExponentialRandomVariable[Mean=50]"),
                                      "PositionAllocator", PointerValue (PositionAlloc));
                                        
  mobility.SetPositionAllocator (PositionAlloc);
  mobility.Install (c);

  // Activar OLSR6
  Olsr6Helper olsr6;
  Ipv6StaticRoutingHelper staticRouting;

  Ipv6ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr6, 10);

  //Configuracion de ipv6
  InternetStackHelper internet;
  internet.SetIpv4StackInstall (false); //desactiva ipv4
  internet.SetRoutingHelper (list); // has effect on the next Install ()
  internet.Install (c);

  Ipv6AddressHelper ipv6;
  NS_LOG_INFO ("Assign IP Addresses.");
  //ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv6.SetBase ("2001:0:1::",Ipv6Prefix (64));
  Ipv6InterfaceContainer ipv6Interface = ipv6.Assign (devices);

  //Aplicaciones

  ApplicationContainer apps1;
  OnOffHelper onOffHelper1 ("ns3::UdpSocketFactory", Inet6SocketAddress (ipv6Interface.GetAddress (sourceNode, 0), 80));//80 es el puerto
  onOffHelper1.SetAttribute ("DataRate", DataRateValue (DataRate ("11Mbps")));
  //onOffHelper1.SetAttribute ("PacketSize", UintegerValue (packetSize));
  //onOffHelper1.SetAttribute ("OnTime",  RandomVariableValue (ConstantVariable (1)));
  //onOffHelper1.SetAttribute ("OffTime", RandomVariableValue (ConstantVariable (0)));
  //onOffHelper1.SetAttribute ("AccessClass", UintegerValue (6));
  apps1.Add (onOffHelper1.Install (c.Get (sourceNode)));
  apps1.Start (Seconds (1.1));

  //Crea sockets asociados a los nodos sink y source y los conecta
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (c.Get (sinkNode), tid);
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (c.Get (sourceNode), tid);
  Inet6SocketAddress remote = Inet6SocketAddress (ipv6Interface.GetAddress (sinkNode, 0), 80);
  source->Connect (remote);

  if (tracing == true)
    {
      AsciiTraceHelper ascii;
      wifiPhy.EnableAsciiAll (ascii.CreateFileStream ("taller1.tr"));
      wifiPhy.EnablePcap ("taller1", devices);
      // Trace routing tables
      Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("taller1.routes", std::ios::out);
      olsr6.PrintRoutingTableAllEvery (Seconds (2), routingStream);
      Ptr<OutputStreamWrapper> neighborStream = Create<OutputStreamWrapper> ("taller1.neighbors", std::ios::out);
      olsr6.PrintNeighborCacheAllEvery (Seconds (2), neighborStream);

      // To do-- enable an IP-level trace that shows forwarding events only
    }

  // Give OLSR time to converge-- 30 seconds perhaps
  Simulator::Schedule (Seconds (30.0), &GenerateTraffic, 
                       source, packetSize, numPackets, interPacketInterval);

  // Output what we are doing
  //NS_LOG_UNCOND ("Testing from node " << sourceNode << " to " << sinkNode << " with grid distance " << distance);

  Simulator::Stop (Seconds (33.0));
  AnimationInterface anim ("taller1_anim.xml");
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
