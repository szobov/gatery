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
#include "ReferenceSimulator.h"
#include "BitAllocator.h"

#include "../utils/Range.h"
#include "../hlim/Circuit.h"
#include "../hlim/coreNodes/Node_Constant.h"
#include "../hlim/coreNodes/Node_Compare.h"
#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Register.h"
#include "../hlim/coreNodes/Node_Arithmetic.h"
#include "../hlim/coreNodes/Node_Logic.h"
#include "../hlim/coreNodes/Node_Multiplexer.h"
#include "../hlim/coreNodes/Node_PriorityConditional.h"
#include "../hlim/coreNodes/Node_Rewire.h"
#include "../hlim/coreNodes/Node_Pin.h"
#include "../hlim/NodeVisitor.h"
#include "../hlim/supportNodes/Node_ExportOverride.h"
#include "../hlim/Subnet.h"


#include <gatery/export/DotExport.h>

#include "simProc/WaitFor.h"
#include "simProc/WaitUntil.h"
#include "simProc/WaitClock.h"
#include "RunTimeSimulationContext.h"

#include <iostream>

#include <immintrin.h>

namespace gtry::sim {


void ExecutionBlock::evaluate(SimulatorCallbacks &simCallbacks, DataState &state) const
{
#if 1
    for (const auto &step : m_steps)
        step.node->simulateEvaluate(simCallbacks, state.signalState, step.internal.data(), step.inputs.data(), step.outputs.data());
#else
    for (auto i : utils::Range(m_steps.size())) {
        if (i+3 < m_steps.size()) {
            _mm_prefetch(m_steps[i+3].node, _MM_HINT_T0);
     //       _mm_prefetch(m_steps[i+1].internal.data(), _MM_HINT_T0);
     //       _mm_prefetch(m_steps[i+1].inputs.data(), _MM_HINT_T0);
     //       _mm_prefetch(m_steps[i+1].outputs.data(), _MM_HINT_T0);
        }
        const auto &step = m_steps[i];
        step.node->simulateEvaluate(simCallbacks, state.signalState, step.internal.data(), step.inputs.data(), step.outputs.data());
    }
#endif
}

void ExecutionBlock::commitState(SimulatorCallbacks &simCallbacks, DataState &state) const
{
    for (const auto &step : m_steps)
        step.node->simulateCommit(simCallbacks, state.signalState, step.internal.data(), step.inputs.data());
}

void ExecutionBlock::addStep(MappedNode mappedNode)
{
    m_steps.push_back(mappedNode);
}

ClockedNode::ClockedNode(MappedNode mappedNode, size_t clockPort) : m_mappedNode(std::move(mappedNode)), m_clockPort(clockPort)
{
}

void ClockedNode::advance(SimulatorCallbacks &simCallbacks, DataState &state) const
{
    m_mappedNode.node->simulateAdvance(simCallbacks, state.signalState, m_mappedNode.internal.data(), m_mappedNode.outputs.data(), m_clockPort);
}

void ClockedNode::changeReset(SimulatorCallbacks &simCallbacks, DataState &state, bool resetHigh) const
{
    m_mappedNode.node->simulateResetChange(simCallbacks, state.signalState, m_mappedNode.internal.data(), m_mappedNode.outputs.data(), m_clockPort, resetHigh);
}


void Program::compileProgram(const hlim::Circuit &circuit, const hlim::Subnet &nodes)
{
    allocateSignals(circuit, nodes);

    m_stateMapping.clockPinAllocation = hlim::extractClockPins(const_cast<hlim::Circuit &>(circuit), nodes);
    m_clockDomains.resize(m_stateMapping.clockPinAllocation.clockPins.size());
    m_resetDomains.resize(m_stateMapping.clockPinAllocation.resetPins.size());

    std::set<hlim::BaseNode*> subnetToConsider(nodes.begin(), nodes.end());

    std::set<hlim::NodePort> outputsReady;

    struct CompareById {
        bool operator()(const hlim::BaseNode* lhs, const hlim::BaseNode* rhs) const {
            return lhs->getId() < rhs->getId();
        }
    };
       
    std::set<hlim::BaseNode*, CompareById> nodesRemaining;

    for (auto node : nodes) {
        if (dynamic_cast<hlim::Node_Signal*>(node) != nullptr) continue;
        if (dynamic_cast<hlim::Node_ExportOverride*>(node) != nullptr) continue;
        nodesRemaining.insert(node);


        MappedNode mappedNode;
        mappedNode.node = node;
        mappedNode.internal = m_stateMapping.nodeToInternalOffset[node];
        for (auto i : utils::Range(node->getNumInputPorts())) {
            auto driver = node->getNonSignalDriver(i);
            mappedNode.inputs.push_back(m_stateMapping.outputToOffset[driver]);
        }
        for (auto i : utils::Range(node->getNumOutputPorts()))
            mappedNode.outputs.push_back(m_stateMapping.outputToOffset[{.node = node, .port = i}]);

        for (auto i : utils::Range(node->getNumOutputPorts())) {

            switch (node->getOutputType(i)) {
                case hlim::NodeIO::OUTPUT_IMMEDIATE:
                    // nothing
                break;
                case hlim::NodeIO::OUTPUT_CONSTANT: {
                    hlim::NodePort driver;
                    driver.node = node;
                    driver.port = i;
                    outputsReady.insert(driver);

//                    m_powerOnNodes.push_back(mappedNode);
                } break;
                case hlim::NodeIO::OUTPUT_LATCHED: {
                    hlim::NodePort driver;
                    driver.node = node;
                    driver.port = i;
                    outputsReady.insert(driver);

//                    m_powerOnNodes.push_back(mappedNode);
                } break;
            }
        }
        m_powerOnNodes.push_back(mappedNode); /// @todo now we do this to all nodes, needs to be found out by some other means

        for (auto clockPort : utils::Range(node->getClocks().size())) {
            if (node->getClocks()[clockPort] != nullptr) {
                size_t clockDomainIdx = m_stateMapping.clockPinAllocation.clock2ClockPinIdx[node->getClocks()[clockPort]];
                auto &clockDomain = m_clockDomains[clockDomainIdx];
                clockDomain.clockedNodes.push_back(ClockedNode(mappedNode, clockPort));
                if (clockDomain.dependentExecutionBlocks.empty()) /// @todo only attach those that actually need to be recomputed
                    clockDomain.dependentExecutionBlocks.push_back(0ull);

                size_t resetDomainIdx = m_stateMapping.clockPinAllocation.clock2ResetPinIdx[node->getClocks()[clockPort]];
                auto &resetDomain = m_resetDomains[resetDomainIdx];
                resetDomain.clockedNodes.push_back(ClockedNode(mappedNode, clockPort));
                if (resetDomain.dependentExecutionBlocks.empty()) /// @todo only attach those that actually need to be recomputed
                    resetDomain.dependentExecutionBlocks.push_back(0ull);
            }
        }
    }


    m_executionBlocks.push_back({});
    ExecutionBlock &execBlock = m_executionBlocks.back();

    while (!nodesRemaining.empty()) {
        hlim::BaseNode *readyNode = nullptr;
        for (auto node : nodesRemaining) {
            bool allInputsReady = true;
            for (auto i : utils::Range(node->getNumInputPorts())) {
                auto driver = node->getNonSignalDriver(i);
                while (dynamic_cast<hlim::Node_ExportOverride*>(driver.node)) // Skip all export override nodes
                    driver = driver.node->getNonSignalDriver(hlim::Node_ExportOverride::SIM_INPUT);
                if (driver.node != nullptr && !outputsReady.contains(driver) && subnetToConsider.contains(driver.node)) {
                    allInputsReady = false;
                    break;
                }
            }

            if (allInputsReady) {
                readyNode = node;
                break;
            }
        }

        if (readyNode == nullptr) {
            std::cout << "nodesRemaining : " << nodesRemaining.size() << std::endl;

            
            std::set<hlim::BaseNode*, CompareById> loopNodes = nodesRemaining;
            while (true) {
                std::set<hlim::BaseNode*, CompareById> tmp = std::move(loopNodes);
                loopNodes.clear();

                bool done = true;
                for (auto* n : tmp) {
                    bool anyDrivenInLoop = false;
                    for (auto i : utils::Range(n->getNumOutputPorts()))
                        for (auto nh : n->exploreOutput(i)) {
                            if (!nh.isSignal()) {
                                if (tmp.contains(nh.node())) {
                                    anyDrivenInLoop = true;
                                    break;
                                }
                                nh.backtrack();
                            }
                        }

                    if (anyDrivenInLoop)
                        loopNodes.insert(n);
                    else
                        done = false;
                }

                if (done) break;
            }


            auto& nonConstCircuit = const_cast<hlim::Circuit&>(circuit);

            auto* loopGroup = nonConstCircuit.getRootNodeGroup()->addChildNodeGroup(hlim::NodeGroup::GroupType::ENTITY);
            loopGroup->setInstanceName("loopGroup");
            loopGroup->setName("loopGroup");

            hlim::Subnet loopSubnet;

            for (auto node : loopNodes) {
                std::cout << node->getName() << " in group " << node->getGroup()->getName() << " - " << std::dec << node->getId() << " -  " << node->getTypeName() << "  " << std::hex << (size_t)node << std::endl;
                for (auto i : utils::Range(node->getNumInputPorts())) {
                    auto driver = node->getNonSignalDriver(i);
                    while (dynamic_cast<hlim::Node_ExportOverride*>(driver.node)) // Skip all export override nodes
                        driver = driver.node->getNonSignalDriver(hlim::Node_ExportOverride::SIM_INPUT);
                    if (driver.node != nullptr && !outputsReady.contains(driver)) {
                        std::cout << "    Input " << i << " not ready." << std::endl;
                        std::cout << "        " << driver.node->getName() << "  " << driver.node->getTypeName() << "  " << std::hex << (size_t)driver.node << std::endl;
                    }
                }
                std::cout << "  stack trace:" << std::endl << node->getStackTrace() << std::endl;

                node->moveToGroup(loopGroup);
                loopSubnet.add(node);

                for (auto i : utils::Range(node->getNumOutputPorts()))
                    for (auto nh : node->exploreOutput(i)) {
                        if (nh.isSignal()) {
                            nh.node()->moveToGroup(loopGroup);
                            loopSubnet.add(nh.node());
                        } 
                        else
                            nh.backtrack();
                    }
            }

            {
                DotExport exp("loop.dot");
                exp(circuit);
                exp.runGraphViz("loop.svg");
            }
            {
                //loopSubnet.dilate(true, true);

                DotExport exp("loop_only.dot");
                exp(circuit, loopSubnet.asConst());
                exp.runGraphViz("loop_only.svg");
            }
            {
                hlim::Subnet all;
                for (auto n : nodes)
                    all.add(n);


                DotExport exp("all.dot");
                exp(circuit, all.asConst());
                exp.runGraphViz("all.svg");
            }
        }

        HCL_DESIGNCHECK_HINT(readyNode != nullptr, "Cyclic dependency!");

        nodesRemaining.erase(readyNode);

        MappedNode mappedNode;
        mappedNode.node = readyNode;
        mappedNode.internal = m_stateMapping.nodeToInternalOffset[readyNode];
        for (auto i : utils::Range(readyNode->getNumInputPorts())) {
            auto driver = readyNode->getNonSignalDriver(i);
            mappedNode.inputs.push_back(m_stateMapping.outputToOffset[driver]);
        }

        for (auto i : utils::Range(readyNode->getNumOutputPorts()))
            mappedNode.outputs.push_back(m_stateMapping.outputToOffset[{.node = readyNode, .port = i}]);

        execBlock.addStep(std::move(mappedNode));

        for (auto i : utils::Range(readyNode->getNumOutputPorts())) {
            hlim::NodePort driver;
            driver.node = readyNode;
            driver.port = i;
            outputsReady.insert(driver);
        }
    }

}

void Program::allocateSignals(const hlim::Circuit &circuit, const hlim::Subnet &nodes)
{
    m_stateMapping.clear();

    BitAllocator allocator;

    struct ReferringNode {
        hlim::BaseNode* node;
        std::vector<std::pair<hlim::BaseNode*, size_t>> refs;
        size_t internalSizeOffset;
    };

    std::vector<ReferringNode> referringNodes;


    // First, loop through all nodes and allocate state and output state space.
    // Keep a list of nodes that refer to other node's internal state to fill in once all internal state has been allocated.
    for (auto node : nodes) {
        // Signals simply point to the actual producer's output, as do export overrides
        if (dynamic_cast<hlim::Node_Signal*>(node) || dynamic_cast<hlim::Node_ExportOverride*>(node)) {
            hlim::NodePort driver;
            if (dynamic_cast<hlim::Node_Signal*>(node))
                driver = node->getNonSignalDriver(0);
            else
                driver = node->getNonSignalDriver(hlim::Node_ExportOverride::SIM_INPUT);
            while (dynamic_cast<hlim::Node_ExportOverride*>(driver.node))
                driver = driver.node->getNonSignalDriver(hlim::Node_ExportOverride::SIM_INPUT);

            size_t width = node->getOutputConnectionType(0).width;

            if (driver.node != nullptr) {
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    auto offset = allocator.allocate(width);
                    m_stateMapping.outputToOffset[driver] = offset;
                    m_stateMapping.outputToOffset[{.node = node, .port = 0ull}] = offset;
                } else {
                    // point to same output port
                    m_stateMapping.outputToOffset[{.node = node, .port = 0ull}] = it->second;
                }
            }
        } else {
            std::vector<size_t> internalSizes = node->getInternalStateSizes();
            ReferringNode refNode;
            refNode.node = node;
            refNode.refs = node->getReferencedInternalStateSizes();
            refNode.internalSizeOffset = internalSizes.size();

            std::vector<size_t> internalOffsets(internalSizes.size() + refNode.refs.size());
            for (auto i : utils::Range(internalSizes.size()))
                internalOffsets[i] = allocator.allocate(internalSizes[i]);
            m_stateMapping.nodeToInternalOffset[node] = std::move(internalOffsets);

            for (auto i : utils::Range(node->getNumOutputPorts())) {
                hlim::NodePort driver = {.node = node, .port = i};
                auto it = m_stateMapping.outputToOffset.find(driver);
                if (it == m_stateMapping.outputToOffset.end()) {
                    size_t width = node->getOutputConnectionType(i).width;
                    m_stateMapping.outputToOffset[driver] = allocator.allocate(width);
                }
            }

            if (!refNode.refs.empty())
                referringNodes.push_back(refNode);
        }
    }

