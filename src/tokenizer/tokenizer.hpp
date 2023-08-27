#pragma once
#include <string>
#include "matcher.hpp"

namespace tokenizer {
    struct token {
        short type;
        size_t index;
        int size;
        size_t line;
        size_t line_index;
        std::string value;
        std::string str() const ;
    };

    class engine {
        std::vector<node> types;

        public:
            short add(const std::string expression);

            template <typename TNode>
            short add()
            {
                static_assert(std::is_base_of<match_node, TNode>::value, "incorrect type");
                types.push_back(std::make_unique<TNode>());
                return types.size() - 1;
            }
            template <typename TNode, typename ...TArgs>
            short add(TArgs... args)
            {
                static_assert(std::is_base_of<match_node, TNode>::value, "incorrect type");
                types.push_back(std::make_unique<TNode>(args...));
                return types.size() - 1;
            }
            std::vector<token> tokenize(const std::string& text) const;
    };

    std::ostream& operator<<(std::ostream& stream, const token& token);
}