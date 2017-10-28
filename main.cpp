/*
 * tun.cpp
 *
 *  Created on: Oct 26, 2017
 *      Author: root
 */

#include "common.h"
#include "log.h"
#include "misc.h"
using namespace std;


int get_tun_fd(char * dev_name)
{
	int tun_fd=open("/dev/net/tun",O_RDWR);

	if(tun_fd <0)
	{
		mylog(log_fatal,"open /dev/net/tun failed");
		myexit(-1);
	}
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN|IFF_NO_PI;

	strncpy(ifr.ifr_name, dev_name, IFNAMSIZ);

	if(ioctl(tun_fd, TUNSETIFF, (void *)&ifr) != 0)
	{
		mylog(log_fatal,"open /dev/net/tun failed");
		myexit(-1);
	}
	return tun_fd;
}

int set_if(char *if_name,u32_t local_ip,u32_t remote_ip,int mtu)
{
	//printf("i m here1\n");
	struct ifreq ifr;
	struct sockaddr_in sai;
	memset(&ifr,0,sizeof(ifr));
	memset(&sai, 0, sizeof(struct sockaddr));

	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);

    sai.sin_family = AF_INET;
    sai.sin_port = 0;

    sai.sin_addr.s_addr = local_ip;
    memcpy(&ifr.ifr_addr,&sai, sizeof(struct sockaddr));
    assert(ioctl(sockfd, SIOCSIFADDR, &ifr)==0); //set source ip

    sai.sin_addr.s_addr = remote_ip;
    memcpy(&ifr.ifr_addr,&sai, sizeof(struct sockaddr));
    assert(ioctl(sockfd, SIOCSIFDSTADDR, &ifr)==0);//set dest ip

    ifr.ifr_mtu=mtu;
    assert(ioctl(sockfd, SIOCSIFMTU, &ifr)==0);//set mtu


    assert(ioctl(sockfd, SIOCGIFFLAGS, &ifr)==0);
   // ifr.ifr_flags |= ( IFF_UP|IFF_POINTOPOINT|IFF_RUNNING|IFF_NOARP|IFF_MULTICAST );
    ifr.ifr_flags = ( IFF_UP|IFF_POINTOPOINT|IFF_RUNNING|IFF_NOARP|IFF_MULTICAST );//set interface flags
    assert(ioctl(sockfd, SIOCSIFFLAGS, &ifr)==0);

    //printf("i m here2\n");
	return 0;
}

const char header_normal=1;
const char header_new_connect=2;
const char header_reject=3;

int put_header(char header,char * data,int &len)
{
	assert(len>=0);
	data[len]=header;
	len+=1;
	return 0;
}
int get_header(char &header,char * data,int &len)
{
	assert(len>=0);
	if(len<1) return -1;
	len-=1;
	header=data[len];

	return 0;
}
int from_normal_to_fec2(conn_info_t & conn_info,dest_t &dest,char * data,int len,char header)
{
	int  out_n;char **out_arr;int *out_len;my_time_t *out_delay;

	from_normal_to_fec(conn_info,data,len,out_n,out_arr,out_len,out_delay);

	for(int i=0;i<out_n;i++)
	{

		char tmp_buf[buf_len];
		int tmp_len=out_len[i];
		memcpy(tmp_buf,out_arr[i],out_len[i]);
		put_header(header,tmp_buf,tmp_len);
		delay_send(out_delay[i],dest,tmp_buf,tmp_len);//this is slow but safer.just use this one

		//put_header(header,out_arr[i],out_len[i]);//modify in place
		//delay_send(out_delay[i],dest,out_arr[i],out_len[i]);//warning this is currently okay,but if you modified fec encoder,you  may have to use the above code
	}
	return 0;
}

