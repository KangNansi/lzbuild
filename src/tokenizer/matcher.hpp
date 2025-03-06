#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace tokenizer {
    struct match_result {
        bool match;
        size_t size;
    };

    struct match_node {
        virtual bool match(std::string_view source, size_t& pos) const = 0;
    };
    using node = std::unique_ptr<match_node>;

    struct char_node : public match_node {
        char c;
        char_node(char c) : c(c) {}
        bool match(std::string_view source, size_t& pos) const override;
    };

    struct any_node : public match_node
    {
        bool match(std::string_view source, size_t& pos) const override { return pos < source.size(); }
    };

    struct range_node : public match_node {
        char from;
        char to;
        range_node(char from, char to) : from(from), to(to) {}
        bool match(std::string_view source, size_t& pos) const override;
    };

    struct or_node : public match_node {
        std::vector<node> selection;
        bool match(std::string_view source, size_t& pos) const override;
    };

    struct repeat_node : public match_node {
        node to_repeat;
        repeat_node(node to_repeat) : to_repeat(std::move(to_repeat)) {}
        bool match(std::string_view source, size_t& pos) const override;
    };

    struct option_node : public match_node {
        node option;
        option_node(node option) : option(std::move(option)) {}
        bool match(std::string_view source, size_t& pos) const override;
    };

    struct group_node : public match_node {
        std::vector<node> nodes;
        bool match(std::string_view source, size_t& pos) const override;
    };

    node build_matcher(const std::string expression);
};