    // Now that all internal states have been allocated, update referring nodes
    for (auto &refNode : referringNodes) {
        auto &mappedInternal = m_stateMapping.nodeToInternalOffset[refNode.node];
        for (auto i : utils::Range(refNode.refs.size())) {
            auto &ref = refNode.refs[i];
            auto refedIdx = m_stateMapping.nodeToInternalOffset[ref.first][ref.second];
            mappedInternal[refNode.internalSizeOffset+i] = refedIdx;
        }
    }


    m_fullStateWidth = allocator.getTotalSize();
}



ReferenceSimulator::ReferenceSimulator()
{
}

void ReferenceSimulator::compileProgram(const hlim::Circuit &circuit, const std::set<hlim::NodePort> &outputs, bool ignoreSimulationProcesses)
{

    if (!ignoreSimulationProcesses)
        for (const auto &simProc : circuit.getSimulationProcesses())
            addSimulationProcess(simProc);

    auto nodes = hlim::Subnet::allForSimulation(const_cast<hlim::Circuit&>(circuit), outputs);

    m_program.compileProgram(circuit, nodes);
}


void ReferenceSimulator::compileStaticEvaluation(const hlim::Circuit& circuit, const std::set<hlim::NodePort>& outputs)
{
    hlim::Subnet nodeSet;
    {
        std::vector<hlim::BaseNode*> stack;
        for (auto nodePort : outputs)
            stack.push_back(nodePort.node);

        while (!stack.empty()) {
            hlim::BaseNode* node = stack.back();
            stack.pop_back();
            if (!nodeSet.contains(node)) {
                // Ignore the export-only part as well as the export node
                if (dynamic_cast<hlim::Node_ExportOverride*>(node)) {
                    if (node->getDriver(hlim::Node_ExportOverride::SIM_INPUT).node != nullptr)
                        stack.push_back(node->getDriver(hlim::Node_ExportOverride::SIM_INPUT).node);
                } else if (dynamic_cast<hlim::Node_Register*>(node)) { // add registers but stop there
                    nodeSet.add(node);
                } else {
                    nodeSet.add(node);
                    for (auto i : utils::Range(node->getNumInputPorts()))
                        if (node->getDriver(i).node != nullptr)
                            stack.push_back(node->getDriver(i).node);
                }
            }
        }
    }
    m_program.compileProgram(circuit, nodeSet);
}


