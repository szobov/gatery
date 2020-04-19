#pragma once

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/Node.h"
#include "../hlim/Circuit.h"

#include "../utils/Enumerate.h"
#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"

#include <boost/format.hpp>


namespace mhdl::core::frontend {
    
class BaseSignal {
    public:
        using isSignal = void;

        virtual ~BaseSignal() = default;
        
        virtual const char *getSignalTypeName() = 0;
        
        virtual void setName(std::string name) { }
    protected:
        //virtual void putNextSignalNodeInGroup(hlim::NodeGroup *group) = 0;
};

template<typename T, typename = std::enable_if<utils::isSignal<T>::value>::type>
void setName(T &signal, std::string name) {
    signal.setName(std::move(name));
}

#define MHDL_NAMED(x) mhdl::core::frontend::setName(x, #x)


class ElementarySignal : public BaseSignal {
    public:
        using isElementarySignal = void;
        
        size_t getWidth() const { return m_node->getConnectionType().width; }
        
        inline hlim::Node_Signal *getNode() const { return m_node; }
        virtual void setName(std::string name) override { m_node->setName(std::move(name)); }
    protected:
        ElementarySignal();
        ElementarySignal(hlim::Node::OutputPort *port, const hlim::ConnectionType &connectionType);
        ElementarySignal &operator=(const ElementarySignal&) = default;
        void assign(const ElementarySignal &rhs);

        mutable hlim::Node_Signal *m_node = nullptr; 

        virtual hlim::ConnectionType getSignalType(size_t width) const = 0;
        
        template<typename SignalType, typename>
        friend SignalType &assign(SignalType &lhs, const SignalType &rhs);
};

/*
hlim::Node_Signal *constructBinaryOperation(hlim::Node::OutputPort *lhs, hlim::Node::OutputPort *rhs, hlim::Node *opNode) {
    MHDL_ASSERT(dynamic_cast<hlim::Node_Signal*>(lhs->node) != nullptr);
    MHDL_ASSERT(dynamic_cast<hlim::Node_Signal*>(rhs->node) != nullptr);
}
*/



#define MHDL_SIGNAL \
        virtual const char *getSignalTypeName() override { return GET_FUNCTION_NAME; }


/*


#define MHDL_SUBSIGNAL(x) \
        registerSignal(#x, x)

        
        // Bundles
class CompoundSignal : public BaseSignal
{
    public:
        void registerSignal(const std::string &name, BaseSignal &signal);
        
        template<class Container, typename = std::enable_if<isContainer<Container>::value>::type>
        void registerSignal(const std::string &name, Container &signals) {
            for (auto pair : utils::Enumerate<Container>(signals))
                registerSignal((boost::format("%s[%i]") % name % pair.first).str(), pair.second);
        }
        
        CompoundSignal &operator=(const CompoundSignal &rhs);
        
        virtual void putNextSignalNodeInGroup(hlim::NodeGroup *group) override { }        
    protected:
        std::vector<BaseSignal*> m_subSignals;
};

*/

    
}
