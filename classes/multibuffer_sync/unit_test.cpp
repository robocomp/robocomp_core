#include <iostream>
#include <random>
#include <chrono>
#include "multibuffer_sync.h"  // Asegúrate de incluir tu SyncBuffer aquí

// Simulamos dos tipos de datos
struct FakeLidar
{
    long long timestamp;
    int id;
};

struct FakeCamera
{
    long long timestamp;
    int id;
};

// Simulamos "procesar" el dato
FakeLidar process_lidar(const FakeLidar& in) { return in; }
FakeCamera process_camera(const FakeCamera& in) { return in; }

int main()
{
    using namespace std::chrono;

    SyncBuffer<
        std::pair<FakeLidar, FakeLidar>,
        std::pair<FakeCamera, FakeCamera>
    > sync_buffer(50 /* buffer capacity */, 5000.0 /* max spread us */, 100000.0 /* timeout us */);

    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> lidar_jitter(-200, 200);     // +-200 us
    std::uniform_int_distribution<int> camera_jitter(-400, 400);    // +-400 us

    long long sim_time_us = 0;   // Tiempo simulado en microsegundos
    int lidar_id = 0;
    int camera_id = 0;

    for (int step = 0; step < 1000; ++step)
    {
        sim_time_us += 33333;  // avance de ~33ms por paso (equivalente a ~30 Hz)

        // Lidar cada paso (30 Hz)
        FakeLidar lidar{sim_time_us + lidar_jitter(rng), lidar_id++};
        sync_buffer.push<0>(lidar, process_lidar);

        // Cámara cada dos pasos (15 Hz)
        if (step % 2 == 0)
        {
            FakeCamera cam{sim_time_us + camera_jitter(rng), camera_id++};
            sync_buffer.push<1>(cam, process_camera);
        }

        // Intentar leer sincronizados
        if (auto synced = sync_buffer.read())
        {
            auto [synced_lidar, synced_camera] = *synced;
            std::cout << "MATCH found:\n";
            std::cout << "  Lidar: t=" << synced_lidar.timestamp << " id=" << synced_lidar.id << "\n";
            std::cout << "  Camera: t=" << synced_camera.timestamp << " id=" << synced_camera.id << "\n";
            std::cout << "  Spread (us): " << std::abs(synced_lidar.timestamp - synced_camera.timestamp) << "\n\n";
        }
    }

    std::cout << "Test finished." << std::endl;
}

