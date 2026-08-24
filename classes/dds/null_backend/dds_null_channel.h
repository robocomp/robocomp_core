#pragma once
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include "core/dds_channel.h"

// Implementación nula, GENÉRICA, de DDSChannel: no toca FastDDS para nada,
// init() nunca falla estrepitosamente, ready() siempre false, publish()
// siempre lo descarta. Un solo tipo reutilizado por cualquier sensor (lidar,
// cámara...) cuando esta build no tiene FastDDS instalado, en vez de un stub
// distinto por cada uno — ver dds_stubs.cpp.
class NullDDSChannel final : public DDSChannel
{
public:
    NullDDSChannel(std::string kind, std::string topic)
        : kind_(std::move(kind)), topic_(std::move(topic)) {}

    bool init() override { return false; }
    [[nodiscard]] bool ready() const override { return false; }
    [[nodiscard]] std::string_view kind() const override { return kind_; }
    [[nodiscard]] std::string_view topic() const override { return topic_; }
    bool publish(const Frame&) override { return false; }

private:
    std::string kind_;
    std::string topic_;
};

[[nodiscard]] inline std::unique_ptr<DDSChannel> make_null_channel(std::string kind, std::string topic)
{
    return std::make_unique<NullDDSChannel>(std::move(kind), std::move(topic));
}
