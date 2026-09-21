#include "gdcode/Compiler.hpp"

#include "gdcode/Lexer.hpp"
#include "gdcode/Lowering.hpp"
#include "gdcode/Parser.hpp"

namespace gdcode {

CompileResult compile(std::string const& source, CompileOptions const& options) {
    CompileResult result;
    auto const& limits = options.limits;

    if (source.size() > limits.maxSourceBytes) {
        result.diagnostics.error({}, "source-too-large",
                                 "source is " + std::to_string(source.size()) +
                                     " bytes; the limit is " +
                                     std::to_string(limits.maxSourceBytes));
        return result;
    }
    std::size_t lineCount = 1;
    for (char c : source) {
        if (c == '\n') {
            if (++lineCount > limits.maxLines) {
                result.diagnostics.error({}, "too-many-lines",
                                         "source has more than " +
                                             std::to_string(limits.maxLines) +
                                             " lines");
                return result;
            }
        }
    }

    Lexer lexer(source, result.diagnostics);
    std::vector<Token> tokens = lexer.lex();

    Parser parser(std::move(tokens), result.diagnostics);
    Program program = parser.parse();

    Lowering lowering(program, result.diagnostics, limits, options.defaultLevelName);
    result.ir = lowering.run();
    result.diagnostics.sortByPosition();

    auto const& stats = lowering.stats();
    result.stats.objectCount = result.ir.objects.size();
    result.stats.expandedStatements = stats.expandedStatements;
    result.stats.seed = stats.seedExplicit ? stats.seed : 1;
    result.stats.seedExplicit = stats.seedExplicit;
    result.stats.randomUsed = stats.randomUsed;
    return result;
}

} // namespace gdcode
