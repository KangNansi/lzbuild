#pragma once
#include <sstream>
#include <stdexcept>
#include <string>
#include "matcher.hpp"
#include <string_view>
#include <type_traits>
#include <functional>
#include <utility>

namespace tokenizer
{
    template<typename token_type>
    struct token {
        token_type type;
        size_t index;
        int size;
        size_t line;
        size_t line_index;
        std::string value;
        std::string str() const {
            std::stringstream ss;
            ss << (int)type << "[" << line << ", " << line_index << "] " << value;
            return ss.str();
        }
        template<token_type type, const char* value>
        inline bool match() const { return this->type == type && this->value == value; }
        template<token_type type, char c>
        inline bool match() const { return this->type == type && this->value.length() == 1 && this->value[0] == c; }
        
        template<token_type type> inline bool match() const { return this->type == type; }
        template<token_type type>
        inline bool match(const char* value) const { return this->type == type && this->value == value; }
        template<token_type type>
        inline bool match(char c) const { return this->type == type && this->value.length() == 1 && this->value[0] == c; }
        
        const token& assert_token(short type, const char* value) const
        {
            if (!match(type, value)) throw std::runtime_error("Unexpected token " + this->str() + ", expected " + value);
            return *this;
        }
        const token& assert_token(short type) const
        {
            if (!match(type)) throw std::runtime_error("Unexpected token " + this->str() + ", expected " + std::to_string(type));
            return *this;
        }
        const token& assert_token(short type, char c) const
        {
            if (!match(type, c))
                throw std::runtime_error("Unexpected token " + this->str() + ", expected " + c);
            return *this;
        }
    };

    template<typename token_type>
    const token<token_type> empty_token = { static_cast<token_type>(-1), 0, 0, 0, 0, ""};

    template<typename token_type>
    class engine {
        using token_t = token<token_type>;
        struct rule {
            token_type type;
            bool ignored;
            node value;
        };

        std::vector<rule> _rules;

    public:
        template<token_type type, bool ignored = false>
        void add(const std::string expression){
            _rules.push_back(rule{
                .type = type,
                .ignored = ignored,
                .value = build_matcher(expression)
            });
        }

        template <token_type type, typename TNode, bool ignored = false, typename ...TArgs>
        void add(TArgs... args)
        {
            static_assert(std::is_base_of<match_node, TNode>::value, "incorrect type");
            _rules.push_back(rule {
                .type = type,
                .ignored = ignored,
                .value = std::make_unique<TNode>(args...)
            });
        }

        std::vector<token_t> tokenize(std::string_view text) const{
            std::vector<token_t> result;
            size_t pos = 0;
            size_t line_count = 1;
            size_t prev_line = 0;
            while (pos < text.size())
            {
                size_t start = pos;
                bool found = false;
                for(auto& rule: _rules){
                    if (rule.value->match(text, pos) && pos > start)
                    {
                        if (!rule.ignored)
                        {
                            token_t tok = {
                                rule.type,
                                start,
                                (int)(pos - start),
                                line_count,
                                start - prev_line,
                                std::string(text.substr(start, pos - start))
                            };
                            result.push_back(tok);
                        }
                        for (size_t i = start; i < pos; i++)
                        {
                            if (text[i] == '\n')
                            {
                                line_count++;
                                prev_line = i;
                            }
                        }
                        found = true;
                        break;
                    }
                    else
                    {
                        pos = start;
                    }
                }
                if (!found)
                {
                    if (text[pos] == '\n')
                    {
                        line_count++;
                        prev_line = pos;
                    }
                    pos++;
                }
            }
            return result;
        }
    };

    template<typename token_type>
    struct parse_error
    {
        token<token_type> target;
        short expected_type;
        std::string expected_value;
    };

    template<typename token_type>
    class parse_context
    {
        using token_t = token<token_type>;
        const std::string_view _txt;
        const std::vector<token_t>& _tokens;
        size_t cursor = 0;

    public:
        parse_context(const std::string_view txt, const std::vector<token_t>& tokens) : _txt(txt), _tokens(tokens) {}

        inline bool eof() const { return cursor >= _tokens.size(); }
        inline const token_t& current() const { return eof() ? empty_token<token_type> : _tokens[cursor]; }
        inline const token_t& get(int offset) const { return cursor + offset >= _tokens.size() ? empty_token<token_type> : _tokens[cursor+offset]; }
        inline void next() { cursor++; }
        inline const token_t& operator++() { return advance(); }
        inline const token_t& operator++(int) { return advance(); }
        inline const token_t& advance() {
            auto& tok = current();
            next();
            return tok;
        }
        template<token_type type>
        inline void skip_block(char open, char close) {
            int ind = 0;
            while(!eof()) {
                if(current().template match<type>(open)) {
                    ind++;
                }
                else if(current().template match<type>(close)) {
                    ind--;
                    if(ind == 0) {
                        next();
                        return;
                    }
                }
                next();
            }
        }
        template<token_type type>
        inline void to_next() {
            while (!eof() && current().template match<type>()) {
                next();
            }
        }
        inline void to_next(std::function<bool(const token_t&)> predicate) {
            while (!eof() && !predicate(current())) {
                next();
            }
        }

        template<token_type type_value, typename ...TArgs>
        inline bool match(TArgs ...args) const {
            auto& token = current();
            if(eof()) return false;
            return token.template match<type_value>(std::forward<TArgs>(args)...);
            // return eof() ? false : current().match<type>(std::forward<TArgs>(args)...);
        }

        template<token_type type, typename ...TArgs>
        inline const token_t& assert_token(TArgs... args) const {
            if(!match<type>(args...))

                throw std::runtime_error(std::string("parse error on ") + current().str());
            return current();
        }

        std::string_view get_substr(token_t& from, token_t& to)
        {
            return _txt.substr(from.index, to.index-from.index);
        }
    };

    template<typename token_type>
    std::ostream& operator<<(std::ostream& stream, const token<token_type>& token){
        stream << token.str();
        return stream;
    }

    template<typename token_type>
    void skip_until(const std::vector<token<token_type>>& tokens, size_t cursor, std::function<bool(const token<token_type>&)> test_func){
        while (cursor < tokens.size() && !test_func(tokens[cursor]))
        {
            cursor++;
        }
    }    
}