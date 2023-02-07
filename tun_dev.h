/*
 * tun.h
 *
 *  Created on: Oct 26, 2017
 *      Author: root
 */

#ifndef TUN_DEV_H_
#define TUN_DEV_H_

#include "common.h"
#include "log.h"
#include "misc.h"

#include <netinet/tcp.h>  //Provides declarations for tcp header
#include <netinet/udp.h>
#include <netinet/ip.h>  //Provides declarations for ip header
#include <netinet/if_ether.h>

#include <linux/if.h>
#include <linux/if_tun.h>

extern my_time_t last_keep_alive_time;

const int keep_alive_interval = 3000;  // 3000ms

const char header_normal = 1;
const char header_new_connect = 2;
const char header_reject = 3;
const char header_keep_alive = 4;

int set_tun(char *if_name, u32_t local_ip, u32_t remote_ip, int mtu);

int get_tun_fd(char *dev_name);

int put_header(char header, char *data, int &len);

int get_header(char &header, char *data, int &len);
int from_normal_to_fec2(conn_info_t &conn_info, dest_t &dest, char *data, int len, char header);

int from_fec_to_normal2(conn_info_t &conn_info, dest_t &dest, char *data, int len);
int do_mssfix(char *s, int len);
int do_keep_alive(dest_t &dest);

int tun_dev_client_event_loop();

int tun_dev_server_event_loop();

#endif /* TUN_H_ */
