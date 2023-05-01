# Simple Nimble Central

```cpp
SimpleNimbleCentral * central = SimpleNimbleCentral::get_instance();

if (!central->connect((const ble_addr_t *)&address)) throw "Could not connect";
if (!central->find_service((const ble_uuid_t *)&service_uuid)) throw "Could not find service";
if (!central->find_characteristic((const ble_uuid_t *)&characteristic_uuid)) throw "Could not find characteristic";
if (!central->write(buffer, length)) return "Could not write data";
```

Write先は最後にfind_characteristicで見つかった宛先です