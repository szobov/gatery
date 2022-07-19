/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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
#include "Node_Signal2Rst.h"

namespace gtry::hlim {

Node_Signal2Rst::Node_Signal2Rst() : Node(1, 0)
{
	m_clocks.resize(1);
}

void Node_Signal2Rst::connect(const NodePort &np)
{
	HCL_ASSERT(hlim::getOutputConnectionType(np).interpretation == ConnectionType::BOOL);
	connectInput(0, np);
}

std::string Node_Signal2Rst::getTypeName() const
{
	return "signal2rst";
}

void Node_Signal2Rst::assertValidity() const
{
}

std::string Node_Signal2Rst::getInputName(size_t idx) const
{
	return "rst";
}

std::string Node_Signal2Rst::getOutputName(size_t idx) const
{
	return "";
}

void Node_Signal2Rst::setClock(Clock *clk)
{
	attachClock(clk, 0);
}

std::unique_ptr<BaseNode> Node_Signal2Rst::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new Node_Signal2Rst());
	copyBaseToClone(res.get());
	return res;
}


}
