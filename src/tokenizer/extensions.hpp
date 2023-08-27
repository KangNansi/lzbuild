#pragma once
#include "matcher.hpp"

namespace tokenizer
{
    struct string_literal_matcher: match_node
    {
        bool match(const std::string& source, size_t& pos) const override
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

    struct keyword_matcher: match_node
    {
        std::vector<std::string> m_words;

        template<typename... Args>
        keyword_matcher(Args... args): m_words({ args... }) {}

        bool match(const std::string& source, size_t& pos) const override
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
        bool match_word(const std::string& source, size_t pos, const std::string& word) const
        {
            for (size_t i = 0; i < word.size(); i++)
            {
                if (pos + i >= source.size() || source[pos + i] != word[i])
                {
                    return false;
                }
            }
            return true;
        }
    };

    struct line_comment_matcher : match_node
    {
        std::string start_word;

        line_comment_matcher(std::string start_word) : start_word(start_word) {}

        bool match(const std::string& source, size_t& pos) const override
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