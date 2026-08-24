// Implementación REAL de LidarDDSSubscriber. Compilada solo cuando esta
// build tiene FastDDS (ver ../CMakeLists.txt).

#include "lidar_subscription.h"
#include "media_transport.h"

#include <cstdio>

struct LidarDDSSubscriber::Impl
{
    DDSSubscription::Config    cfg;
    rc::media::LidarSubscriber sub;
    bool                       ready = false;
};

LidarDDSSubscriber::LidarDDSSubscriber(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg = std::move(cfg);
}

LidarDDSSubscriber::~LidarDDSSubscriber() = default;

bool LidarDDSSubscriber::init()
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

bool LidarDDSSubscriber::ready() const { return pimpl_->ready; }

std::string_view LidarDDSSubscriber::topic() const { return pimpl_->cfg.topic; }

int LidarDDSSubscriber::poll(int timeout_ms)
{
    if (not pimpl_->ready) return 0;
    const std::string& topic = pimpl_->cfg.topic;
    return pimpl_->sub.wait_and_poll(
        [&topic](const rc::media::LidarFrame& f, std::int64_t recv_ns)
        {
            (void)recv_ns;
            const auto& pts = f.points();
            std::printf("[sub][%s] frame_id=%llu count=%u p0=(%.2f,%.2f,%.2f)\n",
                        topic.c_str(), (unsigned long long)f.frame_id(), f.count(),
                        f.count() > 0 ? pts[0] : 0.f,
                        f.count() > 0 ? pts[1] : 0.f,
                        f.count() > 0 ? pts[2] : 0.f);
        },
        timeout_ms);
}

DDSSubscriptionHandle make_lidar_subscription(DDSSubscription::Config cfg)
{
    auto s = std::make_unique<LidarDDSSubscriber>(std::move(cfg));
    if (not s->init()) return {};
    return DDSSubscriptionHandle{std::move(s)};
}
