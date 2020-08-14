#pragma once
    
#include "Node.h"

#include <hcl/hlim/coreNodes/Node_Signal.h>

namespace hcl::vis {
    
class Node_Signal : public Node
{
    public:
        Node_Signal(CircuitView *circuitView, core::hlim::Node_Signal *hlimNode);

        enum { Type = UserType + 2 };
        int type() const override { return Type; }
        
        inline core::hlim::BaseNode *getHlimNode() { return m_hlimNode; }
    protected:
        core::hlim::Node_Signal *m_hlimNode;
};


}