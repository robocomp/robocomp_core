#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include "core/dds_subscription.h"

// Implementación nula, genérica, de DDSSubscription: nunca conecta a DDS,
// poll() siempre devuelve 0 frames. Reutilizada por cualquier sensor cuando
// esta build no tiene FastDDS instalado — mismo papel que NullDDSChannel en
// el lado de publicación (dds_null_channel.h).
class NullDDSSubscription final : public DDSSubscription
{
public:
    NullDDSSubscription(std::string kind, std::string topic)
        : kind_(std::move(kind)), topic_(std::move(topic)) {}

    bool init() override { return false; }
    [[nodiscard]] bool ready() const override { return false; }
    [[nodiscard]] std::string_view kind() const override { return kind_; }
    [[nodiscard]] std::string_view topic() const override { return topic_; }
    int poll(int) override { return 0; }

private:
    std::string kind_;
    std::string topic_;
};

[[nodiscard]] inline std::unique_ptr<DDSSubscription> make_null_subscription(std::string kind, std::string topic)
{
    return std::make_unique<NullDDSSubscription>(std::move(kind), std::move(topic));
}