void ReferenceSimulator::powerOn()
{
    m_simulationTime = 0;
    m_dataState.signalState.resize(m_program.m_fullStateWidth);

    m_dataState.signalState.clearRange(DefaultConfig::VALUE, 0, m_program.m_fullStateWidth);
    m_dataState.signalState.clearRange(DefaultConfig::DEFINED, 0, m_program.m_fullStateWidth);

    for (const auto &mappedNode : m_program.m_powerOnNodes)
        mappedNode.node->simulatePowerOn(m_callbackDispatcher, m_dataState.signalState, mappedNode.internal.data(), mappedNode.outputs.data());

    m_dataState.clockState.resize(m_program.m_stateMapping.clockPinAllocation.clockPins.size());
    m_dataState.resetState.resize(m_program.m_stateMapping.clockPinAllocation.resetPins.size());
    for (auto i : utils::Range(m_program.m_stateMapping.clockPinAllocation.clockPins.size())) {
        auto clock = m_program.m_stateMapping.clockPinAllocation.clockPins[i].source;
        
        auto &cs = m_dataState.clockState[i];
        auto trigType = clock->getTriggerEvent();

        cs.high = trigType == hlim::Clock::TriggerEvent::RISING;

        Event e;
        e.type = Event::Type::clock;
        e.clockEvt.clock = clock;
        e.clockEvt.clockDomainIdx = i;
        e.clockEvt.risingEdge = !cs.high;
        e.timeOfEvent = m_simulationTime + hlim::ClockRational(1,2) / clock->absoluteFrequency();

        if (trigType == hlim::Clock::TriggerEvent::RISING_AND_FALLING ||
            (trigType == hlim::Clock::TriggerEvent::RISING && e.clockEvt.risingEdge) ||
            (trigType == hlim::Clock::TriggerEvent::FALLING && !e.clockEvt.risingEdge)) {

            cs.nextTrigger = e.timeOfEvent;
        } else
            cs.nextTrigger = e.timeOfEvent + hlim::ClockRational(1,2) / clock->absoluteFrequency();

        m_nextEvents.push(e);
    }


    bool resetsInFlight = false;
    for (auto i : utils::Range(m_program.m_stateMapping.clockPinAllocation.resetPins.size())) {
        auto clock = m_program.m_stateMapping.clockPinAllocation.resetPins[i].source;

        auto &rs = m_dataState.resetState[i];
        auto &rstDom = m_program.m_resetDomains[i];

        rs.resetHigh = clock->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH;

        for (auto &cn : rstDom.clockedNodes)
            cn.changeReset(m_callbackDispatcher, m_dataState, rs.resetHigh);
        m_callbackDispatcher.onReset(clock, rs.resetHigh);


        // Deactivate reset
        auto minTime = m_program.m_stateMapping.clockPinAllocation.resetPins[i].minResetTime;
        auto minCycles = m_program.m_stateMapping.clockPinAllocation.resetPins[i].minResetCycles;
        auto minCyclesTime = hlim::ClockRational(minCycles, 1) / clock->absoluteFrequency();
        
        minTime = std::max(minTime, minCyclesTime);
        if (minTime == hlim::ClockRational(0, 1)) {
            // Immediately disable again
            rs.resetHigh = !rs.resetHigh;
            for (auto &cn : rstDom.clockedNodes)
                cn.changeReset(m_callbackDispatcher, m_dataState, rs.resetHigh);
            m_callbackDispatcher.onReset(clock, rs.resetHigh);
        } else {
            // Schedule disabling
            resetsInFlight = true;
            Event e;
            e.type = Event::Type::reset;
            e.resetEvt.clock = clock;
            e.resetEvt.resetDomainIdx = i;
            e.resetEvt.newResetHigh = !rs.resetHigh;
            e.timeOfEvent = m_simulationTime + minTime;

            m_nextEvents.push(e);
        }
    }    

    // reevaluate, to provide fibers with power-on state
    reevaluate();

    m_callbackDispatcher.onPowerOn();

    HCL_ASSERT_HINT(m_program.m_stateMapping.clockPinAllocation.resetPins.size() < 2, "For now, only one reset is supported!");

    if (!resetsInFlight) {
        // For now, start fibers after reset
        {
            RunTimeSimulationContext context(this);
            // start all fibers
            m_runningSimProcs.clear();
            for (auto &f : m_simProcs) {
                m_runningSimProcs.push_back(f());
                m_runningSimProcs.back().resume();
            }
        }

        if (m_stateNeedsReevaluating)
            reevaluate();
    }
}

