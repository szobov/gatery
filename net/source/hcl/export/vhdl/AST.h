#pragma once

#include "NamespaceScope.h"

#include <filesystem>

#include <vector>
#include <memory>
#include <string>

namespace hcl::core::hlim {
    class Circuit;
    class BaseNode;
}

namespace hcl::core::vhdl {

class Entity;
class BaseGrouping;
class BasicBlock;
class CodeFormatting;

class Hlim2AstMapping
{
    public:
        void assignNodeToScope(hlim::BaseNode *node, BaseGrouping *scope);
        BaseGrouping *getScope(hlim::BaseNode *node) const;
    protected:
        std::map<hlim::BaseNode*, BaseGrouping*> m_node2Block;
};


class AST
{
    public:
        AST(CodeFormatting *codeFormatting);
        ~AST();
        
        void convert(hlim::Circuit &circuit);
        
        Entity &createEntity(const std::string &desiredName, BasicBlock *parent);
        inline CodeFormatting &getCodeFormatting() { return *m_codeFormatting; }
        inline NamespaceScope &getNamespaceScope() { return m_namespaceScope; }
        inline Hlim2AstMapping &getMapping() { return m_mapping; }
        
        void writeVHDL(std::filesystem::path destination);
    protected:
        CodeFormatting *m_codeFormatting;
        NamespaceScope m_namespaceScope;
        std::vector<std::unique_ptr<Entity>> m_entities;
        Hlim2AstMapping m_mapping;
};

}