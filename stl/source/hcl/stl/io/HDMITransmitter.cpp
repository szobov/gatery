#include "HDMITransmitter.h"

#include "../utils/BitCount.h"

namespace hcl::stl::hdmi {

using namespace hcl::core::frontend;


core::frontend::BVec tmdsEncode(core::frontend::Clock &pixelClock, core::frontend::Bit dataEnable, core::frontend::BVec data, core::frontend::BVec ctrl)
{
    HCL_NAMED(dataEnable);
    HCL_NAMED(data);
    HCL_NAMED(ctrl);
    
    GroupScope entity(GroupScope::GroupType::ENTITY);
    entity
        .setName("tmdsEncode")
        .setComment("Encodes 8-bit data words to 10-bit TMDS words with control bits");
        

    HCL_DESIGNCHECK_HINT(data.getWidth() == 8, "data must be 8 bit wide");
    HCL_DESIGNCHECK_HINT(ctrl.getWidth() == 2, "data must be 8 bit wide");
    
    HCL_COMMENT << "Count the number of high bits in the input word";
    BVec sumOfOnes_data = bitcount(data);
    HCL_NAMED(sumOfOnes_data);   

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    const size_t subWidth = data.getWidth() - 1;
    BVec dataXNOR = data;
    dataXNOR(1, subWidth) = lxnor(dataXNOR(1, subWidth), dataXNOR(0, subWidth));
    BVec dataXOR = data;
    dataXOR(1, subWidth) ^= dataXOR(0, subWidth);

    HCL_NAMED(dataXNOR);
    HCL_NAMED(dataXOR);
    
    Bit useXnor = !((sumOfOnes_data > 4) | ((sumOfOnes_data == 4) & (!data[0])));
    HCL_NAMED(useXnor);
    BVec q_m = dataXOR;
    HCL_NAMED(q_m);
    IF (useXnor)
        q_m = dataXNOR;
    
    HCL_COMMENT << "Keep a running (signed) counter of the imbalance on the line, to modify future data encodings accordingly";
    Register<BVec> imbalance{ 4u, Expansion::none };
    imbalance.setReset("b0000");
    imbalance.setClock(pixelClock);
    HCL_NAMED(imbalance);
    
    core::frontend::BVec result(10);
    HCL_NAMED(result);
    
    HCL_COMMENT << "If sending data, 8/10 encode the data, otherwise encode the control bits";
    IF (dataEnable) {
        
        HCL_COMMENT << "Count the number of high bits in the xor/xnor word";
        BVec sumOfOnes_q_m = bitcount(q_m);
        HCL_NAMED(sumOfOnes_q_m);   
        
        Bit noPreviousImbalance = imbalance.delay(1) == 0;
        HCL_NAMED(noPreviousImbalance);
        Bit noImbalanceInQ_m = sumOfOnes_q_m == 4;
        HCL_NAMED(noImbalanceInQ_m);
        
        IF (noPreviousImbalance | noImbalanceInQ_m) {
            result(0, 8) = mux(useXnor, {q_m, ~q_m});
            result(8, 2) = cat(useXnor, ~useXnor);
            
            IF (useXnor) 
                imbalance = imbalance.delay(1) - 8 + sumOfOnes_q_m + sumOfOnes_q_m;
            ELSE
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
            
        } ELSE {
            Bit positivePreviousImbalance = !imbalance.delay(1).msb(); // Sign bit
            HCL_NAMED(positivePreviousImbalance);
            Bit positiveImbalanceInQ_m = sumOfOnes_q_m > 4;
            HCL_NAMED(positiveImbalanceInQ_m);
            IF ((positivePreviousImbalance & positiveImbalanceInQ_m) |
                ((!positivePreviousImbalance) & (!positiveImbalanceInQ_m))) {
                
                result(0, 8) = ~q_m;
                result(8, 2) = cat(useXnor, '1');
                
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2;
            } ELSE {
                result(0, 8) = q_m;
                result(8, 2) = cat(useXnor, '1');
                
                imbalance = imbalance.delay(1) + 8 - sumOfOnes_q_m - sumOfOnes_q_m;
                IF (useXnor)
                    imbalance = (BVec) imbalance + 2;
            }
        }
    } ELSE {
        PriorityConditional<core::frontend::BVec> con;
        
        con
            .addCondition(ctrl == "b00", "b1101010100")
            .addCondition(ctrl == "b01", "b0010101011")
            .addCondition(ctrl == "b10", "b0101010100")
            .addCondition(ctrl == "b11", "b1010101011");
            
        result = con("b0000000000");
        
        imbalance = "b0000";
    }

    return result;
}

core::frontend::BVec tmdsEncodeReduceTransitions(const core::frontend::BVec& data)
{
    HCL_COMMENT << "Count the number of high bits in the input word";
    BVec sumOfOnes = bitcount(data);
    HCL_NAMED(sumOfOnes);

    HCL_COMMENT << "Prepare XORed and XNORed data words to select from based on number of high bits";

    Bit invert = (sumOfOnes > 4u) | (sumOfOnes == 4u & !data.lsb());

    HCL_COMMENT << "Decode using 1=xor, 0=xnor";
    BVec tmdsReduced = cat(~invert, data);
    for (auto i : utils::Range<size_t>(1, data.getWidth()))
        tmdsReduced[i] ^= tmdsReduced[i - 1] ^ invert;

    HCL_NAMED(tmdsReduced);
    return tmdsReduced;
}

core::frontend::BVec tmdsDecodeReduceTransitions(const core::frontend::BVec& data)
{
    BVec decoded = data(0, data.getWidth() - 1);
    decoded ^= decoded << 1;
    decoded(1, decoded.getWidth() - 1) ^= ~data.msb();

    HCL_NAMED(decoded);
    return decoded;
}

core::frontend::BVec tmdsEncodeBitflip(const core::frontend::Clock& clk, const core::frontend::BVec& data)
{
    HCL_COMMENT << "count the number of uncompensated ones";
    Register<BVec> global_counter{3ull, Expansion::none };
    global_counter.setClock(clk);
    global_counter.setReset("b000");
    HCL_NAMED(global_counter);

    // TODO: depend with and start value on data width
    BVec word_counter = "b100";
    for (const Bit& b : data)
        word_counter += b;

    Bit invert = word_counter[word_counter.getWidth() - 1] == global_counter.delay(1)[global_counter.getWidth() - 1];
    HCL_NAMED(invert);

    BVec result = cat(invert, data); // TODO: data ^ invert
    HCL_NAMED(result);

    IF(invert)
    {
        // TODO: add sub/add alu
        global_counter = global_counter.delay(1) - word_counter; // TODO: initialize registers with its own delay value
        result = cat('1', ~data);
    }
    ELSE
    {
        global_counter = global_counter.delay(1) + word_counter;
    }

    return result;
}

core::frontend::BVec tmdsDecodeBitflip(const core::frontend::BVec& data)
{
    // TODO: should be return data(0, -1) ^ data[back];
    return data(0, data.getWidth() - 1) ^ data.msb();
}

TmdsEncoder::TmdsEncoder(core::frontend::Clock& clk) :
    m_Clk{clk}
{
    m_Channel.fill("b0010101011"); // no data symbol
    m_Channel[0].setName("redChannel"); // TODO: convinient method to name arrays
    m_Channel[1].setName("greenChannel");
    m_Channel[2].setName("blueChannel");
}

void TmdsEncoder::addSync(const core::frontend::Bit& hsync, const core::frontend::Bit& vsync)
{
    IF(hsync)
        setSync(true, false);
    IF(vsync)
        setSync(false, true);
    IF(hsync & vsync)
        setSync(true, true);
}

void hcl::stl::hdmi::TmdsEncoder::setColor(const ColorRGB& color)
{
    m_Channel[0] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.r));
    m_Channel[1] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.g));
    m_Channel[2] = tmdsEncodeBitflip(m_Clk, tmdsEncodeReduceTransitions(color.b));
}

