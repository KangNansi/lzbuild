#pragma once
#include "tokenizer/tokenizer.hpp"
#include "tokenizer/extensions.hpp"
#include <memory>

namespace lzlang
{
    struct expression;
    struct expr_visitor
    {
        virtual void assign(tokenizer::token name, tokenizer::token value) = 0;
        virtual void _if(tokenizer::token condition, expression& _true, expression* _false = nullptr) = 0;
    };
    struct expression
    {
        virtual void visit(expr_visitor& visitor) = 0;
    };

    struct assign_expr : public expression
    {
        tokenizer::token name;
        tokenizer::token value;
        void visit(expr_visitor& visitor) override { visitor.assign(name, value); }
    };

    struct if_expr : public expression
    {
        tokenizer::token condition;
        std::unique_ptr<expression> _true;
        std::unique_ptr<expression> _false;
        void visit(expr_visitor& visitor) override { visitor._if(condition, *_true, _false.get()); }
    };

    struct block_expr : public expression
    {
        std::vector<std::unique_ptr<expression>> list;
        void visit(expr_visitor& visitor) override
        {
            for (auto& expr : list)
            {
                expr->visit(visitor);
            }
        }
    };

    std::unique_ptr<expression> parse(std::string text);
}
    