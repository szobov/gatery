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

#include "../../simulation/SimulatorCallbacks.h"

#include <string>
#include <vector>

namespace gtry::vhdl {

class BaseTestbenchRecorder : public sim::SimulatorCallbacks
{
    public:
        //BaseTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, const std::string &name);
        virtual ~BaseTestbenchRecorder();

		inline const std::vector<std::string> &getDependencySortedEntities() const { return m_dependencySortedEntities; }
		inline const std::vector<std::string> &getAuxiliaryDataFiles() const { return m_auxiliaryDataFiles; }
	protected:
		std::vector<std::string> m_dependencySortedEntities;
		std::vector<std::string> m_auxiliaryDataFiles;
};


}