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
#include "scl/pch.h"


#include <gatery/frontend/GHDLTestFixture.h>
#include <gatery/scl/arch/xilinx/XilinxDevice.h>
#include <gatery/scl/arch/xilinx/ODDR.h>
#include <gatery/scl/utils/GlobalBuffer.h>
#include <gatery/scl/io/ddr.h>


#include <gatery/scl/arch/xilinx/IOBUF.h>
#include <gatery/hlim/coreNodes/Node_MultiDriver.h>


#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>


using namespace boost::unit_test;

boost::test_tools::assertion_result canCompileXilinx(boost::unit_test::test_unit_id)
{
	return gtry::GHDLGlobalFixture::hasGHDL() && gtry::GHDLGlobalFixture::hasXilinxLibrary();
}


BOOST_AUTO_TEST_SUITE(XilinxTechMapping, * precondition(canCompileXilinx))

BOOST_FIXTURE_TEST_CASE(testGlobalBuffer, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	Bit b = pinIn().setName("input");
	b = scl::bufG(b);
	pinOut(b).setName("output");


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(SCFifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));

	scl::Fifo<UInt> fifo(128, 8_b);

	Bit inValid = pinIn().setName("inValid");
	UInt inData = pinIn(8_b).setName("inData");
	IF (inValid)
		fifo.push(inData);

	UInt outData = fifo.peek();
	Bit outValid = !fifo.empty();
	IF (outValid)
		fifo.pop();
	pinOut(outData).setName("outData");
	pinOut(outValid).setName("outValid");


	fifo.generate();


	testCompilation();
}

BOOST_FIXTURE_TEST_CASE(DCFifo, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	Clock clock2({
			.absoluteFrequency = {{75'000'000,1}},
			//.resetType = Clock::ResetType::ASYNCHRONOUS,
			.initializeRegs = false,
	});
	HCL_NAMED(clock2);

	scl::Fifo<UInt> fifo(128, 8_b);

	{
		ClockScope clkScp(clock1);
		Bit inValid = pinIn().setName("inValid");
		UInt inData = pinIn(8_b).setName("inData");
		IF (inValid)
			fifo.push(inData);
	}

	{
		ClockScope clkScp(clock2);
		UInt outData = fifo.peek();
		Bit outValid = !fifo.empty();
		IF (outValid)
			fifo.pop();
		pinOut(outData).setName("outData");
		pinOut(outValid).setName("outValid");
	}

	fifo.generate();


	testCompilation();
}


BOOST_FIXTURE_TEST_CASE(instantiateODDR, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	ClockScope scp(clock1);

	auto *ddr = design.createNode<scl::arch::xilinx::ODDR>();
	ddr->attachClock(clock1.getClk(), scl::arch::xilinx::ODDR::CLK_IN);

	ddr->setEdgeMode(scl::arch::xilinx::ODDR::SAME_EDGE);
	ddr->setInitialOutputValue(false);

	ddr->setInput(scl::arch::xilinx::ODDR::IN_D1, pinIn().setName("d1"));
	ddr->setInput(scl::arch::xilinx::ODDR::IN_D2, pinIn().setName("d2"));
	ddr->setInput(scl::arch::xilinx::ODDR::IN_SET, clock1.rstSignal());
	ddr->setInput(scl::arch::xilinx::ODDR::IN_CE, Bit('1'));
	
	pinOut(ddr->getOutputBit(scl::arch::xilinx::ODDR::OUT_Q)).setName("ddr_output");


	testCompilation();
	BOOST_TEST(exportContains(std::regex{"ODDR"}));
}

BOOST_FIXTURE_TEST_CASE(instantiate_scl_ddr, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));

	Clock clock1({
			.absoluteFrequency = {{125'000'000,1}},
			.initializeRegs = false,
	});
	HCL_NAMED(clock1);
	ClockScope scp(clock1);

	Bit d1 = pinIn().setName("d1");
	Bit d2 = pinIn().setName("d2");

	Bit o = scl::ddr(d1, d2);
	
	pinOut(o).setName("ddr_output");

	testCompilation();
	BOOST_TEST(exportContains(std::regex{"ODDR"}));
}




BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BOOL, .width = 1 });

	auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
	pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");


	auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
	pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");


	multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
	multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());


	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));



	testCompilation();

	//DesignScope::visualize("test_bidir_intra_connection");
}



BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BOOL, .width = 1 });

	auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
	pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

	multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
	iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

	{
		Area area("test", true);
		auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
		pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}


	testCompilation();

	//DesignScope::visualize("test_bidir_intra_connection_different_entities");
}

BOOST_FIXTURE_TEST_CASE(test_bidir_intra_connection_different_entities2, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BOOL, .width = 1 });

	{
		Area area("test1", true);
		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T1"));
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

		multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}

	{
		Area area("test2", true);
		auto *iobuf2 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_T, pinIn().setName("T2"));
		pinOut(iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		multiDriver->rewireInput(1, iobuf2->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf2->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));
	}


	testCompilation();

	//DesignScope::visualize("test_bidir_intra_connection_different_entities2");
}



BOOST_FIXTURE_TEST_CASE(test_bidir_pin_extnode, gtry::GHDLTestFixture)
{
	using namespace gtry;

	auto device = std::make_unique<scl::XilinxDevice>();
	device->setupZynq7();
	design.setTargetTechnology(std::move(device));


	{
		Area area("test1", true);

		auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BOOL, .width = 1 });

		Bit t = pinIn().setName("T1");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I1"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O1");

		multiDriver->rewireInput(0, iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O).readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

		multiDriver->rewireInput(1, Bit(tristatePin(Bit(SignalReadPort(multiDriver)), t)).readPort());
	}


	{
		Area area("test2", true);

		Bit t = pinIn().setName("T2");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, pinIn().setName("I2"));
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);
		pinOut(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O)).setName("O2");

		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, tristatePin(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O), t));
	}


	{
		Area area("test3", true);

		auto *multiDriver = DesignScope::createNode<hlim::Node_MultiDriver>(2, hlim::ConnectionType{ .interpretation = hlim::ConnectionType::BOOL, .width = 1 });

		Bit t = pinIn().setName("T3");
		Bit i = pinIn().setName("I3");

		auto *iobuf1 = DesignScope::createNode<scl::arch::xilinx::IOBUF>();
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_I, i);
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_T, t);


		Bit bufOut = iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_IO_O);
		multiDriver->rewireInput(0, bufOut.readPort());
		iobuf1->setInput(scl::arch::xilinx::IOBUF::IN_IO_I, Bit(SignalReadPort(multiDriver)));

		Bit biPinIn = i;
		biPinIn.exportOverride(Bit(SignalReadPort(multiDriver)));
		Bit biPinOut = tristatePin(biPinIn, t).setName("biPin_3");

		multiDriver->rewireInput(1, biPinOut.readPort());

		Bit o = biPinOut;
		o.exportOverride(iobuf1->getOutputBit(scl::arch::xilinx::IOBUF::OUT_O));

		pinOut(o).setName("O3");
	}	

	testCompilation();

	//DesignScope::visualize("test_bidir_pin_extnode");
}



BOOST_AUTO_TEST_SUITE_END()