void ReferenceSimulator::reevaluate()
{
    /// @todo respect dependencies between blocks (once they are being expressed and made use of)
    for (auto &block : m_program.m_executionBlocks)
        block.evaluate(m_callbackDispatcher, m_dataState);

    m_stateNeedsReevaluating = false;
}

void ReferenceSimulator::commitState()
{
    for (auto &block : m_program.m_executionBlocks)
        block.commitState(m_callbackDispatcher, m_dataState);

    m_callbackDispatcher.onCommitState();
}

void ReferenceSimulator::advanceEvent()
{
    m_abortCalled = false;

    if (m_nextEvents.empty()) return;

    if (m_currentTimeStepFinished) {
        commitState();
        m_simulationTime = m_nextEvents.top().timeOfEvent;
        m_callbackDispatcher.onNewTick(m_simulationTime);
    }

    while (!m_nextEvents.empty() && m_nextEvents.top().timeOfEvent == m_simulationTime) { // outer loop because fibers can do a waitFor(0) in which we need to run again.
        std::set<size_t> triggeredExecutionBlocks;
        std::vector<std::coroutine_handle<>> simProcsResuming;
        while (!m_nextEvents.empty() && m_nextEvents.top().timeOfEvent == m_simulationTime) {
            auto event = m_nextEvents.top();
            m_nextEvents.pop();

            switch (event.type) {
                case Event::Type::reset: {
                    auto &rstEvent = event.resetEvt;
                    m_dataState.resetState[rstEvent.resetDomainIdx].resetHigh = rstEvent.newResetHigh;

                    auto &rstDom = m_program.m_resetDomains[rstEvent.resetDomainIdx];

                    for (auto id : rstDom.dependentExecutionBlocks)
                        triggeredExecutionBlocks.insert(id);

                    for (auto &cn : rstDom.clockedNodes)
                        cn.changeReset(m_callbackDispatcher, m_dataState, rstEvent.newResetHigh);

                    m_callbackDispatcher.onReset(rstEvent.clock, rstEvent.newResetHigh);

                    if (rstEvent.newResetHigh ^ (rstEvent.clock->getRegAttribs().resetActive == hlim::RegisterAttributes::Active::HIGH)) {
                        // For now, start fibers after reset
                        {
                            RunTimeSimulationContext context(this);
                            // start all fibers
                            m_runningSimProcs.clear();
                            for (auto &f : m_simProcs) {
                                m_runningSimProcs.push_back(f());
                                m_runningSimProcs.back().resume();
                            }
                        }
                        if (m_stateNeedsReevaluating)
                            reevaluate();
                    }
                } break;
                case Event::Type::clock: {
                    auto &clkEvent = event.clockEvt;
                    auto &cs = m_dataState.clockState[clkEvent.clockDomainIdx];
                    cs.high = clkEvent.risingEdge;

                    auto trigType = clkEvent.clock->getTriggerEvent();
                    if (trigType == hlim::Clock::TriggerEvent::RISING_AND_FALLING ||
                        (trigType == hlim::Clock::TriggerEvent::RISING && clkEvent.risingEdge) ||
                        (trigType == hlim::Clock::TriggerEvent::FALLING && !clkEvent.risingEdge)) {

                        auto &clkDom = m_program.m_clockDomains[clkEvent.clockDomainIdx];

                        for (auto id : clkDom.dependentExecutionBlocks)
                            triggeredExecutionBlocks.insert(id);

                        for (auto &cn : clkDom.clockedNodes)
                            cn.advance(m_callbackDispatcher, m_dataState);

                        cs.nextTrigger = event.timeOfEvent + hlim::ClockRational(1) / clkEvent.clock->absoluteFrequency();
                    }
                    m_callbackDispatcher.onClock(clkEvent.clock, clkEvent.risingEdge);

                    clkEvent.risingEdge = !clkEvent.risingEdge;
                    event.timeOfEvent += hlim::ClockRational(1,2) / clkEvent.clock->absoluteFrequency();
                    m_nextEvents.push(event);
                } break;
                case Event::Type::simProcResume: {
                    simProcsResuming.push_back(event.simProcResumeEvt.handle);
                } break;
                default:
                    HCL_ASSERT_HINT(false, "Not implemented!");
            }
        }

        /// @todo respect dependencies between blocks (once they are being expressed and made use of)
        for (auto idx : triggeredExecutionBlocks)
            m_program.m_executionBlocks[idx].evaluate(m_callbackDispatcher, m_dataState);

        {
            RunTimeSimulationContext context(this);
            for (auto &handle : simProcsResuming) {
                HCL_ASSERT(handle);
                handle.resume();

                if (m_abortCalled)
                    return;
            }
        }

        if (m_stateNeedsReevaluating)
            reevaluate();
    }

    m_currentTimeStepFinished = true;
}

