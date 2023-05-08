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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/simulation/Simulator.h>

#include <gatery/scl/stream/StreamArbiter.h>
#include <gatery/scl/stream/adaptWidth.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/stream/PacketStreamFixture.h>
#include <gatery/scl/io/SpiMaster.h>

#include <gatery/debug/websocks/WebSocksInterface.h>
#include <gatery/scl/sim/SimulationSequencer.h>


using namespace boost::unit_test;
using namespace gtry;

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_basic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::RvStream<UInt> in0;
	scl::RvStream<UInt> in1;

	*in0 = pinIn(8_b).setName("in0_data");
	valid(in0)= pinIn().setName("in0_valid");
	pinOut(ready(in0)).setName("in0_ready");

	*in1 = pinIn(8_b).setName("in1_data");
	valid(in1)= pinIn().setName("in1_valid");
	pinOut(ready(in1)).setName("in1_ready");

	scl::arbitrateInOrder uutObj{ in0, in1 };
	scl::RvStream<UInt>& uut = uutObj;
	pinOut(*uut).setName("out_data");
	pinOut(valid(uut)).setName("out_valid");
	ready(uut) = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(uut)) = '1';
		simu(valid(in0)) = '0';
		simu(valid(in1)) = '0';
		simu(*in0) = 0;
		simu(*in1) = 0;
		co_await AfterClk(clock);

		simu(valid(in0)) = '0';
		simu(valid(in1)) = '1';
		simu(*in1) = 1;
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '1';
		simu(*in0) = 2;
		co_await AfterClk(clock);

		simu(valid(in1)) = '1';
		simu(valid(in0)) = '1';
		simu(*in0) = 3;
		simu(*in1) = 4;
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(valid(in1)) = '1';
		simu(valid(in0)) = '1';
		simu(*in0) = 5;
		simu(*in1) = 6;
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		simu(valid(in0)) = '0';
		simu(valid(in1)) = '1';
		simu(*in1) = 7;
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '0';
		simu(ready(uut)) = '0';
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '1';
		simu(*in0) = 8;
		simu(ready(uut)) = '1';
		co_await AfterClk(clock);

		simu(valid(in1)) = '0';
		simu(valid(in0)) = '0';
		co_await AfterClk(clock);
	});

	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while (true)
		{
			co_await OnClk(clock);
			if (simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter == simu(*uut));
				counter++;
			}
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_basic.vcd" };
   //vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("arbitrateInOrder_basic");

	runTicks(clock.getClk(), 16);
}

