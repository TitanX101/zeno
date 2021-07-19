#pragma once

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <tuple>
#include <map>

namespace zfx {

struct Options {
    bool const_parametrize = true;
    bool global_localize = true;
    bool demote_math_funcs = true;
    int arch_maxregs = 8;

    bool reassign_channels = true;

    //Options() = default;

    static constexpr struct {} for_x64{};
    Options(decltype(for_x64))
        : const_parametrize(true)
        , global_localize(true)
        , demote_math_funcs(true)
        , arch_maxregs(8)
    {}

    static constexpr struct {} for_cuda{};
    Options(decltype(for_cuda))
        : const_parametrize(false)
        , global_localize(false)
        , demote_math_funcs(false)
        , arch_maxregs(0)
    {}

    std::map<std::string, int> symdims;
    std::map<std::string, int> pardims;

    void define_symbol(std::string const &name, int dimension) {
        symdims[name] = dimension;
    }

    void define_param(std::string const &name, int dimension) {
        pardims[name] = dimension;
    }

    void dump(std::ostream &os) const {
        for (auto const &[name, dim]: symdims) {
            os << '/' << name << '/' << dim;
        }
        for (auto const &[name, dim]: pardims) {
            os << '\\' << name << '\\' << dim;
        }
        os << '|' << const_parametrize;
        os << '|' << global_localize;
        os << '|' << reassign_channels;
        os << '|' << arch_maxregs;
    }
};

std::tuple
    < std::string
    , std::vector<std::pair<std::string, int>>
    , std::vector<std::pair<std::string, int>>
    > compile_to_assembly
    ( std::string const &code
    , Options const &options
    );

struct Program {
    std::vector<std::pair<std::string, int>> symbols;
    std::vector<std::pair<std::string, int>> params;
    std::string assembly;

    auto const &get_assembly() const {
        return symbols;
    }

    auto const &get_symbols() const {
        return symbols;
    }

    auto const &get_params() const {
        return params;
    }

    int symbol_id(std::string const &name, int dim) const {
        auto it = std::find(
            symbols.begin(), symbols.end(), std::pair{name, dim});
        return it != symbols.end() ? it - symbols.begin() : -1;
    }

    int param_id(std::string const &name, int dim) const {
        auto it = std::find(
            params.begin(), params.end(), std::pair{name, dim});
        return it != params.end() ? it - params.begin() : -1;
    }
};

struct Compiler {
    std::map<std::string, std::unique_ptr<Program>> cache;

    Program *compile
        ( std::string const &code
        , Options const &options
        ) {
        std::ostringstream ss;
        ss << code << "<EOF>";
        options.dump(ss);
        auto key = ss.str();

        auto it = cache.find(key);
        if (it != cache.end()) {
            return it->second.get();
        }

        auto 
            [ assembly
            , symbols
            , params
            ] = compile_to_assembly
            ( code
            , options
            );
        auto prog = std::make_unique<Program>();
        prog->assembly = assembly;
        prog->symbols = symbols;
        prog->params = params;

        auto raw_ptr = prog.get();
        cache[key] = std::move(prog);
        return raw_ptr;
    }
};

}
