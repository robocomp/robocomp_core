#pragma once

// Image360DDSPublisher — publica el panorama 360 (RGB) en su propio topic
// DDS. Tipo separado de CameraDDSPublisher (no solo topic distinto): usa
// Image360Frame, con un buffer inline mucho mayor (~5.5 MB vs ~2.7 MB), para
// no engordar el tipo ImageFrame de la cámara ZED con un caso que casi nadie
// consume. Misma separación declaración/implementación que
// camera/camera_channel.h: ver image360_channel.cpp (real, PIMPL) /
// null_backend/dds_stubs.cpp (sin FastDDS).

#include <cstdint>
#include <memory>
#include "core/dds_channel.h"
#include "core/dds_optional.h"

class Image360DDSPublisher final : public DDSChannel
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

    explicit Image360DDSPublisher(Config cfg);
    ~Image360DDSPublisher() override;
    Image360DDSPublisher(const Image360DDSPublisher&) = delete;
    Image360DDSPublisher& operator=(const Image360DDSPublisher&) = delete;

    bool init() override;
    [[nodiscard]] bool ready() const override;
    [[nodiscard]] std::string_view kind() const override { return "image360"; }
    [[nodiscard]] std::string_view topic() const override;
    bool publish(const DDSChannel::Frame& frame) override;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

[[nodiscard]] DDSOptional make_image360_channel(DDSChannel::Config cfg);