int from_fec_to_normal2(conn_info_t & conn_info,dest_t &dest,char * data,int len)
{
	int  out_n;char **out_arr;int *out_len;my_time_t *out_delay;

	from_fec_to_normal(conn_info,data,len,out_n,out_arr,out_len,out_delay);

	for(int i=0;i<out_n;i++)
	{
		delay_send(out_delay[i],dest,out_arr[i],out_len[i]);
	}

	return 0;
}

int tun_dev_client_event_loop()
{
	char data[buf_len];
	int len;
	int i,j,k,ret;
	int epoll_fd,tun_fd;

	int remote_fd;
	fd64_t remote_fd64;

	tun_fd=get_tun_fd(tun_dev);
	assert(tun_fd>0);

	assert(new_connected_socket(remote_fd,remote_ip_uint32,remote_port)==0);
	remote_fd64=fd_manager.create(remote_fd);

	assert(set_if(tun_dev,htonl((ntohl(sub_net_uint32)&0xFFFFFF00)|2),htonl((ntohl(sub_net_uint32)&0xFFFFFF00 )|1),g_fec_mtu)==0);

	epoll_fd = epoll_create1(0);
	assert(epoll_fd>0);

	const int max_events = 4096;
	struct epoll_event ev, events[max_events];
	if (epoll_fd < 0) {
		mylog(log_fatal,"epoll return %d\n", epoll_fd);
		myexit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64 = remote_fd64;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, remote_fd, &ev);
	if (ret!=0) {
		mylog(log_fatal,"add  remote_fd64 error\n");
		myexit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64 = tun_fd;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tun_fd, &ev);
	if (ret!=0) {
		mylog(log_fatal,"add  tun_fd error\n");
		myexit(-1);
	}


	ev.events = EPOLLIN;
	ev.data.u64 = delay_manager.get_timer_fd();

	mylog(log_debug,"delay_manager.get_timer_fd()=%d\n",delay_manager.get_timer_fd());
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, delay_manager.get_timer_fd(), &ev);
	if (ret!= 0) {
		mylog(log_fatal,"add delay_manager.get_timer_fd() error\n");
		myexit(-1);
	}


    conn_info_t *conn_info_p=new conn_info_t;
    conn_info_t &conn_info=*conn_info_p;  //huge size of conn_info,do not allocate on stack

	u64_t tmp_timer_fd64=conn_info.fec_encode_manager.get_timer_fd64();
	ev.events = EPOLLIN;
	ev.data.u64 = tmp_timer_fd64;

	mylog(log_debug,"conn_info.fec_encode_manager.get_timer_fd64()=%llu\n",conn_info.fec_encode_manager.get_timer_fd64());
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_manager.to_fd(tmp_timer_fd64), &ev);
	if (ret!= 0) {
		mylog(log_fatal,"add fec_encode_manager.get_timer_fd64() error\n");
		myexit(-1);
	}

	conn_info.timer.add_fd_to_epoll(epoll_fd);
	conn_info.timer.set_timer_repeat_us(timer_interval*1000);





	int fifo_fd=-1;

	if(fifo_file[0]!=0)
	{
		fifo_fd=create_fifo(fifo_file);
		ev.events = EPOLLIN;
		ev.data.u64 = fifo_fd;

		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fifo_fd, &ev);
		if (ret!= 0) {
			mylog(log_fatal,"add fifo_fd to epoll error %s\n",strerror(errno));
			myexit(-1);
		}
		mylog(log_info,"fifo_file=%s\n",fifo_file);
	}


	dest_t udp_dest;
	udp_dest.cook=1;
	udp_dest.type=type_fd64;
	udp_dest.inner.fd64=remote_fd64;

	dest_t tun_dest;
	tun_dest.type=type_write_fd;
	tun_dest.inner.fd=tun_fd;

	int got_feed_back=0;

	while(1)////////////////////////
	{

		if(about_to_exit) myexit(0);

		int nfds = epoll_wait(epoll_fd, events, max_events, 180 * 1000);
		if (nfds < 0) {  //allow zero
			if(errno==EINTR  )
			{
				mylog(log_info,"epoll interrupted by signal\n");
				myexit(0);
			}
			else
			{
				mylog(log_fatal,"epoll_wait return %d\n", nfds);
				myexit(-1);
			}
		}
		int idx;
		for (idx = 0; idx < nfds; ++idx)
		{
			if(events[idx].data.u64==(u64_t)conn_info.timer.get_timer_fd())
			{
				uint64_t value;
				read(conn_info.timer.get_timer_fd(), &value, 8);
				mylog(log_trace,"events[idx].data.u64==(u64_t)conn_info.timer.get_timer_fd()\n");
				conn_info.stat.report_as_client();
			}

			else if(events[idx].data.u64==conn_info.fec_encode_manager.get_timer_fd64())
			{
				fd64_t fd64=events[idx].data.u64;
				mylog(log_trace,"events[idx].data.u64 == conn_info.fec_encode_manager.get_timer_fd64()\n");

				uint64_t value;
				if(!fd_manager.exist(fd64))   //fd64 has been closed
				{
					mylog(log_trace,"!fd_manager.exist(fd64)");
					continue;
				}
				if((ret=read(fd_manager.to_fd(fd64), &value, 8))!=8)
				{
					mylog(log_trace,"(ret=read(fd_manager.to_fd(fd64), &value, 8))!=8,ret=%d\n",ret);
					continue;
				}
				if(value==0)
				{
					mylog(log_debug,"value==0\n");
					continue;
				}
				assert(value==1);

				char header=(got_feed_back==0?header_new_connect:header_normal);
				from_normal_to_fec2(conn_info,udp_dest,0,0,header);
			}
			else if(events[idx].data.u64==(u64_t)tun_fd)
			{
				len=read(tun_fd,data,max_data_len);

				if(len<0)
				{
					mylog(log_warn,"read from tun_fd return %d,errno=%s\n",len,strerror(errno));
					continue;
				}
				if(len>=20)
				{
					u32_t dest_ip=htonl(read_u32(data+16));
					if(  ( ntohl(sub_net_uint32)&0xFFFFFF00 ) !=  ( ntohl (dest_ip) &0xFFFFFF00) )
					{
						string sub=my_ntoa(dest_ip);
						string dst=my_ntoa( htonl( ntohl (dest_ip) &0xFFFFFF00)   );
						mylog(log_warn,"packet's dest ip [%s] not in subnet [%s],dropped\n", sub.c_str(), dst.c_str());
						continue;
					}
				}

				mylog(log_trace,"Received packet from tun,len: %d\n",len);

				char header=(got_feed_back==0?header_new_connect:header_normal);
				from_normal_to_fec2(conn_info,udp_dest,data,len,header);

			}
			else if(events[idx].data.u64==(u64_t)remote_fd64)
			{
				fd64_t fd64=events[idx].data.u64;
				int fd=fd_manager.to_fd(fd64);

				len=recv(fd,data,max_data_len,0);

				if(len<0)
				{
					mylog(log_warn,"recv return %d,errno=%s\n",len,strerror(errno));
					continue;
				}

				if(de_cook(data,len)<0)
				{
					mylog(log_warn,"de_cook(data,len)failed \n");
					continue;

				}

				char header=0;
				if(get_header(header,data,len)!=0)
				{
					mylog(log_warn,"get_header failed\n");
					continue;
				}

				if(header==header_reject)
				{
					mylog(log_fatal,"server restarted or switched to handle another client,exited\n");
					myexit(-1);
					continue;
				}
				else if(header==header_normal)
				{
					if(got_feed_back==0)
						mylog(log_info,"connection accepted by server\n");
					got_feed_back=1;
				}
				else
				{
					mylog(log_warn,"invalid header %d %d\n",int(header),len);
					continue;
				}

				mylog(log_trace,"Received packet from udp,len: %d\n",len);

				from_fec_to_normal2(conn_info,tun_dest,data,len);

			}
		    else if (events[idx].data.u64 == (u64_t)delay_manager.get_timer_fd())
		    {
				uint64_t value;
				read(delay_manager.get_timer_fd(), &value, 8);
				mylog(log_trace,"events[idx].data.u64 == (u64_t)delay_manager.get_timer_fd()\n");

			}
			else if (events[idx].data.u64 == (u64_t)fifo_fd)
			{
				char buf[buf_len];
				int len=read (fifo_fd, buf, sizeof (buf));
				if(len<0)
				{
					mylog(log_warn,"fifo read failed len=%d,errno=%s\n",len,strerror(errno));
					continue;
				}
				buf[len]=0;
				handle_command(buf);
			}
			else
			{
				assert(0==1);
			}
		}
		delay_manager.check();
	}


	return 0;
}

