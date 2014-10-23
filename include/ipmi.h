/* Copyright 2013-2014 IBM Corp.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * 	http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __IPMI_H
#define __IPMI_H

#include <stdint.h>
#include <ccan/list/list.h>
#include <stdbool.h>

/*
 * IPMI codes as defined by the standard.
 */
#define IPMI_GET_DEVICE_ID_CMD		0x01
#define IPMI_COLD_RESET_CMD		0x02
#define IPMI_WARM_RESET_CMD		0x03
#define IPMI_CLEAR_MSG_FLAGS_CMD	0x30
#define IPMI_GET_DEVICE_GUID_CMD	0x08
#define IPMI_GET_MSG_FLAGS_CMD		0x31
#define IPMI_SEND_MSG_CMD		0x34
#define IPMI_GET_MSG_CMD		0x33
#define IPMI_SET_BMC_GLOBAL_ENABLES_CMD	0x2e
#define IPMI_GET_BMC_GLOBAL_ENABLES_CMD	0x2f
#define IPMI_READ_EVENT_MSG_BUFFER_CMD	0x35
#define IPMI_GET_CHANNEL_INFO_CMD	0x42

/*
 * 28. Chassis Commands
 */
#define IPMI_CHASSIS_GET_CAP_CMD		0x00
#define IPMI_CHASSIS_GET_STATUS_CMD		0x01
#define IPMI_CHASSIS_CONTROL_CMD		0x02
#define IPMI_CHASSIS_RESET_CMD			0x03
#define IPMI_CHASSIS_IDENTIFY_CMD		0x04
#define IPMI_CHASSIS_SET_PANEL_BUTTON_EN_CMD	0x05
#define IPMI_CHASSIS_SET_CAP_CMD		0x06
#define IPMI_CHASSIS_SET_PWR_RESTORE_CMD	0x07
#define IPMI_CHASSIS_SET_PWR_CYCLE_CMD		0x08
#define IPMI_CHASSIS_GET_SYS_RESTART_CAUSE_CMD	0x09
#define IPMI_CHASSIS_SET_SYS_BOOT_OPT_CMD	0x0a
#define IPMI_CHASSIS_GET_SYS_BOOT_OPT_CMD	0x0b
#define IPMI_CHASSIS_GET_POH_COUNTER_CMD	0x0f


/* 28.3. Chassis Control Command */
#define   IPMI_CHASSIS_PWR_DOWN 		0x00
#define   IPMI_CHASSIS_PWR_UP			0x01
#define   IPMI_CHASSIS_PWR_CYCLE		0x02
#define   IPMI_CHASSIS_HARD_RESET		0x03
#define   IPMI_CHASSIS_PULSE_DIAG		0x04
#define   IPMI_CHASSIS_SOFT_SHUTDOWN		0x05

/* 20.7. ACPI Power State Command */
#define   IPMI_PWR_SYS_S0_WORKING		0x00
#define   IPMI_PWR_SYS_S1			0x01
#define   IPMI_PWR_SYS_S2			0x02
#define   IPMI_PWR_SYS_S3_SUSPEND_TO_RAM	0x03
#define   IPMI_PWR_SYS_S4_SUSPEND_TO_DISK	0x04
#define   IPMI_PWR_SYS_S5_SOFT_OFF		0x05
#define   IPMI_PWR_SYS_SUSPEND			0x06
#define   IPMI_PWR_SYS_LEGACY_ON		0x20
#define   IPMI_PWR_SYS_LEGACY_OFF		0x21
#define   IPMI_PWR_SYS_UNKNOWN			0x2a
#define   IPMI_PWR_NOCHANGE                     0x7f

#define IPMI_CODE(netfn, cmd)		((netfn) << 8 | (cmd))
#define IPMI_CMD(code)			((code) & 0xff)
#define IPMI_NETFN(code)		((code) >> 8 & 0xff)

#define IPMI_NETFN_CHASSIS		0x00
#define IPMI_NETFN_STORAGE		0x0a
#define IPMI_NETFN_APP			0x06

