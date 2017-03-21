/**
 * This is a simple example in order to show how 802.11e compressed block ack mechanism could be used.
 *
 * Network topology:
 * 
 *  Wifi 192.168.1.0
 * 
 *        AP
 *   *    *
 *   |    |
 *   n1   n2 
 *
 * In this example a QoS sta sends UDP datagram packets to access point. On the access point
 * there is no application installed so it replies to every packet with an ICMP frame. However
 * our attention is on originator sta n1. We have set blockAckThreshold (mininum number of packets to use
 * block ack) to 2 so if there are in the BestEffort queue more than 2 packets a block ack will be
 * negotiated. We also set a timeout for block ack inactivity to 3 blocks of 1024 microseconds. This timer is
 * reset when:
 *    - the originator receives a block ack frame.
 *    - the recipient receives a block ack request or a MPDU with ack policy Block Ack. 
 */
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Test-block-ack");

int main (int argc, char * argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);
  
  LogComponentEnable ("EdcaTxopN", LOG_LEVEL_DEBUG);
  LogComponentEnable ("BlockAckManager", LOG_LEVEL_INFO);
 
  Ptr<Node> sta = CreateObject<Node> ();
  Ptr<Node> ap = CreateObject<Node> ();

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  WifiMacHelper mac;
  /* disable fragmentation */
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager", "FragmentationThreshold", UintegerValue (2500));

  Ssid ssid ("My-network");

  mac.SetType ("ns3::StaWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid),
  /* setting blockack threshold for sta's BE queue */
               "BE_BlockAckThreshold", UintegerValue (2),
  /* setting block inactivity timeout to 3*1024 = 3072 microseconds */
               "BE_BlockAckInactivityTimeout", UintegerValue (3));
  NetDeviceContainer staDevice = wifi.Install (phy, mac, sta);

  mac.SetType ("ns3::ApWifiMac",
               "QosSupported", BooleanValue (true),
               "Ssid", SsidValue (ssid),
               "BE_BlockAckThreshold", UintegerValue (0));
  NetDeviceContainer apDevice = wifi.Install (phy, mac, ap);

  /* Setting mobility model */
  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (sta);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (ap);

  /* Internet stack*/
  InternetStackHelper stack;
  stack.SetIpv4StackInstall (false);
  stack.Install (sta);
  stack.Install (ap);

  Ipv6AddressHelper address;

  Ipv6InterfaceContainer staIf;
  Ipv6InterfaceContainer apIf;
  staIf = address.Assign (staDevice);
  apIf = address.Assign (apDevice);

  /* Setting applications */

  uint16_t port = 9;

  DataRate dataRate ("1Mb/s");
  OnOffHelper onOff ("ns3::UdpSocketFactory", Address (Inet6SocketAddress (apIf.GetAddress (0, 0), port)));
  onOff.SetAttribute ("DataRate", DataRateValue (dataRate));
  onOff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.01]"));
  onOff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=8]"));
  onOff.SetAttribute ("PacketSize", UintegerValue (50));

  ApplicationContainer staApps = onOff.Install (sta);

  staApps.Start (Seconds (1.0));
  staApps.Stop (Seconds (10.0));

  //TODO: Averiguar como hacer esto en ipv6
  //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  phy.EnablePcap ("test-blockack-2", ap->GetId (), 0);
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