int tun_dev_server_event_loop()
{
	char data[buf_len];
	int len;
	int i,j,k,ret;
	int epoll_fd,tun_fd;
	int local_listen_fd;

	tun_fd=get_tun_fd(tun_dev);
	assert(tun_fd>0);

	assert(new_listen_socket(local_listen_fd,local_ip_uint32,local_port)==0);
	assert(set_if(tun_dev,htonl((ntohl(sub_net_uint32)&0xFFFFFF00)|1),htonl((ntohl(sub_net_uint32)&0xFFFFFF00 )|2),g_fec_mtu)==0);

	epoll_fd = epoll_create1(0);
	assert(epoll_fd>0);

	const int max_events = 4096;
	struct epoll_event ev, events[max_events];
	if (epoll_fd < 0) {
		mylog(log_fatal,"epoll return %d\n", epoll_fd);
		myexit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64 = local_listen_fd;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_listen_fd, &ev);
	if (ret!=0) {
		mylog(log_fatal,"add  udp_listen_fd error\n");
		myexit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64 = tun_fd;
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tun_fd, &ev);
	if (ret!=0) {
		mylog(log_fatal,"add  tun_fd error\n");
		myexit(-1);
	}

	ev.events = EPOLLIN;
	ev.data.u64 = delay_manager.get_timer_fd();

	mylog(log_debug,"delay_manager.get_timer_fd()=%d\n",delay_manager.get_timer_fd());
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, delay_manager.get_timer_fd(), &ev);
	if (ret!= 0) {
		mylog(log_fatal,"add delay_manager.get_timer_fd() error\n");
		myexit(-1);
	}


    conn_info_t *conn_info_p=new conn_info_t;
    conn_info_t &conn_info=*conn_info_p;  //huge size of conn_info,do not allocate on stack

	u64_t tmp_timer_fd64=conn_info.fec_encode_manager.get_timer_fd64();
	ev.events = EPOLLIN;
	ev.data.u64 = tmp_timer_fd64;

	mylog(log_debug,"conn_info.fec_encode_manager.get_timer_fd64()=%llu\n",conn_info.fec_encode_manager.get_timer_fd64());
	ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_manager.to_fd(tmp_timer_fd64), &ev);
	if (ret!= 0) {
		mylog(log_fatal,"add fec_encode_manager.get_timer_fd64() error\n");
		myexit(-1);
	}

	conn_info.timer.add_fd_to_epoll(epoll_fd);
	conn_info.timer.set_timer_repeat_us(timer_interval*1000);




	int fifo_fd=-1;

	if(fifo_file[0]!=0)
	{
		fifo_fd=create_fifo(fifo_file);
		ev.events = EPOLLIN;
		ev.data.u64 = fifo_fd;

		ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fifo_fd, &ev);
		if (ret!= 0) {
			mylog(log_fatal,"add fifo_fd to epoll error %s\n",strerror(errno));
			myexit(-1);
		}
		mylog(log_info,"fifo_file=%s\n",fifo_file);
	}

	dest_t udp_dest;
	udp_dest.cook=1;
	udp_dest.type=type_fd_ip_port;

	udp_dest.inner.fd_ip_port.fd=local_listen_fd;
	udp_dest.inner.fd_ip_port.ip_port.ip=0;
	udp_dest.inner.fd_ip_port.ip_port.port=0;

	dest_t tun_dest;
	tun_dest.type=type_write_fd;
	tun_dest.inner.fd=tun_fd;

	while(1)////////////////////////
	{

		if(about_to_exit) myexit(0);

		int nfds = epoll_wait(epoll_fd, events, max_events, 180 * 1000);
		if (nfds < 0) {  //allow zero
			if(errno==EINTR  )
			{
				mylog(log_info,"epoll interrupted by signal\n");
				myexit(0);
			}
			else
			{
				mylog(log_fatal,"epoll_wait return %d\n", nfds);
				myexit(-1);
			}
		}
		int idx;
		for (idx = 0; idx < nfds; ++idx)
		{
			if(events[idx].data.u64==(u64_t)conn_info.timer.get_timer_fd())
			{
				uint64_t value;
				read(conn_info.timer.get_timer_fd(), &value, 8);

				if(udp_dest.inner.fd64_ip_port.ip_port.to_u64()==0)
				{
					continue;
				}
				conn_info.stat.report_as_server(udp_dest.inner.fd_ip_port.ip_port);
			}
			else if(events[idx].data.u64==conn_info.fec_encode_manager.get_timer_fd64())
			{
				assert(udp_dest.inner.fd64_ip_port.ip_port.to_u64()!=0);
				mylog(log_trace,"events[idx].data.u64 == conn_info.fec_encode_manager.get_timer_fd64()\n");
				uint64_t fd64=events[idx].data.u64;
				//mylog(log_info,"timer!!!\n");
				uint64_t value;
				if(!fd_manager.exist(fd64))   //fd64 has been closed
				{
					mylog(log_trace,"!fd_manager.exist(fd64)");
					continue;
				}
				if((ret=read(fd_manager.to_fd(fd64), &value, 8))!=8)
				{
					mylog(log_trace,"(ret=read(fd_manager.to_fd(fd64), &value, 8))!=8,ret=%d\n",ret);
					continue;
				}
				if(value==0)
				{
					mylog(log_debug,"value==0\n");
					continue;
				}
				assert(value==1);

				from_normal_to_fec2(conn_info,udp_dest,0,0,header_normal);
			}
			else if(events[idx].data.u64==(u64_t)local_listen_fd)
			{
				struct sockaddr_in udp_new_addr_in={0};
				socklen_t udp_new_addr_len = sizeof(sockaddr_in);
				if ((len = recvfrom(local_listen_fd, data, max_data_len, 0,
						(struct sockaddr *) &udp_new_addr_in, &udp_new_addr_len)) < 0) {
					mylog(log_error,"recv_from error,this shouldnt happen,err=%s,but we can try to continue\n",strerror(errno));
					continue;
					//myexit(1);
				};

				if(de_cook(data,len)<0)
				{
					mylog(log_warn,"de_cook(data,len)failed \n");
					continue;

				}

				char header=0;
				if(get_header(header,data,len)!=0)
				{
					mylog(log_warn,"get_header failed\n");
					continue;
				}

				if((udp_dest.inner.fd_ip_port.ip_port.ip==udp_new_addr_in.sin_addr.s_addr) && (udp_dest.inner.fd_ip_port.ip_port.port==ntohs(udp_new_addr_in.sin_port)))
				{
					if(header!=header_new_connect&& header!=header_normal)
					{
						mylog(log_warn,"invalid header\n");
						continue;
					}
				}
				else
				{
					if(header==header_new_connect)
					{
						mylog(log_info,"new connection from %s:%d \n", inet_ntoa(udp_new_addr_in.sin_addr),
												ntohs(udp_new_addr_in.sin_port));
						udp_dest.inner.fd_ip_port.ip_port.ip=udp_new_addr_in.sin_addr.s_addr;
						udp_dest.inner.fd_ip_port.ip_port.port=ntohs(udp_new_addr_in.sin_port);
						conn_info.fec_decode_manager.clear();
						conn_info.fec_encode_manager.clear();
						memset(&conn_info.stat,0,sizeof(conn_info.stat));

					}
					else if(header==header_normal)
					{
						mylog(log_info,"rejected connection from %s:%d\n", inet_ntoa(udp_new_addr_in.sin_addr),ntohs(udp_new_addr_in.sin_port));


						len=1;
						data[0]=header_reject;
						do_cook(data,len);


						dest_t tmp_dest;
						tmp_dest.type=type_fd_ip_port;

						tmp_dest.inner.fd_ip_port.fd=local_listen_fd;
						tmp_dest.inner.fd_ip_port.ip_port.ip=udp_new_addr_in.sin_addr.s_addr;
						tmp_dest.inner.fd_ip_port.ip_port.port=ntohs(udp_new_addr_in.sin_port);

						delay_manager.add(0,tmp_dest,data,len);;
						continue;
					}
					else
					{
						mylog(log_warn,"invalid header\n");
					}
				}

				mylog(log_trace,"Received packet from %s:%d,len: %d\n", inet_ntoa(udp_new_addr_in.sin_addr),
						ntohs(udp_new_addr_in.sin_port),len);

				from_fec_to_normal2(conn_info,tun_dest,data,len);

			}
			else if(events[idx].data.u64==(u64_t)tun_fd)
			{
				len=read(tun_fd,data,max_data_len);
				if(len<0)
				{
					mylog(log_warn,"read from tun_fd return %d,errno=%s\n",len,strerror(errno));
					continue;
				}

				mylog(log_trace,"Received packet from tun,len: %d\n",len);

				if(udp_dest.inner.fd64_ip_port.ip_port.to_u64()==0)
				{
					mylog(log_debug,"received packet from tun,but there is no client yet,dropped packet\n");
					continue;
				}

				from_normal_to_fec2(conn_info,udp_dest,data,len,header_normal);

			}
		    else if (events[idx].data.u64 == (u64_t)delay_manager.get_timer_fd())
		    {
				uint64_t value;
				read(delay_manager.get_timer_fd(), &value, 8);
				mylog(log_trace,"events[idx].data.u64 == (u64_t)delay_manager.get_timer_fd()\n");
			}
			else if (events[idx].data.u64 == (u64_t)fifo_fd)
			{
				char buf[buf_len];
				int len=read (fifo_fd, buf, sizeof (buf));
				if(len<0)
				{
					mylog(log_warn,"fifo read failed len=%d,errno=%s\n",len,strerror(errno));
					continue;
				}
				buf[len]=0;
				handle_command(buf);
			}
			else
			{
				assert(0==1);
			}
		}
		delay_manager.check();
	}


	return 0;
}