void hcl::stl::hdmi::TmdsEncoder::setSync(bool hsync, bool vsync)
{
    if (hsync && vsync)
        m_Channel[2] = "b1010101011";
    else if (hsync)
        m_Channel[2] = "b0010101011";
    else if (vsync)
        m_Channel[2] = "b0101010100";
    else
        m_Channel[2] = "b0010101011";
}

void hcl::stl::hdmi::TmdsEncoder::setTERC4(core::frontend::BVec ctrl)
{
    std::array<BVec, 16> trec4lookup = {
        "b1010011100",
        "b1001100011",
        "b1011100100",
        "b1011100010",
        "b0101110001",
        "b0100011110",
        "b0110001110",
        "b0100111100",
        "b1011001100",
        "b0100111001",
        "b0110011100",
        "b1011000110",
        "b1010001110",
        "b1001110001",
        "b0101100011",
        "b1011000011"
    };

    HCL_ASSERT(ctrl.getWidth() == 6);
    m_Channel[0] = mux(ctrl(0, 2), trec4lookup); // TODO: improve mux to accept any container as second input
    m_Channel[1] = mux(ctrl(2, 2), trec4lookup); // TODO: subrange as argument for mux
    m_Channel[2] = mux(ctrl(4, 2), trec4lookup);
}

SerialTMDS hcl::stl::hdmi::TmdsEncoder::serialOutput() const
{
    // TODO: use shift register/serdes lib for automatic vendor specific serdes usage

    Clock fastClk = m_Clk.deriveClock(ClockConfig{}.setFrequencyMultiplier(10).setName("TmdsEncoderFastClock"));

    Register<BVec> chan[3];
    chan[0].setClock(fastClk);
    chan[1].setClock(fastClk);
    chan[2].setClock(fastClk);
    
    for (auto& c : chan)
        c = c.delay(1) >> 1;

    Register<BVec> shiftCounter(4u, Expansion::none);
    shiftCounter.setReset("4b0");
    shiftCounter.setClock(fastClk);
    HCL_NAMED(shiftCounter);
    shiftCounter = shiftCounter.delay(1) + 1u;

    IF(shiftCounter == 9)
    {
        shiftCounter = 0;

        for (size_t i = 0; i < m_Channel.size(); ++i)
            chan[i] = m_Channel[i]; // TODO: clock domain crossing lib and warning
    }

    SerialTMDS out;
    // TODO: convinient method to name signals in structs

    out.clock.pos = true; // TODO: signal from clock
    out.clock.neg = false;

    for (size_t i = 0; i < m_Channel.size(); ++i)
    {
        out.data[i].pos = chan[i].lsb();
        out.data[i].neg = ~chan[i].lsb();
    }

    return SerialTMDS();
}

}
