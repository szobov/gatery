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
#include "MemoryDetector.h"

#include "../Circuit.h"

#include "../coreNodes/Node_Signal.h"
#include "../coreNodes/Node_Register.h"
#include "../coreNodes/Node_Constant.h"
#include "../coreNodes/Node_Compare.h"
#include "../coreNodes/Node_Logic.h"
#include "../coreNodes/Node_Multiplexer.h"
#include "../coreNodes/Node_Rewire.h"
#include "../coreNodes/Node_Arithmetic.h"
#include "../supportNodes/Node_Memory.h"
#include "../supportNodes/Node_MemPort.h"
#include "../GraphExploration.h"
#include "../RegisterRetiming.h"
#include "../GraphTools.h"


#define DEBUG_OUTPUT

#ifdef DEBUG_OUTPUT
#include "../Subnet.h"
#include "../../export/DotExport.h"
#endif

#include <sstream>
#include <vector>
#include <set>
#include <optional>

namespace gtry::hlim {


bool MemoryGroup::ReadPort::findOutputRegisters(size_t readLatency, NodeGroup *memoryNodeGroup)
{
    // keep a list of encoutnered signal nodes to move into memory group.
    std::vector<BaseNode*> signalNodes;

    // Clear all and start from scratch
    dedicatedReadLatencyRegisters.resize(readLatency);
    for (auto &reg : dedicatedReadLatencyRegisters)
        reg = nullptr;

    Clock *clock = nullptr;

    // Start from the read port
    dataOutput = {.node = node.get(), .port = (size_t)Node_MemPort::Outputs::rdData};
    for (auto i : utils::Range(dedicatedReadLatencyRegisters.size())) {
        signalNodes.clear();

        // For each output (read port or register in the chain) ensure that it only drives another register, then add that register to the list
        Node_Register *reg = nullptr;
        for (auto nh : dataOutput.node->exploreOutput(dataOutput.port)) {
            if (nh.isSignal()) {
                signalNodes.push_back(nh.node());
            } else {
                if (nh.isNodeType<Node_Register>()) {
                    auto dataReg = (Node_Register *) nh.node();
                    // The register can't have a reset (since it's essentially memory).
                    if (dataReg->getNonSignalDriver(Node_Register::Input::RESET_VALUE).node != nullptr)
                        break;
                    if (reg == nullptr)
                        reg = dataReg;
                    else {
                        // if multiple registers are driven, don't fuse them here but, but fail and let the register retiming handle the fusion
                        reg = nullptr;
                        break;
                    }
                } else {
                    // Don't make use of the regsister if other stuff is also directly driven by the port's output
                    reg = nullptr;
                    break;
                }
                nh.backtrack();
            }
        }

        // If there is a register, move it and all the signal nodes on the way into the memory group.
        if (reg != nullptr) {
            if (clock == nullptr)
                clock = reg->getClocks()[0];
            else if (clock != reg->getClocks()[0])
                break; // Hit a clock domain crossing, break early

            reg->getFlags().clear(Node_Register::ALLOW_RETIMING_BACKWARD).clear(Node_Register::ALLOW_RETIMING_FORWARD).insert(Node_Register::IS_BOUND_TO_MEMORY);
            // Move the entire signal path and the data register into the memory node group
            for (auto opt : signalNodes)
                opt->moveToGroup(memoryNodeGroup);
            reg->moveToGroup(memoryNodeGroup);
            dedicatedReadLatencyRegisters[i] = reg;
            
            // continue from this register and mark it as the output of the read port
            dataOutput = {.node = reg, .port = 0};

            signalNodes.clear();
        } else 
            break;
    }

    return dedicatedReadLatencyRegisters.back() != nullptr; // return true if all were found
}

MemoryGroup::MemoryGroup() : NodeGroup(GroupType::SFU)
{
    m_name = "memory";
}

void MemoryGroup::formAround(Node_Memory *memory, Circuit &circuit)
{
    m_memory = memory;
    m_memory->moveToGroup(this);

    // Initial naive grabbing of everything that might be usefull
    for (auto &np : m_memory->getPorts()) {
        auto *port = dynamic_cast<Node_MemPort*>(np.node);
        HCL_ASSERT(port->isWritePort() || port->isReadPort());
        // Check all write ports
        if (port->isWritePort()) {
            HCL_ASSERT_HINT(!port->isReadPort(), "For now I don't want to mix read and write ports");
            m_writePorts.push_back({.node=NodePtr<Node_MemPort>{port}});
            port->moveToGroup(this);
        }
        // Check all read ports
        if (port->isReadPort()) {
            m_readPorts.push_back({.node = NodePtr<Node_MemPort>{port}});
            ReadPort &rp = m_readPorts.back();
            port->moveToGroup(this);
            rp.dataOutput = {.node = port, .port = (size_t)Node_MemPort::Outputs::rdData};

            NodePort readPortEnable = port->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable);

            // Try and grab as many output registers as possible (up to read latency)
            // rp.findOutputRegisters(m_memory->getRequiredReadLatency(), this);
            // Actually, don't do this yet, makes things easier
        }
    }

