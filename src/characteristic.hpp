#pragma once

#include <initializer_list>

#include <host/ble_uuid.h>
#include <host/ble_hs.h>

#include "simple_nimble_type.hpp"
#include "descriptor.hpp"

using namespace SimpleNimble;

class SimpleNimbleCharacteristicBuffer {
    private:
	static const char *tag;

	ble_uuid_any_t uuid;
	uint16_t data_length;
	size_t buffer_size;
	uint8_t *buffer;
	Chr_AccessFlag flag;
	uint16_t val_handle;
	ble_gatt_dsc_def *descriptors;

	static int access_callback(uint16_t conn_handle, uint16_t attr_handle,
						  struct ble_gatt_access_ctxt *ctxt, void *arg);
						  
	SimpleNimbleCharacteristicBuffer(size_t buffer_size, Chr_AccessFlag flag,
							   std::initializer_list<Descriptor *> descriptors = {});

    public:
	SimpleNimbleCharacteristicBuffer(uint32_t uuid16or32, size_t buffer_size, Chr_AccessFlag flag,
							   std::initializer_list<Descriptor *> descriptors = {});
	~SimpleNimbleCharacteristicBuffer();
	const uint8_t *read();
	void write(const uint8_t *data, uint8_t length);
	void write(const char *data);
	void write_u8(uint8_t data);
	void write_u16(uint16_t data);
	void write_u32(uint32_t data);
	void notify();

	void create_def(struct ble_gatt_chr_def *ptr);
};