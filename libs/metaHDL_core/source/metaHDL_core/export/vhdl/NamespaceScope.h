#pragma once

#include "../../hlim/Node.h"

#include "CodeFormatting.h"

#include <map>
#include <set>
#include <string>

#include <strings.h>

namespace mhdl::core::vhdl {

class AST;

struct NodeInternalStorageSignal
{
    hlim::BaseNode *node;
    size_t signalIdx;

    inline bool operator==(const NodeInternalStorageSignal &rhs) const { return node == rhs.node && signalIdx == rhs.signalIdx; }
    inline bool operator<(const NodeInternalStorageSignal &rhs) const { if (node < rhs.node) return true; if (node > rhs.node) return false; return signalIdx < rhs.signalIdx; }
};

/**
 * @todo write docs
 */
class NamespaceScope
{
    public:
        NamespaceScope(AST &ast, NamespaceScope *parent);
        virtual ~NamespaceScope() { }

        std::string allocateName(hlim::NodePort nodePort, const std::string &desiredName, CodeFormatting::SignalType type);
        const std::string &getName(hlim::NodePort nodePort) const;

        std::string allocateName(NodeInternalStorageSignal nodePort, const std::string &desiredName);
        const std::string &getName(NodeInternalStorageSignal nodePort) const;

        std::string allocateName(hlim::BaseClock *clock, const std::string &desiredName);
        const std::string &getName(hlim::BaseClock *clock) const;

        std::string allocateEntityName(const std::string &desiredName);
        std::string allocateBlockName(const std::string &desiredName);
        std::string allocateProcessName(const std::string &desiredName, bool clocked);
    protected:
        bool isNameInUse(const std::string &name) const;
        AST &m_ast;
        NamespaceScope *m_parent;
        
        struct CaseInsensitiveCompare { 
            bool operator() (const std::string& a, const std::string& b) const {
                return strcasecmp(a.c_str(), b.c_str()) < 0;
            }
        };

        std::set<std::string, CaseInsensitiveCompare> m_namesInUse;
        std::map<hlim::NodePort, std::string> m_nodeNames;
        std::map<NodeInternalStorageSignal, std::string> m_nodeStorageNames;
        std::map<hlim::BaseClock*, std::string> m_clockNames;
};


}
