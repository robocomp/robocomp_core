#pragma once

// ImuDDSSubscriber — se suscribe a un topic IMU y, por cada muestra
// recibida, la imprime (ver imu_subscription.cpp). Misma separación
// declaración/implementación que lidar/lidar_subscription.h.

#include <memory>
#include "core/dds_subscription.h"
#include "core/dds_subscription_handle.h"

class ImuDDSSubscriber final : public DDSSubscription
{
public:
    explicit ImuDDSSubscriber(Config cfg);
    ~ImuDDSSubscriber() override;
    ImuDDSSubscriber(const ImuDDSSubscriber&) = delete;
    ImuDDSSubscriber& operator=(const ImuDDSSubscriber&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "imu"; }
    [[nodiscard]] std::string_view topic() const override;
    int poll(int timeout_ms) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSSubscriptionHandle make_imu_subscription(DDSSubscription::Config cfg);
