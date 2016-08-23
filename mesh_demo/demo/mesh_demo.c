/******************************************************************************
 * Copyright 2015-2016 Espressif Systems
 *
 * FileName: mesh_demo.c
 *
 * Description: mesh demo
 *
 * Modification history:
 *     2015/12/4, v1.0 create this file.
*******************************************************************************/
#include "mem.h"
#include "mesh.h"
#include "osapi.h"
#include "mesh_parser.h"
#include "esp_touch.h"
#include "user_main.h"

#include "ets_sys.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "c_types.h"
#include "gpio.h"
#include "spi_flash.h"
#include "smartconfig.h"
#include "ip_addr.h"
#include "user_mqtt.h"
#include "gpio16.h"
#include "mqtt.h"
#include "debug.h"
#include "V9881.h"
#include "tcpclient.h"
#include "http_post.h"
#include "user_key.h"
#include "user_time.h"
#include "uart.h"

#define MESH_DEMO_PRINT  ets_printf
#define MESH_DEMO_STRLEN ets_strlen
#define MESH_DEMO_MEMCPY ets_memcpy
#define MESH_DEMO_MEMSET ets_memset
#define MESH_DEMO_FREE   os_free
#define MESH_DEMO_ZALLOC os_zalloc
#define MESH_DEMO_MALLOC os_malloc

static esp_tcp ser_tcp;
struct espconn g_ser_conn;

os_timer_t tcp_timer;//����ת���綨ʱ��
os_timer_t udp_timer;//UDP�㲥��ʱ��

MQTT_Client *mqttClient;
V9881_Data *v9881_data;
u32 cpu_id;
u8 mqtt_id[11];
u8 mqtt_topic_send[12];
u8 temp[256];


void  user_init(void);
void ICACHE_FLASH_ATTR esp_mesh_demo_test();
void ICACHE_FLASH_ATTR mesh_enable_cb(int8_t res);
void ICACHE_FLASH_ATTR esp_mesh_demo_con_cb(void *);
void ICACHE_FLASH_ATTR esp_recv_entrance(void *, char *, uint16_t);


void user_rf_pre_init(void){}
void user_pre_init(void){}

void ICACHE_FLASH_ATTR esp_recv_entrance(void *arg, char *pdata, uint16_t len)
{
    uint8_t *src = NULL, *dst = NULL;
    uint8_t *resp = "{\"rsp_key\":\"rsp_key_value\"}";
    struct mesh_header_format *header = (struct mesh_header_format *)pdata;

    MESH_DEMO_PRINT("recv entrance\n");
    
    if (!pdata)
        return;

    /* 
     * process packet
     * call packet_parser(...)
     * general packet_parser demo
     */

    mesh_packet_parser(arg, pdata, len);

    /*
     * then build response packet,
     * and give resonse to controller
     * the following code is response demo
     */
#if 0
    if (!espconn_mesh_get_src_addr(header, &src) || !espconn_mesh_get_dst_addr(header, &dst)) {
        MESH_DEMO_PRINT("get addr fail\n");
        return;
    }

    header = (struct mesh_header_format *)espconn_mesh_create_packet(
                            src,   // destiny address
                            dst,   // source address
                            false, // not p2p packet
                            true,  // piggyback congest request
                            M_PROTO_JSON,  // packe with JSON format
                            MESH_DEMO_STRLEN(resp),  // data length
                            false, // no option
                            0,     // option len
                            false, // no frag
                            0,     // frag type, this packet doesn't use frag
                            false, // more frag
                            0,     // frag index
                            0);    // frag length
    if (!header) {
        MESH_DEMO_PRINT("create packet fail\n");
        return;
    }

    if (!espconn_mesh_set_usr_data(header, resp, MESH_DEMO_STRLEN(resp))) {
        MESH_DEMO_PRINT("set user data fail\n");
        MESH_DEMO_FREE(header);
        return;
    }

    if (espconn_mesh_sent(&g_ser_conn, (uint8_t *)header, header->len)) {
        MESH_DEMO_PRINT("mesh is busy\n");
        MESH_DEMO_FREE(header);
        return;
    }

    MESH_DEMO_FREE(header);
#endif
}

