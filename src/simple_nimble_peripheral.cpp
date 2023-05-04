#include <esp_log.h>
#include <nvs_flash.h>

#include <host/ble_gap.h>
#include <services/gap/ble_svc_gap.h>
#include <nimble/nimble_port_freertos.h>
#include <nimble/nimble_port.h>
#include <esp_nimble_hci.h>
#include <host/util/util.h>

#include "simple_nimble_peripheral.hpp"

const char *SimpleNimblePeripheral::tag = "SimpleNimblePeripheral";

EventGroupHandle_t SimpleNimblePeripheral::event_group	  = xEventGroupCreate();
SimpleNimblePeripheral *SimpleNimblePeripheral::instance = nullptr;

#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
struct ble_gap_adv_params SimpleNimblePeripheral::adv_params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
};
#pragma GCC diagnostic warning "-Wmissing-field-initializers"

SimpleNimblePeripheral::SimpleNimblePeripheral()
    : service_length(0),
	 registered_service_count(0),
	 services(nullptr),
	 fields({}) {
	ESP_LOGI(tag, "create instance");

	fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

	fields.tx_pwr_lvl		    = BLE_HS_ADV_TX_PWR_LVL_AUTO;
	fields.tx_pwr_lvl_is_present = true;

	esp_nimble_hci_and_controller_init();
	nimble_port_init();

	nimble_port_freertos_init([](void *param) {
		ESP_LOGI(tag, "BLE Host Task Started");
		nimble_port_run();

		ESP_LOGI(tag, "nimble_port_freertos_deinit");
		nimble_port_freertos_deinit();
	});
}

enum class DiscType : uintptr_t {
	Service,
	Characteristic,
};

void SimpleNimblePeripheral::set_name(const char *name) {
	ble_svc_gap_device_name_set(name);

	fields.name		    = (uint8_t *)ble_svc_gap_device_name();
	fields.name_len	    = (uint8_t)strlen(name);
	fields.name_is_complete = true;
};

void SimpleNimblePeripheral::initialize_services(uint8_t service_count) {
	ESP_LOGI(tag, "initialize services");
	if (services) {
		for (int i = 0; i < registered_service_count; i++) {
			struct ble_gatt_svc_def *s	   = &services[i];
			const struct ble_gatt_chr_def *c = s->characteristics;
			if (c == nullptr) continue;
			while (c->uuid) {
				if (c->descriptors) delete[] c->descriptors;
				c++;
			}
			delete[] s->characteristics;
		}
		delete[] services;
	}

	this->service_length	= service_count;
	registered_service_count = 0;

	int rc;
	rc = ble_gap_terminate(0, BLE_ERR_REM_USER_CONN_TERM);
	ESP_LOGI(tag, "terminate: %d", rc);

	ble_svc_gap_init();
	ble_svc_gatt_init();
	services = new struct ble_gatt_svc_def[service_count + 1];

	services[0].uuid		   = nullptr;
	services[0].type		   = 0;
	services[0].includes	   = nullptr;
	services[0].characteristics = nullptr;

	if (fields.uuids16) delete[] fields.uuids16;
	if (fields.uuids32) delete[] fields.uuids32;
	if (fields.uuids128) delete[] fields.uuids128;
	fields.num_uuids16	= 0;
	fields.num_uuids32	= 0;
	fields.num_uuids128 = 0;
};

bool SimpleNimblePeripheral::add_service(SimpleNimbleCallback callback, ble_uuid_any_t service_uuid, std::initializer_list<SimpleNimbleCharacteristicBuffer *> charas) {
	ESP_LOGI(tag, "add service");

	if (registered_service_count >= service_length) return false;

	struct ble_gatt_svc_def *s = &services[registered_service_count];

	s->type = registered_service_count == 0 ? BLE_GATT_SVC_TYPE_PRIMARY : BLE_GATT_SVC_TYPE_SECONDARY;
	s->uuid = &service_uuid.u;

	s->characteristics = new struct ble_gatt_chr_def[charas.size() + 1];

	struct ble_gatt_chr_def *chara = (struct ble_gatt_chr_def *)s->characteristics;
	for (auto c : charas) {
		c->create_def(chara);
		chara++;
	}
	chara->uuid = nullptr;

	ESP_LOGI(tag, "created characteristics");

	registered_service_count++;
	s++;
	s->uuid		    = nullptr;
	s->type		    = 0;
	s->includes	    = nullptr;
	s->characteristics = nullptr;

	switch (service_uuid.u.type) {
		case BLE_UUID_TYPE_16:
			fields.num_uuids16++;
			break;
		case BLE_UUID_TYPE_32:
			fields.num_uuids32++;
			break;
		case BLE_UUID_TYPE_128:
			fields.num_uuids128++;
			break;
	}

	return true;
};

