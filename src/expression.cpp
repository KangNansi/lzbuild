#include "expression.hpp"

using namespace tokenizer;
using namespace lzlang;

enum TokenType
{
    Keyword = 0,
    Syntax = 1,
    Name = 2,
    Comment = 3,
    String = 4
};

std::string str(short type)
{
    switch (type)
    {
    case TokenType::Keyword: return "Keyword";
    case TokenType::Syntax: return "Syntax";
    case TokenType::Name: return "Name";
    case TokenType::Comment: return "Comment";
    case TokenType::String: return "String";
    default: return "Unknown";
    }
}

bool match(token token, short type, std::string value)
{
    return token.type == type && token.value == value;
}

bool match(token token, short type)
{
    return token.type == type;
}

token assert(token token, short type, std::string value)
{
    if (!match(token, type, value)) throw "Unexpected token " + token.str() + ", expected " + str(type) + ": " + value;
    return token;
}

token assert(token token, short type)
{
    if (!match(token, type)) throw "Unexpected token " + token.str() + ", expected type: " + str(type);
    return token;
}

std::unique_ptr<expression> parse_expression(const std::vector<token>& tokens, size_t& cursor);

std::unique_ptr<expression> parse_ifexpr(const std::vector<token>& tokens, size_t& cursor)
{
    std::unique_ptr<if_expr> ifexpr = std::make_unique<if_expr>();
    ifexpr->condition = assert(tokens[cursor++], TokenType::Name);
    ifexpr->_true = parse_expression(tokens, cursor);
    if (cursor < tokens.size() && match(tokens[cursor], TokenType::Keyword, "else"))
    {
        cursor++;
        ifexpr->_false = parse_expression(tokens, cursor);
    }
    return ifexpr;
}

std::unique_ptr<expression> parse_assign(const std::vector<token>& tokens, size_t& cursor)
{
    std::unique_ptr<assign_expr> assign = std::make_unique<assign_expr>();
    assign->name = tokens[cursor++];
    assert(tokens[cursor++], TokenType::Syntax, "=");
    assert(tokens[cursor], TokenType::String);
    assign->value = tokens[cursor++];
    return assign;
}

std::unique_ptr<expression> parse_block(const std::vector<token>& tokens, size_t& cursor)
{
    std::unique_ptr<block_expr> block = std::make_unique<block_expr>();
    while (cursor < tokens.size())
    {
        if (match(tokens[cursor], TokenType::Syntax, "}"))
        {
            cursor++;
            return block;
        }
        block->list.push_back(parse_expression(tokens, cursor));
    }
    return block;
}

std::unique_ptr<expression> parse_expression(const std::vector<token>& tokens, size_t& cursor)
{
    auto current = tokens[cursor];
    if (match(current, TokenType::Keyword, "if"))
    {
        return parse_ifexpr(tokens, ++cursor);
    }
    else if(match(current, TokenType::Name))
    {
        return parse_assign(tokens, cursor);
    }
    else if (match(current, TokenType::Syntax, "{"))
    {
        return parse_block(tokens, ++cursor);
    }
    else
    {
        throw "Unexpected token";
    }
}

std::unique_ptr<expression> lzlang::parse(std::string text)
{
    tokenizer::engine token_engine;
    token_engine.add<tokenizer::keyword_matcher>("if", "else");
    token_engine.add("[{}=]");
    token_engine.add("[@_][@_#]*");
    token_engine.add<tokenizer::line_comment_matcher>("#");
    token_engine.add<tokenizer::string_literal_matcher>();

    auto tokens = token_engine.tokenize(text);

    size_t cursor = 0;
    return parse_block(tokens, cursor);
}