void ICACHE_FLASH_ATTR esp_mesh_demo_con_cb(void *arg)
{
    static os_timer_t tst_timer;
    struct espconn *server = (struct espconn *)arg;

    MESH_DEMO_PRINT("esp_mesh_demo_con_cb\n");

    if (server != &g_ser_conn) {
        MESH_DEMO_PRINT("con_cb, para err\n");
        return;
    }

    os_timer_disarm(&tst_timer);
    os_timer_setfn(&tst_timer, (os_timer_func_t *)esp_mesh_demo_test, NULL);
    os_timer_arm(&tst_timer, 7000, true);
}

void ICACHE_FLASH_ATTR mesh_enable_cb(int8_t res)
{
	MESH_DEMO_PRINT("mesh_enable_cb\n");

	LED_ON(LED1);
	FunMqttStart(mqttClient, MQTT_HOST);
	user_check_ip();
	user_time_init();

    if (res == MESH_OP_FAILURE) {
        MESH_DEMO_PRINT("enable mesh fail, re-enable\n");
        espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
        return;
    }

    if (espconn_mesh_get_usr_context() &&
        espconn_mesh_is_root() &&
        res == MESH_LOCAL_SUC)
        goto TEST_SCENARIO;

    /*
     * try to estable user virtual connect
     * user can to use the virtual connect to sent packet to any node, server or mobile.
     * if you want to sent packet to one node in mesh, please build p2p packet
     * if you want to sent packet to server/mobile, please build normal packet (uincast packet)
     * if you want to sent bcast/mcast packet, please build bcast/mcast packet
     */
    MESH_DEMO_MEMSET(&g_ser_conn, 0 ,sizeof(g_ser_conn));
    MESH_DEMO_MEMSET(&ser_tcp, 0, sizeof(ser_tcp));

    MESH_DEMO_MEMCPY(ser_tcp.remote_ip, server_ip, sizeof(server_ip));
    ser_tcp.remote_port = server_port;
    ser_tcp.local_port = espconn_port();
    g_ser_conn.proto.tcp = &ser_tcp;

    if (espconn_regist_connectcb(&g_ser_conn, esp_mesh_demo_con_cb)) {
        MESH_DEMO_PRINT("regist con_cb err\n");
        espconn_mesh_disable(NULL);
        return;
    }

    if (espconn_regist_recvcb(&g_ser_conn, esp_recv_entrance)) {
        MESH_DEMO_PRINT("regist recv_cb err\n");
        espconn_mesh_disable(NULL);
        return;
    }

    /*
     * regist the other callback
     * sent_cb, reconnect_cb, disconnect_cb
     * if you donn't need the above cb, you donn't need to register them.
     */

   /* if (espconn_mesh_connect(&g_ser_conn)) {
        MESH_DEMO_PRINT("connect err\n");
        if (espconn_mesh_is_root())
            espconn_mesh_enable(mesh_enable_cb, MESH_LOCAL);
        else
            espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
        return;
    }*/

TEST_SCENARIO:
    //mesh_device_list_init();
    mesh_topo_test_init();
    //mesh_json_mcast_test_init();
    //mesh_json_bcast_test_init();
    //mesh_json_p2p_test_init();
}

