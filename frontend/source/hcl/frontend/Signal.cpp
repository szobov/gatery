#include "Signal.h"
#include "Scope.h"

#include <hcl/hlim/coreNodes/Node_Rewire.h>

/*
    void ElementarySignal::assign(const ElementarySignal& rhs) {

        if (!m_node)
            init(rhs.getConnType());

        if (getName().empty())
            setName(rhs.getName());

#define INSERT_BRIDGE_NODES
        
#ifdef INSERT_BRIDGE_NODES
        hlim::Node_Signal *bridgeNode = DesignScope::createNode<hlim::Node_Signal>();
        bridgeNode->setConnectionType(rhs.getConnType());
        bridgeNode->recordStackTrace();
        bridgeNode->setName(m_node->getName());
#else
        hlim::Node_Signal *bridgeNode = m_node;
#endif

        if (ConditionalScope::get() == nullptr)
        {
            bridgeNode->connectInput(rhs.getReadPort());
        }
        else
        {
            hlim::Node_Multiplexer* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, getReadPort());
            mux->connectInput(1, rhs.getReadPort()); // assign rhs last in case previous port was undefined
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());

            bridgeNode->connectInput({ .node = mux, .port = 0ull });
        }
#ifdef INSERT_BRIDGE_NODES
        m_node->connectInput({ .node = bridgeNode, .port = 0ull });
#endif
    }
*/
   

hcl::core::frontend::SignalReadPort hcl::core::frontend::SignalReadPort::expand(size_t width, hlim::ConnectionType::Interpretation resultType) const
{
	hlim::ConnectionType type = connType(*this);
	HCL_DESIGNCHECK_HINT(type.width <= width, "signal width cannot be implicitly decreased");
	HCL_DESIGNCHECK_HINT(type.width == width || expansionPolicy != Expansion::none, "missmatching operands size and no expansion policy specified");

	hlim::NodePort ret{ .node = this->node, .port = this->port };

	if (type.width < width || type.interpretation != resultType)
	{
		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->changeOutputType(hlim::ConnectionType{ .interpretation = resultType, .width = width });
		rewire->connectInput(0, ret);

		switch (expansionPolicy)
		{
		case Expansion::one:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ONE);		break;
		case Expansion::zero:	rewire->setPadTo(width, hlim::Node_Rewire::OutputRange::CONST_ZERO);	break;
		case Expansion::sign:	rewire->setPadTo(width);	break;
		default: 
				HCL_ASSERT(type.width == width);
				rewire->setConcat();
				break;
		}

		auto* signal = DesignScope::createNode<hlim::Node_Signal>();
		signal->connectInput({ .node = rewire, .port = 0 });
		signal->setName(node->getName());
		ret = hlim::NodePort{ .node = signal, .port = 0 };
	}
	return SignalReadPort{ ret, expansionPolicy };
}
