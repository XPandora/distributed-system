#include "rte_common.h"
#include "rte_mbuf.h"
#include "rte_meter.h"
#include "rte_red.h"

#include "qos.h"

struct rte_meter_srtcm_params app_srtcm_params[APP_FLOWS_MAX];
struct rte_meter_srtcm app_flows[APP_FLOWS_MAX];

struct rte_red_config app_red_params[e_RTE_METER_COLORS];
struct rte_red app_red[APP_FLOWS_MAX][e_RTE_METER_COLORS];

uint64_t app_time_stamp;
unsigned app_queue_size[APP_FLOWS_MAX][e_RTE_METER_COLORS];

static void set_srtcm_params(uint32_t flow_id, uint64_t cir, uint64_t cbs, uint64_t ebs)
{
    app_srtcm_params[flow_id].cir = cir;
    app_srtcm_params[flow_id].cbs = cbs;
    app_srtcm_params[flow_id].ebs = ebs;
}

static void set_red_params(enum qos_color color,
                           const uint16_t wq_log2, const uint16_t min_th, const uint16_t max_th, const uint16_t maxp_inv)
{
    int ret;
    ret = rte_red_config_init(&app_red_params[color], wq_log2, min_th, max_th, maxp_inv);
    if (ret)
    {
        rte_panic("red config error!\n");
        return;
    }

    for (int i = 0; i < APP_FLOWS_MAX; i++)
    {
        ret = rte_red_rt_data_init(&app_red[i][color]);
        if (ret)
        {
            rte_panic("red rt data init error!\n");
            return;
        }
    }
}

static void red_queue_clear(uint64_t time)
{
    memset(app_queue_size, 0, sizeof(app_queue_size));
    for (int i = 0; i < APP_FLOWS_MAX; i++)
    {
        rte_red_mark_queue_empty(&app_red[i][GREEN], time);
        rte_red_mark_queue_empty(&app_red[i][YELLOW], time);
        rte_red_mark_queue_empty(&app_red[i][RED], time);
    }
}

/**
 * srTCM
 */
int qos_meter_init(void)
{
    int ret;
    /*
        set srtcm params
    */

    set_srtcm_params(0, 1760000000, 88000, 88000);
    set_srtcm_params(1, 880000000, 44000, 44000);
    set_srtcm_params(2, 440000000, 22000, 22000);
    set_srtcm_params(3, 220000000, 11000, 11000);

    for (int i = 0; i < APP_FLOWS_MAX; i++)
    {
        ret = rte_meter_srtcm_config(&app_flows[i], &app_srtcm_params[i]);
        if (ret)
        {
            rte_panic("srtcm config error!\n");
            return ret;
        }
    }
    return 0;
}

enum qos_color
qos_meter_run(uint32_t flow_id, uint32_t pkt_len, uint64_t time)
{
    return rte_meter_srtcm_color_blind_check(&app_flows[flow_id], time, pkt_len);
}

/**
 * WRED
 */

int qos_dropper_init(void)
{
    /* to do */
    set_red_params(GREEN, 9, 999, 1000, 10);
    set_red_params(YELLOW, 9, 999, 1000, 10);
    set_red_params(RED, 9, 0, 1, 10);
    memset(app_queue_size, 0, sizeof(app_queue_size));
    return 0;
}

int qos_dropper_run(uint32_t flow_id, enum qos_color color, uint64_t time)
{
    /* to do */
    int ret;

    if (app_time_stamp != time)
    {
        app_time_stamp = time;
        red_queue_clear(time);
    }

    ret = rte_red_enqueue(&app_red_params[color], &app_red[flow_id][color], app_queue_size[flow_id][color], time);
    if (!ret)
    {
        app_queue_size[flow_id][color]++;
    }

    return ret;
}