void ICACHE_FLASH_ATTR esp_mesh_demo_test()
{
    uint8_t src[6];
    uint8_t dst[6];
    struct mesh_header_format *header = NULL;

    /*
     * this is ucast test case
     */

    /*
     * the mesh data can be any content
     * it can be string(json/http), or binary(MQTT).
     */
    char *tst_data = "{\"req_key\":\"req_key_val\"}\r\n";
    // uint8_t tst_data[] = {'a', 'b', 'c', 'd'};}
    // uint8_t tst_data[] = {0x01, 0x02, 0x03, 0x04, 0x00};

    MESH_DEMO_PRINT("free heap:%u\n", system_get_free_heap_size());

    if (!wifi_get_macaddr(STATION_IF, src)) {
        MESH_DEMO_PRINT("get sta mac fail\n");
        return;
    }
    MESH_DEMO_MEMCPY(dst, server_ip, sizeof(server_ip));
    MESH_DEMO_MEMCPY(dst + sizeof(server_ip), &server_port, sizeof(server_port));

    header = (struct mesh_header_format *)espconn_mesh_create_packet(
                            dst,   // destiny address
                            src,   // source address
                            false, // not p2p packet
                            true,  // piggyback congest request
                            M_PROTO_JSON,  // packe with JSON format
                            MESH_DEMO_STRLEN(tst_data),  // data length
                            false, // no option
                            0,     // option len
                            false, // no frag
                            0,     // frag type, this packet doesn't use frag
                            false, // more frag
                            0,     // frag index
                            0);    // frag length
    if (!header) {
        MESH_DEMO_PRINT("create packet fail\n");
        return;
    }

    if (!espconn_mesh_set_usr_data(header, tst_data, MESH_DEMO_STRLEN(tst_data))) {
        MESH_DEMO_PRINT("set user data fail\n");
        MESH_DEMO_FREE(header);
        return;
    }

    if (espconn_mesh_sent(&g_ser_conn, (uint8_t *)header, header->len)) {
        MESH_DEMO_PRINT("mesh is busy\n");
        MESH_DEMO_FREE(header);
        /*
         * if fail, we re-connect mesh
         */
        espconn_mesh_connect(&g_ser_conn);
        return;
    }

    MESH_DEMO_FREE(header);
}

void ICACHE_FLASH_ATTR esp_mesh_new_child_notify(void *mac)
{
    if (!mac)
        return;
    MESH_DEMO_PRINT("new child node:" MACSTR "\n", MAC2STR(((uint8_t *)mac)));
}

bool ICACHE_FLASH_ATTR esp_mesh_demo_init()
{
    // print version of mesh
    espconn_mesh_print_ver();

    /*
     * set the AP password of mesh node
     */
    if (!espconn_mesh_encrypt_init(MESH_AUTH, MESH_PASSWD, MESH_DEMO_STRLEN(MESH_PASSWD))) {
        MESH_DEMO_PRINT("set pw fail\n");
        return false;
    }

    /*
     * if you want set max_hop > 4
     * please make the heap is avaliable
     * mac_route_table_size = (4^max_hop - 1)/3 * 6
     */
    if (!espconn_mesh_set_max_hops(MESH_MAX_HOP)) {
        MESH_DEMO_PRINT("fail, max_hop:%d\n", espconn_mesh_get_max_hops());
        return false;
    }

    /*
     * mesh_ssid_prefix
     * mesh_group_id and mesh_ssid_prefix represent mesh network
     */
    if (!espconn_mesh_set_ssid_prefix(MESH_SSID_PREFIX, MESH_DEMO_STRLEN(MESH_SSID_PREFIX))) {
        MESH_DEMO_PRINT("set prefix fail\n");
        return false;
    }

    /*
     * mesh_group_id
     * mesh_group_id and mesh_ssid_prefix represent mesh network
     */
    if (!espconn_mesh_group_id_init((uint8_t *)MESH_GROUP_ID, sizeof(MESH_GROUP_ID))) {
        MESH_DEMO_PRINT("set grp id fail\n");
        return false;
    }

    /*
     * set cloud server ip and port for mesh node
     */
    if (!espconn_mesh_server_init((struct ip_addr *)server_ip, server_port)) {
        MESH_DEMO_PRINT("server_init fail\n");
        return false;
    }

    /*
     * when new child joins current AP, system will call the callback
     */
    espconn_mesh_regist_usr_cb(esp_mesh_new_child_notify);

    return true;
}

