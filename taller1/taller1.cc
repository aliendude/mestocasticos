// This script configures two nodes on an 802.11b physical layer, with
// 802.11b NICs in adhoc mode, and by default, sends one packet of 1000
// (application) bytes to the other node.  The physical layer is configured
// to receive at a fixed RSS (regardless of the distance and transmit
// power); therefore, changing position of the nodes has no effect.
//
// There are a number of command-line options available to control
// the default behavior.  The list of available command-line options
// can be listed with the following command:
// ./waf --run "wifi-simple-adhoc --help"
//
// For instance, for this configuration, the physical layer will
// stop successfully receiving packets when rss drops below -97 dBm.
// To see this effect, try running:
//
// ./waf --run "wifi-simple-adhoc --rss=-97 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-98 --numPackets=20"
// ./waf --run "wifi-simple-adhoc --rss=-99 --numPackets=20"
//
// Note that all ns-3 attributes (not just the ones exposed in the below
// script) can be changed at command line; see the documentation.
//
// This script can also be helpful to put the Wifi layer into verbose
// logging mode; this command will turn on all wifi logging:
//
// ./waf --run "wifi-simple-adhoc --verbose=1"
//
// When you are done, you will notice two pcap trace files in your directory.
// If you have tcpdump installed, you can try this:
//
// tcpdump -r wifi-simple-adhoc-0-0.pcap -nn -tt


//Del ejemplo de wifi-blockack:
//On the access point there is no application installed so it replies to every packet with an ICMP frame.

#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/netanim-module.h"
#include "ns3/animation-interface.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Taller1");

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
                           socket, pktSize,pktCount - 1, pktInterval);
    }
  else
    {
      socket->Close ();
    }
}

