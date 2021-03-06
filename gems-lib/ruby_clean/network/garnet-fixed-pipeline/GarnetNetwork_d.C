/*
    Copyright (C) 1999-2008 by Mark D. Hill and David A. Wood for the
    Wisconsin Multifacet Project.  Contact: gems@cs.wisc.edu
    http://www.cs.wisc.edu/gems/

    --------------------------------------------------------------------

    This file is part of Garnet (Princeton's interconnect model),
    a component of the Multifacet GEMS (General Execution-driven 
    Multiprocessor Simulator) software toolset originally developed at 
    the University of Wisconsin-Madison.

    Garnet was developed by Niket Agarwal at Princeton University. Orion was
    developed by Princeton University.

    Substantial further development of Multifacet GEMS at the
    University of Wisconsin was performed by Alaa Alameldeen, Brad
    Beckmann, Jayaram Bobba, Ross Dickson, Dan Gibson, Pacia Harper,
    Derek Hower, Milo Martin, Michael Marty, Carl Mauer, Michelle Moravan,
    Kevin Moore, Andrew Phelps, Manoj Plakal, Daniel Sorin, Haris Volos, 
    Min Xu, and Luke Yen.
    --------------------------------------------------------------------

    If your use of this software contributes to a published paper, we
    request that you (1) cite our summary paper that appears on our
    website (http://www.cs.wisc.edu/gems/) and (2) e-mail a citation
    for your published paper to gems@cs.wisc.edu.

    If you redistribute derivatives of this software, we request that
    you notify us and either (1) ask people to register with us at our
    website (http://www.cs.wisc.edu/gems/) or (2) collect registration
    information and periodically send it to us.

    --------------------------------------------------------------------

    Multifacet GEMS is free software; you can redistribute it and/or
    modify it under the terms of version 2 of the GNU General Public
    License as published by the Free Software Foundation.

    Multifacet GEMS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the Multifacet GEMS; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
    02111-1307, USA

    The GNU General Public License is contained in the file LICENSE.

### END HEADER ###
*/
/*
 * GarnetNetwork_d.C
 *
 * Niket Agarwal, Princeton University
 *
 * */

#include "GarnetNetwork_d.h"
#include "MachineType.h"
#include "NetworkInterface_d.h"
#include "MessageBuffer.h"
#include "Router_d.h"
#include "Topology.h"	
#include "NetworkLink_d.h"
#include "CreditLink_d.h"
#include "NetDest.h"

GarnetNetwork_d::GarnetNetwork_d(int nodes)
{
	m_nodes = MachineType_base_number(MachineType_NUM); // Total nodes in network
	m_virtual_networks = NUMBER_OF_VIRTUAL_NETWORKS; // Number of virtual networks = number of message classes in the coherence protocol
	m_ruby_start = 0;
	m_flits_recieved = 0;
	m_flits_injected = 0;
	m_network_latency = 0.0;
	m_queueing_latency = 0.0;

	m_router_ptr_vector.clear();

	// Allocate to and from queues
	m_toNetQueues.setSize(m_nodes); 	// Queues that are getting messages from protocol
	m_fromNetQueues.setSize(m_nodes); 	// Queues that are feeding the protocol
	m_in_use.setSize(m_virtual_networks);
  	m_ordered.setSize(m_virtual_networks);
    	for (int i = 0; i < m_virtual_networks; i++) 
	{
		m_in_use[i] = false;
		m_ordered[i] = false;
	}

	for (int node = 0; node < m_nodes; node++) 
	{
		//Setting how many vitual message buffers will there be per Network Queue
		m_toNetQueues[node].setSize(m_virtual_networks);
		m_fromNetQueues[node].setSize(m_virtual_networks);

		for (int j = 0; j < m_virtual_networks; j++) 
		{ 
			m_toNetQueues[node][j] = new MessageBuffer();	// Instantiating the Message Buffers that interact with the coherence protocol
			m_fromNetQueues[node][j] = new MessageBuffer();
		}
	}

	// Setup the network switches
	m_topology_ptr = new Topology(this, m_nodes);
	
	int number_of_routers = m_topology_ptr->numSwitches();
	for (int i=0; i<number_of_routers; i++) {
		m_router_ptr_vector.insertAtBottom(new Router_d(i, this));
	}
	
	for (int i=0; i < m_nodes; i++) {
		NetworkInterface_d *ni = new NetworkInterface_d(i, m_virtual_networks, this);
		ni->addNode(m_toNetQueues[i], m_fromNetQueues[i]);
		m_ni_ptr_vector.insertAtBottom(ni);
	}
	m_topology_ptr->createLinks(false);  // false because this isn't a reconfiguration
	for(int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		m_router_ptr_vector[i]->init();
	}
}

