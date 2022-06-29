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
#include <optional>

namespace gtry::scl
{
	template<Signal Payload>
	struct Stream
	{
		Reverse<Bit> ready;
		Bit valid;
		Payload data;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		decltype(auto) operator ->() {
			if constexpr(requires(Payload & p) { p.operator->(); })
				return (Payload&)data;
			else
				return &data;
		}

		decltype(auto) operator ->() const
		{
			if constexpr(requires(Payload & p) { p.operator->(); })
				return (const Payload&)data;
			else
				return &data;
		}
	};

	template<Signal T> Bit transfer(const Stream<T>& stream) { return stream.valid & *stream.ready; }
	template<Signal T> const Bit& ready(const Stream<T>& stream) { return *stream.ready; }
	template<Signal T> const Bit& valid(const Stream<T>& stream) { return stream.valid; }


	template<Signal Payload>
	struct DownStream
	{
		Bit valid;
		Payload data;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		decltype(auto) operator ->()
		{
			if constexpr(requires(Payload & p) { p.operator->(); })
				return (Payload&)data;
			else
				return &data;
		}

		decltype(auto) operator ->() const
		{
			if constexpr(requires(Payload & p) { p.operator->(); })
				return (const Payload&)data;
			else
				return &data;
		}
	};

	template<Signal T> const Bit& transfer(const DownStream<T>& stream) { return stream.valid & *stream.ready; }
	template<Signal T> Bit ready(const DownStream<T>& stream) { return '1'; }
	template<Signal T> const Bit& valid(const DownStream<T>& stream) { return stream.valid; }


	template<Signal Payload>
	struct Packet
	{
		Bit last;
		Payload data;

		Payload& operator *() { return data; }
		const Payload& operator *() const { return data; }

		Payload* operator ->() { return &data; }
		const Payload* operator ->() const { return &data; }
	};

	template<Signal T> Bit sop(const T& stream);
	template<Signal T> const Bit& eop(const Stream<Packet<T>>& stream) { return (*stream).last; }
	
	template<Signal T>
	Bit sop(const T& stream)
	{
		Bit sop;
		IF(transfer(stream))
			sop = '0';
		IF(eop(stream))
			sop = '1';
		sop = reg(sop, '1');
		return sop;
	}

}
