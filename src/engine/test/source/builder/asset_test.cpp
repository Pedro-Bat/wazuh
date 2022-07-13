#include <gtest/gtest.h>

#include <filesystem>
#include <stdexcept>

#include "builder/asset.hpp"
#include "builder/builder.hpp"
#include "builder/register.hpp"
#include "builder/registry.hpp"
#include "json.hpp"

using namespace builder;
using namespace builder::internals;
using namespace base;

constexpr auto outputPath = "/tmp/file";

class AssetTest : public ::testing::Test
{
    void SetUp() override
    {
        if (std::filesystem::exists(outputPath))
        {
            std::filesystem::remove(outputPath);
        }
        registerBuilders();
    }

    void TearDown() override
    {
        if (std::filesystem::exists(outputPath))
        {
            std::filesystem::remove(outputPath);
        }
        Registry::clear();
    }
};

TEST_F(AssetTest, typeToString)
{
    ASSERT_EQ(Asset::typeToString(Asset::Type::DECODER), "decoder");
    ASSERT_EQ(Asset::typeToString(Asset::Type::RULE), "rule");
    ASSERT_EQ(Asset::typeToString(Asset::Type::OUTPUT), "output");
    ASSERT_EQ(Asset::typeToString(Asset::Type::FILTER), "filter");
}

TEST_F(AssetTest, ConstructorSimple)
{
    ASSERT_NO_THROW(Asset("name", Asset::Type::DECODER));
    ASSERT_NO_THROW(Asset("name", Asset::Type::RULE));
    ASSERT_NO_THROW(Asset("name", Asset::Type::OUTPUT));
    ASSERT_NO_THROW(Asset("name", Asset::Type::FILTER));
}

TEST_F(AssetTest, JsonNotObject)
{
    ASSERT_THROW(Asset(json::Json {"[]"}, Asset::Type::DECODER), std::runtime_error);
}

TEST_F(AssetTest, JsonNoName)
{
    auto asset = R"({
        "check": [
            {"decoder": 1}
        ],
        "normalize": [
            {
                "map": {
                    "decoded.names": "+s_append/decoder1"
                }
            }
        ]
    })";
    ASSERT_THROW(Asset(json::Json {asset}, Asset::Type::DECODER), std::runtime_error);
    asset = R"({
        "name": 12,
        "check": [
            {"decoder": 1}
        ],
        "normalize": [
            {
                "map": {
                    "decoded.names": "+s_append/decoder1"
                }
            }
        ]
    })";
    ASSERT_THROW(Asset(json::Json {asset}, Asset::Type::DECODER), std::runtime_error);
}

TEST_F(AssetTest, BuildDecoder)
{
    auto assetJson = R"({
        "name": "decoder1",
        "check": [
            {"decoder": 1}
        ],
        "parse": {
            "logql": [
                {"field": "<other.field> "}
            ]
        },
        "normalize": [
            {
                "map": {
                    "decoded.names": "+s_append/decoder1"
                }
            }
        ]
    })";
    ASSERT_NO_THROW(Asset(json::Json {assetJson}, Asset::Type::DECODER));
    auto asset = Asset(json::Json {assetJson}, Asset::Type::DECODER);
    ASSERT_EQ(asset.m_name, "decoder1");
    ASSERT_EQ(asset.m_type, Asset::Type::DECODER);

    ASSERT_NO_THROW(asset.getExpression());
    auto expression = asset.getExpression();
    ASSERT_TRUE(expression->isImplication());
    auto asOp = expression->getPtr<Operation>();
    ASSERT_EQ(asOp->getOperands().size(), 2);
    ASSERT_EQ(asOp->getOperands()[0], asset.m_check);
    ASSERT_EQ(asOp->getOperands()[1], asset.m_stages);
}

TEST_F(AssetTest, BuildRule)
{
    auto assetJson = R"({
        "name": "rule1",
        "parents": ["ruleParent"],
        "check": [
            {"rule": 1}
        ],
        "normalize": [
            {
                "map": {
                    "alerted.name": "rule1"
                }
            }
        ]
    })";
    ASSERT_NO_THROW(Asset(json::Json {assetJson}, Asset::Type::RULE));
    auto asset = Asset(json::Json {assetJson}, Asset::Type::RULE);
    ASSERT_EQ(asset.m_name, "rule1");
    ASSERT_EQ(asset.m_type, Asset::Type::RULE);
    ASSERT_EQ(asset.m_parents.size(), 1);
    ASSERT_EQ(asset.m_parents.count("ruleParent"), 1);

    ASSERT_NO_THROW(asset.getExpression());
    auto expression = asset.getExpression();
    ASSERT_TRUE(expression->isImplication());
    auto asOp = expression->getPtr<Operation>();
    ASSERT_EQ(asOp->getOperands().size(), 2);
    ASSERT_EQ(asOp->getOperands()[0], asset.m_check);
    ASSERT_EQ(asOp->getOperands()[1], asset.m_stages);
}

TEST_F(AssetTest, BuildOutput)
{
    auto assetJson = R"({
       "name": "output1",
        "check": [
            {"output": 1}
        ],
        "outputs": [
            {
                "file": {
                    "path": "/tmp/file"
                }
            }
        ]
    })";
    ASSERT_NO_THROW(Asset(json::Json {assetJson}, Asset::Type::OUTPUT));
    auto asset = Asset(json::Json {assetJson}, Asset::Type::OUTPUT);
    ASSERT_EQ(asset.m_name, "output1");
    ASSERT_EQ(asset.m_type, Asset::Type::OUTPUT);

    ASSERT_NO_THROW(asset.getExpression());
    auto expression = asset.getExpression();
    ASSERT_TRUE(expression->isImplication());
    auto asOp = expression->getPtr<Operation>();
    ASSERT_EQ(asOp->getOperands().size(), 2);
    ASSERT_EQ(asOp->getOperands()[0], asset.m_check);
    ASSERT_EQ(asOp->getOperands()[1], asset.m_stages);
}

TEST_F(AssetTest, BuildFilter)
{
    auto assetJson = R"({
        "name": "filter1",
        "after": [
            "decoder1"
        ],
        "check": [
            {"filter": 1}
        ]
    })";
    ASSERT_NO_THROW(Asset(json::Json {assetJson}, Asset::Type::FILTER));
    auto asset = Asset(json::Json {assetJson}, Asset::Type::FILTER);
    ASSERT_EQ(asset.m_name, "filter1");
    ASSERT_EQ(asset.m_type, Asset::Type::FILTER);
    ASSERT_EQ(asset.m_parents.size(), 1);
    ASSERT_EQ(asset.m_parents.count("decoder1"), 1);

    ASSERT_NO_THROW(asset.getExpression());
    auto expression = asset.getExpression();
    ASSERT_TRUE(expression->isAnd());
    auto asOp = expression->getPtr<Operation>();
    ASSERT_EQ(asOp->getOperands().size(), 1);
    ASSERT_EQ(asOp->getOperands()[0], asset.m_check);
}