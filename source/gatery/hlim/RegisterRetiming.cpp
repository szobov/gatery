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

#include "RegisterRetiming.h"

#include "Circuit.h"
#include "Subnet.h"
#include "SignalDelay.h"
#include "Node.h"
#include "NodePort.h"
#include "coreNodes/Node_Register.h"
#include "coreNodes/Node_Constant.h"
#include "coreNodes/Node_Signal.h"
#include "supportNodes/Node_Memory.h"
#include "supportNodes/Node_MemPort.h"

#include "../simulation/ReferenceSimulator.h"


#include "../export/DotExport.h"

#include <sstream>
#include <iostream>

#define DEBUG_OUTPUT

namespace gtry::hlim {

/**
 * @brief Determines the exact area to be forward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedForward(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<BaseNode*> openList;
	openList.push_back(output.node);


#ifdef DEBUG_OUTPUT
    auto writeSubnet = [&]{
		Subnet subnet = areaToBeRetimed;
		subnet.dilate(true, true);

        DotExport exp("retiming_area.dot");
        exp(circuit, subnet.asConst());
        exp.runGraphViz("retiming_area.svg");
    };
#endif

	while (!openList.empty()) {
		auto *node = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(node)) continue;
		if (registersToBeRemoved.contains((Node_Register*)node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals leave the specified operation area through node " << node->getName() << " (" << node->getTypeName() 
				<< ") without passing a register that can be retimed forward. Note that registers with enable signals can't be retimed yet.\n"
				<< "First node outside the operation area from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}

		// We may not want to retime nodes to which references are still being held
		if (node->hasRef() && !ignoreRefs) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node to which references are still being held " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
				<< "Node with references from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}			

		// Check that everything is using the same clock.
		for (auto *c : node->getClocks()) {
			if (c != nullptr) {
				if (clock == nullptr) {
					clock = c;
					clockGivingNode = node;
				} else {
					if (clock != c) {
						if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

						std::stringstream error;

						error 
							<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
							<< "Node from:\n" << output.node->getStackTrace() << "\n";

						error 
							<< "The fanning-in signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << node->getName() << " (" << node->getTypeName() << ").\n"
							<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
							<< "Second node from:\n" << node->getStackTrace() << "\n";

						HCL_ASSERT_HINT(false, error.str());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		// Memory ports are handled separately below
		if (node->hasSideEffects() && dynamic_cast<Node_MemPort*>(node) == nullptr) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime forward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-in signals are driven by a node with side effects " << node->getName() << " (" << node->getTypeName() << ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		

		// Everything seems good with this node, so proceeed

		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling
			if (registersToBeRemoved.contains(reg)) continue;

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (anchoredRegisters.contains(reg) || reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr) {
				// Retime over this register. This means the enable port is part of the fan-in and we also need to search it for a register.
				areaToBeRetimed.add(node);
				for (unsigned i : {Node_Register::DATA, Node_Register::ENABLE}) {
					auto driver = reg->getDriver(i);
					if (driver.node != nullptr)
						openList.push_back(driver.node);
				}
			} else {
				// Found a register to retime forward, stop here.
				registersToBeRemoved.insert(reg);

				// It is important to not add the register to the area to be retimed!
				// If the register is part of a loop that is part of the retiming area, 
				// the input to the register effectively leaves the retiming area, thus forcing the placement
				// of a new register with a new reset value. The old register is bypassed thus
				// replacing the old register with the new register.
				// In other words, for registers that are completely embeddded in the retiming area,
				// this mechanism implicitely advances the reset value by one iteration which is necessary 
				// because we can not retime a register out of the reset-pin-path.
			}
		} else {
			// Regular nodes just get added to the retiming area and their inputs are further explored
			areaToBeRetimed.add(node);
			for (unsigned i : utils::Range(node->getNumInputPorts())) {
				auto driver = node->getDriver(i);
				if (driver.node != nullptr)
					openList.push_back(driver.node);
			}

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port (can only be a read port, attempt to retime entire memory)
				auto *memory = memPort->getMemory();
				areaToBeRetimed.add(memory);
			
				// add all memory ports to open list
				for (auto np : memory->getDirectlyDriven(0)) 
					openList.push_back(np.node);
			 }
		}
	}

	return true;
}

bool retimeForwardToOutput(Circuit &circuit, Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, bool ignoreRefs, bool failureIsError)
{
	Subnet areaToBeRetimed;
	std::set<Node_Register*> registersToBeRemoved;
	if (!determineAreaToBeRetimedForward(circuit, area, anchoredRegisters, output, areaToBeRetimed, registersToBeRemoved, ignoreRefs, failureIsError))
		return false;

	/*
	{
		std::array<const BaseNode*,1> arr{output.node};
		ConstSubnet csub = ConstSubnet::allNecessaryForNodes({}, arr);
		csub.add(output.node);

		DotExport exp("DriversOfOutput.dot");
		exp(circuit, csub);
		exp.runGraphViz("DriversOfOutput.svg");
	}
	{
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, areaToBeRetimed.asConst());
		exp.runGraphViz("areaToBeRetimed.svg");
	}
	*/