void ReferenceSimulator::advance(hlim::ClockRational seconds)
{
    hlim::ClockRational targetTime = m_simulationTime + seconds;

    while (hlim::clockLess(m_simulationTime, targetTime) && !m_abortCalled) {
        if (m_nextEvents.empty()) {
            m_simulationTime = targetTime;
            return;
        }

        auto &nextEvent = m_nextEvents.top();
        if (nextEvent.timeOfEvent > targetTime) {
            m_simulationTime = targetTime;
            break;
        } else
            advanceEvent();
    }
}


void ReferenceSimulator::simProcSetInputPin(hlim::Node_Pin *pin, const DefaultBitVectorState &state)
{
    auto it = m_program.m_stateMapping.nodeToInternalOffset.find(pin);
    HCL_ASSERT(it != m_program.m_stateMapping.nodeToInternalOffset.end());
    pin->setState(m_dataState.signalState, it->second.data(), state);
    m_stateNeedsReevaluating = true;
    m_callbackDispatcher.onSimProcOutputOverridden({.node=pin, .port=0}, state);
}


DefaultBitVectorState ReferenceSimulator::simProcGetValueOfOutput(const hlim::NodePort &nodePort)
{
    auto value = getValueOfOutput(nodePort);
    m_callbackDispatcher.onSimProcOutputRead(nodePort, value);
    return value;
}


