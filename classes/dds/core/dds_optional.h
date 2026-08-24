#pragma once
#include <memory>
#include <string>
#include <string_view>
#include "dds_channel.h"

// Contenedor de tipo borrado (type erasure): guarda CUALQUIER DDSChannel
// concreto tras un puntero a la interfaz. "Optional" porque un canal puede
// estar vacío (puntero nulo) sin que eso rompa nada: init/ready/kind/topic/
// publish son no-op seguros sobre un DDSOptional vacío. Así se puede meter
// lidar, cámara, imu... en el mismo std::vector<DDSOptional>.
class DDSOptional
{
public:
    DDSOptional() = default;
    explicit DDSOptional(std::unique_ptr<DDSChannel> channel) : channel_(std::move(channel)) {}
    DDSOptional(DDSOptional&&) = default;
    DDSOptional& operator=(DDSOptional&&) = default;
    DDSOptional(const DDSOptional&) = delete;
    DDSOptional& operator=(const DDSOptional&) = delete;

    bool init() { return channel_ && channel_->init(); }
    [[nodiscard]] bool ready() const { return channel_ && channel_->ready(); }
    [[nodiscard]] std::string_view kind() const { return channel_ ? channel_->kind() : std::string_view{"none"}; }
    [[nodiscard]] std::string_view topic() const { return channel_ ? channel_->topic() : std::string_view{}; }
    bool publish(const DDSChannel::Frame& frame) { return channel_ && channel_->publish(frame); }

    // Acceso al tipo concreto cuando de verdad hace falta (poco frecuente:
    // en los ejemplos aquí basta con kind() + el Frame anidado del canal, ver
    // vector_publisher_example.cpp). OJO: as<T>() obliga al enlazador a
    // necesitar la vtable/RTTI de T -> si T solo existe en la build "con
    // FastDDS" (ver lidar/lidar_channel.h), usar as<T>() en código que
    // también debe enlazar sin FastDDS rompe el build stub. Por eso los
    // ejemplos de este directorio despachan por kind(), nunca con as<T>().
    template <typename T>
    [[nodiscard]] T* as() { return dynamic_cast<T*>(channel_.get()); }
    template <typename T>
    [[nodiscard]] const T* as() const { return dynamic_cast<const T*>(channel_.get()); }

    explicit operator bool() const { return channel_ != nullptr; }

private:
    std::unique_ptr<DDSChannel> channel_;
};