	std::set<hlim::NodePort> outputsLeavingRetimingArea;
	// Find every output leaving the area
	for (auto n : areaToBeRetimed)
		for (auto i : utils::Range(n->getNumOutputPorts()))
			for (auto np : n->getDirectlyDriven(i))
				if (!areaToBeRetimed.contains(np.node)) {
					outputsLeavingRetimingArea.insert({.node = n, .port = i});
					break;
				}

	if (registersToBeRemoved.empty()) // no registers found to retime, probably everything is constant, so no clock available
		return false;

	HCL_ASSERT(!registersToBeRemoved.empty());
	auto *clock = (*registersToBeRemoved.begin())->getClocks()[0];

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileStaticEvaluation(circuit, {outputsLeavingRetimingArea});
    simulator.powerOn();

	// Insert registers
	for (auto np : outputsLeavingRetimingArea) {
		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clock);
		// Setup input data
		reg->connectInput(Node_Register::DATA, np);
		// add to the node group of its new driver
		reg->moveToGroup(np.node->getGroup());
		
		area.add(reg);


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}

		// Find all signals leaving the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (!areaToBeRetimed.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}

	// Bypass input registers for the retimed nodes
	for (auto *reg : registersToBeRemoved) {
		const auto &allDriven = reg->getDirectlyDriven(0);
		for (int i = (int)allDriven.size()-1; i >= 0; i--) {
			if (areaToBeRetimed.contains(allDriven[i].node)) {
				allDriven[i].node->rewireInput(allDriven[i].port, reg->getDriver((unsigned)Node_Register::DATA));
			}	
		}
	}

	return true;
}



