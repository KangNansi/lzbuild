#pragma once
#include "matcher.hpp"
#include <string>
#include <vector>

namespace tokenizer
{
    struct string_literal_matcher: match_node
    {
        bool match(std::string_view source, size_t& pos) const override
        {
            if (source[pos] != '\"')
            {
                return false;
            }
            pos++;
            while (pos < source.size())
            {
                if (source[pos] == '\\')
                {
                    pos+=2;
                }
                else if (source[pos] == '\"')
                {
                    pos++;
                    return true;
                }
                else
                {
                    pos++;
                }
            }
            return false;
        }
    };

    struct bracing_matcher : match_node
    {
        std::string _start;
        std::string _end;
        
        bracing_matcher(std::string start, std::string end): _start(start), _end(end) {}

        bool match(std::string_view source, size_t& pos) const override
        {
            for (size_t s = 0; pos < source.size() && s < _start.length(); s++, pos++)
            {
                if (source[pos] != _start[s])
                {
                    return false;
                }
            }
            size_t end_pos = 0;
            while (pos < source.length() && end_pos < _end.length())
            {
                if (source[pos] == _end[end_pos])
                {
                    end_pos++;
                }
                else
                {
                    end_pos = 0;
                }
                pos++;
            }
            return end_pos == _end.length();
        }
    };

    struct keyword_matcher: match_node
    {
        std::vector<std::string> m_words;

        template<typename... Args>
        keyword_matcher(Args... args): m_words({ args... }) {}
        keyword_matcher(std::vector<std::string>& words): m_words(words) {}

        bool match(std::string_view source, size_t& pos) const override
        {
            for (size_t i = 0; i < m_words.size(); i++)
            {
                if (match_word(source, pos, m_words[i]))
                {
                    pos += m_words[i].size();
                    return true;
                }
            }
            return false;
        }

    private:
        bool match_word(std::string_view source, size_t pos, std::string_view word) const
        {
            for (size_t i = 0; i < word.size(); i++)
            {
                if (pos + i >= source.size() || source[pos + i] != word[i])
                {
                    return false;
                }
            }
            if(pos + word.size() < source.size() && (isalnum(source[pos + word.size()]) || source[pos + word.size()] == '_'))
            {
                return false;
            }
            return true;
        }
    };

    struct line_comment_matcher : match_node
    {
        std::string start_word;

        line_comment_matcher(std::string start_word) : start_word(start_word) {}

        bool match(std::string_view source, size_t& pos) const override
        {
            if (source.substr(pos, start_word.size()) == start_word)
            {
                pos += start_word.size();
                for (;pos < source.size() && source[pos] != '\n'; ++pos);
                return true;
            }
            return false;
        }
    };
}