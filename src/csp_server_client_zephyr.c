#include <csp/csp.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/sensor.h>

void server(void);
void client(void);

#define ROUTER_STACK_SIZE 800
#define SERVER_STACK_SIZE 800
#define CLIENT_STACK_SIZE 400
#define ROUTER_PRIO 0
#define SERVER_PRIO 0
#define CLIENT_PRIO 0

/* Server port, the port the server listens on for incoming connections from the client. */
#define MY_SERVER_PORT		10
#define CPU_TEMPERATURE_PORT		11

#define CPU_TEMP_NODE DT_ALIAS(cputemp)

static const struct device *const cpu_temp = DEVICE_DT_GET(CPU_TEMP_NODE);

static unsigned int server_received = 0;

void server(void)
{
    int ret;
    struct sensor_value val;

    csp_print("Server task started\n");

    /*** CPU temperature sensor ***/
    if (!device_is_ready(cpu_temp))
    {
        printk("sensor: device %s not ready.\n", cpu_temp->name);
        return;
    }

    /*** ***/
    /* Create socket with no specific socket options, e.g. accepts CRC32, HMAC, etc. if enabled during compilation */
    csp_socket_t sock = {0};

    /* Bind socket to all ports, e.g. all incoming connections will be handled here */
    csp_bind(&sock, CSP_ANY);

    /* Create a backlog of 10 connections, i.e. up to 10 new connections can be queued */
    csp_listen(&sock, 10);

    /* Wait for connections and then process packets on the connection */
    while (1) {

        /* Wait for a new connection, 10000 mS timeout */
        csp_conn_t *conn;
        if ((conn = csp_accept(&sock, 10000)) == NULL) {
            /* timeout */
            continue;
        }

        /* Read packets on connection, timout is 100 mS */
        csp_packet_t *packet;
        while ((packet = csp_read(conn, 50)) != NULL) {
            switch (csp_conn_dport(conn)) {
            case MY_SERVER_PORT:
                /* Process packet here */
                csp_print("Packet received on MY_SERVER_PORT: %s\n", (char *) packet->data);
                csp_buffer_free(packet);
                ++server_received;
                break;

            case CPU_TEMPERATURE_PORT:
                ret = sensor_sample_fetch(cpu_temp);
                if (ret)
                {
                    printk("Failed to fetch sample (%d)\n", ret);
                }

                ret = sensor_channel_get(cpu_temp, SENSOR_CHAN_DIE_TEMP, &val);
                if (ret)
                {
                    printk("Failed to get data (%d)\n", ret);
                }
                
                printk("CPU Die temperature[%s] request: %.1f °C (%d %d)\n", cpu_temp->name, sensor_value_to_double(&val), val.val1, val.val2);

                memcpy(packet->data, &val.val1, sizeof(val.val1));
                memcpy(packet->data+sizeof(val.val1), &val.val2, sizeof(val.val2));
                packet->length = sizeof(val.val1)+sizeof(val.val2);

                csp_sendto_reply(packet, packet, CSP_O_SAME);
                break;

            default:
                /* Call the default CSP service handler, handle pings, buffer use, etc. */
                csp_service_handler(packet);
                break;
            }
        }

        /* Close current connection */
        csp_close(conn);

    }

    return;

}

static void * router_task(void * param) {

    /* Here there be routing */
    while (1) {
        csp_route_work();
        k_msleep(1);
    }

    return NULL;
}

static void * server_task(void * p1, void * p2, void * p3) {

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    server();

    return NULL;
}

static void * client_task(void * p1, void * p2, void * p3) {

    ARG_UNUSED(p1);
    ARG_UNUSED(p2);
    ARG_UNUSED(p3);

    // client();

    return NULL;
}

K_THREAD_DEFINE(router_id, ROUTER_STACK_SIZE,
                router_task, NULL, NULL, NULL,
                ROUTER_PRIO, 0, K_TICKS_FOREVER);
K_THREAD_DEFINE(server_id, SERVER_STACK_SIZE,
                server_task, NULL, NULL, NULL,
                SERVER_PRIO, 0, K_TICKS_FOREVER);
K_THREAD_DEFINE(client_id, CLIENT_STACK_SIZE,
                client_task, NULL, NULL, NULL,
                CLIENT_PRIO, 0, K_TICKS_FOREVER);

void router_start(void) {
    k_thread_start(router_id);
}

void server_start(void) {
    k_thread_start(server_id);
}

void client_start(void) {
    k_thread_start(client_id);
}
