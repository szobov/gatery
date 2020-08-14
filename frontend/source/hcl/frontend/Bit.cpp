#include "Bit.h"
#include "BitVector.h"
#include "ConditionalScope.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>
#include <hcl/hlim/coreNodes/Node_Rewire.h>
#include <hcl/hlim/coreNodes/Node_Multiplexer.h>

namespace hcl::core::frontend {

    Bit::Bit()
    {
        createNode();
        assign('x');
    }

    Bit::Bit(const Bit& rhs) : Bit(rhs.getReadPort())
    {
    }

    Bit::Bit(const SignalReadPort& port)
    {
        createNode();
        m_node->connectInput(port);
    }

    Bit::Bit(hlim::Node_Signal* node, size_t offset) :
        m_node(node),
        m_offset(offset)
    {}

    const Bit Bit::operator*() const
    {
        hlim::NodePort ret{ .node = m_node, .port = 0 };
        hlim::ConnectionType type = ret.node->getOutputConnectionType(ret.port);

        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, ret);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            ret = hlim::NodePort{ .node = rewire, .port = 0 };
        }
        return SignalReadPort(ret, Expansion::zero);
    }

    size_t Bit::getWidth() const
    {
        return 1;
    }

    hlim::ConnectionType Bit::getConnType() const
    {
        return hlim::ConnectionType{
            .interpretation = hlim::ConnectionType::BOOL,
            .width = 1
        };
    }

    SignalReadPort Bit::getReadPort() const
    {
        hlim::NodePort ret = m_node->getDriver(0);
        hlim::ConnectionType type = ret.node->getOutputConnectionType(ret.port);

        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            // TODO: cache rewire node if m_node's input is unchanged
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
            rewire->connectInput(0, ret);
            rewire->changeOutputType(getConnType());

            size_t offset = std::min(m_offset, type.width-1); // used for msb alias, but can alias any future offset
            rewire->setExtract(offset, 1);

            ret = hlim::NodePort{ .node = rewire, .port = 0 };
        }
        return SignalReadPort{ ret, Expansion::zero };
    }

    std::string_view Bit::getName() const
    {
        return m_node->getName();
    }

    void Bit::setName(std::string name)
    {
        m_node->setName(move(name));
    }

    void Bit::createNode()
    {
        m_node = DesignScope::createNode<hlim::Node_Signal>();
        m_node->setConnectionType(getConnType());
        m_node->recordStackTrace();
    }

    void Bit::assign(bool value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(value, getConnType());
        assign(SignalReadPort(constant));
    }

    void Bit::assign(char value)
    {
        auto* constant = DesignScope::createNode<hlim::Node_Constant>(value, getConnType());
        assign(SignalReadPort(constant));
    }

    void Bit::assign(SignalReadPort in)
    {
        hlim::ConnectionType type = m_node->getOutputConnectionType(0);

        if (type.interpretation != hlim::ConnectionType::BOOL)
        {
            auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
            rewire->connectInput(0, m_node->getDriver(0));
            rewire->connectInput(1, in);
            rewire->changeOutputType(type);

            size_t offset = std::min(m_offset, type.width - 1); // used for msb alias, but can alias any future offset
            rewire->setReplaceRange(offset);

            in = SignalReadPort(rewire);
        }
        
        if (ConditionalScope::get())
        {
            auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(2);
            mux->connectInput(0, m_node->getDriver(0));
            mux->connectInput(1, in); // assign rhs last in case previous port was undefined
            mux->connectSelector(ConditionalScope::getCurrentConditionPort());

            in = SignalReadPort(mux);
        }

        m_node->connectInput(in);
    }

    bool Bit::valid() const
    {
        return true;
    }
    

}