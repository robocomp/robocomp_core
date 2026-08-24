#pragma once
#include <memory>
#include <string_view>
#include "dds_subscription.h"

// Equivalente a DDSOptional pero para el lado de suscripción: tipo borrado,
// std::vector<DDSSubscriptionHandle> puede mezclar lidar/cámara/..., y un
// handle vacío es un no-op seguro. No expone as<T>() a propósito: el bucle
// de suscripción (ver vector_subscriber_example.cpp) nunca necesita el tipo
// concreto, solo llamar poll() — así se mantiene enlazable también en la
// build "sin FastDDS" (ver dds_null_subscription.h).
class DDSSubscriptionHandle
{
public:
    DDSSubscriptionHandle() = default;
    explicit DDSSubscriptionHandle(std::unique_ptr<DDSSubscription> sub) : sub_(std::move(sub)) {}
    DDSSubscriptionHandle(DDSSubscriptionHandle&&) = default;
    DDSSubscriptionHandle& operator=(DDSSubscriptionHandle&&) = default;
    DDSSubscriptionHandle(const DDSSubscriptionHandle&) = delete;
    DDSSubscriptionHandle& operator=(const DDSSubscriptionHandle&) = delete;

    bool init() { return sub_ && sub_->init(); }
    [[nodiscard]] bool ready() const { return sub_ && sub_->ready(); }
    [[nodiscard]] std::string_view kind() const { return sub_ ? sub_->kind() : std::string_view{"none"}; }
    [[nodiscard]] std::string_view topic() const { return sub_ ? sub_->topic() : std::string_view{}; }
    int poll(int timeout_ms) { return sub_ ? sub_->poll(timeout_ms) : 0; }

    explicit operator bool() const { return sub_ != nullptr; }

private:
    std::unique_ptr<DDSSubscription> sub_;
};
