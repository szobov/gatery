/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"

#include "../hlim/Node.h"

namespace hcl::hlim {
    
Clock::Clock()
{
    m_name = "clk";
    m_resetName = "reset";
    m_triggerEvent = TriggerEvent::RISING;
    m_resetType = ResetType::SYNCHRONOUS;
    m_initializeRegs = true;
    m_resetHighActive = true;
    m_phaseSynchronousWithParent = false;
    m_parentClock = nullptr;
}

    
Clock::~Clock()
{
    while (!m_clockedNodes.empty())
        m_clockedNodes.front().node->detachClock(m_clockedNodes.front().port);
}
/*
Clock &Clock::createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0)
{
    assert(false);
    return 
}
*/

std::unique_ptr<Clock> Clock::cloneUnconnected(Clock *newParent)
{
    std::unique_ptr<Clock> res = allocateClone(newParent);
    res->m_name = m_name;
    res->m_resetName = m_resetName;
    res->m_triggerEvent = m_triggerEvent;
    res->m_resetType = m_resetType;
    res->m_initializeRegs = m_initializeRegs;
    res->m_resetHighActive = m_resetHighActive;
    res->m_phaseSynchronousWithParent = m_phaseSynchronousWithParent;
    return res;
}

RootClock::RootClock(std::string name, ClockRational frequency) : m_frequency(frequency)
{
    m_name = std::move(name);
}
    

ClockRational RootClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}


std::unique_ptr<Clock> RootClock::cloneUnconnected(Clock *newParent)
{
    HCL_ASSERT(newParent == nullptr);
    std::unique_ptr<Clock> res = Clock::cloneUnconnected(newParent);
    ((RootClock*)res.get())->m_frequency = m_frequency;
    return res;
}

std::unique_ptr<Clock> RootClock::allocateClone(Clock *newParent)
{
    return std::unique_ptr<Clock>(new RootClock(m_name, m_frequency));
}

    
DerivedClock::DerivedClock(Clock *parentClock)
{
    m_parentClock = parentClock;
    m_parentRelativeMultiplicator = 1;
    
    m_name = m_parentClock->getName();
    
    m_resetName = m_parentClock->getResetName();
    m_triggerEvent = m_parentClock->getTriggerEvent();
    m_resetType = m_parentClock->getResetType();
    m_resetHighActive = m_parentClock->getResetHighActive();
    m_phaseSynchronousWithParent = m_parentClock->getPhaseSynchronousWithParent();
}

    
ClockRational DerivedClock::getAbsoluteFrequency()
{
    return m_parentClock->getAbsoluteFrequency() * m_parentRelativeMultiplicator;
}

ClockRational DerivedClock::getFrequencyRelativeTo(Clock &other)
{
    return {};
}


std::unique_ptr<Clock> DerivedClock::cloneUnconnected(Clock *newParent)
{
    std::unique_ptr<Clock> res = Clock::cloneUnconnected(newParent);
    ((DerivedClock*)res.get())->m_parentRelativeMultiplicator = m_parentRelativeMultiplicator;
    return res;
}

std::unique_ptr<Clock> DerivedClock::allocateClone(Clock *newParent)
{
    return std::unique_ptr<Clock>(new DerivedClock(newParent));
}

        
}
