// Implementación REAL de ImuDDSSubscriber. Compilada solo cuando esta build
// tiene FastDDS (ver ../CMakeLists.txt).

#include "imu_subscription.h"
#include "media_transport.h"

#include <cstdio>

struct ImuDDSSubscriber::Impl
{
    DDSSubscription::Config  cfg;
    rc::media::ImuSubscriber sub;
    bool                     ready = false;
};

ImuDDSSubscriber::ImuDDSSubscriber(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg = std::move(cfg);
}

ImuDDSSubscriber::~ImuDDSSubscriber() = default;

bool ImuDDSSubscriber::init()
{
    rc::media::SubscriberConfig sc;
    sc.domain_id          = pimpl_->cfg.domain_id;
    sc.topic_name         = pimpl_->cfg.topic;
    sc.history_depth      = pimpl_->cfg.history_depth;
    sc.shared_memory_only = pimpl_->cfg.shared_memory_only;
    sc.data_sharing       = pimpl_->cfg.data_sharing;
    pimpl_->ready = pimpl_->sub.init(sc);
    return pimpl_->ready;
}

bool ImuDDSSubscriber::ready() const { return pimpl_->ready; }

std::string_view ImuDDSSubscriber::topic() const { return pimpl_->cfg.topic; }

int ImuDDSSubscriber::poll(int timeout_ms)
{
    if (not pimpl_->ready) return 0;
    const std::string& topic = pimpl_->cfg.topic;
    return pimpl_->sub.wait_and_poll(
        [&topic](const rc::media::ImuFrame& f, std::int64_t recv_ns)
        {
            (void)recv_ns;
            std::printf("[sub][%s] frame_id=%llu acc=(%.2f,%.2f,%.2f) gyro=(%.2f,%.2f,%.2f)\n",
                        topic.c_str(), (unsigned long long)f.frame_id(),
                        f.acc_x(), f.acc_y(), f.acc_z(),
                        f.gyro_x(), f.gyro_y(), f.gyro_z());
        },
        timeout_ms);
}

DDSSubscriptionHandle make_imu_subscription(DDSSubscription::Config cfg)
{
    auto s = std::make_unique<ImuDDSSubscriber>(std::move(cfg));
    if (not s->init()) return {};
    return DDSSubscriptionHandle{std::move(s)};
}
