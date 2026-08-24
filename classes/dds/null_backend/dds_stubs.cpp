// Compilado EN VEZ DE {lidar,camera,imu,image360}_{channel,subscription}.cpp
// cuando esta build no tiene FastDDS (ver CMakeLists.txt). Las ocho
// factories son cada una una línea que envuelve el mismo Null* genérico
// (dds_null_channel.h / dds_null_subscription.h) — ninguna construye jamás
// la clase concreta del sensor real ni referencia un símbolo de FastDDS, así
// que no hay motivo para ocho traducciones separadas: se juntan aquí en vez
// de repetir cabecera de comentario + #include por fichero.
//
// Añadir un sensor nuevo es sumar dos líneas más aquí, no un fichero nuevo.

#include "sensors/lidar/lidar_channel.h"
#include "sensors/lidar/lidar_subscription.h"
#include "sensors/camera/camera_channel.h"
#include "sensors/camera/camera_subscription.h"
#include "sensors/imu/imu_channel.h"
#include "sensors/imu/imu_subscription.h"
#include "sensors/image360/image360_channel.h"
#include "sensors/image360/image360_subscription.h"
#include "null_backend/dds_null_channel.h"
#include "null_backend/dds_null_subscription.h"

DDSOptional make_lidar_channel(DDSChannel::Config cfg)
{
    return DDSOptional{make_null_channel("lidar", cfg.topic)};
}

DDSOptional make_camera_channel(DDSChannel::Config cfg)
{
    return DDSOptional{make_null_channel("camera", cfg.topic)};
}

DDSOptional make_imu_channel(DDSChannel::Config cfg)
{
    return DDSOptional{make_null_channel("imu", cfg.topic)};
}

DDSOptional make_image360_channel(DDSChannel::Config cfg)
{
    return DDSOptional{make_null_channel("image360", cfg.topic)};
}

DDSSubscriptionHandle make_lidar_subscription(DDSSubscription::Config cfg)
{
    return DDSSubscriptionHandle{make_null_subscription("lidar", cfg.topic)};
}

DDSSubscriptionHandle make_camera_subscription(DDSSubscription::Config cfg)
{
    return DDSSubscriptionHandle{make_null_subscription("camera", cfg.topic)};
}

DDSSubscriptionHandle make_imu_subscription(DDSSubscription::Config cfg)
{
    return DDSSubscriptionHandle{make_null_subscription("imu", cfg.topic)};
}

DDSSubscriptionHandle make_image360_subscription(DDSSubscription::Config cfg)
{
    return DDSSubscriptionHandle{make_null_subscription("image360", cfg.topic)};
}
