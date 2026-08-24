// Implementación REAL de CameraDDSPublisher. Compilada solo cuando esta
// build tiene FastDDS.

#include "camera_channel.h"
#include "media_transport.h"

#include <cstring>

struct CameraDDSPublisher::Impl
{
    DDSChannel::Config       cfg;
    rc::media::MediaPublisher pub;
    bool                      ready    = false;
    std::uint64_t             frame_id = 0;
};

CameraDDSPublisher::CameraDDSPublisher(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg = std::move(cfg);
}

CameraDDSPublisher::~CameraDDSPublisher() = default;

bool CameraDDSPublisher::init()
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

bool CameraDDSPublisher::ready() const { return pimpl_->ready; }

std::string_view CameraDDSPublisher::topic() const { return pimpl_->cfg.topic; }

bool CameraDDSPublisher::publish(const DDSChannel::Frame& base)
{
    if (not pimpl_->ready) return false;
    const auto* f = dynamic_cast<const Frame*>(&base);
    if (f == nullptr || f->rgb == nullptr) return false;

    const std::uint32_t bytes = f->width * f->height * 3;
    if (bytes == 0 || bytes > rc::media::MAX_IMAGE_BYTES) return false;

    rc::media::ImageFrame* loaned = pimpl_->pub.loan();
    if (loaned == nullptr) return false;

    loaned->frame_id(++pimpl_->frame_id);
    loaned->stamp_ms(f->stamp_ms);
    loaned->stream_id(rc::media::STREAM_ZED_RGB);
    loaned->width(f->width);
    loaned->height(f->height);
    loaned->step(f->width * 3);
    loaned->format(rc::media::FORMAT_RGB8);
    loaned->size(bytes);
    auto& buf = loaned->data();
    std::memcpy(buf.data(), f->rgb, bytes);

    if (pimpl_->pub.publish(loaned)) return true;
    pimpl_->pub.discard(loaned);
    return false;
}

DDSOptional make_camera_channel(DDSChannel::Config cfg)
{
    auto ch = std::make_unique<CameraDDSPublisher>(std::move(cfg));
    if (not ch->init()) return {};
    return DDSOptional{std::move(ch)};
}
