/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"

#include "GraphTools.h"
#include "GraphExploration.h"

#include "Attributes.h"
#include "coreNodes/Node_Pin.h"
#include "coreNodes/Node_Signal.h"
#include "coreNodes/Node_Register.h"
#include "supportNodes/Node_ExportOverride.h"
#include "supportNodes/Node_External.h"
#include "supportNodes/Node_RegSpawner.h"
#include "supportNodes/Node_RegHint.h"

#include "NodePort.h"
#include "Node.h"
#include "../simulation/ReferenceSimulator.h"

#include <set>

namespace gtry::hlim {

sim::DefaultBitVectorState evaluateStatically(Circuit &circuit, hlim::NodePort output)
{
	sim::SimulatorCallbacks ignoreCallbacks;
	sim::ReferenceSimulator simulator(false);
	simulator.compileStaticEvaluation(circuit, {output});
	simulator.powerOn();

	// Fetch result
	return simulator.getValueOfOutput(output);
}

Node_Pin *findInputPin(hlim::NodePort output)
{
	// Check if the driver is actually an input pin
	Node_Pin* res;
	if (res = dynamic_cast<Node_Pin*>(output.node))
		return res;

	// Otherwise follow signal nodes until an input pin is found.

	// The driver must be a signal node
	HCL_DESIGNCHECK(output.node != nullptr);
	if (dynamic_cast<const Node_Signal*>(output.node) == nullptr)
		return nullptr;

	utils::UnstableSet<BaseNode*> encounteredNodes;

	// Any preceding node must be a signal node or the pin we look for
	for (auto nh : output.node->exploreInput(0)) {
		if (encounteredNodes.contains(nh.node())) {
			nh.backtrack();
			continue;
		}
		encounteredNodes.insert(nh.node());
		if (res = dynamic_cast<Node_Pin*>(nh.node()))
			return res;
		else // If we hit s.th. that is neither pin nor signal, there is nothing we can do.
			if (!nh.isSignal())
				return nullptr;
	}
	return nullptr;
}


Node_Pin *findOutputPin(NodePort output)
{
	// Explore the local graph, traveling along all signal nodes to find any
	// output pin that is driven by whatever drives (directly or indirectly) output. 
	// All such output pins (if there are multiple) receive the same signal and
	// are thus equivalent, so we can just pick any.

	HCL_DESIGNCHECK(output.node != nullptr);
	// First: Find the non-signal driver that drives output:
	NodePort driver;
	if (dynamic_cast<const Node_Signal*>(output.node) == nullptr)
		driver = output;
	else
		driver = output.node->getNonSignalDriver(0);

	// Second: From there on explore all nodes driven directly or via signal nodes.
	for (auto nh : driver.node->exploreOutput(driver.port)) {
		Node_Pin* res;
		if (res = dynamic_cast<Node_Pin*>(nh.node()))
			return res;
		else
			if (!nh.isSignal())
				nh.backtrack();
	}

	return nullptr;
}

Clock* findFirstOutputClock(NodePort output)
{
	Clock* clockFound = nullptr;
	utils::UnstableSet<BaseNode*> alreadySeen;
	alreadySeen.insert(output.node);
	for (auto nh : output.node->exploreOutput(output.port)) {
		if (alreadySeen.contains(nh.node())) {
			nh.backtrack();
			continue;
		}

		alreadySeen.insert(nh.node());

		if (auto* reg = dynamic_cast<Node_Register*>(nh.node())) {
			if (clockFound == nullptr)
				clockFound = reg->getClocks()[0];
			else
				if (clockFound != reg->getClocks()[0])
					return nullptr;
			nh.backtrack();
		}
		else if (nh.isNodeType<Node_External>()) {
			nh.backtrack();
		}
	}
	return clockFound;
}

Clock* findFirstInputClock(NodePort input)
{
	Clock* clockFound = nullptr;
	for (auto nh : input.node->exploreInput(input.port).skipExportOnly().skipDependencies()) {
		if (auto* reg = dynamic_cast<Node_Register*>(nh.node())) {
			if (clockFound == nullptr)
				clockFound = reg->getClocks()[0];
			else
				if (clockFound != reg->getClocks()[0])
					return nullptr;
			nh.backtrack();
		}
		else if (nh.isNodeType<Node_External>()) {
			nh.backtrack();
		}
	}
	return clockFound;
}

std::vector<Node_Register*> findAllOutputRegisters(NodePort output)
{
	std::vector<Node_Register*> result;
	utils::UnstableSet<NodePort> alreadyHandled;
	for (auto nh : output.node->exploreOutput(output.port).skipDependencies()) {
		if(alreadyHandled.contains(nh.nodePort())) {
			nh.backtrack();
			continue;
		}
		if (auto* reg = dynamic_cast<Node_Register*>(nh.node())) {
			result.push_back(reg);
			nh.backtrack();
		}
		else if (nh.isNodeType<Node_External>()) {
			nh.backtrack();
		}
		alreadyHandled.insert(nh.nodePort());
	}
	return result;
}

std::vector<Node_Register*> findAllInputRegisters(NodePort input)
{
	std::vector<Node_Register*> result;
	utils::UnstableSet<NodePort> alreadyHandled;
	for (auto nh : input.node->exploreInput(input.port).skipExportOnly().skipDependencies()) {
		if(alreadyHandled.contains(nh.nodePort())) {
			nh.backtrack();
			continue;
		}
		if (auto* reg = dynamic_cast<Node_Register*>(nh.node())) {
			result.push_back(reg);
			nh.backtrack();
		}
		else if (nh.isNodeType<Node_External>()) {
			nh.backtrack();
		}
		alreadyHandled.insert(nh.nodePort());
	}
	return result;
}



void reccurFindRegistersAffectedByReset(std::vector<Node_Register*> &regs, Clock *clock)
{
	if (clock->getRegAttribs().resetType != RegisterAttributes::ResetType::NONE)
		for (auto node : clock->getClockedNodes())
			if (auto *reg = dynamic_cast<Node_Register*>(node.node))
				regs.push_back(reg);

	for (auto derivedClock : clock->getDerivedClocks())
		if (derivedClock->inheritsResetPinSource())
			reccurFindRegistersAffectedByReset(regs, derivedClock);
}



std::vector<Node_Register*> findRegistersAffectedByReset(Clock *clock)
{
	std::vector<Node_Register*> result;
	reccurFindRegistersAffectedByReset(result, clock);
	return result;
}



std::vector<std::pair<size_t, Node_RegHint*>> getRegHintDistanceToSpawners(std::span<Node_RegSpawner*> spawners, const Subnet &subnet)
{
	DijkstraExploreNodesForward forwardExploration;

	// Start from spawners
	for (auto spawner : spawners)
		forwardExploration.addAllOutputPorts(spawner);

	std::vector<std::pair<size_t, Node_RegHint*>> result;

	for (auto handle : forwardExploration) {
		// Stay inside subnet
		if (!subnet.contains(handle.node())) {
			handle.skip();
			continue;
		}

		if (auto *regHint = dynamic_cast<Node_RegHint*>(handle.node())) {
			// Add it to the result list, using distance to count the number of Node_RegHint encountered
			result.emplace_back(handle.getDistance(), regHint);
			// Proceed with a cost of 1 to increase "distance" for everything that is found through this node
			handle.proceed(1);
		}
	}

	return result;
}   


size_t getMinRegsBetween(NodePort sourceOutput, NodePort destinationInput)
{
	DijkstraExploreNodesForward forwardExploration;

	forwardExploration.addOutputPort(sourceOutput);

	for (auto handle : forwardExploration) {
		// Return cumulative distance if reached destination.
		if (handle.input() == destinationInput)
			return handle.getDistance();

		if (dynamic_cast<Node_Register*>(handle.node()))
			// Proceed with a cost of 1 to increase "distance" for everything that is found through this node
			handle.proceed(1);
	}

	// No connection found, return max
	return ~0ull;
}

size_t getMinRegHintsBetween(NodePort sourceOutput, NodePort destinationInput)
{
	DijkstraExploreNodesForward forwardExploration;

	forwardExploration.addOutputPort(sourceOutput);

	for (auto handle : forwardExploration) {
		// Return cumulative distance if reached destination.
		if (handle.input() == destinationInput)
			return handle.getDistance();

		if (dynamic_cast<Node_RegHint*>(handle.node()))
			// Proceed with a cost of 1 to increase "distance" for everything that is found through this node
			handle.proceed(1);
	}

	// No connection found, return max
	return ~0ull;
}


hlim::NodePort findDriver(hlim::BaseNode *node, const FindDriverOpts &opts)
{
	utils::UnstableSet<hlim::NodePort> visited;

	auto driver = node->getDriver(opts.inputPortIdx);
	while (driver.node != nullptr) {
		if (visited.contains(driver)) return {};
		visited.insert(driver);

		if (opts.skipSignalNodes && dynamic_cast<Node_Signal*>(driver.node) && (opts.skipNamedSignalNodes || !node->hasGivenName()))
			driver = driver.node->getDriver(0);
		else if (opts.skipExportOverrideNodes && dynamic_cast<Node_ExportOverride*>(driver.node))
			driver = driver.node->getDriver(*opts.skipExportOverrideNodes);
		else 
			break;
	}
	return driver;
}

}
