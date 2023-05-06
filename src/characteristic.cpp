#include "characteristic.hpp"

const char *SimpleNimbleCharacteristicBuffer::tag = "CharacteristicBuffer";

SimpleNimbleCharacteristicBuffer::SimpleNimbleCharacteristicBuffer(
    size_t size, Chr_AccessFlag flag,
    std::initializer_list<Descriptor *> descriptors)
    : data_length(0),
	 buffer_size(size),
	 buffer(new uint8_t[size]),
	 flag(flag) {
	if (descriptors.size() == 0) {
		this->descriptors = nullptr;
	} else {
		ble_gatt_dsc_def *d = new ble_gatt_dsc_def[descriptors.size() + 1];
		this->descriptors	= d;
		for (auto desc : descriptors) {
			d->access_cb	 = access_callback;
			d->arg		 = d;
			d->att_flags	 = static_cast<int>(desc->flag);
			d->min_key_size = 0;
			d->uuid		 = &desc->uuid.u;
			d++;

			memcpy(desc->buffer, "hello, world!!?", 16);
			desc->data_length = 15;
		}
		d->access_cb	 = nullptr;
		d->arg		 = nullptr;
		d->att_flags	 = 0;
		d->min_key_size = 0;
		d->uuid		 = nullptr;
	}
}

SimpleNimbleCharacteristicBuffer::SimpleNimbleCharacteristicBuffer(
    uint32_t uuid16or32, size_t size, Chr_AccessFlag flag,
    std::initializer_list<Descriptor *> descriptors)
    : SimpleNimbleCharacteristicBuffer(size, flag, descriptors) {
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
}

const uint8_t *SimpleNimbleCharacteristicBuffer::read() { return buffer; }

void SimpleNimbleCharacteristicBuffer::write(const uint8_t *data, uint8_t length) {
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len);
	this->data_length = cpy_len;
}
void SimpleNimbleCharacteristicBuffer::write(const char *data) {
	size_t length	= strlen(data);
	size_t cpy_len = buffer_size > length ? length : buffer_size;
	memcpy(buffer, data, cpy_len + 1);
	this->data_length = cpy_len;
}

void SimpleNimbleCharacteristicBuffer::write_u8(uint8_t data) {
	buffer[0] = data;

	this->data_length = 1;
}

void SimpleNimbleCharacteristicBuffer::write_u16(uint16_t data) {
	buffer[0] = (data >> 8) & 0xff;
	buffer[1] = (data >> 0) & 0xff;

	this->data_length = 2;
}

void SimpleNimbleCharacteristicBuffer::write_u32(uint32_t data) {
	buffer[0] = (data >> 24) & 0xff;
	buffer[1] = (data >> 16) & 0xff;
	buffer[2] = (data >> 8) & 0xff;
	buffer[3] = (data >> 0) & 0xff;

	this->data_length = 4;
}

void SimpleNimbleCharacteristicBuffer::notify() {}

SimpleNimbleCharacteristicBuffer::~SimpleNimbleCharacteristicBuffer() {
	delete[] buffer;
	delete[] descriptors;
}

void SimpleNimbleCharacteristicBuffer::create_def(struct ble_gatt_chr_def *ptr) {
	ptr->uuid		   = &uuid.u;
	ptr->access_cb	   = access_callback;
	ptr->arg		   = this;
	ptr->descriptors  = descriptors;
	ptr->flags	   = static_cast<int>(flag);
	ptr->min_key_size = 0;
	ptr->val_handle   = &val_handle;
}

int SimpleNimbleCharacteristicBuffer::access_callback(uint16_t conn_handle, uint16_t attr_handle,
										    struct ble_gatt_access_ctxt *ctxt, void *arg) {
	int rc;
	SimpleNimbleCharacteristicBuffer *s = (SimpleNimbleCharacteristicBuffer *)arg;
	Descriptor *d					 = (Descriptor *)arg;

	ESP_LOGI(tag, "cb: %d, %d, %d", conn_handle, attr_handle, ctxt->op);

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
			if (attr_handle == s->val_handle) {
				rc = os_mbuf_append(ctxt->om,
								s->buffer,
								s->data_length);
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

			if (attr_handle == s->val_handle) {
				rc = ble_hs_mbuf_to_flat(ctxt->om, s->buffer, s->buffer_size, &s->data_length);
				ble_gatts_chr_updated(attr_handle);
				MODLOG_DFLT(INFO,
						  "Notification/Indication scheduled for "
						  "all subscribed peers.\n");
				return rc;
			}
			break;

		case BLE_GATT_ACCESS_OP_READ_DSC:
			ESP_LOGI(tag, "desc read");
			rc = os_mbuf_append(ctxt->om,
							d->buffer,
							d->data_length);
			return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
			break;
		case BLE_GATT_ACCESS_OP_WRITE_DSC:
			ESP_LOGI(tag, "desc write");

			rc = ble_hs_mbuf_to_flat(ctxt->om, d->buffer, d->buffer_size, &d->data_length);
			MODLOG_DFLT(INFO,
					  "Notification/Indication scheduled for "
					  "all subscribed peers.\n");
			return rc;
			break;
	}

	return BLE_ATT_ERR_UNLIKELY;
}
