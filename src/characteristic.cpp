#include "characteristic.hpp"

#include <host/ble_gatt.h>

#include "simple_nimble_peripheral.hpp"

using namespace SimpleNimble;

const char *Characteristic::tag = "CharacteristicBuffer";

Characteristic::Characteristic(
    uint32_t uuid16or32, size_t size, Chr_AccessFlag flag,
    std::initializer_list<Descriptor *> descriptors)
    : data_length(0),
	 buffer_size(size),
	 buffer(new uint8_t[size]),
	 flag(flag),
	 callback(nullptr),
	 callback_param(nullptr) {
	if (uuid16or32 & 0xffff0000) {
		// uuid32
		uuid.u32 = {
		    .u	 = {.type = BLE_UUID_TYPE_32},
		    .value = uuid16or32,
		};
	} else {
		// uuid16
		uuid.u16 = {
		    .u	 = {.type = BLE_UUID_TYPE_16},
		    .value = (uint16_t)uuid16or32,
		};
	}

	if (descriptors.size() == 0) {
		this->descriptors = nullptr;
	} else {
		ble_gatt_dsc_def *d = new ble_gatt_dsc_def[descriptors.size() + 1];
		this->descriptors	= d;
		for (auto desc : descriptors) {
			d->access_cb	 = access_callback;
			d->arg		 = desc;
			d->att_flags	 = static_cast<int>(desc->flag);
			d->min_key_size = 0;
			d->uuid		 = desc->uuid;
			d++;
		}
		d->access_cb	 = nullptr;
		d->arg		 = nullptr;
		d->att_flags	 = 0;
		d->min_key_size = 0;
		d->uuid		 = nullptr;
	}

	uint8_t _zero[size] = {0};
	write(_zero, size);
}

const uint8_t *Characteristic::read() { return buffer; }

void Characteristic::write(const uint8_t *data, uint8_t length) {
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len);
	this->data_length = cpy_len;
}

void Characteristic::write(const char *data) {
	size_t length	= strlen(data);
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len + 1);
	this->data_length = cpy_len;
}

void Characteristic::write(std::initializer_list<uint8_t> data) {
	int i = 0;
	for (uint8_t d : data) buffer[i++] = d;
	this->data_length = data.size();
}

void Characteristic::write_u8(uint8_t data) {
	buffer[0] = data;

	this->data_length = 1;
}

void Characteristic::write_u16(uint16_t data) {
	buffer[0] = (data >> 8) & 0xff;
	buffer[1] = (data >> 0) & 0xff;

	this->data_length = 2;
}

void Characteristic::write_u32(uint32_t data) {
	buffer[0] = (data >> 24) & 0xff;
	buffer[1] = (data >> 16) & 0xff;
	buffer[2] = (data >> 8) & 0xff;
	buffer[3] = (data >> 0) & 0xff;

	this->data_length = 4;
}

void Characteristic::clear(uint8_t size) {
	memset(buffer, 0, size);
	data_length = size;
}

void Characteristic::notify() {
	SimpleNimblePeripheral *p = SimpleNimblePeripheral::get_instance();
	if (!p->is_connected()) return;

	int conn_handle = p->get_conn_handle();
	// ESP_LOGI(tag, "notify: %d, %d", conn_handle, val_handle);
	ble_gatts_chr_updated(val_handle);
}

Characteristic::~Characteristic() {
	delete[] buffer;
	delete[] descriptors;
}

void Characteristic::set_callback(std::function<void(NimbleCallbackReason)> callback) {
	this->callback		 = callback;
}

void Characteristic::create_def(struct ble_gatt_chr_def *ptr) {
	ptr->uuid		   = &uuid.u;
	ptr->access_cb	   = access_callback;
	ptr->arg		   = this;
	ptr->descriptors  = descriptors;
	ptr->flags	   = static_cast<int>(flag);
	ptr->min_key_size = 0;
	ptr->val_handle   = &val_handle;
}

int Characteristic::access_callback(uint16_t conn_handle, uint16_t attr_handle,
							 struct ble_gatt_access_ctxt *ctxt, void *arg) {
	int rc;
	Characteristic *c = (Characteristic *)arg;
	Descriptor *d	   = (Descriptor *)arg;

	ESP_LOGI(tag, "cb: %d, %d, %d, %p", conn_handle, attr_handle, ctxt->op, arg);

	switch (ctxt->op) {
		case BLE_GATT_ACCESS_OP_READ_CHR:
			ESP_LOGI(tag, "read");
			if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
				MODLOG_DFLT(INFO, "Characteristic read; conn_handle=%d attr_handle=%d\n",
						  conn_handle, attr_handle);
			} else {
				MODLOG_DFLT(INFO, "Characteristic read by NimBLE stack; attr_handle=%d\n",
						  attr_handle);
			}

			if (c->callback) c->callback(NimbleCallbackReason::CHARACTERISTIC_READ);

			if (attr_handle == c->val_handle) {
				rc = os_mbuf_append(ctxt->om,
								c->buffer,
								c->data_length);
				ESP_LOGI(tag, "read complete: %d (%d)", rc, c->data_length);
				return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			}
			break;

		case BLE_GATT_ACCESS_OP_WRITE_CHR:
			ESP_LOGI(tag, "write");
			if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
				MODLOG_DFLT(INFO, "Characteristic write; conn_handle=%d attr_handle=%d",
						  conn_handle, attr_handle);
			} else {
				MODLOG_DFLT(INFO, "Characteristic write by NimBLE stack; attr_handle=%d",
						  attr_handle);
			}

			if (attr_handle == c->val_handle) {
				rc = ble_hs_mbuf_to_flat(ctxt->om, c->buffer, c->buffer_size, &c->data_length);
				ble_gatts_chr_updated(attr_handle);
				MODLOG_DFLT(INFO,
						  "Notification/Indication scheduled for "
						  "all subscribed peers.\n");

				if (c->callback) c->callback(NimbleCallbackReason::CHARACTERISTIC_WRITE);
				return rc;
			}
			break;

		case BLE_GATT_ACCESS_OP_READ_DSC:
			ESP_LOGI(tag, "desc read");
			rc = os_mbuf_append(ctxt->om,
							d->buffer,
							d->data_length);
			ESP_LOGI(tag, "desc result: %d %p", rc, d);
			ESP_LOG_BUFFER_HEXDUMP(tag, d->buffer, d->data_length, esp_log_level_t::ESP_LOG_INFO);
			return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			break;

		case BLE_GATT_ACCESS_OP_WRITE_DSC:
			ESP_LOGI(tag, "desc write");

			rc = ble_hs_mbuf_to_flat(ctxt->om, d->buffer, descriptor_buffer_size, &d->data_length);
			MODLOG_DFLT(INFO,
					  "Notification/Indication scheduled for "
					  "all subscribed peers.\n");
			return rc;
			break;
	}

	return BLE_ATT_ERR_UNLIKELY;
}
