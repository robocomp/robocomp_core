#pragma once

// LidarDDSPublisher — publica un escaneo lidar (puntos xyz en metros) en su
// propio topic DDS. Declarada aquí de forma INCONDICIONAL (Config/Frame son
// tipos simples, sin dependencia de FastDDS, así que este header se puede
// incluir siempre) pero sus métodos solo tienen CUERPO cuando esta build
// tiene FastDDS: ver lidar_channel.cpp (real, con PIMPL para no filtrar
// fastdds/media_transport.h a quien incluya este .h) vs null_backend/dds_stubs.cpp
// (sin FastDDS: solo define make_lidar_channel(), que devuelve un canal nulo
// genérico sin construir jamás un LidarDDSPublisher real).
//
// Instanciable varias veces con topics distintos -> así se tienen "dos
// lidars" en el mismo vector<DDSOptional> sin duplicar ni una línea de
// código, solo cambiando el Config de cada uno (ver vector_publisher_example.cpp).

#include <cstdint>
#include <memory>
#include "core/dds_channel.h"
#include "core/dds_optional.h"

class LidarDDSPublisher final : public DDSChannel
{
public:
    // Interleaved x,y,z en METROS, `count` puntos válidos (stride 3). `xyz`
    // es una vista no propietaria: el buffer del llamante debe seguir vivo
    // durante la llamada a publish().
    struct Frame : DDSChannel::Frame
    {
        const float*  xyz   = nullptr;
        std::uint32_t count = 0;
    };

    explicit LidarDDSPublisher(Config cfg);
    ~LidarDDSPublisher() override;
    LidarDDSPublisher(const LidarDDSPublisher&) = delete;
    LidarDDSPublisher& operator=(const LidarDDSPublisher&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "lidar"; }
    [[nodiscard]] std::string_view topic() const override;
    bool publish(const DDSChannel::Frame& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

// Construye + init() un canal lidar a partir de un Config ya relleno (topic
// incluido) y lo envuelve en un DDSOptional para meter en el vector. Vacío
// si init() falla o si esta build no tiene FastDDS.
[[nodiscard]] DDSOptional make_lidar_channel(DDSChannel::Config cfg);
