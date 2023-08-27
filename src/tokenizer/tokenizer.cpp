#include "tokenizer.hpp"
#include <sstream>

namespace tokenizer{
    short engine::add(const std::string expression) {
        short index = types.size();
        types.push_back(build_matcher(expression));
        return index;
    }

    std::vector<token> engine::tokenize(const std::string& text) const
    {
        std::vector<token> result;
        size_t pos = 0;
        size_t line_count = 1;
        size_t prev_line = 0;
        while (pos < text.size())
        {
            size_t start = pos;
            bool found = false;
            for(size_t ruleIndex = 0; ruleIndex < types.size(); ruleIndex++){
                if(types[ruleIndex]->match(text, pos) && pos > start){
                    token tok = {
                        (short)ruleIndex,
                        start,
                        (int)(pos - start),
                        line_count,
                        start - prev_line,
                        text.substr(start, pos - start)
                    };
                    for (size_t i = start; i < pos; i++)
                    {
                        if (text[i] == '\n')
                        {
                            line_count++;
                            prev_line = i;
                        }
                    }
                    result.push_back(tok);
                    found = true;
                    break;
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

    std::string token::str() const
    {
        std::stringstream ss;
        ss << type << " [" << line << ", " << line_index << "] " << value;
        return std::string(ss.str());
    }

    std::ostream& operator<<(std::ostream& stream, const token& token) {
        stream << token.type << " [" << token.line << ", " << token.line_index << "] " << token.value;
        return stream;
    }
}