    // Verify writing is only happening with one clock:
    {
        Node_MemPort *firstWritePort = nullptr;
        for (auto &np : m_memory->getPorts()) {
            auto *port = dynamic_cast<Node_MemPort*>(np.node);
            if (port->isWritePort()) {
                if (firstWritePort == nullptr)
                    firstWritePort = port;
                else {
                    if (firstWritePort->getClocks()[0] != port->getClocks()[0]) {
                        std::stringstream issues;
                        issues << "All write ports to a memory must have the same clock!\n";
                        issues << "from:\n" << firstWritePort->getStackTrace() << "\n and from:\n" << port->getStackTrace();
                        HCL_DESIGNCHECK_HINT(false, issues.str());
                    }
                }
            }
        }
    }

    if (m_memory->type() == Node_Memory::MemType::DONT_CARE) {
        if (!m_memory->getPorts().empty()) {
            size_t numWords = m_memory->getSize() / m_memory->getMaxPortWidth();
            if (numWords > 64)
                m_memory->setType(Node_Memory::MemType::BRAM, std::max<size_t>(1, m_memory->getRequiredReadLatency()));
            // todo: Also depend on other things
        }
    }

}

void MemoryGroup::lazyCreateFixupNodeGroup()
{
    if (m_fixupNodeGroup == nullptr) {
        m_fixupNodeGroup = m_parent->addChildNodeGroup(GroupType::ENTITY);
        m_fixupNodeGroup->recordStackTrace();
        m_fixupNodeGroup->setName("Memory_Helper");
        m_fixupNodeGroup->setComment("Auto generated to handle various memory access issues such as read during write and read modify write hazards.");
        moveInto(m_fixupNodeGroup);
    }
}

