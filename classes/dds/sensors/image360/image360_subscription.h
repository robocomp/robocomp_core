#pragma once

// Image360DDSSubscriber — se suscribe a un topic de panorama 360 y, por cada
// frame recibido, lo vuelca a un fichero PPM (nombre derivado del topic).
// Misma separación que camera/camera_subscription.h.

#include <memory>
#include "core/dds_subscription.h"
#include "core/dds_subscription_handle.h"

class Image360DDSSubscriber final : public DDSSubscription
{
public:
    explicit Image360DDSSubscriber(Config cfg);
    ~Image360DDSSubscriber() override;
    Image360DDSSubscriber(const Image360DDSSubscriber&) = delete;
    Image360DDSSubscriber& operator=(const Image360DDSSubscriber&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "image360"; }
    [[nodiscard]] std::string_view topic() const override;
    int poll(int timeout_ms) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSSubscriptionHandle make_image360_subscription(DDSSubscription::Config cfg);