bool ReferenceSimulator::outputOptimizedAway(const hlim::NodePort &nodePort)
{
    return !m_program.m_stateMapping.nodeToInternalOffset.contains(nodePort.node);
}


DefaultBitVectorState ReferenceSimulator::getValueOfInternalState(const hlim::BaseNode *node, size_t idx)
{
    if (m_stateNeedsReevaluating)
        reevaluate();


    DefaultBitVectorState value;
    auto it = m_program.m_stateMapping.nodeToInternalOffset.find((hlim::BaseNode *) node);
    if (it == m_program.m_stateMapping.nodeToInternalOffset.end()) {
        value.resize(0);
    } else {
        size_t width = node->getInternalStateSizes()[idx];
        value = m_dataState.signalState.extract(it->second[idx], width);
    }
    return value;
}

DefaultBitVectorState ReferenceSimulator::getValueOfOutput(const hlim::NodePort &nodePort)
{
    if (m_stateNeedsReevaluating)
        reevaluate();

    auto it = m_program.m_stateMapping.outputToOffset.find(nodePort);
    if (it == m_program.m_stateMapping.outputToOffset.end()) {
        DefaultBitVectorState value;
        value.resize(0);
        return value;
    } else {
        size_t width = nodePort.node->getOutputConnectionType(nodePort.port).width;
        return m_dataState.signalState.extract(it->second, width);
    }
}

