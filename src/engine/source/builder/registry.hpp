#ifndef _REGISTRY_H
#define _REGISTRY_H

#include <glog/logging.h>
#include <map>
#include <rxcpp/rx.hpp>
#include <string>
#include <variant>
#include <functional>

#include "connectable.hpp"
#include "json.hpp"

namespace builder::internals
{

class Registry
{
public:
    using Event_t = json::Document;
    // The type of the observable which will compose the processing graph
    using Obs_t = rxcpp::observable<Event_t>;
    // The type of the connectables whisch will help us connect the assets ina graph
    using Con_t = builder::internals::Connectable<Obs_t>;
    // The type of a connectable operation
    using Op_t = std::function<Obs_t(const Obs_t &)>;

    using BuildDocument = std::function<Op_t(const json::Document &)>;
    using BuildValue = std::function<Op_t(const json::Value &)>;
    using BuildType = std::variant<BuildValue, BuildDocument>;

private:
    std::map<std::string, BuildType> m_registry;

public:
    /**
     * @brief
     *
     * @param builderName
     * @param builder
     */
    void registerBuilder(const std::string & builderName, const BuildType & builder);

    /**
     * @brief Get the Builder object
     *
     * @param builderName
     * @return builder_t
     */
    BuildType getBuilder(const std::string & builderName);
};

} // namespace builder::internals

#endif // _REGISTRY_H
