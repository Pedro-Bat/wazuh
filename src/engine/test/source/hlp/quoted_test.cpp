#include <gtest/gtest.h>

#include "hlp_test.hpp"

auto constexpr NAME = "quotedParser";
static const std::string TARGET = "/TargetField";

INSTANTIATE_TEST_SUITE_P(QuotedBuild,
                         HlpBuildTest,
                         ::testing::Values(BuildT(SUCCESS, getQuotedParser, {NAME, TARGET, {}, {}}),
                                           BuildT(SUCCESS, getQuotedParser, {NAME, TARGET, {}, {"q"}}),
                                           BuildT(SUCCESS, getQuotedParser, {NAME, TARGET, {}, {"q", "e"}}),
                                           BuildT(FAILURE, getQuotedParser, {NAME, TARGET, {}, {"quoted"}}),
                                           BuildT(FAILURE, getQuotedParser, {NAME, TARGET, {}, {"q", "escape"}}),
                                           BuildT(FAILURE, getQuotedParser, {NAME, TARGET, {}, {"q", "e", "?"}})));

INSTANTIATE_TEST_SUITE_P(QuotedParse,
                         HlpParseTest,
                         ::testing::Values(
                             // Default parameters
                             ParseT(FAILURE, "wazuh", {}, 0, getQuotedParser, {NAME, TARGET, {}, {}}),
                             ParseT(FAILURE, R"("wazuh 123)", {}, 0, getQuotedParser, {NAME, TARGET, {}, {}}),
                             ParseT(SUCCESS,
                                    R"("Wazuh" 123)",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {}}),
                             ParseT(SUCCESS,
                                    R"("Wazuh")",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {}}),
                             ParseT(SUCCESS,
                                    R"("hi my name is \"Wazuh\"")",
                                    j(fmt::format(R"({{"{}":"hi my name is \"Wazuh\""}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {}}),
                             ParseT(SUCCESS,
                                    R"("hi my name is \"Wazuh\"" 123456)",
                                    j(fmt::format(R"({{"{}":"hi my name is \"Wazuh\""}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {}}),
                             // Change " to '
                             ParseT(FAILURE, "wazuh'", {}, 0, getQuotedParser, {NAME, TARGET, {}, {"'"}}),
                             ParseT(FAILURE, R"('wazuh 123)", {}, 0, getQuotedParser, {NAME, TARGET, {}, {"'"}}),
                             ParseT(SUCCESS,
                                    R"('Wazuh' 123)",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'"}}),
                             ParseT(SUCCESS,
                                    R"('Wazuh')",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'"}}),
                             ParseT(SUCCESS,
                                    R"('hi my name is \'Wazuh\'')",
                                    j(fmt::format(R"({{"{}":"hi my name is 'Wazuh'"}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'"}}),
                             ParseT(SUCCESS,
                                    R"('hi my name is \'Wazuh\'' 123456)",
                                    j(fmt::format(R"({{"{}":"hi my name is 'Wazuh'"}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'"}}),
                             // Change " to ' and \ to :
                             ParseT(FAILURE, "wazuh'", {}, 0, getQuotedParser, {NAME, TARGET, {}, {"'", ":"}}),
                             ParseT(FAILURE, R"('wazuh 123)", {}, 0, getQuotedParser, {NAME, TARGET, {}, {"'", ":"}}),
                             ParseT(SUCCESS,
                                    R"('Wazuh' 123)",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'", ":"}}),
                             ParseT(SUCCESS,
                                    R"('Wazuh')",
                                    j(fmt::format(R"({{"{}":"Wazuh"}})", TARGET.substr(1))),
                                    7,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'", ":"}}),
                             ParseT(SUCCESS,
                                    R"('hi my name is :'Wazuh:'')",
                                    j(fmt::format(R"({{"{}":"hi my name is 'Wazuh'"}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'", ":"}}),
                             ParseT(SUCCESS,
                                    R"('hi my name is :'Wazuh:'' 123456)",
                                    j(fmt::format(R"({{"{}":"hi my name is 'Wazuh'"}})", TARGET.substr(1))),
                                    25,
                                    getQuotedParser,
                                    {NAME, TARGET, {}, {"'", ":"}})));

//         // TODO: We want to support this case ?
//         // Mantain " but change escape character to " (Like CSV files)
//         // TestCase {R"(wazuh")", false, {""}, Options {"\"", "\""}, fn(R"({})"), 0},
//         // TestCase {R"("wazuh 123)", false, {""}, Options {"\"", "\""}, fn(R"({})"), 10},
//         // TestCase {
//         //     R"("Wazuh" 123)", true, {""}, Options {"\"", "\""}, fn(R"("Wazuh")"), 7},
//         // TestCase {R"("Wazuh")", true, {""}, Options {"\"", "\""}, fn(R"("Wazuh")"), 7},
//         // TestCase {R"("hi my name is ""Wazuh""")",
//         //           true,
//         //           {""},
//         //           Options {"\"", "\""},
//         //           fn(R"("hi my name is \"Wazuh\"")"),
//         //           25},
//         // TestCase {R"("hi my name is ""Wazuh"" 123456)",
//         //           true,
//         //           {""},
//         //           Options {"\"", "\""},
//         //           fn(R"("hi my name is \"Wazuh\"")"),
//         //           25}