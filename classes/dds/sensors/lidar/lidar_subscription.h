#pragma once

// LidarDDSSubscriber — se suscribe a un topic lidar y, por cada frame
// recibido, lo imprime (ver lidar_subscription.cpp). Misma separación
// declaración/implementación que lidar_channel.h: este header no depende de
// FastDDS, así que se puede incluir siempre; el PIMPL vive en
// lidar_subscription.cpp (real) o no se construye nunca (null_backend/dds_stubs.cpp).
//
// Instanciable varias veces con topics distintos -> "dos lidars" en el
// vector<DDSSubscriptionHandle> del lado de suscripción, igual que en el de
// publicación.

#include <memory>
#include "core/dds_subscription.h"
#include "core/dds_subscription_handle.h"

class LidarDDSSubscriber final : public DDSSubscription
{
public:
    explicit LidarDDSSubscriber(Config cfg);
    ~LidarDDSSubscriber() override;
    LidarDDSSubscriber(const LidarDDSSubscriber&) = delete;
    LidarDDSSubscriber& operator=(const LidarDDSSubscriber&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "lidar"; }
    [[nodiscard]] std::string_view topic() const override;
    int poll(int timeout_ms) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSSubscriptionHandle make_lidar_subscription(DDSSubscription::Config cfg);
