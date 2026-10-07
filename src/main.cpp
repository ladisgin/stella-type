#include <cstdio>
#include <iostream>
#include <memory>

#include "Parser.H"

#include "checker.hpp"

int main() {
    try {
        std::unique_ptr<Program> program(pProgram(stdin));
        if (!program)
            return 1;
        stella::check(static_cast<AProgram *>(program.get()));
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