void MemoryGroup::convertToReadBeforeWrite(Circuit &circuit)
{
    // If an async read happens after a write, it must
    // check if an address collision occured and if so directly forward the new value.
    for (auto &rp : m_readPorts) {
        // Collect a list of all potentially conflicting write ports and sort them in write order, so that conflict resolution can also happen in write order
        std::vector<WritePort*> sortedWritePorts;
        for (auto &wp : m_writePorts)
            if (wp.node->isOrderedBefore(rp.node))
                sortedWritePorts.push_back(&wp);

        // sort last to first because multiplexers are prepended.
        // todo: this assumes that write ports do have an order.
        std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](WritePort *left, WritePort *right)->bool{
            return left->node->isOrderedAfter(right->node);
        });

        for (auto wp_ptr : sortedWritePorts) {
            auto &wp = *wp_ptr;

            lazyCreateFixupNodeGroup();

            auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::EQ);
            addrCompNode->recordStackTrace();
            addrCompNode->moveToGroup(m_fixupNodeGroup);
            addrCompNode->setComment("Compare read and write addr for conflicts");
            addrCompNode->connectInput(0, rp.node->getDriver((unsigned)Node_MemPort::Inputs::address));
            addrCompNode->connectInput(1, wp.node->getDriver((unsigned)Node_MemPort::Inputs::address));

            NodePort conflict = {.node = addrCompNode, .port = 0ull};
            circuit.appendSignal(conflict)->setName("conflict");

            if (rp.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                logicAnd->moveToGroup(m_fixupNodeGroup);
                logicAnd->recordStackTrace();
                logicAnd->connectInput(0, conflict);
                logicAnd->connectInput(1, rp.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                conflict = {.node = logicAnd, .port = 0ull};
                circuit.appendSignal(conflict)->setName("conflict_and_rdEn");
            }

            HCL_ASSERT(wp.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
            if (wp.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                logicAnd->moveToGroup(m_fixupNodeGroup);
                logicAnd->recordStackTrace();
                logicAnd->connectInput(0, conflict);
                logicAnd->connectInput(1, wp.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                conflict = {.node = logicAnd, .port = 0ull};
                circuit.appendSignal(conflict)->setName("conflict_and_wrEn");
            }

            NodePort wrData = wp.node->getDriver((unsigned)Node_MemPort::Inputs::wrData);

            auto delayLike = [&](Node_Register *refReg, NodePort &np, const char *name, const char *comment) {
                auto *reg = circuit.createNode<Node_Register>();
                reg->recordStackTrace();
                reg->moveToGroup(m_fixupNodeGroup);
                reg->setComment(comment);
                reg->setClock(refReg->getClocks()[0]);
                for (auto i : {Node_Register::Input::ENABLE, Node_Register::Input::RESET_VALUE})
                    reg->connectInput(i, refReg->getDriver(i));
                reg->connectInput(Node_Register::Input::DATA, np);
                reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);
                np = {.node = reg, .port = 0ull};
                circuit.appendSignal(np)->setName(name);
            };


            // If the read data gets delayed, we will have to delay the write data and conflict decision as well
            // Actually: Don't fetch them beforehand, makes things easier
            HCL_ASSERT(rp.dedicatedReadLatencyRegisters.empty());
            /*
            if (rp.syncReadDataReg != nullptr) {
                // read data gets delayed so we will have to delay the write data and conflict decision as well
                delayLike(rp.syncReadDataReg, wrData, "delayedWrData", "The memory read gets delayed by a register so the write data bypass also needs to be delayed.");
                delayLike(rp.syncReadDataReg, conflict, "delayedConflict", "The memory read gets delayed by a register so the collision detection decision also needs to be delayed.");

                if (rp.outputReg != nullptr) {
                    // need to delay even more
                    delayLike(rp.syncReadDataReg, wrData, "delayed_2_WrData", "The memory read gets delayed by an additional register so the write data bypass also needs to be delayed.");
                    delayLike(rp.syncReadDataReg, conflict, "delayed_2_Conflict", "The memory read gets delayed by an additional register so the collision detection decision also needs to be delayed.");
                }
            }
            */

            std::vector<NodePort> consumers = rp.dataOutput.node->getDirectlyDriven(rp.dataOutput.port);

            // Finally the actual mux to arbitrate between the actual read and the forwarded write data.
            auto *muxNode = circuit.createNode<Node_Multiplexer>(2);

            // Then bind the mux
            muxNode->recordStackTrace();
            muxNode->moveToGroup(m_fixupNodeGroup);
            muxNode->setComment("If read and write addr match and read and write are enabled, forward write data to read output.");
            muxNode->connectSelector(conflict);
            muxNode->connectInput(0, rp.dataOutput);
            muxNode->connectInput(1, wrData);

            NodePort muxOut = {.node = muxNode, .port=0ull};

            circuit.appendSignal(muxOut)->setName("conflict_bypass_mux");

            // Rewire all original consumers to the mux output
            for (auto np : consumers)
                np.node->rewireInput(np.port, muxOut);
        }
    }


    std::vector<Node_MemPort*> sortedWritePorts;
    for (auto &wp : m_writePorts)
        sortedWritePorts.push_back(wp.node);

    std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](Node_MemPort *left, Node_MemPort *right)->bool{
        return left->isOrderedBefore(right);
    });


    // Reorder all writes to happen after all reads
    Node_MemPort *lastPort = nullptr;
    for (auto &rp : m_readPorts) {
        rp.node->orderAfter(lastPort);
        lastPort = rp.node;
    }
    // But preserve write order for now
    if (!sortedWritePorts.empty())
        sortedWritePorts[0]->orderAfter(lastPort);
    for (size_t i = 1; i < sortedWritePorts.size(); i++)
        sortedWritePorts[i]->orderAfter(sortedWritePorts[i-1]);
}


