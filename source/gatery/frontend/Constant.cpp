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
#include "Constant.h"

#include "BVec.h"
#include "UInt.h"

#include <optional>
#include <boost/spirit/home/x3.hpp>

using namespace gtry;

sim::DefaultBitVectorState gtry::parseBit(char value)
{
	HCL_DESIGNCHECK(value == '0' || value == '1' || value == 'x' || value == 'X');

	sim::DefaultBitVectorState ret;
	ret.resize(1);
	ret.set(sim::DefaultConfig::VALUE, 0, value != '0');
	ret.set(sim::DefaultConfig::DEFINED, 0, value != 'x' && value != 'X');
	return ret;
}

sim::DefaultBitVectorState gtry::parseBit(bool value)
{
	return parseBit(value ? '1' : '0');
}

sim::DefaultBitVectorState gtry::parseBitVector(std::string_view value)
{
	using namespace boost::spirit::x3;

	sim::DefaultBitVectorState ret;

	auto parseWidth = [&](auto& ctx) {
		if (_attr(ctx))
		{
			const size_t width = *_attr(ctx);
			ret.resize(width);
			ret.setRange(sim::DefaultConfig::VALUE, 0, width, false);
			ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
		}
	};

	auto parseHex = [&](size_t bps, auto& ctx) {

		std::string_view num = _attr(ctx);
		if (ret.size() == 0)
		{
			ret.resize(num.size() * bps);
		}
		else
			HCL_DESIGNCHECK_HINT(ret.size() >= num.size() * bps, "string UInt constant width is to small for its value");

		for (size_t i = 0; i < num.size(); ++i)
		{
			uint8_t value = 0;
			uint8_t defined = 0xFF;
			if (num[i] >= '0' && num[i] <= '9')
				value = num[i] - '0';
			else if (num[i] >= 'a' && num[i] <= 'f')
				value = num[i] - 'a' + 10;
			else if (num[i] >= 'A' && num[i] <= 'F')
				value = num[i] - 'A' + 10;
			else
				defined = 0;

			size_t dstIdx = num.size() - 1 - i;
			ret.insertNonStraddling(sim::DefaultConfig::VALUE, dstIdx * bps, bps, value);
			ret.insertNonStraddling(sim::DefaultConfig::DEFINED, dstIdx * bps, bps, defined);
		}
	};

	auto parseDec = [&](auto& ctx) {
		std::string_view numStr = _attr(ctx);
		uint64_t num = std::strtoull(numStr.data(), nullptr, 10);
		uint64_t width = utils::Log2C(num + 1);

		if (ret.size() == 0)
			ret.resize(width);
		HCL_DESIGNCHECK_HINT(ret.size() >= width, "string UInt constant width is to small for its value");

		ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
		for (size_t i = 0; i < width; ++i)
			ret.set(sim::DefaultConfig::VALUE, i, (num & (1ull << i)) != false);
	};

	try {
		parse(value.begin(), value.end(),
			(-uint_)[parseWidth] > (
				(char_('x') > (*char_("0-9a-fA-FxX"))[std::bind(parseHex, 4, std::placeholders::_1)]) |
				(char_('o') > (*char_("0-7xX"))[std::bind(parseHex, 3, std::placeholders::_1)]) |
				(char_('b') > (*char_("0-1xX"))[std::bind(parseHex, 1, std::placeholders::_1)]) |
				(char_('d') > (*char_("0-9"))[parseDec])
			) > eoi
		);
	}
	catch (const expectation_failure<std::string_view::iterator>&)
	{
		HCL_DESIGNCHECK_HINT(false, "parsing of UInt literal failed (32xF, b0, ...)");
	}

	return ret;
}

sim::DefaultBitVectorState gtry::parseBitVector(uint64_t value, size_t width)
{
	//HCL_ASSERT(width <= sizeof(size_t) * 8);

	sim::DefaultBitVectorState ret;
	ret.resize(width);
	ret.clearRange(sim::DefaultConfig::VALUE, 0, width);
	ret.setRange(sim::DefaultConfig::DEFINED, 0, width);

	ret.insertNonStraddling(sim::DefaultConfig::VALUE, 0, std::min(sizeof(size_t) * 8, width), value);
	return ret;
}


gtry::BVec gtry::ConstBVec(uint64_t value, BitWidth width, std::string_view name)
{
	auto* node = DesignScope::createNode<hlim::Node_Constant>(parseBitVector(value, width.value), hlim::ConnectionType::BITVEC);
	if (!name.empty())
		node->setName(std::string(name));

	return SignalReadPort(node);
}

gtry::BVec gtry::ConstBVec(BitWidth width, std::string_view name)
{
	sim::DefaultBitVectorState value;
	value.resize(width.value);
	value.setRange(sim::DefaultConfig::DEFINED, 0, width.value, false);

	auto* node = DesignScope::createNode<hlim::Node_Constant>(value, hlim::ConnectionType::BITVEC);
	if (!name.empty())
		node->setName(std::string(name));
	return SignalReadPort(node);
}

gtry::UInt gtry::ConstUInt(uint64_t value, BitWidth width, std::string_view name)
{
	auto* node = DesignScope::createNode<hlim::Node_Constant>(parseBitVector(value, width.value), hlim::ConnectionType::BITVEC);
	if (!name.empty())
		node->setName(std::string(name));

	return SignalReadPort(node);
}

gtry::UInt gtry::ConstUInt(BitWidth width, std::string_view name)
{
	sim::DefaultBitVectorState value;
	value.resize(width.value);
	value.setRange(sim::DefaultConfig::DEFINED, 0, width.value, false);

	auto* node = DesignScope::createNode<hlim::Node_Constant>(value, hlim::ConnectionType::BITVEC);
	if (!name.empty())
		node->setName(std::string(name));
	return SignalReadPort(node);
}
