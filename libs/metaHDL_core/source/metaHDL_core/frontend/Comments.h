#pragma once

#include <sstream>
#include <string>

namespace mhdl::core::frontend {
    
class Comments
{
    public:
        static std::stringstream &get() { return m_comments; }
        static std::string retrieve() { std::string s = m_comments.str(); m_comments.str(std::string()); return s; }
    protected:
        static thread_local std::stringstream m_comments;
};

}

#define MHDL_COMMENT mhdl::core::frontend::Comments::get()

