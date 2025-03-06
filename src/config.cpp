#include "config.hpp"
#include "programs/pkg_config.hpp"
#include "utility/term.hpp"
#include "tokenizer/tokenizer.hpp"
#include "tokenizer/extensions.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

using namespace std;

enum class token_type
{
    keyword,
    syntax,
    name,
    number,
    string_literal,
    comment,
};

enum class keywords 
{
    name,
    output,
    include,
    lib,
    link_etc,
    source,
    compiler,
    standard,
};

std::unordered_map<std::string, keywords> keywords_values = {
    {"name", keywords::name},
    {"output", keywords::output},
    {"include", keywords::include},
    {"lib", keywords::lib},
    {"link_etc", keywords::link_etc},
    {"source", keywords::source},
    {"compiler", keywords::compiler},
    {"standard", keywords::standard},
};
std::vector<std::string> keywords_keys = ([]() {
    std::vector<string> keys(keywords_values.size());
    std::transform(keywords_values.begin(), keywords_values.end(), keys.begin(), [](auto pair) { return pair.first; });
    return keys;
})();

tokenizer::engine<token_type> create_tokenizer_engine()
{
    tokenizer::engine<token_type> engine;
    engine.add<token_type::keyword, tokenizer::keyword_matcher>(keywords_keys);
    engine.add<token_type::syntax>("[\n]");
    engine.add<token_type::name>("[@_][@_#+-]*");
    engine.add<token_type::comment, tokenizer::line_comment_matcher, true>("#");
    engine.add<token_type::string_literal, tokenizer::string_literal_matcher>();
    return engine;
}

std::string read_str(const tokenizer::token<token_type>& token)
{
    if(token.type == token_type::string_literal) return token.value.substr(1, token.value.size()-2);
    else return token.value;
}

std::string read_name(tokenizer::parse_context<token_type>& ctx)
{
    auto token = ctx.advance();
    if(token.match<token_type::string_literal>() || token.match<token_type::name>())
    {
        return read_str(token);
    }
    throw std::runtime_error("unexpected token" + token.str());
}

std::vector<std::string> read_name_list(tokenizer::parse_context<token_type>& ctx)
{
    std::vector<std::string> values;
    while(ctx.match<token_type::string_literal>() || ctx.match<token_type::name>())
    {
        values.push_back(read_name(ctx));
    }
    return values;
}

void read_config(config& config, std::filesystem::path path)
{
    std::ifstream file(path);
    stringstream ss;
    ss << file.rdbuf();
    string text = ss.str();

    auto engine = create_tokenizer_engine();
    auto tokens = engine.tokenize(text);
    auto ctx = tokenizer::parse_context<token_type>(text, tokens);

    std::unordered_map<keywords, std::function<void()>> kw_parsers = {
        {keywords::lib, [&]() {
            auto libs = read_name_list(ctx);
            for(auto& lib: libs) config.libraries.push_back({
                .name = lib,
                .config = pkg_config::get_config(lib)
            });
        }},
        {keywords::name, [&]() {
            config.name = read_name(ctx);
        }},
        {keywords::include, [&]() {
            auto includes = read_name_list(ctx);
            config.include_folder.insert(config.include_folder.end(), includes.begin(), includes.end());
        }},
        {keywords::link_etc, [&]() {
            auto link_etc = read_name_list(ctx);
            config.link_etc.insert(config.link_etc.end(), link_etc.begin(), link_etc.end());
        }},
        {keywords::output, [&]() {
            auto token = ctx.advance();
            if(token.match<token_type::name>("library")) {
                config.is_library = true;
            }
            else if (token.match<token_type::name>("binary")) {
                config.is_library = false;
            }
            else {
                throw std::runtime_error("Invalid output value(library|binary): " + token.str());
            }
        }},
        {keywords::source, [&]() {
            auto sources = read_name_list(ctx);
            config.source_folders.insert(config.source_folders.end(), sources.begin(), sources.end());
        }},
        {keywords::compiler, [&]() {
            config.compiler = read_name(ctx);
        }},
        {keywords::standard, [&]() {
            config.standard = read_name(ctx);
        }},
    };

    while (!ctx.eof()) {
        auto token = ctx.advance();
        switch (token.type) {
        case token_type::keyword:
            if(auto keyword_it = keywords_values.find(token.value); keyword_it != keywords_values.end())
            {
                if(auto parser_it = kw_parsers.find(keyword_it->second); parser_it != kw_parsers.end())
                {
                    parser_it->second();
                    break;
                }
            }
            throw std::runtime_error("unexpected keyword " + token.str());
        case token_type::syntax:
            break;
        case token_type::name:
        case token_type::number:
        case token_type::string_literal:
        case token_type::comment:
            std::cerr << term::red << "Invalid token: " << term::reset << token.str() << std::endl;
            while (!ctx.eof() && !ctx.match<token_type::syntax>("\n"))
            {
                ctx.advance();
            }
          break;
        }
    }

    if(config.source_folders.empty())
    {
        config.source_folders.push_back("./src");
    }
}