static void print_help()
{
	char git_version_buf[100]={0};
	strncpy(git_version_buf,gitversion,10);

	printf("UDPspeeder V2\n");
	printf("git version: %s    ",git_version_buf);
	printf("build date: %s %s\n",__DATE__,__TIME__);
	printf("repository: https://github.com/wangyu-/UDPspeeder\n");
	printf("\n");
	printf("usage:\n");
	printf("    run as client: ./this_program -c -l local_listen_ip:local_port -r server_ip:server_port  [options]\n");
	printf("    run as server: ./this_program -s -l server_listen_ip:server_port -r remote_ip:remote_port  [options]\n");
	printf("\n");
	printf("common options, must be same on both sides:\n");
	printf("    -k,--key              <string>        key for simple xor encryption. if not set, xor is disabled\n");

	printf("main options:\n");
	printf("    -f,--fec              x:y             forward error correction, send y redundant packets for every x packets\n");
	printf("    --timeout             <number>        how long could a packet be held in queue before doing fec, unit: ms, default: 8ms\n");
	printf("    --mode                <number>        fec-mode,available values: 0, 1; 0 cost less bandwidth, 1 cost less latency(default)\n");
	printf("    --report              <number>        turn on send/recv report, and set a period for reporting, unit: s\n");

	printf("advanced options:\n");
	printf("    --mtu                 <number>        mtu. for mode 0, the program will split packet to segment smaller than mtu_value.\n");
	printf("                                          for mode 1, no packet will be split, the program just check if the mtu is exceed.\n");
	printf("                                          default value: 1250\n");
	printf("    -j,--jitter           <number>        simulated jitter. randomly delay first packet for 0~<number> ms, default value: 0.\n");
	printf("                                          do not use if you dont know what it means.\n");
	printf("    -i,--interval         <number>        scatter each fec group to a interval of <number> ms, to protect burst packet loss.\n");
	printf("                                          default value: 0. do not use if you dont know what it means.\n");
	printf("    --random-drop         <number>        simulate packet loss, unit: 0.01%%. default value: 0\n");
	printf("    --disable-obscure     <number>        disable obscure, to save a bit bandwidth and cpu\n");
//	printf("    --disable-xor         <number>        disable xor\n");

	printf("developer options:\n");
	printf("    --fifo                <string>        use a fifo(named pipe) for sending commands to the running program, so that you\n");
	printf("                                          can change fec encode parameters dynamically, check readme.md in repository for\n");
	printf("                                          supported commands.\n");
	printf("    -j ,--jitter          jmin:jmax       similiar to -j above, but create jitter randomly between jmin and jmax\n");
	printf("    -i,--interval         imin:imax       similiar to -i above, but scatter randomly between imin and imax\n");
    printf("    -q,--queue-len        <number>        max fec queue len, only for mode 0\n");
    printf("    --decode-buf          <number>        size of buffer of fec decoder,u nit: packet, default: 2000\n");
    printf("    --fix-latency         <number>        try to stabilize latency, only for mode 0\n");
    printf("    --delay-capacity      <number>        max number of delayed packets\n");
	printf("    --disable-fec         <number>        completely disable fec, turn the program into a normal udp tunnel\n");
	printf("    --sock-buf            <number>        buf size for socket, >=10 and <=10240, unit: kbyte, default: 1024\n");
	printf("log and help options:\n");
	printf("    --log-level           <number>        0: never    1: fatal   2: error   3: warn \n");
	printf("                                          4: info (default)      5: debug   6: trace\n");
	printf("    --log-position                        enable file name, function name, line number in log\n");
	printf("    --disable-color                       disable log color\n");
	printf("    -h,--help                             print this help message\n");

	//printf("common options,these options must be same on both side\n");
}


int main(int argc, char *argv[])
{
	//working_mode=tunnel_mode;

	assert(sizeof(u64_t)==8);
	assert(sizeof(i64_t)==8);
	assert(sizeof(u32_t)==4);
	assert(sizeof(i32_t)==4);
	assert(sizeof(u16_t)==2);
	assert(sizeof(i16_t)==2);
	dup2(1, 2);		//redirect stderr to stdout
	int i, j, k;
	if (argc == 1)
	{
		print_help();
		myexit( -1);
	}
	for (i = 0; i < argc; i++)
	{
		if(strcmp(argv[i],"-h")==0||strcmp(argv[i],"--help")==0)
		{
			print_help();
			myexit(0);
		}
	}

	process_arg(argc,argv);

	delay_manager.set_capacity(delay_capacity);
	local_ip_uint32=inet_addr(local_ip);
	remote_ip_uint32=inet_addr(remote_ip);
	sub_net_uint32=inet_addr(sub_net);

	if(strlen(tun_dev)==0)
	{
		sprintf(tun_dev,"tun%u",get_true_random_number()%1000);
	}

	if(client_or_server==client_mode)
	{
		tun_dev_client_event_loop();
	}
	else
	{
		tun_dev_server_event_loop();
	}

	return 0;
}
