/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2016 NITK Surathkal
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
 * Authors: Ankit Deepak <adadeepak8@gmail.com>
 *          Shravya Ks <shravya.ks0@gmail.com>,
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 */

/*
 * PORT NOTE: This code was ported from ns-3 IPv4 implementation  (src/olsr).  Almost all
 * comments have also been ported from the same
 */


//
// This script, adapted from examples/wireless/wifi-simple-adhoc illustrates
// the use of OLSR6 HNA.
//
// Network Topology:
//
//             |-OLSR6(2001:1::/64)-|  |-non-OLSR6(2001:2::/64)-|
//           A ))))            (((( B ------------------- C
//
// Node A needs to send a UDP packet to node C. This can be done only after
// A receives an HNA message from B, in which B announces 2001:2""/64
// as an associated network.
//
// If no HNA message is generated by B, a will not be able to form a route to C.
// This can be verified as follows:
//
// ./waf --run olsr6-hna
//
// There are two ways to make a node to generate HNA messages.
//
// One way is to use olsr6::RoutingProtocol::SetRoutingTableAssociation ()
// to use which you may run:
//
// ./waf --run "olsr6-hna --assocMethod1=1"
//
// The other way is to use olsr6::RoutingProtocol::AddHostNetworkAssociation ()
// to use which you may run:
//
// ./waf --run "olsr6-hna --assocMethod2=1"
//
#include "ns3/ipv6-static-routing-helper.h"


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/config-store-module.h"
#include "ns3/wifi-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/olsr6-routing-protocol.h"
#include "ns3/olsr6-helper.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>





#include "ns3/internet-apps-module.h"
#include "ns3/ipv6-static-routing-helper.h"

#include "ns3/ipv6-routing-table-entry.h"



using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("Olsr6Hna");

void ReceivePacket (Ptr<Socket> socket)
{
  NS_LOG_UNCOND ("Received one packet!");
}



class StackHelper
{
public:

  /**
   * \brief Add an address to a IPv6 node.
   * \param n node
   * \param interface interface index
   * \param address IPv6 address to add
   */
  inline void AddAddress (Ptr<Node>& n, uint32_t interface, Ipv6Address address)
  {
    Ptr<Ipv6> ipv6 = n->GetObject<Ipv6> ();
    ipv6->AddAddress (interface, address);
  }

  /**
   * \brief Print the routing table.
   * \param n the node
   */
  inline void PrintRoutingTable (Ptr<Node>& n)
  {
    Ptr<Ipv6StaticRouting> routing = 0;
    Ipv6StaticRoutingHelper routingHelper;
    Ptr<Ipv6> ipv6 = n->GetObject<Ipv6> ();
    uint32_t nbRoutes = 0;
    Ipv6RoutingTableEntry route;

    routing = routingHelper.GetStaticRouting (ipv6);

    std::cout << "Routing table of " << n << " : " << std::endl;
    std::cout << "Destination\t\t\t\t" << "Gateway\t\t\t\t\t" << "Interface\t" <<  "Prefix to use" << std::endl;

    nbRoutes = routing->GetNRoutes ();
    for (uint32_t i = 0; i < nbRoutes; i++)
      {
        route = routing->GetRoute (i);
        std::cout << route.GetDest () << "\t"
                  << route.GetGateway () << "\t"
                  << route.GetInterface () << "\t"
                  << route.GetPrefixToUse () << "\t"
                  << std::endl;
      }
  }
};


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
  uint32_t numPackets = 1;
  double interval = 1.0; // seconds
  bool verbose = false;
  bool assocMethod1 = false;
  bool assocMethod2 = false;

  CommandLine cmd;

  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.AddValue ("rss", "received signal strength", rss);
  cmd.AddValue ("packetSize", "size of application packet sent", packetSize);
  cmd.AddValue ("numPackets", "number of packets generated", numPackets);
  cmd.AddValue ("interval", "interval (seconds) between packets", interval);
  cmd.AddValue ("verbose", "turn on all WifiNetDevice log components", verbose);
  cmd.AddValue ("assocMethod1", "Use SetRoutingTableAssociation () method", assocMethod1);
  cmd.AddValue ("assocMethod2", "Use AddHostNetworkAssociation () method", assocMethod2);

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

  NodeContainer olsr6Nodes;
  olsr6Nodes.Create (3);

  //
 //Ptr<Node> s1 = CreateObject<Node> ();
  //
