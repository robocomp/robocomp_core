// Implementación REAL de LidarDDSPublisher. Compilada solo cuando esta build
// tiene FastDDS (ver ../CMakeLists.txt) — es el único fichero de todo el
// ejemplo que incluye media_transport.h.

#include "lidar_channel.h"
#include "media_transport.h"

#include <cstring>

struct LidarDDSPublisher::Impl
{
    DDSChannel::Config       cfg;
    rc::media::LidarPublisher pub;
    bool                      ready    = false;
    std::uint64_t             frame_id = 0;
};

LidarDDSPublisher::LidarDDSPublisher(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg = std::move(cfg);
}

LidarDDSPublisher::~LidarDDSPublisher() = default;

bool LidarDDSPublisher::init()
{
    rc::media::PublisherConfig pc;
    pc.domain_id          = pimpl_->cfg.domain_id;
    pc.topic_name         = pimpl_->cfg.topic;
    pc.history_depth      = pimpl_->cfg.history_depth;
    pc.shared_memory_only = pimpl_->cfg.shared_memory_only;
    pc.data_sharing       = pimpl_->cfg.data_sharing;
    pimpl_->ready = pimpl_->pub.init(pc);
    return pimpl_->ready;
}

bool LidarDDSPublisher::ready() const { return pimpl_->ready; }

std::string_view LidarDDSPublisher::topic() const { return pimpl_->cfg.topic; }

bool LidarDDSPublisher::publish(const DDSChannel::Frame& base)
{
    if (not pimpl_->ready) return false;
    const auto* f = dynamic_cast<const Frame*>(&base);
    if (f == nullptr || f->xyz == nullptr || f->count == 0 || f->count > rc::media::MAX_LIDAR_POINTS)
        return false;

    rc::media::LidarFrame* loaned = pimpl_->pub.loan();
    if (loaned == nullptr) return false;

    loaned->frame_id(++pimpl_->frame_id);
    loaned->stamp_ms(f->stamp_ms);
    loaned->stream_id(rc::media::STREAM_LIDAR);
    loaned->count(f->count);
    loaned->format(rc::media::LIDAR_FORMAT_XYZ_F32);
    loaned->stride(3);
    auto& buf = loaned->points();
    std::memcpy(buf.data(), f->xyz, static_cast<std::size_t>(f->count) * 3 * sizeof(float));

    if (pimpl_->pub.publish(loaned)) return true;
    pimpl_->pub.discard(loaned);
    return false;
}

DDSOptional make_lidar_channel(DDSChannel::Config cfg)
{
    auto ch = std::make_unique<LidarDDSPublisher>(std::move(cfg));
    if (not ch->init()) return {};
    return DDSOptional{std::move(ch)};
}
