#pragma once

#include "Scope.h"
#include "Bit.h"

#include <hcl/utils/Traits.h>
#include <hcl/hlim/coreNodes/Node_PriorityConditional.h>

#include <vector>

namespace hcl::core::frontend {
    
template<typename DataSignal, typename = std::enable_if_t<utils::isSignal<DataSignal>::value>>
class PriorityConditional
{
    public:
        PriorityConditional<DataSignal> &addCondition(const Bit &enableSignal, const DataSignal &value) {
            m_choices.push_back({});
            m_choices.back().first = enableSignal;
            m_choices.back().first.setName("");
            m_choices.back().second = value;
            m_choices.back().second.setName("");
            return *this;
        }

        DataSignal operator()(const DataSignal &defaultCase) {
            hlim::Node_PriorityConditional *node = DesignScope::createNode<hlim::Node_PriorityConditional>();
            node->recordStackTrace();
            node->connectDefault({.node = defaultCase.getNode(), .port = 0ull});
            
            for (auto &c : m_choices)
                node->addInput({.node = c.first.getNode(), .port = 0ull},
                               {.node = c.second.getNode(), .port = 0ull});

            return DataSignal({.node = node, .port = 0ull});
        }
    protected:
        std::vector<std::pair<Bit, DataSignal>> m_choices;
};



}