BOOST_FIXTURE_TEST_CASE(arbitrateInOrder_fuzz, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::RvStream<UInt> in0;
	scl::RvStream<UInt> in1;

	*in0 = pinIn(8_b).setName("in0_data");
	valid(in0) = pinIn().setName("in0_valid");
	pinOut(ready(in0)).setName("in0_ready");

	*in1 = pinIn(8_b).setName("in1_data");
	valid(in1) = pinIn().setName("in1_valid");
	pinOut(ready(in1)).setName("in1_ready");

	scl::arbitrateInOrder uutObj{ in0, in1 };
	scl::RvStream<UInt>& uut = uutObj;
	pinOut(*uut).setName("out_data");
	pinOut(valid(uut)).setName("out_valid");
	ready(uut) = pinIn().setName("out_ready");

	addSimulationProcess([&]()->SimProcess {
		simu(ready(uut)) = '1';
		simu(valid(in0)) = '0';
		simu(valid(in1)) = '0';

		std::mt19937 rng{ 10179 };
		size_t counter = 1;
		while(true)
		{
			co_await OnClk(clock);
			if (simu(ready(in0)))
			{
				if(rng() % 2 == 0)
				{
					simu(valid(in0)) = '1';
					simu(*in0) = counter++;
				}
				else
				{
					simu(valid(in0)) = '0';
				}

				if(rng() % 2 == 0)
				{
					simu(valid(in1)) = '1';
					simu(*in1) = counter++;
				}
				else
				{
					simu(valid(in1)) = '0';
				}
			}

			// chaos monkey
			simu(ready(uut)) = rng() % 8 != 0;
		}
	});


	addSimulationProcess([&]()->SimProcess {

		size_t counter = 1;
		while(true)
		{
			co_await OnClk(clock);
			if(simu(ready(uut)) && simu(valid(uut)))
			{
				BOOST_TEST(counter % 256 == simu(*uut));
				counter++;
			}
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "arbitrateInOrder_fuzz.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
   // design.visualize("arbitrateInOrder_fuzz");

	runTicks(clock.getClk(), 256);
}

BOOST_FIXTURE_TEST_CASE(stream_transform, scl::scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	{
		// const compile test
		const scl::VStream<UInt, scl::Eop> vs{ 5_b };
		auto res = vs.remove<scl::Eop>();
		auto rsr = vs.reduceTo<scl::Stream<UInt>>();
		auto vso = vs.transform(std::identity{});
	}

	scl::RvStream<UInt> in = scl::RvPacketStream<UInt, scl::Sop>{ 5_b }
								.remove<scl::Sop>()
								.reduceTo<scl::RvStream<UInt>>()
								.remove<scl::Eop>();
	In(in);

	struct Intermediate
	{
		UInt data;
		Bit test;
	};

	scl::RvStream<Intermediate> im = in
		.reduceTo<scl::RvStream<UInt>>()
		.transform([](const UInt& data) {
			return Intermediate{ data, '1' };
		});

	scl::RvStream<UInt> out = im.transform(&Intermediate::data);
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_downstreamReg, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_downstreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_uptreamReg, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regReady();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_uptreamReg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_reg, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in { .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = reg(in);
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_reg.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(stream_reg_chaining, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	scl::RvStream<UInt> out = in.regDownstreamBlocking().regDownstreamBlocking().regDownstreamBlocking().regDownstream();
	Out(out);

	simulateTransferTest(in, out);

	//recordVCD("stream_reg_chaining.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_fifo, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::RvStream<UInt> out = in.fifo();
	Out(out);

	transfers(500);
	simulateTransferTest(in, out);

	//recordVCD("stream_fifo.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}
BOOST_FIXTURE_TEST_CASE(streamArbiter_low1, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 10_b };
	In(in);

	scl::StreamArbiter<scl::RvStream<UInt>> arbiter;
	arbiter.attach(in);
	arbiter.generate();

	Out(arbiter.out());

	simulateArbiterTestSource(in);
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low1.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_low4, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>> arbiter;
	std::array<scl::RvStream<UInt>, 4> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low4.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_low4_packet, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvPacketStream<UInt>> arbiter;
	std::array<scl::RvPacketStream<UInt>, 4> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_low4_packet.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rr5, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyRoundRobin> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for (size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rr5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);

	//dbg::vis();
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_reg_rr5, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyReg<scl::ArbiterPolicyRoundRobin>> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for (size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_reg_rr5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rrb5, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvStream<UInt>, scl::ArbiterPolicyRoundRobinBubble> arbiter;
	std::array<scl::RvStream<UInt>, 5> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rrb5.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(streamArbiter_rrb5_packet, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::StreamArbiter<scl::RvPacketStream<UInt>, scl::ArbiterPolicyRoundRobinBubble> arbiter;
	std::array<scl::RvPacketStream<UInt>, 5> in;
	for(size_t i = 0; i < in.size(); ++i)
	{
		*in[i] = 10_b;
		In(in[i], "in" + std::to_string(i) + "_");
		simulateArbiterTestSource(in[i]);
		arbiter.attach(in[i]);
	}
	arbiter.generate();

	Out(arbiter.out());
	simulateArbiterTestSink(arbiter.out());

	//recordVCD("streamArbiter_rrb5_packet.vcd");
	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_extendWidth, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	{
		// add valid no ready compile test
		scl::Stream<UInt> inT{ 4_b };
		auto outT = scl::extendWidth(inT, 8_b);
	}
	{
		// add valid compile test
		scl::Stream<UInt, scl::Ready> inT{ 4_b };
		auto outT = scl::extendWidth(inT, 8_b);
	}

	scl::RvStream<UInt> in{ 4_b };
	In(in);

	auto out = scl::extendWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		for (size_t i = 0; i < 4; ++i)
			co_await AfterClk(m_clock);

		for (size_t i = 0; i < 32; ++i)
		{
			for (size_t j = 0; j < 2; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = (i >> (j * 4)) & 0xF;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
		});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	//dbg::vis();
	
	runTicks(m_clock.getClk(), 1024);
}


BOOST_FIXTURE_TEST_CASE(stream_reduceWidth, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 24_b };
	In(in);

	scl::RvStream<UInt> out = scl::reduceWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();

		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = '1';
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);

			co_await scl::performTransferWait(in, m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_reduceWidth_RvPacketStream, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 24_b };
	In(in);

	auto out = scl::reduceWidth(in, 8_b);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		for(size_t i = 0; i < 8; ++i)
		{
			simu(valid(in)) = '1';
			simu(eop(in)) = i % 2 == 1;
			simu(*in) =
				((i * 3 + 0) << 0) |
				((i * 3 + 1) << 8) |
				((i * 3 + 2) << 16);

			co_await scl::performTransferWait(in, m_clock);
		}
	});

	transfers(8 * 3);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_eraseFirstBeat, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	scl::RvPacketStream<UInt> out = scl::eraseBeat(in, 0, 1);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = uint8_t(i + j - 1);
				simu(eop(in)) = j == 4;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_eraseLastBeat, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	scl::RvPacketStream<UInt> out = scl::eraseLastBeat(in);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 5; ++j)
			{
				simu(valid(in)) = '1';
				simu(*in) = uint8_t(i + j);
				simu(eop(in)) = j == 4;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_insertFirstBeat, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in{ 8_b };
	In(in);

	UInt insertData = pinIn(8_b).setName("insertData");
	scl::RvPacketStream<UInt> out = scl::insertBeat(in, 0, insertData);
	Out(out);

	// send data
	addSimulationProcess([=, this, &in]()->SimProcess {
		simu(valid(in)) = '0';
		simu(*in).invalidate();
		co_await AfterClk(m_clock);

		for(size_t i = 0; i < 32; i += 4)
		{
			for(size_t j = 0; j < 3; ++j)
			{
				simu(valid(in)) = '1';
				simu(insertData) = i + j;
				simu(*in) = uint8_t(i + j + 1);
				simu(eop(in)) = j == 2;

				co_await scl::performTransferWait(in, m_clock);
			}
		}
	});

	transfers(32);
	groups(1);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_addEopDeferred, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 8_b };
	In(in);

	Bit eop = pinIn().setName("eop");
	scl::RvPacketStream<UInt> out = scl::addEopDeferred(in, eop);
	Out(out);

	// generate eop insert signal
	addSimulationProcess([=, this, &in]()->SimProcess {
		
		simu(eop) = '0';
		while(true)
		{
			co_await WaitStable();
			while (simu(valid(in)) == '0')
			{
				co_await AfterClk(m_clock);
				co_await WaitStable();
			}
			while(simu(valid(in)) == '1')
			{
				co_await AfterClk(m_clock);
				co_await WaitStable();
			}
			co_await WaitFor(Seconds{1,10}/m_clock.absoluteFrequency());
			simu(eop) = '1';
			co_await AfterClk(m_clock);
			simu(eop) = '0';
		}

	});

	transfers(32);
	groups(1);
	simulateSendData(in, 0);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(stream_addPacketSignalsFromSize, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ 8_b };
	In(in);

	UInt size = 4_b;
	size = reg(size, 1);
	scl::RvPacketStream<UInt, scl::Sop> out = scl::addPacketSignalsFromCount(in, size);

	IF(transfer(out) & eop(out))
		size += 1;

	Out(out);

	transfers(32);
	groups(1);
	simulateSendData(in, 0);
	simulateBackPressure(out);
	simulateRecvData(out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(spi_stream_test, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 8_b };
	In(in);

	scl::RvStream<BVec> inBVec = in.transform([](const UInt& v) { return (BVec)v; });
	scl::RvStream<BVec> outBVec = scl::SpiMaster{}.pinTestLoop().clockDiv(3).generate(inBVec);

	scl::RvStream<UInt> out = outBVec.transform([](const BVec& v) { return (UInt)v; });
	Out(out);

	simulateTransferTest(in, out);

	design.postprocess();
	runTicks(m_clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(stream_stall, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);

	Bit stallCondition = pinIn().setName("stall");
	scl::RvStream<UInt> out = stall(in, stallCondition);
	Out(out);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {

		simu(stallCondition) = '0';

		do
			co_await OnClk(m_clock);
		while (simu(valid(out)) == '0');
		co_await AfterClk(m_clock);
		co_await AfterClk(m_clock);

		std::mt19937 rng{ std::random_device{}() };
		while (true)
		{
			if (rng() % 4 != 0)
			{
				simu(stallCondition) = '1';
				co_await WaitStable();
				BOOST_TEST(simu(valid(out)) == '0');
				BOOST_TEST(simu(ready(in)) == '0');
			}
			else
			{
				simu(stallCondition) = '0';
			}
			co_await AfterClk(m_clock);
		}

	});

	simulateTransferTest(in, out);

	design.postprocess();
	runTicks(m_clock.getClk(), 1024);
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_1_10, scl::StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 10'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_1_1, scl::StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(ReqAckSync_10_1, scl::StreamTransferFixture)
{
	Clock outClk({ .absoluteFrequency = 1000'000'000 });
	HCL_NAMED(outClk);
	ClockScope clkScp(m_clock);

	scl::RvStream<UInt> in{ .data = 5_b };
	In(in);
	simulateSendData(in, 0);
	groups(1);

	scl::RvStream<UInt> out = gtry::scl::synchronizeStreamReqAck(in, m_clock, outClk);
	{
		ClockScope clock(outClk);
		Out(out);

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}


BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RvPacketStream<UInt> out = scl::storeForwardFifo(in, 32);
	In(in);
	error(in) = '0';
	Out(out);
	transfers(1000);
	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream_PayloadOnly, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::TransactionalFifo<UInt> fifo{ 32, 16_b };

	scl::RvPacketStream<UInt, scl::Error> in = { 16_b };
	In(in);
	error(in) = '0';
	fifo <<= in;

	scl::RvPacketStream<UInt> out = { 16_b };
	out <<= fifo;
	Out(out);

	fifo.generate();

	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifo_StoreForwardStream_sopeop, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RsPacketStream<UInt> in = { 16_b };
	scl::RsPacketStream<UInt> out = scl::storeForwardFifo(in, 32);

	In(in);
	Out(out);
	transfers(1000);
	simulateTransferTest(in, out);

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(TransactionalFifoCDCSafe, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::RvPacketStream<UInt> in = { 16_b };
	scl::RvPacketStream<UInt> out;
	

	scl::TransactionalFifo fifo(32, scl::PacketStream<UInt>{ in->width() });
	
	In(in);

	fifo <<= in;

	simulateSendData(in, 0);
	transfers(100);
	groups(1);

	Clock outClk({ .absoluteFrequency = 100'000'000 });
	HCL_NAMED(outClk);
	{
		ClockScope clock(outClk);
		out <<= fifo;
		Out(out);
		fifo.generate();

		simulateBackPressure(out);
		simulateRecvData(out);
	}

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

namespace gtry::scl
{
}

BOOST_FIXTURE_TEST_CASE(addReadyAndFailOnBackpressure_test, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::VPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RvPacketStream<UInt, scl::Error> out = scl::addReadyAndFailOnBackpressure(in);

	In(in);
	Out(out);
	groups(1);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {
		simu(error(in)) = '0';
		simu(ready(out)) = '1';

		// simple packet passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while(simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// simple error passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		simu(error(in)) = '1';
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');
		simu(error(in)) = '0';

		// one beat not ready
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(valid(out)) == '1');
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and NO bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';

		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');


		stopTest();

	});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

BOOST_FIXTURE_TEST_CASE(addReadyAndFailOnBackpressure_sop_test, scl::StreamTransferFixture)
{
	ClockScope clkScp(m_clock);

	scl::SPacketStream<UInt, scl::Error> in = { 16_b };
	scl::RsPacketStream<UInt, scl::Error> out = scl::addReadyAndFailOnBackpressure(in);

	In(in);
	Out(out);
	groups(1);

	addSimulationProcess([=, this, &out, &in]()->SimProcess {
		simu(error(in)) = '0';
		simu(ready(out)) = '1';

		// simple packet passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// simple error passthrough test
		fork(sendDataPacket(in, 0, 0, 3));
		simu(error(in)) = '1';
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');
		simu(error(in)) = '0';

		// one beat not ready
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and bubble for generated eop
		fork(sendDataPacket(in, 0, 6, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';
		co_await OnClk(m_clock);
		BOOST_TEST(simu(eop(out)) == '1');
		BOOST_TEST(simu(error(out)) == '1');

		// next packet after error should be valid
		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '0');

		// eop beat not ready and NO bubble for generated eop
		fork(sendDataPacket(in, 0, 0, 3));
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '0';
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		co_await OnClk(m_clock);
		simu(ready(out)) = '1';

		fork(sendDataPacket(in, 0, 0, 3));
		do co_await performTransferWait(out, m_clock);
		while (simu(eop(out)) == '0');
		BOOST_TEST(simu(error(out)) == '1');


		stopTest();

		});

	design.postprocess();
	BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
}

template<typename StreamType>
struct PacketSendAndReceiveTest : public scl::StreamTransferFixture
{
	std::vector<SimPacket> allPackets;
	bool addPipelineReg = true;
	BitWidth txIdSize = 4_b;
	std::uint64_t unreadyMask = 0;

	void runTest() {
		ClockScope clkScp(m_clock);

		StreamType in = { 16_b };
		StreamType out = { 16_b };

		if constexpr (StreamType::template has<scl::Empty>()) {
			empty(in) = BitWidth::last(in->width().bytes()-1);
			empty(out) = BitWidth::last(in->width().bytes()-1);
		}
		if constexpr (StreamType::template has<scl::TxId>()) {
			txid(in) = txIdSize;
			txid(out) = txIdSize;
		}

		if (addPipelineReg)
			out <<= in.regDownstream();
		else
			out <<= in;

		In(in);
		Out(out);
		groups(1);
		scl::SimulationSequencer garbage;

		addSimulationProcess([&, this]()->SimProcess {

			fork([this,&in,&garbage]()->SimProcess {
				for (const auto &packet : allPackets)
					co_await this->sendPacket(in, packet, garbage, m_clock);
			});

			for (const auto &packet : allPackets) {
				SimPacket rvdPacket = co_await this->receivePacket(out, garbage, m_clock, unreadyMask);
				BOOST_TEST(rvdPacket.payload == packet.payload);
				if (packet.txid()) {
					//BOOST_TEST(rvdPacket.txid());
					BOOST_TEST(*rvdPacket.txid() == *packet.txid());
				}
				if (packet.error()) {
					//BOOST_TEST(rvdPacket.error());
					BOOST_TEST(*rvdPacket.error() == *packet.error());
				}
			}

			stopTest();
		});

		design.postprocess();
		BOOST_TEST(!runHitsTimeout({ 50, 1'000'000 }));
	}
};

/*

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_singleBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<gtry::BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_multiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_longMultiBeatPacket, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_packetStream, PacketSendAndReceiveTest<scl::PacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	addPipelineReg = false;
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_VPacketStream, PacketSendAndReceiveTest<scl::VPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream, PacketSendAndReceiveTest<scl::RsPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_SPacketStream, PacketSendAndReceiveTest<scl::SPacketStream<BVec>>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }),
	};
	runTest();
}
*/

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream_bubbles, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}


BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_seqeuence_of_packets_RvPacketStream_bubbles_backpressure, PacketSendAndReceiveTest<scl::RvPacketStream<BVec>>) {
	std::mt19937 rng(2678);
	unreadyMask = 0b10110001101;
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07 }).invalidBeats(rng()),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).invalidBeats(rng()),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23 }).invalidBeats(rng()),
	};
	runTest();
}


using RsePacketStream = scl::RsPacketStream<BVec, scl::Empty>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty, PacketSendAndReceiveTest<RsePacketStream>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }),
	};
	runTest();
}

using RseePacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::Error>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_error, PacketSendAndReceiveTest<RseePacketStream>) {
	
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).error(false),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).error(true),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).error(false),
		SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).error(true),
	};
	runTest();
}

using RsetPacketStream = scl::RsPacketStream<BVec, scl::Empty, scl::TxId>;

BOOST_FIXTURE_TEST_CASE(packetSenderFramework_testsimple_sequence_of_packets_RsPacketStream_empty_txid, PacketSendAndReceiveTest<RsetPacketStream>) {
	allPackets = std::vector<SimPacket>{
		SimPacket(std::vector<uint8_t>{ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 }).txid(0),
		SimPacket(std::vector<uint8_t>{ 0x10, 0x11 }).txid(1),
		SimPacket(std::vector<uint8_t>{ 0x20, 0x21, 0x22, 0x23, 0x24 }).txid(2),
		SimPacket(std::vector<uint8_t>{ 0x30, 0x31, 0x32 }).txid(0),
	};
	runTest();
}


