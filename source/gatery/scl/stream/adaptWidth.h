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
#include "Packet.h"
#include "../Counter.h"

namespace gtry::scl
{
	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>)
	auto extendWidth(T& source, BitWidth width, Bit reset = '0');

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>
				and T::template has<Ready>())
	T reduceWidth(T& source, BitWidth width, Bit reset = '0');

	template<StreamSignal T> 
	requires (T::template has<Ready>() and T::template has<Valid>())
	T eraseBeat(T& source, UInt beatOffset, UInt beatCount);

	template<StreamSignal T, SignalValue Tval> 
	requires (T::template has<Ready>())
	T insertBeat(T& source, UInt beatOffset, const Tval& value);

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T& source, Bit stallCondition);

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stallPacket(T& source, Bit stallCondition);
}

namespace gtry::scl
{
	template<BaseSignal T>
	T makeShiftReg(BitWidth size, const T& in, const Bit& en)
	{
		T value = size;

		T newValue = value >> (int)in.width().bits();
		newValue.upper(in.width()) = in;

		IF(en)
			value = newValue;
		value = reg(value);
		return newValue;
	}

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>)
	auto extendWidth(T& source, BitWidth width, Bit reset)
	{
		HCL_DESIGNCHECK(source->width() <= width);
		const size_t ratio = width / source->width();

		auto scope = Area{ "scl_extendWidth" }.enter();

		Counter counter{ ratio };
		IF(transfer(source))
			counter.inc();
		IF(reset)
			counter.reset();

		auto ret = source.add(
			Valid{ counter.isLast() & valid(source) }
		);
		if constexpr (T::template has<Ready>())
			ready(source) = ready(ret) | !counter.isLast();

		ret->resetNode();
		*ret = makeShiftReg(width, *source, transfer(source));

		if constexpr (T::template has<ByteEnable>())
		{
			auto& be = byteEnable(ret);
			BitWidth srcBeWidth = be.width();
			be.resetNode();
			be = makeShiftReg(srcBeWidth * ratio, byteEnable(source), transfer(source));
		}

		HCL_NAMED(ret);
		return ret;
	}

	template<StreamSignal T>
	requires (std::is_base_of_v<BaseBitVector, typename T::Payload>
				and T::template has<Ready>())
	T reduceWidth(T& source, BitWidth width, Bit reset)
	{
		auto scope = Area{ "scl_reduceWidth" }.enter();
		T out;

		HCL_DESIGNCHECK(source->width() >= width);
		const size_t ratio = source->width() / width;

		Counter counter{ ratio };
		IF(transfer(out))
			counter.inc();
		IF(!valid(source) | reset)
			counter.reset();

		out <<= source;
		ready(source) &= counter.isLast();

		out->resetNode();
		*out = (*source)(zext(counter.value(), +width) * width.bits(), width);

		if constexpr (out.template has<ByteEnable>())
		{
			BVec& be = byteEnable(out);
			BitWidth w = be.width() / ratio;
			be.resetNode();
			be = byteEnable(source)(zext(counter.value(), +w) * w.bits(), w);
		}

		if constexpr (out.template has<Eop>())
			eop(out) &= counter.isLast();
		if constexpr (out.template has<Sop>())
			sop(out) &= counter.isFirst();

		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T> 
	requires (T::template has<Ready>() and T::template has<Valid>())
	T eraseBeat(T& source, UInt beatOffset, UInt beatCount)
	{
		auto scope = Area{ "scl_eraseBeat" }.enter();

		BitWidth beatLimit = std::max(beatOffset.width(), beatCount.width()) + 1;
		Counter beatCounter{ beatLimit.count() };
		IF(transfer(source))
		{
			IF(beatCounter.value() < zext(beatOffset + beatCount))
				beatCounter.inc();
			IF(eop(source))
				beatCounter.reset();
		}

		T out;
		out <<= source;

		IF(beatCounter.value() >= zext(beatOffset) & 
			beatCounter.value() < zext(beatOffset + beatCount))
		{
			valid(out) = '0';
			ready(source) = '1';
		}
		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T, SignalValue Tval> 
	requires (T::template has<Ready>())
	T insertBeat(T& source, UInt beatOffset, const Tval& value)
	{
		auto scope = Area{ "scl_insertBeat" }.enter();
		T out;
		out <<= source;

		Counter beatCounter{ (beatOffset.width() + 1).count() };
		IF(transfer(out))
		{
			IF(beatCounter.value() < zext(beatOffset + 1))
				beatCounter.inc();
			IF(eop(source) & beatCounter.value() != zext(beatOffset))
				beatCounter.reset();
		}

		IF(beatCounter.value() == zext(beatOffset))
		{
			*out = value;
			ready(source) = '0';
			eop(out) = '0';
		}
		HCL_NAMED(out);
		return out;
	}

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stall(T& source, Bit stallCondition)
	{
		T out;
		out <<= source;

		IF(stallCondition)
		{
			valid(out) = '0';
			ready(source) = '0';
		}
		return out;
	}

	template<StreamSignal T>
	requires (T::template has<Ready>() and T::template has<Valid>())
	T stallPacket(T& source, Bit stallCondition)
	{
		return stall(source, stallCondition & sop(source));
	}
}
