// Implementación REAL de ImuDDSPublisher. Compilada solo cuando esta build
// tiene FastDDS (ver ../CMakeLists.txt).

#include "imu_channel.h"
#include "media_transport.h"

struct ImuDDSPublisher::Impl
{
    DDSChannel::Config      cfg;
    rc::media::ImuPublisher pub;
    bool                    ready    = false;
    std::uint64_t           frame_id = 0;
};

ImuDDSPublisher::ImuDDSPublisher(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg = std::move(cfg);
}

ImuDDSPublisher::~ImuDDSPublisher() = default;

bool ImuDDSPublisher::init()
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

bool ImuDDSPublisher::ready() const { return pimpl_->ready; }

std::string_view ImuDDSPublisher::topic() const { return pimpl_->cfg.topic; }

bool ImuDDSPublisher::publish(const DDSChannel::Frame& base)
{
    if (not pimpl_->ready) return false;
    const auto* f = dynamic_cast<const Frame*>(&base);
    if (f == nullptr) return false;

    rc::media::ImuFrame* loaned = pimpl_->pub.loan();
    if (loaned == nullptr) return false;

    loaned->frame_id(++pimpl_->frame_id);
    loaned->stamp_ms(f->stamp_ms);
    loaned->sim_stamp_ms(0);
    loaned->stream_id(rc::media::STREAM_IMU);
    loaned->acc_x(f->acc_x);   loaned->acc_y(f->acc_y);   loaned->acc_z(f->acc_z);
    loaned->gyro_x(f->gyro_x); loaned->gyro_y(f->gyro_y); loaned->gyro_z(f->gyro_z);
    loaned->mag_x(f->mag_x);   loaned->mag_y(f->mag_y);   loaned->mag_z(f->mag_z);
    loaned->roll(f->roll);     loaned->pitch(f->pitch);   loaned->yaw(f->yaw);
    loaned->temperature(f->temperature);
    loaned->gyro_var(f->gyro_var);

    if (pimpl_->pub.publish(loaned)) return true;
    pimpl_->pub.discard(loaned);
    return false;
}

DDSOptional make_imu_channel(DDSChannel::Config cfg)
{
    auto ch = std::make_unique<ImuDDSPublisher>(std::move(cfg));
    if (not ch->init()) return {};
    return DDSOptional{std::move(ch)};
}