void retimeForward(Circuit &circuit, Subnet &subnet)
{
	std::set<Node_Register*> anchoredRegisters;

	// Anchor all registers driven by memory
	for (auto &n : subnet)
		if (auto *reg = dynamic_cast<Node_Register*>(n)) {
			bool drivenByMemory = false;
			for (auto nh : reg->exploreInput((unsigned)Node_Register::Input::DATA)) {
				if (nh.isNodeType<Node_MemPort>()) {
					drivenByMemory = true;
					break;
				}
				if (!nh.node()->isCombinatorial()) {
					nh.backtrack();
					continue;
				}
			}
			if (drivenByMemory)
				anchoredRegisters.insert(reg);
		}

	bool done = false;
	while (!done) {

		// estimate signal delays
		hlim::SignalDelay delays;
		delays.compute(subnet);

		// Find critical output
		hlim::NodePort criticalOutput;
		unsigned criticalBit = ~0u;
		float criticalTime = 0.0f;
		for (auto &n : subnet)
			for (auto i : utils::Range(n->getNumOutputPorts())) {
				hlim::NodePort np = {.node = n, .port = i};
				auto d = delays.getDelay(np);
				for (auto i : utils::Range(d.size())) {   
					if (d[i] > criticalTime) {
						criticalTime = d[i];
						criticalOutput = np;
						criticalBit = i;
					}
				}
			}
/*
		{
            DotExport exp("signalDelays.dot");
            exp(circuit, (hlim::ConstSubnet &)subnet, delays);
            exp.runGraphViz("signalDelays.svg");


			hlim::ConstSubnet criticalPathSubnet;
			{
				hlim::NodePort np = criticalOutput;
				unsigned bit = criticalBit;
				while (np.node != nullptr) {
					criticalPathSubnet.add(np.node);
					unsigned criticalInputPort, criticalInputBit;
					np.node->estimateSignalDelayCriticalInput(delays, np.port, bit, criticalInputPort, criticalInputBit);
					if (criticalInputPort == ~0u)
						np.node = nullptr;
					else {
						np = np.node->getDriver(criticalInputPort);
						bit = criticalInputBit;
					}
				}
			}

            DotExport exp2("criticalPath.dot");
            exp2(circuit, criticalPathSubnet, delays);
            exp2.runGraphViz("criticalPath.svg");
		}
*/
		// Split in half
		float splitTime = criticalTime * 0.5f;

		std::cout << "Critical path time: " << criticalTime << " Attempting to split at " << splitTime << std::endl;

		// Trace back critical path to find point where to retime register to
		hlim::NodePort retimingTarget;
		{
			hlim::NodePort np = criticalOutput;
			unsigned bit = criticalBit;
			while (np.node != nullptr) {

				float thisTime = delays.getDelay(np)[bit];
				if (thisTime < splitTime) {
					retimingTarget = np;
					break;
				}

				unsigned criticalInputPort, criticalInputBit;
				np.node->estimateSignalDelayCriticalInput(delays, np.port, bit, criticalInputPort, criticalInputBit);
				if (criticalInputPort == ~0u)
					np.node = nullptr;
				else {
					float nextTime = delays.getDelay(np.node->getDriver(criticalInputPort))[criticalInputBit];
					if ((thisTime + nextTime) * 0.5f < splitTime) {
					//if (nextTime < splitTime) {
						retimingTarget = np;
						break;
					}

					np = np.node->getDriver(criticalInputPort);
					bit = criticalInputBit;
				}
			}
		}

		if (retimingTarget.node != nullptr && dynamic_cast<Node_Register*>(retimingTarget.node) == nullptr) {
			done = !retimeForwardToOutput(circuit, subnet, anchoredRegisters, retimingTarget, false, false);
		} else
			done = true;
	
// For debugging
//circuit.optimizeSubnet(subnet);
	}
}





/**
 * @brief Determines the exact area to be backward retimed (but doesn't do any retiming).
 * @details This is the entire fan in up to registers that can be retimed forward.
 * @param area The area to which retiming is to be restricted.
 * @param anchoredRegisters Set of registers that are not to be moved (e.g. because they are already deemed in a good location).
 * @param output The output that shall recieve a register.
 * @param retimeableWritePorts List of write ports that may be retimed individually without retiming all other ports as well.
 * @param areaToBeRetimed Outputs the area that will be retimed forward (excluding the registers).
 * @param registersToBeRemoved Output of the registers that lead into areaToBeRetimed and which will have to be removed.
 * @param ignoreRefs Whether or not to throw an exception if a node has to be retimed to which a reference exists.
 * @param failureIsError Whether to throw an exception if a retiming area limited by registers can be determined
 * @returns Whether a valid retiming area could be determined
 */