/******************************************************************************
 * FunctionName : router_init
 * Description  : Initialize router related settings & Connecting router
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static bool ICACHE_FLASH_ATTR router_init()
{
    struct station_config config;
    MESH_DEMO_MEMSET(&config, 0, sizeof(config));
    espconn_mesh_get_router(&config);
    if (config.ssid[0] == 0xff ||
        config.ssid[0] == 0x00) {
        /*
         * please change MESH_ROUTER_SSID and MESH_ROUTER_PASSWD according to your router
         */
        MESH_DEMO_MEMSET(&config, 0, sizeof(config));
        MESH_DEMO_MEMCPY(config.ssid, MESH_ROUTER_SSID, MESH_DEMO_STRLEN(MESH_ROUTER_SSID));
        MESH_DEMO_MEMCPY(config.password, MESH_ROUTER_PASSWD, MESH_DEMO_STRLEN(MESH_ROUTER_PASSWD));
        /*
         * if you use router with hide ssid, you MUST set bssid in config,
         * otherwise, node will fail to connect router.
         *
         * if you use normal router, please pay no attention to the bssid,
         * and you don't need to modify the bssid, mesh will ignore the bssid.
         */
        config.bssid_set = 1;
        MESH_DEMO_MEMCPY(config.bssid, MESH_ROUTER_BSSID, sizeof(config.bssid));
    }

    /*
     * use espconn_mesh_set_router to set router for mesh node
     */
    if (!espconn_mesh_set_router(&config)) {
        MESH_DEMO_PRINT("set_router fail\n");
		
        return false;
    }

    /*
     * use esp-touch(smart configure) to sent information about router AP to mesh node
     */
    //esptouch_init();

    MESH_DEMO_PRINT("flush ssid:%s pwd:%s\n", config.ssid, config.password);
	MESH_DEMO_PRINT("************espconn_mesh_set_router***********\n");

	
    return true;
}

/******************************************************************************
 * FunctionName : wait_esptouch_over
 * Description  : wait for esptouch to run over and then enable mesh
 * Parameters   :
 * Returns      : none
*******************************************************************************/
static void ICACHE_FLASH_ATTR wait_esptouch_over(os_timer_t *timer)
{
    //if (esptouch_is_running()) return;     ����Ҫ�õ�esptouch

    if (!timer) return;
    os_timer_disarm(timer);
    os_free(timer);

    /*
     * enable mesh
     * after enable mesh, you should wait for the mesh_enable_cb to be triggered.
     */

    espconn_mesh_enable(mesh_enable_cb, MESH_ONLINE);
	os_printf("************ into %s ************\n", __func__);
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
    /*
     * set uart baut ratio
     */
	uart_div_modify(0, UART_CLK_FREQ / UART_BAUT_RATIO);
	uart_div_modify(1, UART_CLK_FREQ / UART_BAUT_RATIO);
	//uart_init_2(9600, 115200);//���ô��ڲ�����

	gpio16_output_conf();
	gpio16_output_set(1);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0);
	wifi_set_event_handler_cb(wifi_handle_event_cb);
	user_key_start();
	mqttClient = (MQTT_Client *)os_zalloc(sizeof(MQTT_Client));
	v9881_data = (V9881_Data *)os_zalloc(sizeof(V9881_Data));
	v9881_data->jsonString = (u8 *)os_zalloc(128);
	os_printf("User app compile time=%s\n", __TIME__);
	cpu_id = system_get_chip_id();
	os_bzero(temp, 12);
	os_sprintf(temp, "%d", cpu_id);
	os_memset(mqtt_id, 0x30, 10);
	mqtt_id[10] = 0;
	os_sprintf(mqtt_id + (10 - strlen(temp)), "%d", cpu_id);
	os_bzero(mqtt_topic_send, 12);
	os_sprintf(mqtt_topic_send, "%s_", mqtt_id);
	os_printf("CPU_ID: %s, SendTopic:%s\r\n", mqtt_id, mqtt_topic_send);
	

	//wifi_set_event_handler_cb(wifi_handle_event_cb);
	if (!router_init()) {
        return;
    }
    if (!esp_mesh_demo_init())
        return;
    user_devicefind_init();
		
    /*
     * while system runs smartconfig (STA mode), mesh (STA + SoftAp mode) must not been enabled,
     * So wait for esptouch to run over and then execute espconn_mesh_enable
     */
    os_timer_t *wait_timer = (os_timer_t *)os_zalloc(sizeof(os_timer_t));
    if (NULL == wait_timer) return;
    os_timer_disarm(wait_timer);
    os_timer_setfn(wait_timer, (os_timer_func_t *)wait_esptouch_over, wait_timer);
    os_timer_arm(wait_timer, 1000, true);

	os_timer_disarm(&tcp_timer);
	os_timer_setfn(&tcp_timer, (os_timer_func_t *)FuncSer2Net, NULL);//����ת��������
	os_timer_disarm(&udp_timer);
	os_timer_setfn(&udp_timer, (os_timer_func_t *)UdpFunc, NULL);//UDP�㲥����
	os_timer_arm(&udp_timer, 1000, 1);
	
}
 