/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once

#include "../frontend/BitWidth.h"

namespace gtry::utils
{
	std::optional<std::string_view> globbingMatchPath(std::string_view pattern, std::string_view str);
	std::string replaceEnvVars(const std::string& src);

	class DummyConfigTree
	{
	public:

		DummyConfigTree operator[](std::string_view path) const { return {}; }

		bool isDefined() const { return false; }
		bool isNull() const { return false; }
		bool isScalar() const { return false; }
		bool isSequence() const { return false; }
		bool isMap() const { return false; }

		explicit operator bool() const { return isDefined(); }

		using map_iterator = const DummyConfigTree*;
		map_iterator mapBegin() const { return nullptr; }
		map_iterator mapEnd() const { return nullptr; }

		using iterator = const DummyConfigTree*;
		iterator begin() const { return nullptr; }
		iterator end() const { return nullptr; }
		size_t size() const { return 0; }
		DummyConfigTree operator[](size_t index) const { return {}; }

		template<typename T> T as(const T& def) const { return def; }

		void loadFromFile(const std::filesystem::path& filename) {}
	};
}

#ifdef USE_YAMLCPP

namespace gtry::utils 
{

	class YamlConfigTree 
	{
	public:
		struct iterator {
			YAML::Node::const_iterator it;

			void operator++() { ++it; }
			YamlConfigTree operator*() { return YamlConfigTree(*it); }
			bool operator==(const iterator& rhs) const { return it == rhs.it; }
			bool operator!=(const iterator& rhs) const { return it != rhs.it; }
		};

		struct map_iterator
		{
			YAML::Node::const_iterator it;

			void operator++() { ++it; }
			YamlConfigTree operator*() { return YamlConfigTree(it->second); }
			bool operator==(const map_iterator& rhs) const { return it == rhs.it; }
			bool operator!=(const map_iterator& rhs) const { return it != rhs.it; }

			std::string key() const { return it->first.as<std::string>(); }
		};

	public:
		YamlConfigTree() = default;
		YamlConfigTree(YAML::Node node) { m_nodes.push_back(node); }

		explicit operator bool() const { return isDefined(); }
		bool isDefined() const;
		bool isNull() const;
		bool isScalar() const;
		bool isSequence() const;
		bool isMap() const;

		map_iterator mapBegin() const;
		map_iterator mapEnd() const;

		iterator begin() const;
		iterator end() const;
		size_t size() const;
		YamlConfigTree operator[](size_t index) const;

		YamlConfigTree operator[](std::string_view path) const;
		template<typename T> T as(const T& def) const;

		void loadFromFile(const std::filesystem::path &filename);

	protected:
		std::vector<YAML::Node> m_nodes;
	};

	template<typename T>
	inline T YamlConfigTree::as(const T& def) const
	{
		if (m_nodes.size() != 1)
			return def;

		return m_nodes.front().as<T>();
	}

	template<>
	inline std::string YamlConfigTree::as(const std::string& def) const
	{
		std::string ret;
		if (m_nodes.size() != 1)
			ret = def;
		else
			ret = m_nodes.front().as<std::string>();

		return replaceEnvVars(ret);
	}

	using ConfigTree = YamlConfigTree;
}

namespace YAML
{
	template<>
	struct convert<gtry::BitWidth>
	{
		static bool decode(const Node& node, gtry::BitWidth& out)
		{
			out = gtry::BitWidth{ node.as<uint64_t>() };
			return true;
		}
	};

	template<typename T>
	struct convert
	{
		static auto decode(const Node& node, T& out) -> std::enable_if_t<std::is_enum_v<T>, bool>
		{
			const std::string value = node.as<std::string>();
			const std::optional<T> eval = magic_enum::enum_cast<T>(value);
			if (eval)
			{
				out = *eval;
				return true;
			}
			else
			{
				std::ostringstream err;
				err << "unknown value '" << value << "' for enum " << magic_enum::enum_type_name<T>()
					<< ". Valid values are:";
				for (auto&& name : magic_enum::enum_names<T>())
					err << ' ' << name;
				err << '\n';

				throw std::runtime_error{ err.str() };
			}
			return false;
		}
	};
}

#else

namespace gtry::utils
{
	using ConfigTree = DummyConfigTree;
}

#endif // USE_YAMLCPP