void MemoryGroup::resolveWriteOrder(Circuit &circuit)
{
    // If two write ports have an explicit ordering, then the later write always trumps the former if both happen to the same address.
    // Search for such cases and build explicit logic that disables the earlier write.
    for (auto &wp1 : m_writePorts)
        for (auto &wp2 : m_writePorts) {
            if (&wp1 == &wp2) continue;

            if (wp1.node->isOrderedBefore(wp2.node)) {
                // Potential addr conflict, build hazard logic

                lazyCreateFixupNodeGroup();


                auto *addrCompNode = circuit.createNode<Node_Compare>(Node_Compare::NEQ);
                addrCompNode->recordStackTrace();
                addrCompNode->moveToGroup(m_fixupNodeGroup);
                addrCompNode->setComment("We can enable the former write if the write adresses differ.");
                addrCompNode->connectInput(0, wp1.node->getDriver((unsigned)Node_MemPort::Inputs::address));
                addrCompNode->connectInput(1, wp2.node->getDriver((unsigned)Node_MemPort::Inputs::address));

                // Enable write if addresses differ
                NodePort newWrEn1 = {.node = addrCompNode, .port = 0ull};
                circuit.appendSignal(newWrEn1)->setName("newWrEn");

                // Alternatively, enable write if wp2 does not write (no connection on enable means yes)
                HCL_ASSERT(wp2.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp2.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
                if (wp2.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {

                    auto *logicNot = circuit.createNode<Node_Logic>(Node_Logic::NOT);
                    logicNot->moveToGroup(m_fixupNodeGroup);
                    logicNot->recordStackTrace();
                    logicNot->connectInput(0, wp2.node->getDriver((unsigned)Node_MemPort::Inputs::enable));

                    auto *logicOr = circuit.createNode<Node_Logic>(Node_Logic::OR);
                    logicOr->moveToGroup(m_fixupNodeGroup);
                    logicOr->setComment("We can also enable the former write if the latter write is disabled.");
                    logicOr->recordStackTrace();
                    logicOr->connectInput(0, newWrEn1);
                    logicOr->connectInput(1, {.node = logicNot, .port = 0ull});
                    newWrEn1 = {.node = logicOr, .port = 0ull};
                    circuit.appendSignal(newWrEn1)->setName("newWrEn");
                }

                // But only enable write if wp1 actually wants to write (no connection on enable means yes)
                HCL_ASSERT(wp1.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::enable) == wp1.node->getNonSignalDriver((unsigned)Node_MemPort::Inputs::wrEnable));
                if (wp1.node->getDriver((unsigned)Node_MemPort::Inputs::enable).node != nullptr) {
                    auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
                    logicAnd->moveToGroup(m_fixupNodeGroup);
                    logicAnd->setComment("But we can only enable the former write if the former write actually wants to write.");
                    logicAnd->recordStackTrace();
                    logicAnd->connectInput(0, newWrEn1);
                    logicAnd->connectInput(1, wp1.node->getDriver((unsigned)Node_MemPort::Inputs::enable));
                    newWrEn1 = {.node = logicAnd, .port = 0ull};
                    circuit.appendSignal(newWrEn1)->setName("newWrEn");
                }


                wp1.node->rewireInput((unsigned)Node_MemPort::Inputs::enable, newWrEn1);
                wp1.node->rewireInput((unsigned)Node_MemPort::Inputs::wrEnable, newWrEn1);
            }
        }


    // Reorder all writes to happen after all reads
    Node_MemPort *lastPort = nullptr;
    for (auto &rp : m_readPorts) {
        rp.node->orderAfter(lastPort);
        lastPort = rp.node;
    }
    // Writes can happen in any order now, but after the last read
    for (auto &wp : m_writePorts)
        wp.node->orderAfter(lastPort);
}



void MemoryGroup::ensureNotEnabledFirstCycles(Circuit &circuit, NodeGroup *ng, Node_MemPort *writePort, size_t numCycles)
{
    std::vector<BaseNode*> nodesToMove;
    auto moveNodes = [&] {
        for (auto n : nodesToMove)
            n->moveToGroup(ng);
        nodesToMove.clear();
    };


    // Ensure enable is low in first cycles
    auto enableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::enable);
    auto wrEnableDriver = writePort->getNonSignalDriver((size_t)Node_MemPort::Inputs::wrEnable);
    HCL_ASSERT(enableDriver == wrEnableDriver);

    NodePort input = {.node = writePort, .port = (size_t)Node_MemPort::Inputs::enable};
    size_t unhandledCycles = numCycles;
    while (unhandledCycles > 0) {
        auto driver = input.node->getDriver(input.port);

        if (driver.node == nullptr) break;

        // something else is driven by the same signal, abort here
        bool onlyUser = true;
        std::set<BaseNode*> alreadyEncountered;
        for (auto nh : driver.node->exploreOutput(driver.port)) {
            if (alreadyEncountered.contains(nh.node())) {
                nh.backtrack();
                continue;
            }
            alreadyEncountered.insert(nh.node());

            if (nh.isSignal()) continue;
            if (nh.node() == writePort && (nh.port() == (size_t)Node_MemPort::Inputs::enable || nh.port() == (size_t)Node_MemPort::Inputs::wrEnable)) {
                nh.backtrack();
                continue;
            }
            if (nh.nodePort() == input) {
                nh.backtrack();
                continue;
            }
            onlyUser = false;
            break;
        }        
        if (!onlyUser)
            break;

        nodesToMove.push_back(driver.node);

        // If signal, continue scanning input chain
        if (auto *signal = dynamic_cast<Node_Signal*>(driver.node)) {
            input = {.node = signal, .port = 0ull};
            continue;
        }

        // Check if already driven by register
        if (auto *enableReg = dynamic_cast<Node_Register*>(driver.node)) {

            // If that register is already resetting to zero everything is fine
            auto resetDriver = enableReg->getNonSignalDriver(Node_Register::RESET_VALUE);
            if (resetDriver.node != nullptr) {
                auto resetValue = evaluateStatically(circuit, resetDriver);
                HCL_ASSERT(resetValue.size() == 1);
                if (resetValue.get(sim::DefaultConfig::DEFINED, 0) && !resetValue.get(sim::DefaultConfig::VALUE, 0)) {
                    input = {.node = enableReg, .port = 0ull};
                    unhandledCycles--;
                    continue;
                }
            }

            sim::DefaultBitVectorState state;
            state.resize(1);
            state.set(sim::DefaultConfig::DEFINED, 0);
            state.set(sim::DefaultConfig::VALUE, 0, false);
            auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
            constZero->recordStackTrace();
            constZero->moveToGroup(ng);
            enableReg->connectInput(Node_Register::RESET_VALUE, {.node = constZero, .port = 0ull});

            input = {.node = enableReg, .port = 0ull};
            unhandledCycles--;
            moveNodes();
            continue;
        }

        break;
    }

    // if there are cycles remaining, build counter and AND the enable signal
    if (unhandledCycles > 0) {
        moveNodes();

        NodePort newEnable;

        // no counter necessary, just use a single register
        if (unhandledCycles == 1) {

            // Build single register with reset 0 and input 1 
            sim::DefaultBitVectorState state;
            state.resize(1);
            state.set(sim::DefaultConfig::DEFINED, 0);
            state.set(sim::DefaultConfig::VALUE, 0, false);
            auto *constZero = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
            constZero->recordStackTrace();
            constZero->moveToGroup(ng);

            state.set(sim::DefaultConfig::VALUE, 0, true);
            auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BOOL);
            constOne->recordStackTrace();
            constOne->moveToGroup(ng);

            auto *reg = circuit.createNode<Node_Register>();
            reg->recordStackTrace();
            reg->moveToGroup(ng);
            reg->setComment("Register that generates a zero after reset and a one on all later cycles");
            reg->setClock(writePort->getClocks()[0]);

            reg->connectInput(Node_Register::Input::RESET_VALUE, {.node = constZero, .port = 0ull});
            reg->connectInput(Node_Register::Input::DATA, {.node = constOne, .port = 0ull});
            reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);

            newEnable = {.node = reg, .port = 0ull};
        } else {
            size_t counterWidth = utils::Log2C(unhandledCycles)+1;

            /*
                Build a counter which starts at unhandledCycles-1 but with one bit more than needed.
                Subtract from it and use the MSB as the indicator that zero was reached, which is the output but also, when negated, the enable to the register.
            */



            auto *reg = circuit.createNode<Node_Register>();
            reg->moveToGroup(ng);
            reg->recordStackTrace();
            reg->setClock(writePort->getClocks()[0]);
            reg->getFlags().insert(Node_Register::ALLOW_RETIMING_BACKWARD).insert(Node_Register::ALLOW_RETIMING_FORWARD);

            sim::DefaultBitVectorState state;
            state.resize(counterWidth);
            state.setRange(sim::DefaultConfig::DEFINED, 0, counterWidth);
            state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, counterWidth, unhandledCycles-1);

            auto *resetConst = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
            resetConst->moveToGroup(ng);
            resetConst->recordStackTrace();
            reg->connectInput(Node_Register::RESET_VALUE, {.node = resetConst, .port = 0ull});


            NodePort counter = {.node = reg, .port = 0ull};
            circuit.appendSignal(counter)->setName("delayedWrEnableCounter");


            // build a one
            state.insertNonStraddling(sim::DefaultConfig::VALUE, 0, counterWidth, 1);
            auto *constOne = circuit.createNode<Node_Constant>(state, ConnectionType::BITVEC);
            constOne->moveToGroup(ng);
            constOne->recordStackTrace();

            auto *subNode = circuit.createNode<Node_Arithmetic>(Node_Arithmetic::SUB);
            subNode->moveToGroup(ng);
            subNode->recordStackTrace();
            subNode->connectInput(0, counter);
            subNode->connectInput(1, {.node = constOne, .port = 0ull});

            reg->connectInput(Node_Register::DATA, {.node = subNode, .port = 0ull});


            auto *rewireNode = circuit.createNode<Node_Rewire>(1);
            rewireNode->moveToGroup(ng);
            rewireNode->recordStackTrace();
            rewireNode->connectInput(0, counter);
            rewireNode->setExtract(counterWidth-1, 1);
            rewireNode->changeOutputType({.interpretation = ConnectionType::BOOL, .width = 1});

            NodePort counterExpired = {.node = rewireNode, .port = 0ull};
            circuit.appendSignal(counterExpired)->setName("delayedWrEnableCounterExpired");

            auto *logicNot = circuit.createNode<Node_Logic>(Node_Logic::NOT);
            logicNot->moveToGroup(ng);
            logicNot->recordStackTrace();
            logicNot->connectInput(0, counterExpired);
            reg->connectInput(Node_Register::ENABLE, {.node = logicNot, .port = 0ull});

            newEnable = counterExpired;
        }

        auto driver = input.node->getDriver(input.port);
        if (driver.node != nullptr) {

            // AND to existing enable input
            auto *logicAnd = circuit.createNode<Node_Logic>(Node_Logic::AND);
            logicAnd->moveToGroup(ng);
            logicAnd->recordStackTrace();
            logicAnd->connectInput(0, newEnable);
            logicAnd->connectInput(1, driver);

            newEnable = {.node = logicAnd, .port = 0ull};
        }

        input.node->rewireInput(input.port, newEnable);

        writePort->rewireInput((size_t)Node_MemPort::Inputs::wrEnable, writePort->getDriver((size_t)Node_MemPort::Inputs::enable));
    }
}


