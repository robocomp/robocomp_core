from collections import deque
from typing import Any, Callable, Dict, List, Optional, Tuple
import numpy as np

class SyncBuffer:
    def __init__(self, 
                 sensor_names: List[str],
                 max_allowed_spread_usec: float,
                 timeout_usec: float,
                 relaxation_factor: float = 1.33,
                 max_relaxed_spread_usec: float = 100000.0,
                 max_candidate_age_usec: float = 100000.0,
                 timestamp_scale_factor: float = 1000.0):
        self.sensors = {name: deque() for name in sensor_names}
        self.transforms = {name: None for name in sensor_names}
        self.max_allowed_spread = max_allowed_spread_usec
        self.timeout = timeout_usec
        self.relaxation_factor = relaxation_factor
        self.max_relaxed_spread = max_relaxed_spread_usec
        self.max_candidate_age = max_candidate_age_usec
        self.timestamp_scale_factor = timestamp_scale_factor

    def push(self, sensor_name: str, data: Any, timestamp: float, transform: Optional[Callable[[Any], Any]] = None):
        if transform is not None:
            self.transforms[sensor_name] = transform
        self.sensors[sensor_name].append((timestamp * self.timestamp_scale_factor, data))

    def read(self) -> Optional[Dict[str, Any]]:
        if not all(len(queue) > 0 for queue in self.sensors.values()):
            return None

        newest_ts = max(queue[0][0] for queue in self.sensors.values())

        base_sensor = next(iter(self.sensors))
        base_queue = self.sensors[base_sensor]

        best_spread = float('inf')
        best_match = None

        for base_ts, base_data in base_queue:
            candidate = {base_sensor: (base_ts, base_data)}
            valid = True
            timestamps = [base_ts]

            for name, queue in self.sensors.items():
                if name == base_sensor:
                    continue

                closest_ts, closest_data = min(queue, key=lambda x: abs(x[0] - base_ts))
                timestamps.append(closest_ts)
                candidate[name] = (closest_ts, closest_data)

            spread = max(timestamps) - min(timestamps)
            candidate_age = newest_ts - base_ts

            allowed_spread = min(self.max_allowed_spread + self.relaxation_factor * candidate_age, self.max_relaxed_spread)

            if spread <= allowed_spread:
                if candidate_age <= self.max_candidate_age:
                    return {name: self._apply_transform(name, data) for name, (ts, data) in candidate.items()}
                elif spread < best_spread:
                    best_spread = spread
                    best_match = candidate

        if best_match is not None:
            selected_ts = list(best_match.values())[0][0]
            if newest_ts - selected_ts <= self.max_candidate_age:
                return {name: self._apply_transform(name, data) for name, (ts, data) in best_match.items()}

        return None

    def _apply_transform(self, sensor_name: str, data: Any) -> Any:
        transform = self.transforms.get(sensor_name)
        if transform:
            return transform(data)
        return data

    def clear(self):
        for queue in self.sensors.values():
            queue.clear()

# Example usage
if __name__ == "__main__":
    # Initialize the SyncBuffer
    sync = SyncBuffer(sensor_names=["lidar", "camera"], max_allowed_spread_usec=10000.0, timeout_usec=100000.0)

    # Push some dummy sensor data
    sync.push("lidar", {"points": [1, 2, 3]}, timestamp=1000)
    sync.push("camera", {"image": [255, 0, 0]}, timestamp=1001)

    # Try reading synchronized data
    result = sync.read()

    if result:
        print("Synchronized data:")
        for sensor, data in result.items():
            print(f"Sensor: {sensor}, Data: {data}")
    else:
        print("No synchronized data available.")

