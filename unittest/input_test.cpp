#include "external/boost/ut.hpp"

#include "game.h"

#include <sstream>
#include <iostream>

namespace ut = boost::ut;

// read_input_line() is the whole of the engine's defence against being driven by something that is
// not a person. Every prompt sits in a loop that asks again when it cannot use the answer, so if
// the end of input is not reported the loop never ends.
ut::suite<"Input"> input_suite = [] {
    using namespace ut;

    "a line is read and handed to the parser"_test = [] {
        std::istringstream input("24 24\n");
        auto *previous = std::cin.rdbuf(input.rdbuf());

        parser::string_parser parser;
        bool read = read_input_line(parser);

        std::cin.rdbuf(previous);
        std::cin.clear();

        expect(read) << "input was available";
        expect(parser.get_token().get_number().value_or(-1) == 24_i);
    };

    "the end of input is reported rather than looped on"_test = [] {
        std::istringstream input("");
        auto *previous = std::cin.rdbuf(input.rdbuf());

        parser::string_parser parser;
        bool first = read_input_line(parser);
        bool second = read_input_line(parser); // a caller that ignores the first answer must not
                                               // be told something different the second time

        std::cin.rdbuf(previous);
        std::cin.clear();

        expect(!first) << "empty input ends immediately";
        expect(!second) << "and stays ended";
    };

    "blank lines are not answers"_test = [] {
        // The parser's operator>> starts with `>> std::ws`, so it steps over blank lines rather
        // than returning one. At a terminal that is what a person expects: pressing Enter on an
        // empty prompt does not answer it, and the engine waits. A stream that holds nothing but
        // whitespace therefore reads as ended -- correctly, since it will never say anything.
        std::istringstream blank("   \n\n  \n");
        auto *previous = std::cin.rdbuf(blank.rdbuf());

        parser::string_parser parser;
        bool read = read_input_line(parser);

        std::cin.rdbuf(previous);
        std::cin.clear();

        expect(!read) << "whitespace alone is not an answer";
    };

    "a blank line between two answers is stepped over"_test = [] {
        std::istringstream input("24\n\n32\n");
        auto *previous = std::cin.rdbuf(input.rdbuf());

        parser::string_parser first_line;
        parser::string_parser second_line;
        bool first = read_input_line(first_line);
        bool second = read_input_line(second_line);

        std::cin.rdbuf(previous);
        std::cin.clear();

        expect(first && second) << "both answers arrive";
        expect(first_line.get_token().get_number().value_or(-1) == 24_i);
        expect(second_line.get_token().get_number().value_or(-1) == 32_i);
    };
};
