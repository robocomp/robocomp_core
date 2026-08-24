#pragma once
#include <string_view>
#include "dds_channel.h"

// Interfaz común de un canal DDS de SUSCRIPCIÓN. Reutiliza DDSChannel::Config
// (mismo dominio/topic/QoS que un publicador — deben coincidir para
// engancharse al mismo topic). A diferencia de DDSChannel::publish(Frame&),
// aquí no hay un "Frame de entrada": los datos llegan DESDE DDS, así que cada
// canal concreto los gestiona internamente dentro de poll() (imprimir,
// volcar a fichero...). Eso hace innecesario un Frame polimórfico en este
// lado: poll() ya es uniforme para cualquier sensor.
class DDSSubscription
{
public:
    using Config = DDSChannel::Config;

    virtual ~DDSSubscription() = default;
    virtual bool init() = 0;
    [[nodiscard]] virtual bool ready() const = 0;
    [[nodiscard]] virtual std::string_view kind() const = 0;
    [[nodiscard]] virtual std::string_view topic() const = 0;

    // Drena las muestras disponibles hasta timeout_ms (o hasta que llegue al
    // menos una). Devuelve cuántos frames se recibieron.
    virtual int poll(int timeout_ms) = 0;
};
