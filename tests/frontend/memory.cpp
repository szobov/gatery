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
#include "frontend/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/hlim/RegisterRetiming.h>

#include <cstdint>

using namespace boost::unit_test;
using BoostUnitTestSimulationFixture = gtry::BoostUnitTestSimulationFixture;

BOOST_FIXTURE_TEST_CASE(async_ROM, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> rom(contents.size(), 4_b);
	rom.fillPowerOnState(createDefaultBitVectorState(16, 4, [&contents](std::size_t i, std::size_t *words){
		words[DefaultConfig::VALUE] = contents[i];
		words[DefaultConfig::DEFINED] = ~0ull;
	}));


	UInt addr = pinIn(4_b).setName("addr");
	auto output = pinOut(rom[addr]).setName("output");


	addSimulationProcess([=,this,&contents]()->SimProcess {
		
		for (auto i : Range(16)) {
			simu(addr) = i;
			co_await WaitStable();

			BOOST_TEST(simu(output) == contents[i]);
			co_await WaitFor({1, 1000});
		}

		stopTest();
	});

	design.postprocess();
	runTest({1, 1});
}



BOOST_FIXTURE_TEST_CASE(sync_ROM, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> rom(contents.size(), 4_b);
	rom.fillPowerOnState(createDefaultBitVectorState(16, 4, [&contents](std::size_t i, std::size_t *words){
		words[DefaultConfig::VALUE] = contents[i];
		words[DefaultConfig::DEFINED] = ~0ull;
	}));


	UInt addr = pinIn(4_b);
	auto output = pinOut(reg(rom[addr], {.allowRetimingBackward=true}));


	addSimulationProcess([=,this,&contents]()->SimProcess {
		
		for (auto i : Range(16)) {
			simu(addr) = i;
			co_await AfterClk(clock);
			BOOST_TEST(simu(output) == contents[i]);
		}

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> mem(contents.size(), 4_b);
	mem.noConflicts();

	UInt addr = pinIn(4_b);
	auto output = pinOut(mem[addr]);
	UInt input = pinIn(4_b);
	Bit wrEn = pinIn();
	IF (wrEn)
		mem[addr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		simu(wrEn) = '1';
		for (auto i : Range(16)) {
			simu(addr) = i;
			simu(input) = contents[i];
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';

		for (auto i : Range(16)) {
			simu(addr) = i;
			co_await WaitStable();
			BOOST_TEST(simu(output) == contents[i]);
			co_await AfterClk(clock);
		}		

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(sync_mem, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> mem(contents.size(), 4_b);
	mem.noConflicts();

	UInt addr = pinIn(4_b);
	auto output = pinOut(reg(mem[addr], {.allowRetimingBackward=true}));
	UInt input = pinIn(4_b);
	Bit wrEn = pinIn();
	IF (wrEn)
		mem[addr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		simu(wrEn) = '1';
		for (auto i : Range(16)) {
			simu(addr) = i;
			simu(input) = contents[i];
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';

		for (auto i : Range(16)) {
			simu(addr) = i;
			co_await AfterClk(clock);
			BOOST_TEST(simu(output) == contents[i]);
		}		

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem_read_before_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> mem(contents.size(), 4_b);

	UInt rdAddr = pinIn(4_b);
	auto output = pinOut(mem[rdAddr]);

	UInt wrAddr = pinIn(4_b);
	UInt input = pinIn(4_b);
	Bit wrEn = pinIn();
	IF (wrEn)
		mem[wrAddr] = input;

	addSimulationProcess([=,this,&contents]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		simu(wrEn) = '1';
		for (auto i : Range(16)) {
			simu(wrAddr) = i;
			simu(input) = contents[i];
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';

		for (auto i : Range(16)) {

			bool doWrite = i % 2;
			bool writeSameAddr = i % 3;

			simu(wrEn) = doWrite;
			if (writeSameAddr)
				simu(wrAddr) = i;
			else
				simu(wrAddr) = 0;

			simu(input) = 0;

			simu(rdAddr) = i;

			co_await WaitStable();

			BOOST_TEST(simu(output) == contents[i]);
			co_await AfterClk(clock);
		}		

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(async_mem_write_before_read, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(16);
	std::mt19937 rng{ 18055 };
	for (auto &e : contents)
		e = rng() % 16;

	Memory<UInt> mem(contents.size(), 4_b);

	UInt rdAddr = pinIn(4_b);

	UInt wrAddr = pinIn(4_b);
	UInt input = pinIn(4_b);
	Bit wrEn = pinIn();
	IF (wrEn)
		mem[wrAddr] = input;

	auto output = pinOut(mem[rdAddr]);

	addSimulationProcess([=,this,&contents]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		simu(wrEn) = '1';
		for (auto i : Range(16)) {
			simu(wrAddr) = i;
			simu(input) = contents[i];
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';

		for (auto i : Range(16)) {

			bool doWrite = i % 2;
			bool writeSameAddr = i % 3;

			simu(wrEn) = doWrite;
			if (writeSameAddr)
				simu(wrAddr) = i;
			else
				simu(wrAddr) = 0;

			simu(input) = 0;

			simu(rdAddr) = i;

			co_await WaitStable();

			if (doWrite && writeSameAddr)
				BOOST_TEST(simu(output) == 0);
			else
				BOOST_TEST(simu(output) == contents[i]);
			co_await AfterClk(clock);
		}		

		stopTest();
	});

	design.postprocess();
	runTest(hlim::ClockRational(100, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(async_mem_read_modify_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::SMALL);
	mem.initZero();

	UInt addr = pinIn(4_b);
	UInt output;
	Bit wrEn = pinIn();
	{
		UInt elem = mem[addr];
		output = reg(elem, {.allowRetimingBackward=true});

		IF (wrEn)
			mem[addr] = elem + 1;
	}
	pinOut(output);

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(10000)) {
			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			lastWasWrite = doInc;
			lastAddr = incAddr;
			co_await AfterClk(clock);
		}

		BOOST_TEST(collisions > 1000u);

		simu(wrEn) = '0';

		for (auto i : Range(4)) {
			simu(addr) = i;
			co_await AfterClk(clock);
			BOOST_TEST(simu(output) == contents[i]);
		}		

		stopTest();
	});


	design.postprocess();

	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt addr = pinIn(4_b);
	UInt output;
	Bit wrEn = pinIn();
	{
		UInt elem = mem[addr];
		output = reg(elem, {.allowRetimingBackward=true});

		IF (wrEn)
			mem[addr] = elem + 1;
	}
	pinOut(output);

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(10000)) {
			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			lastWasWrite = doInc;
			lastAddr = incAddr;
			co_await AfterClk(clock);
		}

		BOOST_TEST(collisions > 1000u, "Too few collisions to verify correct RMW behavior");

		simu(wrEn) = '0';

		for (auto i : Range(4)) {
			simu(addr) = i;
			co_await AfterClk(clock);
			BOOST_TEST(simu(output) == contents[i]);
		}		

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}





BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_reads, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt addr = pinIn(4_b).setName("rmw_addr");
	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	Bit wrEn = pinIn().setName("wr_en");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");
	{
		UInt elem = mem[addr];
		IF (wrEn)
			mem[addr] = elem + 1;
	}
	UInt readOutputAfter = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputAfter).setName("readOutputAfter");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<uint64_t> randomAddr(0, 3);

		uint64_t collisions = 0;

		bool lastWasWrite = false;
		uint64_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {

			uint64_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			uint64_t expectedReadContentBefore = contents[read_addr];

			bool doInc = zeroOne(rng) > 0.1f;
			uint64_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			uint64_t expectedReadContentAfter = contents[read_addr];

			co_await AfterClk(clock);

			uint64_t actualReadContentBefore = simu(readOutputBefore).value();
			BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
				"Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);
			BOOST_TEST(simu(readOutputAfter) == expectedReadContentAfter);

			lastWasWrite = doInc;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_on_wrEn, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt addr = pinIn(4_b).setName("rmw_addr");
	Bit shuffler = pinIn().setName("shuffler");

	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");
	{
		UInt elem = mem[addr];
		Bit doWrite = elem[0] ^ shuffler;
		elem += 1;
		IF (doWrite)
			mem[addr] = elem;
	}


	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(10000)) {

			size_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			size_t expectedReadContentBefore = contents[read_addr];

			bool shfl = zeroOne(rng) > 0.5f;
			size_t incAddr = randomAddr(rng);
			simu(shuffler) = shfl;
			simu(addr) = incAddr;

			bool doWrite = (contents[incAddr] & 1) ^ shfl;
			if (doWrite)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			co_await AfterClk(clock);

			uint64_t actualReadContentBefore = simu(readOutputBefore).value();
			BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
				"Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);

			lastWasWrite = doWrite;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(sync_mem_multiple_writes, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt wrData1 = pinIn(32_b).setName("wr_data1");
	UInt wrAddr1 = pinIn(4_b).setName("wr_addr1");

	UInt wrData2 = pinIn(32_b).setName("wr_data2");
	UInt wrAddr2 = pinIn(4_b).setName("wr_addr2");

	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");

	mem[wrAddr1] = wrData1;
	mem[wrAddr2] = wrData2;

	UInt readOutputAfter = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputAfter).setName("readOutputAfter");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		
		std::uniform_int_distribution<size_t> randomData(0, 1000);

		size_t collisions = 0;

		for ([[maybe_unused]] auto i : Range(5000)) {

			size_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			size_t expectedReadContentBefore = contents[read_addr];


			size_t write_addr1 = randomAddr(rng);
			simu(wrAddr1) =  write_addr1;
			size_t write_data1 = randomData(rng);
			simu(wrData1) = write_data1;
			contents[write_addr1] = write_data1;


			size_t write_addr2 = randomAddr(rng);
			simu(wrAddr2) =  write_addr2;
			size_t write_data2 = randomData(rng);
			simu(wrData2) = write_data2;
			contents[write_addr2] = write_data2;

			if (write_addr2 == write_addr1) collisions++;

			size_t expectedReadContentAfter = contents[read_addr];

			co_await AfterClk(clock);

			BOOST_TEST(simu(readOutputBefore) == expectedReadContentBefore);
			BOOST_TEST(simu(readOutputAfter) == expectedReadContentAfter);
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_writes_wrFirst, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt wrData = pinIn(32_b).setName("wr_data");
	UInt wrAddr = pinIn(4_b).setName("wr_addr");
	UInt addr = pinIn(4_b).setName("rmw_addr");
	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	Bit wrEn = pinIn().setName("wr_en");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");
	{
		mem[wrAddr] = wrData;

		UInt elem = mem[addr];
		IF (wrEn)
			mem[addr] = elem + 1;
	}

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		
		std::uniform_int_distribution<size_t> randomData(0, 1000);

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {
			size_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			size_t expectedReadContentBefore = contents[read_addr];

			size_t write_addr = randomAddr(rng);
			simu(wrAddr) =  write_addr;
			size_t write_data = randomData(rng);
			simu(wrData) = write_data;
			contents[write_addr] = write_data;

			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			co_await AfterClk(clock);

			size_t actualReadContentBefore = simu(readOutputBefore).value();
			BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
				"Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);

			lastWasWrite = doInc;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}


BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_writes_wrLast, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt wrData = pinIn(32_b).setName("wr_data");
	UInt wrAddr = pinIn(4_b).setName("wr_addr");
	UInt addr = pinIn(4_b).setName("rmw_addr");
	Bit wrEn = pinIn().setName("wr_en");
	
	UInt elem = mem[addr];
	IF (wrEn)
		mem[addr] = elem + 1;

	mem[wrAddr] = wrData;
	pinOut(reg(elem, {.allowRetimingBackward=true})).setName("read");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		
		std::uniform_int_distribution<size_t> randomData(0, 1000);

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {

			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			size_t expectedReadContent = contents[incAddr];
			if (doInc)
				contents[incAddr]++;

			size_t write_addr = randomAddr(rng);
			simu(wrAddr) =  write_addr;
			size_t write_data = randomData(rng);
			simu(wrData) = write_data;
			contents[write_addr] = write_data;


			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			co_await AfterClk(clock);

			size_t actualReadContent = simu(elem).value();
			BOOST_TEST(actualReadContent == expectedReadContent, 
				"Read-port (before RMW) yields " << actualReadContent << " but expected " << expectedReadContent << ". RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr << " wrAddr " << write_addr << " wrData " << write_data);

			lastWasWrite = doInc;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_reads_multiple_writes_wrFirst, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt wrData = pinIn(32_b).setName("wr_data");
	UInt wrAddr = pinIn(4_b).setName("wr_addr");
	UInt addr = pinIn(4_b).setName("rmw_addr");
	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	Bit wrEn = pinIn().setName("wr_en");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");
	{
		mem[wrAddr] = wrData;

		UInt elem = mem[addr];
		IF (wrEn)
			mem[addr] = elem + 1;
	}
	UInt readOutputAfter = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputAfter).setName("readOutputAfter");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		
		std::uniform_int_distribution<size_t> randomData(0, 1000);

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {
			size_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			size_t expectedReadContentBefore = contents[read_addr];

			size_t write_addr = randomAddr(rng);
			simu(wrAddr) =  write_addr;
			size_t write_data = randomData(rng);
			simu(wrData) = write_data;
			contents[write_addr] = write_data;

			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			size_t expectedReadContentAfter = contents[read_addr];

			co_await AfterClk(clock);

			size_t actualReadContentBefore = simu(readOutputBefore).value();
			BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
				"Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);
			BOOST_TEST(simu(readOutputAfter) == expectedReadContentAfter);

			lastWasWrite = doInc;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}

BOOST_FIXTURE_TEST_CASE(sync_mem_read_modify_write_multiple_reads_multiple_writes_wrLast, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt wrData = pinIn(32_b).setName("wr_data");
	UInt wrAddr = pinIn(4_b).setName("wr_addr");
	UInt addr = pinIn(4_b).setName("rmw_addr");
	UInt rd_addr = pinIn(4_b).setName("rd_addr");
	Bit wrEn = pinIn().setName("wr_en");
	UInt readOutputBefore = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputBefore).setName("readOutputBefore");
	{
		UInt elem = mem[addr];
		IF (wrEn)
			mem[addr] = elem + 1;

	}
	UInt readOutputAfter = reg(mem[rd_addr], {.allowRetimingBackward=true});
	pinOut(readOutputAfter).setName("readOutputAfter");

	mem[wrAddr] = wrData;

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		
		std::uniform_int_distribution<size_t> randomData(0, 1000);

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {
			size_t read_addr = randomAddr(rng);
			simu(rd_addr) = read_addr;
			size_t expectedReadContentBefore = contents[read_addr];

			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			size_t expectedReadContentAfter = contents[read_addr];

			size_t write_addr = randomAddr(rng);
			simu(wrAddr) =  write_addr;
			size_t write_data = randomData(rng);
			simu(wrData) = write_data;
			contents[write_addr] = write_data;

			co_await AfterClk(clock);

			size_t actualReadContentBefore = simu(readOutputBefore).value();
			BOOST_TEST(actualReadContentBefore == expectedReadContentBefore, 
				"Read-port (before RMW) yields " << actualReadContentBefore << " but expected " << expectedReadContentBefore << ". Read-port address: " << read_addr << " RMW address: " << incAddr << " last clock cycle RMW addr: " << lastAddr);
			BOOST_TEST(simu(readOutputAfter) == expectedReadContentAfter);

			lastWasWrite = doInc;
			lastAddr = incAddr;
		}

		BOOST_TEST(collisions > 1000, "Too few collisions to verify correct RMW behavior");

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(200000, 1) / clock.getClk()->absoluteFrequency());
}



BOOST_FIXTURE_TEST_CASE(sync_mem_dual_read_modify_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;

	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM);
	mem.initZero();

	UInt addr1 = pinIn(4_b);
	UInt output1;
	Bit wrEn1 = pinIn();
	{
		UInt elem = mem[addr1];
		output1 = reg(elem, {.allowRetimingBackward=true});

		IF (wrEn1)
			mem[addr1] = elem + 1;
	}
	pinOut(output1);


	UInt addr2 = pinIn(4_b);
	UInt output2;
	Bit wrEn2 = pinIn();
	{
		UInt elem = mem[addr2];
		output2 = reg(elem, {.allowRetimingBackward=true});

		IF (wrEn2)
			mem[addr2] = elem + 1;
	}
	pinOut(output2);	

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn1) = '0';
		simu(wrEn2) = '0';
		co_await AfterClk(clock);

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		for ([[maybe_unused]] auto i : Range(1000)) {
			bool doInc1 = zeroOne(rng) > 0.1f;
			size_t incAddr1 = randomAddr(rng);
			simu(wrEn1) = doInc1;
			simu(addr1) = incAddr1;
			if (doInc1)
				contents[incAddr1]++;

			bool doInc2 = zeroOne(rng) > 0.1f;
			size_t incAddr2 = randomAddr(rng);
			simu(wrEn2) = doInc2;
			simu(addr2) = incAddr2;
			if (doInc2)
				contents[incAddr2]++;

			co_await AfterClk(clock);
		}


		simu(wrEn1) = '0';
		simu(wrEn2) = '0';

		for (auto i : Range(4)) {
			simu(addr1) = i;
			co_await AfterClk(clock);
			BOOST_TEST(simu(output1) == contents[i]);
		}		

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(long_latency_mem_read_modify_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;


	size_t memReadLatency = 5;


	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	Memory<UInt> mem(contents.size(), 32_b);
	mem.setType(MemType::MEDIUM, memReadLatency);
	mem.initZero();
	mem.noConflicts();

	UInt addr = pinIn(4_b);
	UInt output;
	Bit wrEn = pinIn();
	{

		UInt elem = mem[addr];
		for ([[maybe_unused]] auto i : Range(memReadLatency))
			elem = reg(elem, {.allowRetimingBackward=true});
		output = elem;

		UInt delayedAddr = addr;
		for ([[maybe_unused]] auto i : Range(memReadLatency))
			delayedAddr = reg(delayedAddr, {.allowRetimingBackward=true});

		Bit delayedWrEn = wrEn;
		for ([[maybe_unused]] auto i : Range(memReadLatency))
			delayedWrEn = reg(delayedWrEn, false, {.allowRetimingBackward=true});

		UInt modifiedElem = elem + 1;

		IF (delayedWrEn)
			mem[delayedAddr] = modifiedElem;

		hlim::ReadModifyWriteHazardLogicBuilder rmwBuilder(DesignScope::get()->getCircuit(), clock.getClk());
		
		rmwBuilder.addReadPort(hlim::ReadModifyWriteHazardLogicBuilder::ReadPort{
			.addrInputDriver = addr.readPort(),
			.enableInputDriver = {},
			.dataOutOutputDriver = elem.readPort(),
		});

		rmwBuilder.addWritePort(hlim::ReadModifyWriteHazardLogicBuilder::WritePort{
			.addrInputDriver = delayedAddr.readPort(),
			.enableInputDriver = delayedWrEn.readPort(),
			.enableMaskInputDriver = {},
			.dataInInputDriver = modifiedElem.readPort(),
			.latencyCompensation = memReadLatency,
		});

		rmwBuilder.retimeRegisterToMux();
		rmwBuilder.build(true);
	}
	pinOut(output);

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn) = '0';
		co_await AfterClk(clock);

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(10000)) {
			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc)
				contents[incAddr]++;

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			lastWasWrite = doInc;
			lastAddr = incAddr;
			co_await AfterClk(clock);
		}

		BOOST_TEST(collisions > 1000u, "Too few collisions to verify correct RMW behavior");

		simu(wrEn) = '0';

		for (auto i : Range(4)) {
			simu(addr) = i;
			for ([[maybe_unused]] auto i : Range(memReadLatency))
				co_await AfterClk(clock);
			BOOST_TEST(simu(output) == contents[i]);
		}		

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}




BOOST_FIXTURE_TEST_CASE(long_latency_memport_read_modify_write, BoostUnitTestSimulationFixture)
{
	using namespace gtry;
	using namespace gtry::sim;
	using namespace gtry::utils;


	size_t memReadLatency = 10;


	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	std::vector<size_t> contents;
	contents.resize(4, 0);
	std::mt19937 rng{ 18055 };

	UInt addr = pinIn(4_b).setName("addr");
	UInt output;
	Bit wrEn = pinIn().setName("wrEn");
	Bit initOverride = pinIn().setName("initOverride");
	{
		Memory<UInt> mem(contents.size(), 32_b);
		mem.setType(MemType::EXTERNAL, memReadLatency);
		//mem.setType(MemType::MEDIUM, memReadLatency);

		UInt elem = mem[addr];
		HCL_NAMED(elem);
		UInt modifiedElem = elem + 1;
		HCL_NAMED(modifiedElem);

		IF (initOverride)
			modifiedElem = 0;

		IF (wrEn)
			mem[addr] = modifiedElem;

		output = elem;
		HCL_NAMED(output);
		for ([[maybe_unused]] auto i : Range(memReadLatency))
			output = reg(output, {.allowRetimingBackward=true});
	}
	pinOut(output).setName("output");

	addSimulationProcess([=,this,&contents,&rng]()->SimProcess {

		simu(wrEn) = '1';
		simu(initOverride) = '1';
		for (auto i : Range(4)) {
			simu(addr) = i;
			co_await AfterClk(clock);
		}
		simu(wrEn) = '0';
		simu(initOverride) = '0';

		std::uniform_real_distribution<float> zeroOne(0.0f, 1.0f);
		std::uniform_int_distribution<size_t> randomAddr(0, 3);		

		size_t collisions = 0;

		bool lastWasWrite = false;
		size_t lastAddr = 0;
		for ([[maybe_unused]] auto i : Range(5000)) {
			bool doInc = zeroOne(rng) > 0.1f;
			size_t incAddr = randomAddr(rng);
			simu(wrEn) = doInc;
			simu(addr) = incAddr;
			if (doInc) {
				contents[incAddr]++;
			}

			if (lastWasWrite && lastAddr == incAddr)
				collisions++;

			lastWasWrite = doInc;
			lastAddr = incAddr;
			co_await AfterClk(clock);
		}

		BOOST_TEST(collisions > 1000u, "Too few collisions to verify correct RMW behavior");

		simu(wrEn) = '0';

		for (auto i : Range(4)) {

			simu(addr) = i;
			for ([[maybe_unused]] auto i : Range(memReadLatency))
				co_await AfterClk(clock);

			co_await WaitStable();

			BOOST_TEST(simu(output) == contents[i]);

			co_await AfterClk(clock);
		}

		co_await AfterClk(clock);
		co_await AfterClk(clock);
		co_await AfterClk(clock);

		stopTest();
	});

	//design.visualize("before");
	design.postprocess();
	//design.visualize("after");
	//recordVCD("test.vcd");
	runTest(hlim::ClockRational(20000, 1) / clock.getClk()->absoluteFrequency());
}