GarnetNetwork_d::~GarnetNetwork_d()
{
	for (int i = 0; i < m_nodes; i++) 
	{
		m_toNetQueues[i].deletePointers();
		m_fromNetQueues[i].deletePointers();
	}
	m_router_ptr_vector.deletePointers();
	m_ni_ptr_vector.deletePointers();
	m_link_ptr_vector.deletePointers();
	m_creditlink_ptr_vector.deletePointers();
	delete m_topology_ptr;
}

void GarnetNetwork_d::reset()
{
	for (int node = 0; node < m_nodes; node++) 
	{
		for (int j = 0; j < m_virtual_networks; j++) 
		{
			m_toNetQueues[node][j]->clear();
			m_fromNetQueues[node][j]->clear();
		}
	}
}

/* 
 * This function creates a link from the Network Interface (NI) into the Network. 
 * It creates a Network Link from the NI to a Router and a Credit Link from  
 * the Router to the NI
*/

void GarnetNetwork_d::makeInLink(NodeID src, SwitchID dest, const NetDest& routing_table_entry, int link_latency, int bw_multiplier, bool isReconfiguration)
{
	assert(src < m_nodes);
	
	if(!isReconfiguration)
	{	
		NetworkLink_d *net_link = new NetworkLink_d(m_link_ptr_vector.size(), link_latency, this);
		CreditLink_d *credit_link = new CreditLink_d(m_creditlink_ptr_vector.size());
		m_link_ptr_vector.insertAtBottom(net_link);
		m_creditlink_ptr_vector.insertAtBottom(credit_link);

		m_router_ptr_vector[dest]->addInPort(net_link, credit_link);
		m_ni_ptr_vector[src]->addOutPort(net_link, credit_link);
	}
	else 
	{
		ERROR_MSG("Fatal Error:: Reconfiguration not allowed here");
		// do nothing
	}
}

/* 
 * This function creates a link from the Network to a NI. 
 * It creates a Network Link from a Router to the NI and 
 * a Credit Link from NI to the Router 
*/

void GarnetNetwork_d::makeOutLink(SwitchID src, NodeID dest, const NetDest& routing_table_entry, int link_latency, int link_weight, int bw_multiplier, bool isReconfiguration)
{
	assert(dest < m_nodes);
	assert(src < m_router_ptr_vector.size());
	assert(m_router_ptr_vector[src] != NULL);
	
	if(!isReconfiguration)
	{
		NetworkLink_d *net_link = new NetworkLink_d(m_link_ptr_vector.size(), link_latency, this);
		CreditLink_d *credit_link = new CreditLink_d(m_creditlink_ptr_vector.size());
		m_link_ptr_vector.insertAtBottom(net_link);
		m_creditlink_ptr_vector.insertAtBottom(credit_link);

		m_router_ptr_vector[src]->addOutPort(net_link, routing_table_entry, link_weight, credit_link);
		m_ni_ptr_vector[dest]->addInPort(net_link, credit_link);
	} 
	else 
	{
		ERROR_MSG("Fatal Error:: Reconfiguration not allowed here");
		//do nothing
	}
}

/* 
 * This function creates a internal network links 
*/

void GarnetNetwork_d::makeInternalLink(SwitchID src, SwitchID dest, const NetDest& routing_table_entry, int link_latency, int link_weight, int bw_multiplier, bool isReconfiguration)
{
	if(!isReconfiguration)
	{
		NetworkLink_d *net_link = new NetworkLink_d(m_link_ptr_vector.size(), link_latency, this);
		CreditLink_d *credit_link = new CreditLink_d(m_creditlink_ptr_vector.size());
		m_link_ptr_vector.insertAtBottom(net_link);
		m_creditlink_ptr_vector.insertAtBottom(credit_link);

		m_router_ptr_vector[dest]->addInPort(net_link, credit_link);
		m_router_ptr_vector[src]->addOutPort(net_link, routing_table_entry, link_weight, credit_link);
	}
	else
	{	
		ERROR_MSG("Fatal Error:: Reconfiguration not allowed here");
		// do nothing
	}
}

void GarnetNetwork_d::checkNetworkAllocation(NodeID id, bool ordered, int network_num)
{
	ASSERT(id < m_nodes);
	ASSERT(network_num < m_virtual_networks);

	if (ordered) 
	{
		m_ordered[network_num] = true;
	}
	m_in_use[network_num] = true;
}

