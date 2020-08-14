#pragma once

#include "SignalMiscOp.h"
#include "Clock.h"

#include <hcl/hlim/supportNodes/Node_SignalGenerator.h>

#include <functional>
#include <vector>

namespace hcl::core::frontend {
    
class ElementarySignal;

class SimpleSignalGeneratorContext
{
    public:
        SimpleSignalGeneratorContext(const hlim::Node_SignalGenerator *node, sim::DefaultBitVectorState *state, const size_t *outputOffsets, std::uint64_t tick);
        
        inline std::uint64_t getTick() const { return m_tick; }
        void setValue(size_t output, std::uint64_t value);
        void setDefined(size_t output, std::uint64_t defined);
        void set(size_t output, std::uint64_t value, std::uint64_t defined = std::uint64_t(-1));
    protected:
        const hlim::Node_SignalGenerator *m_node;
        sim::DefaultBitVectorState *m_state;
        const size_t *m_outputOffsets;
        std::uint64_t m_tick;
};

namespace internal
{
    struct SignalDesc
    {
        SignalDesc(const ElementarySignal& sig) :
            connType(sig.getConnType()),
            name(sig.getName())
        {}

        hlim::ConnectionType connType;
        std::string_view name;
    };

    hlim::Node_SignalGenerator* createSigGenNode(const Clock& refClk, std::vector<SignalDesc>& signals, const std::function<void(SimpleSignalGeneratorContext& context)>& genCallback);
}
    


template<class ...Signals>
void simpleSignalGenerator(const Clock &refClk, const std::function<void(SimpleSignalGeneratorContext &context)> &genCallback, Signals &...allSignals)
{
    std::vector<internal::SignalDesc> signals = {allSignals...};
    hlim::Node_SignalGenerator* sigGenNode = internal::createSigGenNode(refClk, signals, genCallback);

    SignalReadPort port(sigGenNode);
    auto assign = [&](auto& signal) {
        signal = port;
        port.port++;
    };

    (assign(allSignals), ...);
}


}
