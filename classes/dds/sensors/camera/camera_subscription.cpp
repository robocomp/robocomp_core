// Implementación REAL de CameraDDSSubscriber. Compilada solo cuando esta
// build tiene FastDDS.

#include "camera_subscription.h"
#include "media_transport.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace
{
std::string sanitize_filename(std::string s)
{
    std::replace(s.begin(), s.end(), '/', '_');
    return "received_" + s + ".ppm";
}

void save_ppm(const std::string& path, const rc::media::ImageFrame& f)
{
    std::FILE* fp = std::fopen(path.c_str(), "wb");
    if (fp == nullptr) return;
    std::fprintf(fp, "P6\n%u %u\n255\n", f.width(), f.height());
    std::fwrite(f.data().data(), 1, f.size(), fp);
    std::fclose(fp);
}
}  // namespace

struct CameraDDSSubscriber::Impl
{
    DDSSubscription::Config    cfg;
    rc::media::MediaSubscriber sub;
    bool                       ready = false;
    std::string                out_file;
};

CameraDDSSubscriber::CameraDDSSubscriber(Config cfg) : pimpl_(std::make_unique<Impl>())
{
    pimpl_->cfg      = cfg;
    pimpl_->out_file = sanitize_filename(cfg.topic);
}

CameraDDSSubscriber::~CameraDDSSubscriber() = default;

bool CameraDDSSubscriber::init()
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

bool CameraDDSSubscriber::ready() const { return pimpl_->ready; }

std::string_view CameraDDSSubscriber::topic() const { return pimpl_->cfg.topic; }

int CameraDDSSubscriber::poll(int timeout_ms)
{
    if (not pimpl_->ready) return 0;
    const std::string& topic    = pimpl_->cfg.topic;
    const std::string& out_file = pimpl_->out_file;
    return pimpl_->sub.wait_and_poll(
        [&topic, &out_file](const rc::media::ImageFrame& f, std::int64_t recv_ns)
        {
            (void)recv_ns;
            std::printf("[sub][%s] frame_id=%llu %ux%u -> %s\n",
                        topic.c_str(), (unsigned long long)f.frame_id(),
                        f.width(), f.height(), out_file.c_str());
            save_ppm(out_file, f);
        },
        timeout_ms);
}

DDSSubscriptionHandle make_camera_subscription(DDSSubscription::Config cfg)
{
    auto s = std::make_unique<CameraDDSSubscriber>(std::move(cfg));
    if (not s->init()) return {};
    return DDSSubscriptionHandle{std::move(s)};
}
