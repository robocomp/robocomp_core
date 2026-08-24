#pragma once

// ImuDDSPublisher — publica una muestra IMU (aceleración, giro, campo
// magnético, orientación) en su propio topic DDS. Declarada aquí de forma
// INCONDICIONAL (Config/Frame son tipos simples, sin dependencia de FastDDS)
// pero sus métodos solo tienen CUERPO cuando esta build tiene FastDDS: ver
// imu_channel.cpp (real, PIMPL) vs null_backend/dds_stubs.cpp (sin FastDDS).

#include <cstdint>
#include <memory>
#include "core/dds_channel.h"
#include "core/dds_optional.h"

class ImuDDSPublisher final : public DDSChannel
{
public:
    // Muestra IMU completa: todo son escalares copiados por valor, sin
    // buffers no-propietarios (a diferencia de LidarDDSPublisher::Frame /
    // CameraDDSPublisher::Frame).
    struct Frame : DDSChannel::Frame
    {
        float acc_x = 0, acc_y = 0, acc_z = 0;
        float gyro_x = 0, gyro_y = 0, gyro_z = 0;
        float mag_x = 0, mag_y = 0, mag_z = 0;
        float roll = 0, pitch = 0, yaw = 0;
        float temperature = 0;
        float gyro_var = -1.f;   // negativo = varianza desconocida (ver imu_frame.idl)
    };

    explicit ImuDDSPublisher(Config cfg);
    ~ImuDDSPublisher() override;
    ImuDDSPublisher(const ImuDDSPublisher&) = delete;
    ImuDDSPublisher& operator=(const ImuDDSPublisher&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "imu"; }
    [[nodiscard]] std::string_view topic() const override;
    bool publish(const DDSChannel::Frame& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSOptional make_imu_channel(DDSChannel::Config cfg);
