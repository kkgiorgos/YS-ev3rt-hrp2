#include <kernel.h>
#include "common/module_common.h"
#include "target_config.h"
#include "platform_interface_layer.h"
#include "csl.h"
#include "target_timer.h"
#include "syssvc/syslog.h"
#include "syssvc/banner.h"
#include "target_serial.h"
#include "target_serial_dbsio.h"
#include "syssvc/serial.h"
#include "syssvc/logtask.h"
#include "api.cfg.h"
#include "app.h"

ID _module_id_APP_INIT_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_APP_INIT_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_MAIN_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_MAIN_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_OPEN_GRABBER_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_OPEN_GRABBER_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_INIT_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_INIT_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_CLOSE_RAMP_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_CLOSE_RAMP_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_WATER_GRABBER_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_WATER_GRABBER_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_PICK_BLOCK_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_PICK_BLOCK_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_EMPTY_WATER_RAMP_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_EMPTY_WATER_RAMP_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_BASKET_SCAN_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_BASKET_SCAN_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_ROOM_TASK_SCAN_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_ROOM_TASK_SCAN_TASK[COUNT_STK_T(STACK_SIZE)];

ID _module_id_END_TASK __attribute__((section (".module.text")));
static STK_T _module_ustack_END_TASK[COUNT_STK_T(STACK_SIZE)];

static const T_CTSK _module_ctsk_tab[11] = {
	{ TA_ACT, 0, _app_init_task, TPRI_APP_INIT_TASK, ROUND_STK_T(STACK_SIZE), _module_ustack_APP_INIT_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_ACT, 0, main_task, TMIN_APP_TPRI + 1, ROUND_STK_T(STACK_SIZE), _module_ustack_MAIN_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, open_grabber_task, HIGH_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_OPEN_GRABBER_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, init_task, LOW_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_INIT_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, close_ramp_task, LOW_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_CLOSE_RAMP_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, water_grabber_task, HIGH_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_WATER_GRABBER_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, pick_block_task, HIGH_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_PICK_BLOCK_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, empty_water_ramp_task, LOW_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_EMPTY_WATER_RAMP_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, basket_scan_task, HIGH_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_BASKET_SCAN_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, room_task_scan_task, HIGH_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_ROOM_TASK_SCAN_TASK, DEFAULT_SSTKSZ, NULL },
	{ TA_NULL, 0, end_task, LOW_PRIORITY, ROUND_STK_T(STACK_SIZE), _module_ustack_END_TASK, DEFAULT_SSTKSZ, NULL },
};

const SIZE _module_cfg_entry_num = 11;

const MOD_CFG_ENTRY _module_cfg_tab[11] = {
	{ TSFN_CRE_TSK, &_module_ctsk_tab[0], &_module_id_APP_INIT_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[1], &_module_id_MAIN_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[2], &_module_id_OPEN_GRABBER_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[3], &_module_id_INIT_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[4], &_module_id_CLOSE_RAMP_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[5], &_module_id_WATER_GRABBER_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[6], &_module_id_PICK_BLOCK_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[7], &_module_id_EMPTY_WATER_RAMP_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[8], &_module_id_BASKET_SCAN_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[9], &_module_id_ROOM_TASK_SCAN_TASK },
	{ TSFN_CRE_TSK, &_module_ctsk_tab[10], &_module_id_END_TASK },
};

const uint32_t _module_pil_version = PIL_VERSION;
