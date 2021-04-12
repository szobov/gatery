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
#pragma once

#include "NodeIO.h"
#include "ClockRational.h"


#include <string>
#include <memory>

namespace hcl::core::hlim {

class Clock
{
    public:
        enum class TriggerEvent {
            RISING,
            FALLING,
            RISING_AND_FALLING
        };
        
        enum class ResetType {
            SYNCHRONOUS,
            ASYNCHRONOUS,
            NONE
        };
        
        Clock();
        virtual ~Clock();
        
        virtual ClockRational getAbsoluteFrequency() = 0;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) = 0;
        
        inline Clock *getParentClock() const { return m_parentClock; }
        
//        Clock &createClockDivider(ClockRational frequencyDivider, ClockRational phaseShift = 0);
        
        inline const std::string &getName() const { return m_name; }        
        inline const std::string &getResetName() const { return m_resetName; }
        inline const TriggerEvent &getTriggerEvent() const { return m_triggerEvent; }
        inline const ResetType &getResetType() const { return m_resetType; }
        inline const bool &getInitializeRegs() const { return m_initializeRegs; }
        inline const bool &getResetHighActive() const { return m_resetHighActive; }
        inline const bool &getPhaseSynchronousWithParent() const { return m_phaseSynchronousWithParent; }
        
        inline void setName(std::string name) { m_name = std::move(name); }
        inline void setResetName(std::string name) { m_resetName = std::move(name); }
        inline void setTriggerEvent(TriggerEvent trigEvt) { m_triggerEvent = trigEvt; }
        inline void setResetType(ResetType rstType) { m_resetType = rstType; }
        inline void setInitializeRegs(bool initializeRegs) { m_initializeRegs = initializeRegs; }
        inline void setResetHighActive(bool rstHigh) { m_resetHighActive = rstHigh; }
        inline void setPhaseSynchronousWithParent(bool phaseSync) { m_phaseSynchronousWithParent = phaseSync; }
        
        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent);
    protected:
        Clock *m_parentClock = nullptr;

        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) = 0;
        
        std::string m_name;
        
        std::string m_resetName;
        TriggerEvent m_triggerEvent;
        ResetType m_resetType;
        bool m_initializeRegs;
        bool m_resetHighActive;
        bool m_phaseSynchronousWithParent;
        // todo:
        /*
            * clock enable
            * clock disable high/low
            * clock2signal
            */
        
        std::vector<NodePort> m_clockedNodes;
        friend class BaseNode;        
};

class RootClock : public Clock
{
    public:
        RootClock(std::string name, ClockRational frequency);
        
        virtual ClockRational getAbsoluteFrequency() override { return m_frequency; }
        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        
        void setFrequency(ClockRational frequency) { m_frequency = m_frequency; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_frequency;
};

class DerivedClock : public Clock
{
    public:
        DerivedClock(Clock *parentClock);
        
        virtual ClockRational getAbsoluteFrequency() override;
        virtual ClockRational getFrequencyRelativeTo(Clock &other) override;
        
        inline void setFrequencyMuliplier(ClockRational m) { m_parentRelativeMultiplicator = m; }

        virtual std::unique_ptr<Clock> cloneUnconnected(Clock *newParent) override;
    protected:
        virtual std::unique_ptr<Clock> allocateClone(Clock *newParent) override;

        ClockRational m_parentRelativeMultiplicator;
};


}