MessageBuffer* GarnetNetwork_d::getToNetQueue(NodeID id, bool ordered, int network_num)
{
	checkNetworkAllocation(id, ordered, network_num);
	return m_toNetQueues[id][network_num];
}

MessageBuffer* GarnetNetwork_d::getFromNetQueue(NodeID id, bool ordered, int network_num)
{
	checkNetworkAllocation(id, ordered, network_num);
	return m_fromNetQueues[id][network_num];
}

void GarnetNetwork_d::clearStats()
{
	m_ruby_start = g_eventQueue_ptr->getTime();
}

Time GarnetNetwork_d::getRubyStartTime()
{
	return m_ruby_start;
}

void GarnetNetwork_d::printStats(ostream& out) const
{	double average_link_utilization = 0;
	Vector<double > average_vc_load;
	average_vc_load.setSize(m_virtual_networks*NetworkConfig::getVCsPerClass());	

	for(int i = 0; i < m_virtual_networks*NetworkConfig::getVCsPerClass(); i++)
	{
		average_vc_load[i] = 0;
	}

	out << endl;
	out << "Network Stats" << endl;
	out << "-------------" << endl;
	out << endl;
	for(int i = 0; i < m_link_ptr_vector.size(); i++) 
	{
		average_link_utilization += (double(m_link_ptr_vector[i]->getLinkUtilization())) / (double(g_eventQueue_ptr->getTime()-m_ruby_start));

		Vector<int > vc_load = m_link_ptr_vector[i]->getVcLoad();
		for(int j = 0; j < vc_load.size(); j++)
		{
			assert(vc_load.size() == NetworkConfig::getVCsPerClass()*m_virtual_networks);
			average_vc_load[j] += vc_load[j];
		}
	}
	average_link_utilization = average_link_utilization/m_link_ptr_vector.size();
	out << "Average Link Utilization :: " << average_link_utilization << " flits/cycle" << endl;
	out << "-------------" << endl;

	for(int i = 0; i < NetworkConfig::getVCsPerClass()*NUMBER_OF_VIRTUAL_NETWORKS; i++)
	{
		average_vc_load[i] = (double(average_vc_load[i]) / (double(g_eventQueue_ptr->getTime()) - m_ruby_start));
		out << "Average VC Load [" << i << "] = " << average_vc_load[i] << " flits/cycle " << endl;
	}
	out << "-------------" << endl;

	//	out << "Total flits injected = " << m_flits_injected << endl;
	//	out << "Total flits recieved = " << m_flits_recieved << endl;
	out << "Average network latency = " << ((double) m_network_latency/ (double) m_flits_recieved)<< endl;
	//	out << "Average queueing latency = " << ((double) m_queueing_latency/ (double) m_flits_recieved)<< endl;
	//	out << "Average latency = " << ((double)  (m_queueing_latency + m_network_latency) / (double) m_flits_recieved)<< endl;
	out << "-------------" << endl;

	double m_total_link_power = 0.0;
	double m_total_router_power = 0.0;

	for(int i = 0; i < m_link_ptr_vector.size(); i++)
	{
		m_total_link_power += m_link_ptr_vector[i]->calculate_power();
	}

	for(int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		m_total_router_power += m_router_ptr_vector[i]->calculate_power();
	}
	out << "Total Link Power = " << m_total_link_power << " W " << endl;
	out << "Total Router Power = " << m_total_router_power << " W " <<endl;
	out << "-------------" << endl;
}

void GarnetNetwork_d::printConfig(ostream& out) const
{
	out << endl;
	out << "Network Configuration" << endl;
	out << "---------------------" << endl;
	out << "network: GarnetNetwork_d" << endl;
	out << "topology: " << g_NETWORK_TOPOLOGY << endl;
	out << endl;

	for (int i = 0; i < m_virtual_networks; i++) 
	{
		out << "virtual_net_" << i << ": ";
		if (m_in_use[i]) 
		{
			out << "active, ";
			if (m_ordered[i]) 
			{
				out << "ordered" << endl;
			} 
			else 
			{
				out << "unordered" << endl;
			}
		} 
		else 
		{
			out << "inactive" << endl;
		}
	}
  	out << endl;

	for(int i = 0; i < m_ni_ptr_vector.size(); i++)
	{
		m_ni_ptr_vector[i]->printConfig(out);
	}
	for(int i = 0; i < m_router_ptr_vector.size(); i++)
	{
		m_router_ptr_vector[i]->printConfig(out);
	}	
	if (g_PRINT_TOPOLOGY) 
	{
		m_topology_ptr->printConfig(out);
	}
}

void GarnetNetwork_d::print(ostream& out) const
{
	out << "[GarnetNetwork_d]";
}
