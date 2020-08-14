#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <hcl/frontend.h>
#include <hcl/simulation/UnitTestSimulationFixture.h>

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

using namespace boost::unit_test;

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, TestGCD, data::make({1, 2, 3, 4, 5, 10, 42}) * data::make({1, 2, 3, 4, 5, 23, 56, 126}), x, y)
{
    using namespace hcl::core::frontend;
    
    DesignScope design;
    
    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);
    
    auto gcd_ref = [](unsigned a, unsigned b) -> unsigned {
        while (a != b) {
            if (a > b)
                a -= b;
            else
                b -= a;
        }
        return a;
    };
    
    
    unsigned maxTicks = 200;
    
    {
        BVec x_vec = ConstBVec(x, 8);
        BVec y_vec = ConstBVec(y, 8);

        Bit start;
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick() == 0);
        }, start);
        
        BVec result;
        Bit done;
        
        {
            HCL_NAMED(x_vec);
            HCL_NAMED(y_vec);
            
            GroupScope entity(GroupScope::GroupType::ENTITY);
            entity
                .setName("gcd")
                .setComment("Statemachine to compute the GCD of two 8-bit integers.");
            
            
            fsm::ImmediateState idle;
            HCL_NAMED(idle);
            fsm::DelayedState running;
            HCL_NAMED(running);

            Register<BVec> a(8u, Expansion::none);
            a.setReset(0b00000000_bvec);
            Register<BVec> b(8u, Expansion::none);
            b.setReset(0b00000000_bvec);

#if 0
            // Euclid's gcd
            idle.onActive([&]{
                IF (start) {
                    a = x_vec;
                    b = y_vec;
                    fsm::delayedSwitch(running);
                }
            });
            running.onActive([&]{
                IF (a.delay(1) == b.delay(1)) {
                    fsm::immediateSwitch(idle);
                } ELSE {
                    IF (a.delay(1) > b.delay(1)) {
                        a = a.delay(1) - b.delay(1);
                    } ELSE {
                        b = b.delay(1) - a.delay(1);
                    }
                }
            });
#else
            // Binary GCD
            fsm::ImmediateState shifting;
            HCL_NAMED(shifting);

            Register<BVec> d(4u, Expansion::none);
            d.setReset(0b0000_bvec);
            
            idle.onActive([&]{
                IF (start) {
                    a = x_vec;
                    b = y_vec;
                    d = ConstBVec(0, 4);
                    fsm::delayedSwitch(running);
                }
            });
            running.onActive([&]{
                IF (a == b) {
                    fsm::immediateSwitch(shifting);
                } ELSE {
                    Bit a_odd = a[0];
                    Bit b_odd = b[0];
                    IF (!a_odd & !b_odd) {
                        a >>= 1;
                        b >>= 1;
                        d += 1;
                    } 
                    IF (!a_odd & b_odd) {
                        a >>= 1;
                    }
                    IF (a_odd & !b_odd) {
                        b >>= 1;
                    }
                    IF (a_odd & b_odd) {
                        IF (a < b) {
                            BVec help = a;
                            a = b;
                            b = help;
                        } ELSE {
                            BVec difference = a - b;
                            a = difference >> 1;
                        }
                    }
                }
            });
            shifting.onActive([&]{
                IF(d == ConstBVec(0, 4)) {
                    fsm::immediateSwitch(idle);
                } ELSE {
                    a <<= 1;
                    d -= 1;
                }
            });            
#endif
            fsm::FSM stateMachine(clock, idle);
            result = a.delay(1);
            sim_debug() << result << "," << a << "," << a.delay(1);
            HCL_NAMED(result);
            done = stateMachine.isInState(idle);
            HCL_NAMED(done);

            //sim_debug() << "a is " << a.delay(1) << " and b is " << b.delay(1);
        }
        
        BVec ticks(8, Expansion::none);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext &context){
            context.set(0, context.getTick());
        }, ticks);
        
        sim_assert((ticks < ConstBVec(maxTicks-1, 8)) | done) << "The state machine should be idle after " << maxTicks << " cycles";
        BVec gtruth = ConstBVec(gcd_ref(x, y), 8);
        sim_assert((ticks < ConstBVec(maxTicks-1, 8)) | (result == gtruth)) << "The state machine computed " << result << " but the correct answer is " << gtruth;
    }

    runTicks(design.getCircuit(), clock.getClk(), maxTicks);
}

BOOST_DATA_TEST_CASE_F(hcl::core::sim::UnitTestSimulationFixture, FSMlessTestGCD, data::make({ 1, 2, 3, 4, 5, 10, 42 })* data::make({ 1, 2, 3, 4, 5, 23, 56, 126 }), x, y)
{
    using namespace hcl::core::frontend;

    DesignScope design;

    Clock clock(ClockConfig{}.setAbsoluteFrequency(10'000));
    ClockScope clkScp(clock);

    auto gcd_ref = [](unsigned a, unsigned b) -> unsigned {
        while (a != b) {
            if (a > b)
                a -= b;
            else
                b -= a;
        }
        return a;
    };


    unsigned maxTicks = 200;

    {
        BVec x_vec = ConstBVec(x, 8);
        BVec y_vec = ConstBVec(y, 8);

        Bit start;
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext& context) {
            context.set(0, context.getTick() == 0);
            }, start);

        BVec result;
        Bit done = false;

        {
            HCL_NAMED(x_vec);
            HCL_NAMED(y_vec);

            GroupScope entity(GroupScope::GroupType::ENTITY);
            entity
                .setName("gcd")
                .setComment("Statemachine to compute the GCD of two 8-bit integers.");


            Register<BVec> a(8u, Expansion::none);
            a.setReset(0b00000000_bvec);
            Register<BVec> b(8u, Expansion::none);
            b.setReset(0b00000000_bvec);

            IF(start) {
                a = x_vec;
                b = y_vec;
            }

            IF(a == b) {
                done = true;
            } ELSE {
                IF(a > b) {
                    a = a - b;
                } ELSE {
                    b = b - a;
                }
            }

            result = a;
            HCL_NAMED(result);
            HCL_NAMED(done);

            sim_debug() << "a is " << a.delay(1) << " and b is " << b.delay(1);
        }

        BVec ticks(8, Expansion::none);
        simpleSignalGenerator(clock, [](SimpleSignalGeneratorContext& context) {
            context.set(0, context.getTick());
            }, ticks);

        sim_assert((ticks < ConstBVec(maxTicks - 1, 8)) | done) << "The state machine should be idle after " << maxTicks << " cycles";
        BVec gtruth = ConstBVec(gcd_ref(x, y), 8);
        sim_assert((ticks < ConstBVec(maxTicks - 1, 8)) | (result == gtruth)) << "The state machine computed " << result << " but the correct answer is " << gtruth;
    }

    runTicks(design.getCircuit(), clock.getClk(), maxTicks);
}