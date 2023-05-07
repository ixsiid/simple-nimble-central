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
ble_gap_adv_params SimpleNimblePeripheral::adv_params = {
    .conn_mode = BLE_GAP_CONN_MODE_UND,
    .disc_mode = BLE_GAP_DISC_MODE_GEN,
};
#pragma GCC diagnostic warning "-Wmissing-field-initializers"

SimpleNimblePeripheral::SimpleNimblePeripheral()
    : service_length(0),
	 registered_service_count(0),
	 services(nullptr),
	 fields({}),
	 service_uuids(nullptr),
	 connected(false),
	 conn_handle(0) {
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
};

SimpleNimblePeripheral::~SimpleNimblePeripheral() {
	initialize_services(0);
};

void SimpleNimblePeripheral::set_name(const char *name) {
	ble_svc_gap_device_name_set(name);

	fields.name		    = (uint8_t *)ble_svc_gap_device_name();
	fields.name_len	    = (uint8_t)strlen(name);
	fields.name_is_complete = true;
};

void SimpleNimblePeripheral::initialize_services(uint8_t service_count) {
	ESP_LOGI(tag, "initialize services");

	// 既に定義済みの場合、メモリ消去
	if (services) {
		for (int i = 0; i < registered_service_count; i++) {
			struct ble_gatt_svc_def *s	   = &services[i];
			const struct ble_gatt_chr_def *c = s->characteristics;
			if (c == nullptr) continue;
			while (c->uuid) {
				ESP_LOGI(tag, "delete descriptor: %p", c->descriptors);
				if (c->descriptors) delete[] c->descriptors;
				ESP_LOGI(tag, "It could stop here.");
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

	if (service_uuids) delete[] service_uuids;
	service_uuids = new ble_uuid_any_t[service_count];

	if (fields.uuids16) delete[] fields.uuids16;
	if (fields.uuids32) delete[] fields.uuids32;
	if (fields.uuids128) delete[] fields.uuids128;
	fields.num_uuids16	= 0;
	fields.num_uuids32	= 0;
	fields.num_uuids128 = 0;
};

bool SimpleNimblePeripheral::add_service(SimpleNimbleCallback callback, Service *service) {
	ESP_LOGI(tag, "add service");

	if (registered_service_count >= service_length) return false;

	service->create_def(&services[registered_service_count++]);

	services[registered_service_count].type			 = 0;
	services[registered_service_count].characteristics = nullptr;
	services[registered_service_count].includes		 = nullptr;
	services[registered_service_count].uuid			 = nullptr;

	switch (service->type()) {
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

	SimpleNimblePeripheral *p = (SimpleNimblePeripheral *)arg;

	switch (event->type) {
		// event->connectとevent->conn_updateは同一型
		case BLE_GAP_EVENT_CONNECT:
		case BLE_GAP_EVENT_CONN_UPDATE:
			ESP_LOGI(tag, "ble gap connect or connection update");
			if (event->connect.status == 0) {
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: success (%d)", event->connect.conn_handle);
				rc = ble_gap_conn_find(event->connect.conn_handle, &desc);
				ESP_LOGI(tag, "connection found: %d, %d", rc, desc.conn_handle);
				p->conn_handle = event->connect.conn_handle;
				p->connected	= true;
			} else {
				ESP_LOGI(tag, "BLE_GAP_EVENT_CONNECT: failed");
				p->connected = false;
				p->start_advertise();
			}

			// Windowsの再接続対策に、0x2902 Descriptorに {0x01, 0x00} をセットするといいらしい
			// https://github.com/nkolban/esp32-snippets/issues/632
			// NimBLEでは 0x2902は自動的に作られるためどうやってデータを書き込む？

			return 0;

		case BLE_GAP_EVENT_CONN_UPDATE_REQ:
			ESP_LOGI(tag, "[PEER] Interval: %d - %d, ce len: %d - %d, timeout: %d, latency: %d",
				    event->conn_update_req.peer_params->itvl_min, event->conn_update_req.peer_params->itvl_max,
				    event->conn_update_req.peer_params->min_ce_len, event->conn_update_req.peer_params->max_ce_len,
				    event->conn_update_req.peer_params->supervision_timeout, event->conn_update_req.peer_params->latency);
			ESP_LOGI(tag, "[SELF] Interval: %d - %d, ce len: %d - %d, timeout: %d, latency: %d",
				    event->conn_update_req.self_params->itvl_min, event->conn_update_req.self_params->itvl_max,
				    event->conn_update_req.self_params->min_ce_len, event->conn_update_req.self_params->max_ce_len,
				    event->conn_update_req.self_params->supervision_timeout, event->conn_update_req.self_params->latency);
			return 0;

		case BLE_GAP_EVENT_DISCONNECT:
			ESP_LOGI(tag, "ble gap disconnect");
			p->connected = false;
			p->start_advertise();
			return 0;

		case BLE_GAP_EVENT_SUBSCRIBE:
			ESP_LOGI(tag, "ble gap subscribe, value handle: %d, reason: %d",
				    event->subscribe.attr_handle,
				    event->subscribe.reason);
			return 0;

		case BLE_GAP_EVENT_MTU:
			ESP_LOGI(tag, "mtu conn: %hx, channel: %x, value: %d", event->mtu.conn_handle, event->mtu.channel_id, event->mtu.value);
			return 0;
	}

	return 0;
};

void SimpleNimblePeripheral::start() {
	ESP_LOGI(tag, "start");
	int rc;

	ESP_LOGI(tag, "service count 16: %d, 32: %d, 128: %d", fields.num_uuids16, fields.num_uuids32, fields.num_uuids128);

	if (fields.num_uuids16 > 0) {
		ble_uuid16_t *uuids = new ble_uuid16_t[fields.num_uuids16];
		ble_gatt_svc_def *s = services;
		for (int i = 0; i < fields.num_uuids16;) {
			if (s->type == 0) break;
			if (s->uuid->type == BLE_UUID_TYPE_16) {
				ESP_LOGI(tag, "uuid [%p]: 0x%hx", s, ((ble_uuid16_t *)s->uuid)->value);
				uuids[i++] = *((ble_uuid16_t *)(s->uuid));
			}
			s++;
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
			if (s->type == 0) break;
			if (s->uuid->type == BLE_UUID_TYPE_32) {
				uuids[i++] = *((ble_uuid32_t *)(s->uuid));
			}
			s++;
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
			if (s->type == 0) break;
			if (s->uuid->type == BLE_UUID_TYPE_128) {
				uuids[i++] = *((ble_uuid128_t *)(s->uuid));
			}
			s++;
		}
		fields.uuids128 = uuids;
	} else {
		fields.uuids128 = nullptr;
	}
	fields.uuids128_is_complete = true;

	ble_gatts_stop();

	rc = ble_gatts_count_cfg(services);
	ESP_LOGI(tag, "count cfg: %d", rc);
	rc = ble_gatts_add_svcs(services);
	ESP_LOGI(tag, "add_svcs: %d", rc);

	rc = ble_gatts_start();
	ESP_LOGI(tag, "gatts start: %d", rc);

	rc = ble_gap_adv_set_fields(&fields);
	ESP_LOGI(tag, "gap field: %d", rc);

	start_advertise();
};

void SimpleNimblePeripheral::start_advertise() {
	int rc;

	rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, ble_gap_event, this);
	ESP_LOGI(tag, "gap adv start: %d", rc);

	// debug();
};

SimpleNimblePeripheral *SimpleNimblePeripheral::get_instance() {
	if (instance == nullptr) instance = new SimpleNimblePeripheral();
	return instance;
};

void SimpleNimblePeripheral::print_services(SimpleNimblePeripheral *obj) {
	ESP_LOGI(tag, "print services");

	ESP_LOGI(tag, "  %p, %p", obj, obj->services);

	for (ble_gatt_svc_def *s = obj->services; s->type; s++) {
		ESP_LOGI(tag, "  [%p] %d, %p, 0x%hx", s, s->type, s->uuid, ((ble_uuid16_t *)s->uuid)->value);
		if (s->characteristics == nullptr) continue;
		ESP_LOGI(tag, "  characteristic");
		for (const ble_gatt_chr_def *c = s->characteristics; c->uuid; c++) {
			ESP_LOGI(tag, "    [%p] %p 0x%hx", c, c->uuid, ((ble_uuid16_t *)c->uuid)->value);
			if (c->descriptors == nullptr) continue;
			ESP_LOGI(tag, "    desciptor");
			for (const ble_gatt_dsc_def *d = c->descriptors; d->uuid; d++) {
				ESP_LOGI(tag, "    [%p] %p 0x%hx", d, d->uuid, ((ble_uuid16_t *)d->uuid)->value);
			}
		}
	}
};

bool SimpleNimblePeripheral::is_connected() { return connected; }
uint16_t SimpleNimblePeripheral::get_conn_handle() { return conn_handle; }

void SimpleNimblePeripheral::debug() {
	print_services(this);

	ble_gatts_show_local();
}