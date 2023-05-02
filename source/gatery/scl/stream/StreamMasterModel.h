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
#include <gatery/frontend.h>
#include "Packet.h"


namespace gtry::scl
{
	class PacketStreamMasterModel
	{

	public:

		void init(BitWidth payloadW, bool debug = false);

		void probability(float valid);

		SimFunction<TransactionIn> request(TransactionOut tx, const Clock& clk);
		SimProcess idle(size_t requestsPending = 0);

		SimFunction<bool> push(uint64_t address, uint64_t logByteSize, uint64_t data, const Clock& clk);

		auto& getLink() { return m_link; }

	protected:
		SimFunction<size_t> allocSourceId(const Clock& clk);

		std::tuple<size_t, size_t> prepareTransaction(TransactionOut& tx) const;

	private:
		size_t m_requestCurrent = 0;
		size_t m_requestNext = 0;
		Condition m_requestCurrentChanged;

		float m_validProbability = 1;
		float m_readyProbability = 1;
		std::vector<bool> m_sourceInUse;
		std::mt19937 m_rng;
		std::uniform_real_distribution<float> m_dis;
	};
}