void MemoryGroup::attemptRegisterRetiming(Circuit &circuit)
{
    if (m_memory->getRequiredReadLatency() == 0) return;

    //visualize(circuit, "beforeRetiming");

    std::set<Node_MemPort*> retimeableWritePorts;
    for (auto np : m_memory->getPorts()) {
        auto *memPort = dynamic_cast<Node_MemPort*>(np.node);
        if (memPort->isWritePort()) {
            HCL_ASSERT_HINT(!memPort->isReadPort(), "Retiming for combined read and write ports not yet implemented!");
            retimeableWritePorts.insert(memPort);
        }
    }



    std::map<Node_MemPort*, size_t> actuallyRetimedWritePorts;

    // If we are aiming for memory with a read latency > 0
    // Check if any read ports are lacking the registers that models that read latency.
    // If they do, scan the read data output bus for any registers buried in the combinatorics that could be pulled back and fused.
    // Keep note of which write ports are "delayed" through this retiming to then, in a second step, build rw hazard bypass logic.

    for (auto &rp : m_readPorts) {
        while (!rp.findOutputRegisters(m_memory->getRequiredReadLatency(), this)) {
            auto subnet = Subnet::all(circuit);
            Subnet retimedArea;
            // On multi-readport memories there can already appear a register due to the retiming of other read ports. In this case, retimeBackwardtoOutput is a no-op.
            retimeBackwardtoOutput(circuit, subnet, {}, retimeableWritePorts, retimedArea, rp.dataOutput, true, true);

            //visualize(circuit, "afterRetiming");

            for (auto wp : retimeableWritePorts) {
                if (retimedArea.contains(wp)) {
                    // Take note that this write port is delayed by one more cycle
                    actuallyRetimedWritePorts[wp]++;
                }
            }
        }
    }

    //visualize(circuit, "afterRetiming");

    if (actuallyRetimedWritePorts.empty()) return;

    lazyCreateFixupNodeGroup();

    // For all WPs that got retimed:
    std::vector<std::pair<Node_MemPort*, size_t>> sortedWritePorts;
    for (auto wp : actuallyRetimedWritePorts) {
        // ... prep a list to sort them in write order
        sortedWritePorts.push_back(wp);
        // ... Ensure their (write-)enable is deasserted for at least as long as they were delayed.
        ensureNotEnabledFirstCycles(circuit, m_fixupNodeGroup, wp.first, wp.second); 
    }

    //visualize(circuit, "afterEnableFix");    

    std::sort(sortedWritePorts.begin(), sortedWritePorts.end(), [](const auto &left, const auto &right)->bool{
        return left.first->isOrderedBefore(right.first);
    });

    if (sortedWritePorts.size() >= 2)
        HCL_ASSERT(sortedWritePorts[0].first->isOrderedBefore(sortedWritePorts[1].first));

    auto *clock = sortedWritePorts.front().first->getClocks()[0];
    ReadModifyWriteHazardLogicBuilder rmwBuilder(circuit, clock);
    
    size_t maxLatency = 0;
    
    for (auto &rp : m_readPorts)
        rmwBuilder.addReadPort(ReadModifyWriteHazardLogicBuilder::ReadPort{
            .addrInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::address),
            .enableInputDriver = rp.node->getDriver((size_t)Node_MemPort::Inputs::enable),
            .dataOutOutputDriver = (NodePort) rp.dataOutput,
        });

    for (auto wp : sortedWritePorts) {
        HCL_ASSERT(wp.first->getDriver((size_t)Node_MemPort::Inputs::enable) == wp.first->getDriver((size_t)Node_MemPort::Inputs::wrEnable));
        rmwBuilder.addWritePort(ReadModifyWriteHazardLogicBuilder::WritePort{
            .addrInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::address),
            .enableInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::enable),
            .enableMaskInputDriver = {},
            .dataInInputDriver = wp.first->getDriver((size_t)Node_MemPort::Inputs::wrData),
            .latencyCompensation = wp.second
        });

        maxLatency = std::max(maxLatency, wp.second);
    }

    bool useMemory = maxLatency > 2;
    rmwBuilder.retimeRegisterToMux();
    rmwBuilder.build(useMemory);

    const auto &newNodes = rmwBuilder.getNewNodes();
    for (auto n : newNodes) 
        n->moveToGroup(m_fixupNodeGroup);

    //visualize(circuit, "afterRMW");