int SimpleNimblePeripheral::ble_gap_event(struct ble_gap_event *event, void *arg) {
	ESP_LOGI(tag, "GAP event: %d", event->type);
	struct ble_gap_conn_desc desc;
	int rc;

	switch (event->type) {
		case BLE_GAP_EVENT_CONNECT:
		case BLE_GAP_EVENT_CONN_UPDATE:
			ESP_LOGI(tag, "ble gap connect or connection update");
			if (event->connect.status == 0) {
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: success");
				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
			} else {
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: failed");
			}

			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			ESP_LOGI(tag, "ble gap disconnect");
			// rc = ble_gap_adv_stop(); // 自動的にstopされるため不要
			rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, ble_gap_event, nullptr);
			ESP_LOGI(tag, "gap adv start: %d", rc);
			return 0;
	}

	return 0;
};

void SimpleNimblePeripheral::start() {
	ESP_LOGI(tag, "start");
	int rc;

	rc = ble_gatts_count_cfg(services);
	ESP_LOGI(tag, "count cfg: %d", rc);
	rc = ble_gatts_add_svcs(services);
	ESP_LOGI(tag, "add_svcs: %d", rc);

	if (fields.num_uuids16 > 0) {
		ble_uuid16_t *uuids = new ble_uuid16_t[fields.num_uuids16];
		ble_gatt_svc_def *s = services;
		for (int i = 0; i < fields.num_uuids16;) {
			if (s->uuid == nullptr) break;
			if (s->uuid->type == BLE_UUID_TYPE_16) {
				uuids[i++] = *((ble_uuid16_t *)(s->uuid));
			}
		}
		fields.uuids16 = uuids;
	} else {
		fields.uuids16 = nullptr;
	}
	fields.uuids16_is_complete = true;

	if (fields.num_uuids32 > 0) {
		ble_uuid32_t *uuids = new ble_uuid32_t[fields.num_uuids32];
		ble_gatt_svc_def *s = services;
		for (int i = 0; i < fields.num_uuids32;) {
			if (s->uuid == nullptr) break;
			if (s->uuid->type == BLE_UUID_TYPE_32) {
				uuids[i++] = *((ble_uuid32_t *)(s->uuid));
			}
		}
		fields.uuids32 = uuids;
	} else {
		fields.uuids32 = nullptr;
	}
	fields.uuids32_is_complete = true;

	if (fields.num_uuids128 > 0) {
		ble_uuid128_t *uuids = new ble_uuid128_t[fields.num_uuids16];
		ble_gatt_svc_def *s	 = services;
		for (int i = 0; i < fields.num_uuids128;) {
			if (s->uuid == nullptr) break;
			if (s->uuid->type == BLE_UUID_TYPE_128) {
				uuids[i++] = *((ble_uuid128_t *)(s->uuid));
			}
		}
		fields.uuids128 = uuids;
	} else {
		fields.uuids128 = nullptr;
	}
	fields.uuids128_is_complete = true;

	rc = ble_gatts_start();
	ESP_LOGI(tag, "gatts start: %d", rc);

	rc = ble_gap_adv_set_fields(&fields);
	ESP_LOGI(tag, "gap field: %d", rc);

	rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, ble_gap_event, nullptr);
	ESP_LOGI(tag, "gap adv start: %d", rc);
};

SimpleNimblePeripheral *SimpleNimblePeripheral::get_instance() {
	if (instance == nullptr) instance = new SimpleNimblePeripheral();
	return instance;
};