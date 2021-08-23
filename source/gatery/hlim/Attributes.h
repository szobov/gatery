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
#pragma once

#include <map>
#include <string>
#include <optional>


namespace gtry::hlim {


struct AttribValue {
	std::string type;
	std::string value;
};

typedef std::map<std::string, AttribValue> ResolvedAttributes;
typedef std::map<std::string, AttribValue> VendorSpecificAttributes;

struct Attributes {
	std::map<std::string, VendorSpecificAttributes> userDefinedVendorAttributes;

	void fuseWith(const Attributes &rhs);
};


struct SignalAttributes : public Attributes {
	/// Max fanout of this signal before it's driver is duplicated. 0 is don't care.
	std::optional<size_t> maxFanout; 
	/// Signal crosses a clock domain
	std::optional<bool> crossingClockDomain;
	/// Whether the signal may be fused away (e.g. signal between regs to shiftreg)
	std::optional<bool> allowFusing;

	void fuseWith(const SignalAttributes &rhs);
};

struct RegisterAttributes : public Attributes {
	enum class ResetType {
		SYNCHRONOUS,
		ASYNCHRONOUS,
		NONE
	};

	enum class UsageType {
		DONT_CARE,
		USE,
		DONT_USE
	};

	ResetType resetType = ResetType::SYNCHRONOUS;
	bool initializeRegs = true;
	bool resetHighActive = true;

	UsageType registerResetPinUsage = UsageType::DONT_CARE;
	UsageType registerEnablePinUsage = UsageType::DONT_CARE;
};

// all used defined attributes ignore type and value and replace $src and $dst in the attrib name with source and destination cells
struct PathAttributes : public Attributes {
	size_t multiCycle = 0; 
	bool falsePath = false;
};

struct MemoryAttributes : public Attributes {
	bool noConflicts = false;
};

}