int main (int argc, char *argv[])
{
  std::string phyMode ("DsssRate1Mbps");
  double rss = -80;  // -dBm
  uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 10;
  double interval = 10.0; // seconds
  bool verbose = false;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);

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

  NodeContainer n1;
  n1.Create (21);
  Ptr<Node> s1 = CreateObject<Node> ();

  Ptr<Node> s2 = CreateObject<Node> ();

  Ptr<Node> s3 = CreateObject<Node> ();

  Ptr<Node> s4 = CreateObject<Node> ();
  //internet stack ipv6
  InternetStackHelper internet;
  internet.SetIpv4StackInstall (false);//desactiva ipv6

  internet.Install (n1);
  internet.Install (s1);
  internet.Install (s2);
  internet.Install (s3);
  internet.Install (s4);

  // The below set of helpers will help us to put together the wifi NICs we want
  WifiHelper wifi;
  if (verbose)
    {
      wifi.EnableLogComponents ();  // Turn on all Wifi logging
    }
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  // This is one parameter that matters when using FixedRssLossModel
  // set it to zero; otherwise, gain will be added
  wifiPhy.Set ("RxGain", DoubleValue (0) );
  // ns-3 supports RadioTap and Prism tracing extensions for 802.11b
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  // The below FixedRssLossModel will cause the rss to be fixed regardless
  // of the distance between the two stations, and the transmit power
  wifiChannel.AddPropagationLoss ("ns3::FixedRssLossModel","Rss",DoubleValue (rss));
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Add a mac and disable rate control
  WifiMacHelper wifiMac;
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue (phyMode),
                                "ControlMode",StringValue (phyMode));
  // Set it to adhoc mode
  wifiMac.SetType ("ns3::AdhocWifiMac");
  NetDeviceContainer devices_n1 = wifi.Install (wifiPhy, wifiMac, n1);


  ////////////configuracion de Estacion 1 /////////////////////////////
  Ssid ssid ("My-network1");

  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "QosSupported", BooleanValue (true),
                   "Ssid", SsidValue (ssid),
  /* setting blockack threshold for sta's BE queue */
                   "BE_BlockAckThreshold", UintegerValue (2),
  /* setting block inactivity timeout to 3*1024 = 3072 microseconds */
                   "BE_BlockAckInactivityTimeout", UintegerValue (3));
  NetDeviceContainer device_s1 = wifi.Install (wifiPhy, wifiMac, s1);


  wifiMac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid),
               "BE_BlockAckThreshold", UintegerValue (0));
  device_s1 = wifi.Install (wifiPhy, wifiMac, s1);
  //AnimationInterface::UpdateNodeColor (s1, 12, 2, 3); 

  ////////////configuracion de Estacion 2 /////////////////////////////
  Ssid ssid1 ("My-network2");

  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "QosSupported", BooleanValue (false),
                   "Ssid", SsidValue (ssid1),
  /* setting blockack threshold for sta's BE queue */
                   "BE_BlockAckThreshold", UintegerValue (2),
  /* setting block inactivity timeout to 3*1024 = 3072 microseconds */
                   "BE_BlockAckInactivityTimeout", UintegerValue (3));
  NetDeviceContainer device_s2 = wifi.Install (wifiPhy, wifiMac, s2);

  wifiMac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (false),
               "Ssid", SsidValue (ssid),
               "BE_BlockAckThreshold", UintegerValue (0));
  device_s2 = wifi.Install (wifiPhy, wifiMac, s2);

    ////////////configuracion de Estacion 3 /////////////////////////////
  Ssid ssid3 ("My-network3");

  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "QosSupported", BooleanValue (true),
                   "Ssid", SsidValue (ssid3),
  /* setting blockack threshold for sta's BE queue */
                   "BE_BlockAckThreshold", UintegerValue (2),
  /* setting block inactivity timeout to 3*1024 = 3072 microseconds */
                   "BE_BlockAckInactivityTimeout", UintegerValue (3));
  NetDeviceContainer device_s3 = wifi.Install (wifiPhy, wifiMac, s3);


  wifiMac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid3),
               "BE_BlockAckThreshold", UintegerValue (0));
  device_s3 = wifi.Install (wifiPhy, wifiMac, s3);
  //AnimationInterface::UpdateNodeColor (s1, 12, 2, 3); 

  ////////////configuracion de Estacion 4 /////////////////////////////
  Ssid ssid4 ("My-network4");

  wifiMac.SetType ("ns3::AdhocWifiMac",
                   "QosSupported", BooleanValue (false),
                   "Ssid", SsidValue (ssid4),
  /* setting blockack threshold for sta's BE queue */
                   "BE_BlockAckThreshold", UintegerValue (2),
  /* setting block inactivity timeout to 3*1024 = 3072 microseconds */
                   "BE_BlockAckInactivityTimeout", UintegerValue (3));
  NetDeviceContainer device_s4 = wifi.Install (wifiPhy, wifiMac, s4);

  wifiMac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (false),
               "Ssid", SsidValue (ssid),
               "BE_BlockAckThreshold", UintegerValue (0));
  device_s4 = wifi.Install (wifiPhy, wifiMac, s4);



  //Ipv6 addresshelper:
  Ipv6AddressHelper ipv6;
  NS_LOG_INFO ("Assign IP Addresses IPv6.");
  ipv6.Assign(devices_n1);
  ipv6.Assign(device_s1);
  ipv6.Assign(device_s2);
  ipv6.Assign(device_s3);
  ipv6.Assign(device_s4);


  // Note that with FixedRssLossModel, the positions below are not
  // used for received signal strength.
  MobilityHelper mobility;
  ObjectFactory position;

  position.SetTypeId ("ns3::RandomRectanglePositionAllocator");
  position.Set ("X", StringValue ("ns3::UniformRandomVariable[Min=20|Max=70]"));
  position.Set ("Y", StringValue ("ns3::UniformRandomVariable[Min=20|Max=70]"));
  Ptr<PositionAllocator> PositionAlloc = position.Create ()->GetObject<PositionAllocator> ();
  mobility.SetMobilityModel ("ns3::RandomWaypointMobilityModel",
                                      "Speed", StringValue ("ns3::ExponentialRandomVariable[Mean=5]"),
                                      "Pause",StringValue ("ns3::ExponentialRandomVariable[Mean=5]"),
                                      "PositionAllocator", PointerValue (PositionAlloc));
                                        
  mobility.SetPositionAllocator (PositionAlloc);
  mobility.Install (n1);
  mobility.Install (s1);
  mobility.Install (s2);
  mobility.Install (s3);
  mobility.Install (s4);

  // Ipv4AddressHelper ipv4;
  // NS_LOG_INFO ("Assign IP Addresses.");
  // ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  // Ipv4InterfaceContainer i = ipv4.Assign (devices);

  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (s1, tid);
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));

  Ptr<Socket> source = Socket::CreateSocket (s2, tid);
  Inet6SocketAddress remote = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  source->SetAllowBroadcast (true);
  source->Connect (remote);

  // Tracing
  wifiPhy.EnablePcap ("taller1", devices_n1);

  // Output what we are doing
  NS_LOG_UNCOND ("Testing " << numPackets  << " packets sent with receiver rss " << rss );

  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (1.0), &GenerateTraffic,
                                  source, packetSize, numPackets, interPacketInterval);

  Simulator::Stop (Seconds (50.0));
  
  AnimationInterface anim ("taller1.xml");
  

  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
