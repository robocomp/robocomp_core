#pragma once

// CameraDDSSubscriber — se suscribe a un topic de imagen y, por cada frame
// recibido, lo vuelca a un fichero PPM (nombre derivado del topic, para que
// varias cámaras no se pisen) además de imprimir sus datos. Misma
// separación que lidar/lidar_subscription.h.

#include <memory>
#include "core/dds_subscription.h"
#include "core/dds_subscription_handle.h"

class CameraDDSSubscriber final : public DDSSubscription
{
public:
    explicit CameraDDSSubscriber(Config cfg);
    ~CameraDDSSubscriber() override;
    CameraDDSSubscriber(const CameraDDSSubscriber&) = delete;
    CameraDDSSubscriber& operator=(const CameraDDSSubscriber&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "camera"; }
    [[nodiscard]] std::string_view topic() const override;
    int poll(int timeout_ms) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSSubscriptionHandle make_camera_subscription(DDSSubscription::Config cfg);