/*
    {
        auto all = ConstSubnet::all(circuit);
        DotExport exp("afterRMW.dot");
        exp(circuit, all);
        exp.runGraphViz("afterRMW.svg");            
    }        
*/    
}

void MemoryGroup::buildReset(Circuit &circuit)
{
    if (m_memory->getNonSignalDriver((size_t)Node_Memory::Inputs::INITIALIZATION_DATA).node != nullptr) {
        buildResetLogic(circuit);
    } else {
        if (sim::anyDefined(m_memory->getPowerOnState())) 
            buildResetRom(circuit);
    }
}

void MemoryGroup::buildResetLogic(Circuit &circuit)
{

}

void MemoryGroup::buildResetRom(Circuit &circuit)
{
}

void MemoryGroup::verify()
{
    switch (m_memory->type()) {
        case Node_Memory::MemType::BRAM:
            for (auto &rp : m_readPorts) {
                if (rp.dedicatedReadLatencyRegisters.size() < 1) {
                    std::stringstream issue;
                    issue << "Memory can not become BRAM because a read port is missing it's data register.\nMemory from:\n"
                        << m_memory->getStackTrace() << "\nRead port from:\n" << rp.node->getStackTrace();
                    HCL_DESIGNCHECK_HINT(false, issue.str());
                }
            }
            /*
            if (m_readPorts.size() + m_writePorts.size() > 2) {
                std::stringstream issue;
                issue << "Memory can not become BRAM because it has too many memory ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
            */
        break;
        case Node_Memory::MemType::LUTRAM:
            if (m_readPorts.size() > 1) {
                std::stringstream issue;
                issue << "Memory can not become LUTRAM because it has too many read ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
            if (m_writePorts.size() > 1) {
                std::stringstream issue;
                issue << "Memory can not become LUTRAM because it has too many write ports.\nMemory from:\n"
                      << m_memory->getStackTrace();
                HCL_DESIGNCHECK_HINT(false, issue.str());
            }
        break;
    }
}

MemoryGroup *formMemoryGroupIfNecessary(Circuit &circuit, Node_Memory *memory)
{
    auto* memoryGroup = dynamic_cast<MemoryGroup*>(memory->getGroup());
    if (memoryGroup == nullptr) {
        memoryGroup = memory->getGroup()->addSpecialChildNodeGroup<MemoryGroup>();
        if (memory->getName().empty())
            memoryGroup->setName("memory");
        else
            memoryGroup->setName(memory->getName());
        memoryGroup->setComment("Auto generated");
        memoryGroup->formAround(memory, circuit);
    }
    return memoryGroup;
}

void findMemoryGroups(Circuit &circuit)
{
    for (auto &node : circuit.getNodes())
        if (auto *memory = dynamic_cast<Node_Memory*>(node.get()))
            formMemoryGroupIfNecessary(circuit, memory);
}

void buildExplicitMemoryCircuitry(Circuit &circuit)
{
    for (size_t i = 0; i < circuit.getNodes().size(); i++) {
        auto& node = circuit.getNodes()[i];
        if (auto* memory = dynamic_cast<Node_Memory*>(node.get())) {
            auto* memoryGroup = formMemoryGroupIfNecessary(circuit, memory);

            memoryGroup->convertToReadBeforeWrite(circuit);
            memoryGroup->attemptRegisterRetiming(circuit);
            memoryGroup->resolveWriteOrder(circuit);
            memoryGroup->verify();
        }
    }
}


}