std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfClock(const hlim::Clock *clk)
{
    std::array<bool, DefaultConfig::NUM_PLANES> res;

    auto it = m_program.m_stateMapping.clockPinAllocation.clock2ClockPinIdx.find((hlim::Clock *)clk);
    if (it == m_program.m_stateMapping.clockPinAllocation.clock2ClockPinIdx.end()) {
        res[DefaultConfig::DEFINED] = false;
        return res;
    }

    res[DefaultConfig::DEFINED] = true;
    res[DefaultConfig::VALUE] = m_dataState.clockState[it->second].high;
    return res;
}

std::array<bool, DefaultConfig::NUM_PLANES> ReferenceSimulator::getValueOfReset(const hlim::Clock *clk)
{
    std::array<bool, DefaultConfig::NUM_PLANES> res;

    auto it = m_program.m_stateMapping.clockPinAllocation.clock2ResetPinIdx.find((hlim::Clock *)clk);
    if (it == m_program.m_stateMapping.clockPinAllocation.clock2ResetPinIdx.end()) {
        res[DefaultConfig::DEFINED] = false;
        return res;
    }

    res[DefaultConfig::DEFINED] = true;
    res[DefaultConfig::VALUE] = m_dataState.resetState[it->second].resetHigh;
    return res;
}