bool determineAreaToBeRetimedBackward(Circuit &circuit, const Subnet &area, const std::set<Node_Register*> &anchoredRegisters, NodePort output, const std::set<Node_MemPort*> &retimeableWritePorts, 
								Subnet &areaToBeRetimed, std::set<Node_Register*> &registersToBeRemoved, bool ignoreRefs = false, bool failureIsError = true)
{
	BaseNode *clockGivingNode = nullptr;
	Clock *clock = nullptr;

	std::vector<BaseNode*> openList;
	for (auto np : output.node->getDirectlyDriven(output.port))
		openList.push_back(np.node);


#ifdef DEBUG_OUTPUT
    auto writeSubnet = [&]{
		Subnet subnet = areaToBeRetimed;
		subnet.dilate(true, true);

        DotExport exp("retiming_area.dot");
        exp(circuit, subnet.asConst());
        exp.runGraphViz("retiming_area.svg");
    };
#endif

	while (!openList.empty()) {
		auto *node = openList.back();
		openList.pop_back();
		// Continue if the node was already encountered.
		if (areaToBeRetimed.contains(node)) continue;

		//std::cout << "determineAreaToBeRetimed: processing node " << node->getId() << std::endl;


		// Do not leave the specified playground, abort if no register is found before.
		if (!area.contains(node)) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals leave the specified operation area through node " << node->getName() << " (" << node->getTypeName() 
				<< ") without passing a register that can be retimed backward. Note that registers with enable signals can't be retimed yet.\n"
				<< "First node outside the operation area from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}

		// We may not want to retime nodes to which references are still being held
		if (node->hasRef() && !ignoreRefs) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals are driving a node to which references are still being held " << node->getName() << " (" << node->getTypeName() << ", id " << node->getId() << ").\n"
				<< "Node with references from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}			

		// Check that everything is using the same clock.
		for (auto *c : node->getClocks()) {
			if (c != nullptr) {
				if (clock == nullptr) {
					clock = c;
					clockGivingNode = node;
				} else {
					if (clock != c) {
						if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

						std::stringstream error;

						error 
							<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
							<< "Node from:\n" << output.node->getStackTrace() << "\n";

						error 
							<< "The fanning-out signals are driven by different clocks. Clocks differ between nodes " << clockGivingNode->getName() << " (" << clockGivingNode->getTypeName() << ") and  " << node->getName() << " (" << node->getTypeName() << ").\n"
							<< "First node from:\n" << clockGivingNode->getStackTrace() << "\n"
							<< "Second node from:\n" << node->getStackTrace() << "\n";

						HCL_ASSERT_HINT(false, error.str());
					}
				}
			}
		}

		// We can not retime nodes with a side effect
		// Memory ports are handled separately below
		if (node->hasSideEffects() && dynamic_cast<Node_MemPort*>(node) == nullptr) {
			if (!failureIsError) return false;

#ifdef DEBUG_OUTPUT
    writeSubnet();
#endif

			std::stringstream error;

			error 
				<< "An error occured attempting to retime backward to output " << output.port << " of node " << output.node->getName() << " (" << output.node->getTypeName() << ", id " << output.node->getId() << "):\n"
				<< "Node from:\n" << output.node->getStackTrace() << "\n";

			error 
				<< "The fanning-out signals are driving a node with side effects " << node->getName() << " (" << node->getTypeName() << ") which can not be retimed.\n"
				<< "Node with side effects from:\n" << node->getStackTrace() << "\n";

			HCL_ASSERT_HINT(false, error.str());
		}		

		// Everything seems good with this node, so proceeed

		if (auto *reg = dynamic_cast<Node_Register*>(node)) {  // Registers need special handling
			if (registersToBeRemoved.contains(reg)) continue;

			// Retime over anchored registers and registers with enable signals (since we can't move them yet).
			if (anchoredRegisters.contains(reg) || reg->getNonSignalDriver(Node_Register::ENABLE).node != nullptr) {
				// Retime over this register.
				areaToBeRetimed.add(node);
				for (unsigned i : utils::Range(node->getNumOutputPorts()))
					for (auto np : node->getDirectlyDriven(i))
						openList.push_back(np.node);
			} else {
				// Found a register to retime backward, stop here.
				registersToBeRemoved.insert(reg);
				areaToBeRetimed.add(node);
			}
		} else {
			// Regular nodes just get added to the retiming area and their outputs are further explored
			areaToBeRetimed.add(node);
			for (unsigned i : utils::Range(node->getNumOutputPorts()))
				for (auto np : node->getDirectlyDriven(i))
					openList.push_back(np.node);

 			if (auto *memPort = dynamic_cast<Node_MemPort*>(node)) { // If it is a memory port		 	
				// Check if it is a write port that will be fixed later on
				if (retimeableWritePorts.contains(memPort)) {
					areaToBeRetimed.add(memPort);
				} else {
					// attempt to retime entire memory
					auto *memory = memPort->getMemory();
					areaToBeRetimed.add(memory);
				
					// add all memory ports to open list
					for (auto np : memory->getDirectlyDriven(0)) 
						openList.push_back(np.node);
				}
			 }
		}
	}

	return true;
}

#if 0

bool retimeBackwardtoOutput(Circuit &circuit, Subnet &subnet, const std::set<Node_Register*> &anchoredRegisters, const std::set<Node_MemPort*> &retimeableWritePorts,
                        Subnet &retimedArea, NodePort output, bool ignoreRefs, bool failureIsError)
{
	std::set<Node_Register*> registersToBeRemoved;
	if (!determineAreaToBeRetimedBackward(circuit, subnet, anchoredRegisters, output, retimeableWritePorts, retimedArea, registersToBeRemoved, ignoreRefs, failureIsError))
		return false;

/*
	{
		std::array<const BaseNode*,1> arr{output.node};
		ConstSubnet csub = ConstSubnet::allNecessaryForNodes({}, arr);
		csub.add(output.node);

		DotExport exp("DriversOfOutput.dot");
		exp(circuit, csub);
		exp.runGraphViz("DriversOfOutput.svg");
	}
	{
		ConstSubnet csub;
		for (auto n : areaToBeRetimed) csub.add(n);
		DotExport exp("areaToBeRetimed.dot");
		exp(circuit, csub);
		exp.runGraphViz("areaToBeRetimed.svg");
	}
*/
	std::set<hlim::NodePort> outputsEnteringRetimingArea;
	// Find every output leaving the area
	for (auto n : retimedArea)
		for (auto i : utils::Range(n->getNumInputPorts())) {
			auto driver = n->getDriver(i);
			if (!retimedArea.contains(driver.node)) {
				outputsEnteringRetimingArea.insert(driver);
				break;
			}
		}

	HCL_ASSERT(!registersToBeRemoved.empty());
	auto *clock = (*registersToBeRemoved.begin())->getClocks()[0];

	// Run a simulation to determine the reset values of the registers that will be placed there
	/// @todo Clone and optimize to prevent issues with loops
    sim::SimulatorCallbacks ignoreCallbacks;
    sim::ReferenceSimulator simulator;
    simulator.compileProgram(circuit, {outputsEnteringRetimingArea});
    simulator.powerOn();

	// Insert registers
	for (auto np : outputsLeavingRetimingArea) {
		auto *reg = circuit.createNode<Node_Register>();
		reg->recordStackTrace();
		// Setup clock
		reg->setClock(clock);
		// Setup input data
		reg->connectInput(Node_Register::DATA, np);
		// add to the node group of its new driver
		reg->moveToGroup(np.node->getGroup());
		
		area.add(reg);


		// If any input bit is defined uppon reset, add that as a reset value
		auto resetValue = simulator.getValueOfOutput(np);
		if (sim::anyDefined(resetValue, 0, resetValue.size())) {
			auto *resetConst = circuit.createNode<Node_Constant>(resetValue, getOutputConnectionType(np).interpretation);
			resetConst->recordStackTrace();
			resetConst->moveToGroup(reg->getGroup());
			area.add(resetConst);
			reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});
		}

		// Find all signals leaving the retiming area and rewire them to the register's output
		std::vector<NodePort> inputsToRewire;
		for (auto inputNP : np.node->getDirectlyDriven(np.port))
			if (inputNP.node != reg) // don't rewire the register we just attached
				if (!areaToBeRetimed.contains(inputNP.node))
					inputsToRewire.push_back(inputNP);

		for (auto inputNP : inputsToRewire)
			inputNP.node->rewireInput(inputNP.port, {.node = reg, .port = 0ull});
	}

	// Remove input registers that have now been retimed forward
	for (auto *reg : registersToBeRemoved)
		reg->bypassOutputToInput(0, (unsigned)Node_Register::DATA);

	return true;
}

#endif

}
