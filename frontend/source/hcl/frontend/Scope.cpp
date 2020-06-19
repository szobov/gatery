#include "Scope.h"


namespace hcl::core::frontend {
    
GroupScope::GroupScope(hlim::NodeGroup::GroupType groupType) : BaseScope<GroupScope>()
{
    m_nodeGroup = m_parentScope->m_nodeGroup->addChildNodeGroup(groupType);
    m_nodeGroup->recordStackTrace();
}

GroupScope::GroupScope(hlim::NodeGroup *nodeGroup) : BaseScope<GroupScope>()
{
    m_nodeGroup = nodeGroup;
}


GroupScope &GroupScope::setName(std::string name)
{
    m_nodeGroup->setName(std::move(name));
    return *this;
}

GroupScope &GroupScope::setComment(std::string comment)
{
    m_nodeGroup->setComment(std::move(comment));
    return *this;
}


DesignScope::DesignScope() : BaseScope<DesignScope>(), m_rootScope(m_circuit.getRootNodeGroup())
{ 
    m_rootScope.setName("root");
    
    HCL_DESIGNCHECK_HINT(m_parentScope == nullptr, "Only one design scope can be active at a time!");
}

}
