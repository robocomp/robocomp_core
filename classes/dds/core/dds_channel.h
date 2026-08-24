#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// Interfaz común de un canal DDS de PUBLICACIÓN (lidar, cámara, ...). Permite
// guardar canales de distinto tipo en un mismo std::vector<DDSOptional>
// (dds_optional.h). Config es el mismo "molde" para todos (dominio/topic/QoS);
// solo los valores por defecto cambian por sensor. Frame es polimórfico y
// cada canal define su propio subtipo con los campos que necesite (ver
// lidar/lidar_channel.h, camera/camera_channel.h).
class DDSChannel
{
public:
    struct Config
    {
        std::uint32_t domain_id = 7;
        std::string   topic;              // obligatorio: cada canal necesita el suyo
        int           history_depth      = 8;
        bool          shared_memory_only = true;
        bool          data_sharing       = false;
    };

    struct Frame
    {
        std::uint64_t stamp_ms = 0;
        virtual ~Frame() = default;
    };

    virtual ~DDSChannel() = default;
    virtual bool init() = 0;
    [[nodiscard]] virtual bool ready() const = 0;
    [[nodiscard]] virtual std::string_view kind() const = 0;   // "lidar", "camera", ...
    [[nodiscard]] virtual std::string_view topic() const = 0;
    virtual bool publish(const Frame& frame) = 0;
};