//  NodeContainer csmaNodes;
  //csmaNodes.Create (1);




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
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, olsr6Nodes);


  /*

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate (5000000)));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
  NetDeviceContainer csmaDevices = csma.Install (NodeContainer (csmaNodes.Get (0), olsr6Nodes.Get (1)));
*/


  // Note that with FixedRssLossModel, the positions below are not
  // used for received signal strength.

  /////////////////////////////////////////////////////////////////////////////
  /*

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();
  positionAlloc->Add (Vector (0.0, 0.0, 0.0));
  positionAlloc->Add (Vector (5.0, 0.0, 0.0));
  mobility.SetPositionAllocator (positionAlloc);
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (olsr6Nodes);
  */
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
    mobility.Install (olsr6Nodes);

  //////////////////////////////////////////////////////////////////////
  

  Olsr6Helper olsr6;

  // Specify Node B's csma device as a non-OLSR6 device.

  //olsr6.ExcludeInterface (olsr6Nodes.Get (1), 2);

  /////

  Ipv6StaticRoutingHelper staticRouting;

  Ipv6ListRoutingHelper list;
  list.Add (staticRouting, 0);
  list.Add (olsr6, 10);

  InternetStackHelper internet_olsr6;
  internet_olsr6.SetRoutingHelper (list); // has effect on the next Install ()
  internet_olsr6.Install (olsr6Nodes);

  


  /////////////////////////////////////////////

  ////////////////////////////////////////


  /*
  InternetStackHelper internet_csma;
  internet_csma.Install (csmaNodes);
  */


  Ipv6AddressHelper ipv6;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv6.SetBase ("2001:0:1::",Ipv6Prefix (64));
  ipv6.Assign (devices);




  //////////////////////////////////////////////////////////////////////////////////////
  /*

  ipv6.SetBase ("2001:0:2::", Ipv6Prefix (64));
  ipv6.Assign (csmaDevices);
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (csmaNodes.Get (0), tid);
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
  */
  
  TypeId tid = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket (olsr6Nodes.Get (1), tid);
  //Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  Inet6SocketAddress local = Inet6SocketAddress (Ipv6Address::GetAny (), 80);
  recvSink->Bind (local);
  recvSink->SetRecvCallback (MakeCallback (&ReceivePacket));
  


  ////////////////////////////////////////////////////////////////////////////////////////////////////////

  Ptr<Socket> source = Socket::CreateSocket (olsr6Nodes.Get (0), tid);
  //Inet6SocketAddress remote = Inet6SocketAddress (Ipv6Address ("2001:0:2:0:200:ff:fe00:1"), 80);
  Inet6SocketAddress remote = Inet6SocketAddress (Ipv6Address ("2001:0:2:0:200:ff:fe00:1"), 80);
  
  source->Connect (remote);



  // Obtain olsr::RoutingProtocol instance of gateway node
  // (namely, node B) and add the required association

  Ptr<Ipv6> stack = olsr6Nodes.Get (1)->GetObject<Ipv6> ();
  Ptr<Ipv6RoutingProtocol> rp_Gw = (stack->GetRoutingProtocol ());
  Ptr<Ipv6ListRouting> lrp_Gw = DynamicCast<Ipv6ListRouting> (rp_Gw);

  Ptr<olsr6::RoutingProtocol> olsrrp_Gw;





  for (uint32_t i = 0; i < lrp_Gw->GetNRoutingProtocols ();  i++)
    {
      int16_t priority;
      Ptr<Ipv6RoutingProtocol> temp = lrp_Gw->GetRoutingProtocol (i, priority);
      if (DynamicCast<olsr6::RoutingProtocol> (temp))
        {
          olsrrp_Gw = DynamicCast<olsr6::RoutingProtocol> (temp);
        }
    }

  if (assocMethod1)
    {
      // Create a special Ipv6StaticRouting instance for RoutingTableAssociation
      // Even the Ipv6StaticRouting instance added to list may be used
      Ptr<Ipv6StaticRouting> hnaEntries = Create<Ipv6StaticRouting> ();

      // Add the required routes into the Ipv6StaticRouting Protocol instance
      // and have the node generate HNA messages for all these routes
      // which are associated with non-OLSR interfaces specified above.
      hnaEntries->AddNetworkRouteTo (Ipv6Address ("2001:0:2::"), Ipv6Prefix (64), uint32_t (2), uint32_t (1));
      olsrrp_Gw->SetRoutingTableAssociation (hnaEntries);
    }

  if (assocMethod2)
    {
      // Specify the required associations directly.
      olsrrp_Gw->AddHostNetworkAssociation (Ipv6Address ("2001:0:2::"), Ipv6Prefix (64));
    }










    std::cout<<typeid(olsr6Nodes.Get (0)).name()<<"\t"<< "A" <<"\n";





  // Tracing



   wifiPhy.EnablePcap ("olsr-hna", devices);

  //////////////////////
  //csma.EnablePcap ("olsr-hna", csmaDevices, false);
////////////////////
   
  Simulator::ScheduleWithContext (source->GetNode ()->GetId (),
                                  Seconds (15.0), &GenerateTraffic,
                                  source, packetSize, numPackets, interPacketInterval);

  
  Simulator::Stop (Seconds (20.0));
  Simulator::Run ();
  Simulator::Destroy ();

  return 0;
}