void ReferenceSimulator::addSimulationProcess(std::function<SimulationProcess()> simProc)
{
    m_simProcs.push_back(std::move(simProc));
}

void ReferenceSimulator::simulationProcessSuspending(std::coroutine_handle<> handle, WaitFor &waitFor, utils::RestrictTo<RunTimeSimulationContext>)
{
    HCL_ASSERT(handle);
    Event e;
    e.type = Event::Type::simProcResume;
    e.timeOfEvent = m_simulationTime + waitFor.getDuration();
    e.simProcResumeEvt.handle = handle;
    e.simProcResumeEvt.insertionId = m_nextSimProcInsertionId++;
    m_nextEvents.push(e);
}

void ReferenceSimulator::simulationProcessSuspending(std::coroutine_handle<> handle, WaitUntil &waitUntil, utils::RestrictTo<RunTimeSimulationContext>)
{
    HCL_ASSERT_HINT(false, "Not implemented yet!");
}


void ReferenceSimulator::simulationProcessSuspending(std::coroutine_handle<> handle, WaitClock &waitClock, utils::RestrictTo<RunTimeSimulationContext>)
{
    auto it = m_program.m_stateMapping.clockPinAllocation.clock2ClockPinIdx.find(const_cast<hlim::Clock*>(waitClock.getClock()));

    if (it == m_program.m_stateMapping.clockPinAllocation.clock2ClockPinIdx.end()) {
        // This clock is not part of the simulation, so just wait for as long as it would take for the next tick to arrive if it was there.

        size_t ticksSoFar = hlim::floor(m_simulationTime * waitClock.getClock()->absoluteFrequency());
        size_t nextTick = ticksSoFar + 1;

        auto nextTickTime = hlim::ClockRational(nextTick, 1) / waitClock.getClock()->absoluteFrequency();

        Event e;
        e.type = Event::Type::simProcResume;
        e.timeOfEvent = nextTickTime;
        e.simProcResumeEvt.handle = handle;
        e.simProcResumeEvt.insertionId = m_nextSimProcInsertionId++;
        m_nextEvents.push(e);
    } else {
        Event e;
        e.type = Event::Type::simProcResume;
        e.timeOfEvent = m_dataState.clockState[it->second].nextTrigger;
        e.simProcResumeEvt.handle = handle;
        e.simProcResumeEvt.insertionId = m_nextSimProcInsertionId++;
        m_nextEvents.push(e);
    }
}



}
