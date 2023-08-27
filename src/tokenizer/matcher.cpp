#include "matcher.hpp"

namespace tokenizer {

    bool char_node::match(const std::string& source, size_t& pos) const
    {
        if(pos < source.size() && source[pos] == c){
            pos++;
            return true;
        }
        return false;
    }
    
    bool range_node::match(const std::string& source, size_t& pos) const {
        if (pos >= source.size())
        {
            return false;
        }
        char c = source[pos];
        if (c >= from && c <= to)
        {
            pos++;
            return true;
        } 
        return false;
    }
    
    bool or_node::match(const std::string& source, size_t& pos) const {
        for(auto& sub_node : selection){
            size_t sub_pos = pos;
            if(sub_node->match(source, sub_pos)){
                pos = sub_pos;
                return true;
            }
        }
        return false;
    }
    
    bool repeat_node::match(const std::string& source, size_t& pos) const {
        while (to_repeat->match(source, pos));
        return true;
    }

    bool option_node::match(const std::string& source, size_t& pos) const {
        option->match(source, pos);
        return true;
    }
    
    bool group_node::match(const std::string& source, size_t& pos) const {
        for (auto& sub_node : nodes)
        {
            if(!sub_node->match(source, pos)){
                return false;
            }
        }
        return true;
    }

    node alpha_node(){
        std::unique_ptr<or_node> select = std::make_unique<or_node>();
        select->selection.push_back(std::make_unique<range_node>('a', 'z'));
        select->selection.push_back(std::make_unique<range_node>('A', 'Z'));
        return select;
    }

    node parse_single(char c){
        switch (c)
        {
            case '@':
                return alpha_node();

            case '#':
                return std::make_unique<range_node>('0', '9');

            case '~':
                return std::make_unique<range_node>(32, CHAR_MAX);

            default:
                return std::make_unique<char_node>(c);
        }
    }

    node parse_group(const std::string expression, size_t& pos);

    node parse_or(const std::string expression, size_t& pos){
        std::unique_ptr<or_node> _or = std::make_unique<or_node>();
        while (pos < expression.size())
        {
            char c = expression[pos];
            switch (c)
            {
                case '\\':
                    _or->selection.push_back(parse_single(expression[++pos]));
                    break;

                case '(':
                    _or->selection.push_back(parse_group(expression, ++pos));
                    break;
                
                case ']':
                    if(_or->selection.size() == 1){
                        return std::move(_or->selection[0]);
                    }
                    return _or;
            
                default:
                    _or->selection.push_back(parse_single(expression[pos]));
                    break;
            }
            pos++;
        }
        return _or;
    }

    node parse_group(const std::string expression, size_t& pos){
        std::unique_ptr<group_node> group = std::make_unique<group_node>();
        while (pos < expression.size())
        {
            switch (expression[pos])
            {
                case '*':
                    {
                        group->nodes.back() = std::make_unique<repeat_node>(std::move(group->nodes.back()));
                    }
                    break;

                case '?':
                    group->nodes.back() = std::make_unique<option_node>(std::move(group->nodes.back()));
                    break;

                case '(':
                    group->nodes.push_back(parse_group(expression, ++pos));
                    break;

                case ')':
                    if(group->nodes.size() == 1){
                        return std::move(group->nodes[0]);
                    }
                    return group;

                case '[':
                    {
                        group->nodes.push_back(parse_or(expression, ++pos));
                    }
                break;

                case '\\':
                    group->nodes.push_back(std::make_unique<char_node>(expression[++pos]));
                    break;
            
                default:
                    group->nodes.push_back(parse_single(expression[pos]));
                    break;
            }
            pos++;
        }
        if(group->nodes.size() == 1){
            return std::move(group->nodes[0]);
        }
        return group;
    }
    
    node build_matcher(const std::string expression) {
        size_t pos = 0;
        return parse_group(expression, pos);
    }
};