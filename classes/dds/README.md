# dds

Librería DDS mínima, standalone (sin FastDDS en el contrato) para publicar y
suscribirse a los sensores típicos de un robot (lidar, cámara, imu, image360)
desde un componente RoboComp. Compila y corre igual **con o sin FastDDS
instalado**: sin FastDDS, nunca da error de compilación ni crashea en
runtime, los canales quedan simplemente "no listos".

Copiada desde
[`experimental/dds_minimal_examples`](../../../components/robocomp-robolab/experimental/dds_minimal_examples)
(mismo repo `robocomp-robolab`), donde está la documentación completa de la
arquitectura (`core/` vs `null_backend/` vs `sensors/`+`media_transport/`,
invariante `kind()`/`ready()`, etc.) y dos binarios de demo
(`vector_publisher_example`/`vector_subscriber_example`) que sirven de
referencia de uso. Esta copia **no se sincroniza sola**: si el original
cambia, hay que volver a copiarla a mano.

## Uso desde un componente

```cmake
add_subdirectory($ENV{ROBOCOMP}/classes/dds ${CMAKE_BINARY_DIR}/dds)
target_link_libraries(${PROJECT_NAME} PRIVATE dds)
```

Con eso el componente puede incluir `core/dds_channel.h`,
`core/dds_optional.h`, `core/dds_subscription.h`,
`core/dds_subscription_handle.h` y `sensors/<lidar|camera|imu|image360>/*.h`
sin `../`, gracias a `target_include_directories(dds PUBLIC ...)` en la raíz
de esta carpeta.

### Flag DDS_BACKEND

Igual que en el proyecto original: `AUTO` (por defecto) detecta FastDDS y
cae al backend nulo si falta; `ON` lo exige (error de configuración si no
está); `OFF` fuerza el backend nulo aunque FastDDS esté instalado. Se pasa
igual como variable de CMake al configurar el componente:
`cmake -DDDS_BACKEND=OFF ..`.

## API mínima

- Publicar: `DDSOptional` envuelve un `DDSChannel` (`init()`, `ready()`,
  `kind()`, `topic()`, `publish(Frame)`), un `std::vector<DDSOptional>`
  agrupa canales de distinto sensor.
- Suscribirse: `DDSSubscriptionHandle` envuelve un `DDSSubscription`
  (`init()`, `ready()`, `kind()`, `topic()`, `poll(timeout_ms)`), mismo
  patrón de `std::vector`.
- El bucle de usuario despacha por `kind()` (qué `Frame` construir/leer) y
  solo publica/lee si `ready()` es `true`.

Ver los `.h` de `core/` y los `main()` de los dos ejemplos en
`experimental/dds_minimal_examples` para el patrón completo.