#define IPMI_GET_SEL_INFO		IPMI_CODE(IPMI_NETFN_STORAGE, 0x40)
#define IPMI_GET_SEL_TIME		IPMI_CODE(IPMI_NETFN_STORAGE, 0x48)
#define IPMI_SET_SEL_TIME		IPMI_CODE(IPMI_NETFN_STORAGE, 0x49)
#define IPMI_CHASSIS_CONTROL		IPMI_CODE(IPMI_NETFN_CHASSIS, 0x02)
#define IPMI_SET_POWER_STATE		IPMI_CODE(IPMI_NETFN_APP, 0x06)
#define IPMI_GET_POWER_STATE		IPMI_CODE(IPMI_NETFN_APP, 0x07)

/*
 * IPMI response codes.
 */
#define IPMI_CC_NO_ERROR		0x00
#define IPMI_NODE_BUSY_ERR		0xc0
#define IPMI_INVALID_COMMAND_ERR	0xc1
#define IPMI_TIMEOUT_ERR		0xc3
#define IPMI_ERR_MSG_TRUNCATED		0xc6
#define IPMI_REQ_LEN_INVALID_ERR	0xc7
#define IPMI_REQ_LEN_EXCEEDED_ERR	0xc8
#define IPMI_NOT_IN_MY_STATE_ERR	0xd5	/* IPMI 2.0 */
#define IPMI_LOST_ARBITRATION_ERR	0x81
#define IPMI_BUS_ERR			0x82
#define IPMI_NAK_ON_WRITE_ERR		0x83
#define IPMI_ERR_UNSPECIFIED		0xff

#define IPMI_DEFAULT_INTERFACE		0

#define IPMI_MAX_REQ_SIZE		64
#define IPMI_MAX_RESP_SIZE		64

struct ipmi_backend;
struct ipmi_msg {
	/* Can be used by command implementations to track requests */
	struct list_node link;

	struct ipmi_backend *backend;
	uint8_t netfn;
	uint8_t cmd;
	uint8_t cc;

	/* Called when a response is received to the ipmi message */
	void (*complete)(struct ipmi_msg *);

	/* Called if non-NULL when the ipmi layer detects an error */
	void (*error)(struct ipmi_msg *);
	void *user_data;

	uint8_t req_size;
	uint8_t resp_size;
	uint8_t *data;
};

struct ipmi_backend {
	uint64_t opal_event_ipmi_recv;
	struct ipmi_msg *(*alloc_msg)(size_t, size_t);
	void (*free_msg)(struct ipmi_msg *);
	int (*queue_msg)(struct ipmi_msg *);
	int (*dequeue_msg)(struct ipmi_msg *);
};

extern struct ipmi_backend *ipmi_backend;

/* Initialise the IPMI interface */
void ipmi_init(void);

bool ipmi_present(void);

void ipmi_free_msg(struct ipmi_msg *msg);

struct ipmi_msg *ipmi_mkmsg_simple(uint32_t code, void *req_data, size_t req_size);
struct ipmi_msg *ipmi_mkmsg(int interface, uint32_t code,
			    void (*complete)(struct ipmi_msg *),
			    void *user_data, void *req_data, size_t req_size,
			    size_t resp_size);

/* Add an ipmi message to the queue */
int ipmi_queue_msg(struct ipmi_msg *msg);

/* Process a completed message */
void ipmi_cmd_done(uint8_t cmd, uint8_t netfn, uint8_t cc, struct ipmi_msg *msg);

/* 28.3 Chassis Control Command. Changes the power state of the P8. */
int ipmi_chassis_control(uint8_t request);

/* 20.7 ACPI Power State Command (without the ACPI part). Informative only,
 * use chassis control to perform power off and reboot. */
int ipmi_set_power_state(uint8_t system, uint8_t device);

/* Register a backend with the ipmi core. Currently we only support one. */
void ipmi_register_backend(struct ipmi_backend *backend);

/* Register rtc ipmi commands with as opal callbacks. */
void ipmi_rtc_init(void);

/* Register ipmi host interface access callbacks */
void ipmi_opal_init(void);

#endif
