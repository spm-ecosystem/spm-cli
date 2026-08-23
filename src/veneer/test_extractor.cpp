#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <optional>
#include "extractor_pipeline.hpp"

void testParseCleanNumber() {
    using namespace veneer;

    // Metric suffix conversions
    assert(parseCleanNumber("1.5k").value_or("") == "1500");
    assert(parseCleanNumber("2.5M").value_or("") == "2500000");
    assert(parseCleanNumber("10B").value_or("") == "10000000000");
    assert(parseCleanNumber("0.5k").value_or("") == "500");
    assert(parseCleanNumber("100k").value_or("") == "100000");

    // Currency and thousands / decimal separators
    assert(parseCleanNumber("  $ 2,500.50 ").value_or("") == "2500.5");
    assert(parseCleanNumber("€ 1.234,56").value_or("") == "1234.56");
    assert(parseCleanNumber("R$ 99,90").value_or("") == "99.9");
    assert(parseCleanNumber("£ 1,000,000").value_or("") == "1000000");
    assert(parseCleanNumber("$1,200").value_or("") == "1200");
    assert(parseCleanNumber("1,200.50").value_or("") == "1200.5");
    assert(parseCleanNumber("1.200,50").value_or("") == "1200.5");
    assert(parseCleanNumber("1200,50").value_or("") == "1200.5");
    assert(parseCleanNumber("1,500").value_or("") == "1500");

    // Negative numbers
    assert(parseCleanNumber(" - R$ 100 ").value_or("") == "-100");
    assert(parseCleanNumber("-$50.25").value_or("") == "-50.25");
    assert(parseCleanNumber("$-10").value_or("") == "-10");
    assert(parseCleanNumber("- 50").value_or("") == "-50");

    // Invalid / empty values
    assert(!parseCleanNumber("").has_value());
    assert(!parseCleanNumber("   ").has_value());
    assert(!parseCleanNumber("abc").has_value());
    assert(!parseCleanNumber("$ -- ").has_value());

    std::cout << "[PASS] testParseCleanNumber passed!\n";
}

void testApplyPipes() {
    using namespace veneer;

    // No pipes -> returns initial value
    assert(applyPipes("hello", {}).value_or("") == "hello");

    // cleanNumber pipe
    std::vector<std::string> pipesClean = {"cleanNumber"};
    assert(applyPipes(" 5.2k ", pipesClean).value_or("") == "5200");
    assert(applyPipes("  $ 2,500.50 ", pipesClean).value_or("") == "2500.5");
    assert(!applyPipes("invalid_num", pipesClean).has_value());

    // number pipe
    std::vector<std::string> pipesNumber = {"number"};
    assert(applyPipes("42", pipesNumber).value_or("") == "42");
    assert(applyPipes(" 3.14 ", pipesNumber).value_or("") == "3.14");
    assert(applyPipes("-100", pipesNumber).value_or("") == "-100");
    assert(!applyPipes("abc", pipesNumber).has_value());
    assert(!applyPipes("", pipesNumber).has_value());

    // split pipe (whitespace delimited)
    std::vector<std::string> pipesSplit = {"split"};
    assert(applyPipes("foo  bar\tbaz", pipesSplit).value_or("") == "[\"foo\",\"bar\",\"baz\"]");
    assert(applyPipes("", pipesSplit).value_or("") == "[]");

    // split:delim pipe
    std::vector<std::string> pipesSplitComma = {"split:,"};
    assert(applyPipes("apple, banana , orange", pipesSplitComma).value_or("") == "[\"apple\",\"banana\",\"orange\"]");

    std::vector<std::string> pipesSplitColon = {"split::"};
    assert(applyPipes("a:b:c", pipesSplitColon).value_or("") == "[\"a\",\"b\",\"c\"]");

    // Unknown pipe -> fails
    std::vector<std::string> pipesUnknown = {"invalidPipe"};
    assert(!applyPipes("hello", pipesUnknown).has_value());

    // Chained pipes
    std::vector<std::string> chained = {"cleanNumber", "number"};
    assert(applyPipes(" $ 1.5k ", chained).value_or("") == "1500");

    std::cout << "[PASS] testApplyPipes passed!\n";
}

int main() {
    std::cout << "=== Extractor Pipeline Test ===\n";
    testParseCleanNumber();
    testApplyPipes();
    std::cout << "ALL EXTRACTOR TESTS PASSED!\n";
    return 0;
}
