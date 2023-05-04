#pragma once

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

class SimpleNimbleCharacteristicBuffer {
    public:
	enum class Flag {
		Read			   = BLE_GATT_CHR_F_READ,
		Write_no_response = BLE_GATT_CHR_F_WRITE_NO_RSP,
		Write		   = BLE_GATT_CHR_F_WRITE,
		Notify		   = BLE_GATT_CHR_F_NOTIFY,
	};

    private:
	static const char *tag;

	ble_uuid_any_t uuid;
	uint16_t data_length;
	size_t buffer_size;
	uint8_t *buffer;
	Flag flag;
	uint16_t val_handle;

	static int access_callback(uint16_t conn_handle, uint16_t attr_handle,
						  struct ble_gatt_access_ctxt *ctxt, void *arg);

    public:
	SimpleNimbleCharacteristicBuffer(ble_uuid_any_t uuid, size_t buffer_size, Flag flag);
	~SimpleNimbleCharacteristicBuffer();
	const uint8_t *read();
	void write(const uint8_t *data, uint8_t length);
	void write_u8(uint8_t data);
	void write_u16(uint16_t data);
	void write_u32(uint32_t data);
	void notify();

	void create_def(struct ble_gatt_chr_def *ptr);
};

constexpr SimpleNimbleCharacteristicBuffer::Flag operator|(SimpleNimbleCharacteristicBuffer::Flag l, SimpleNimbleCharacteristicBuffer::Flag r) {
	return static_cast<SimpleNimbleCharacteristicBuffer::Flag>(static_cast<int>(l) | static_cast<int>(r));
}