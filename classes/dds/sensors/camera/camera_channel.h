#pragma once

// CameraDDSPublisher — publica una imagen RGB en su propio topic DDS. Misma
// separación declaración/implementación que lidar/lidar_channel.h: ver
// camera_channel.cpp (real, PIMPL) / null_backend/dds_stubs.cpp (sin FastDDS).

#include <cstdint>
#include <memory>
#include "core/dds_channel.h"
#include "core/dds_optional.h"

class CameraDDSPublisher final : public DDSChannel
{
public:
    // RGB8 empaquetado fila a fila, sin padding. `rgb` es una vista no
    // propietaria: el buffer del llamante debe seguir vivo durante publish().
    struct Frame : DDSChannel::Frame
    {
        const std::uint8_t* rgb    = nullptr;
        std::uint32_t        width  = 0;
        std::uint32_t        height = 0;
    };

    explicit CameraDDSPublisher(Config cfg);
    ~CameraDDSPublisher() override;
    CameraDDSPublisher(const CameraDDSPublisher&) = delete;
    CameraDDSPublisher& operator=(const CameraDDSPublisher&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "camera"; }
    [[nodiscard]] std::string_view topic() const override;
    bool publish(const DDSChannel::Frame& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSOptional make_camera_channel(DDSChannel::Config cfg);
