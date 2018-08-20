#include "tun_dev.h"

static dest_t udp_dest;
static dest_t tun_dest;

static void local_listen_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char data[buf_len];
	int len;

	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	int local_listen_fd=watcher->fd;

	//struct sockaddr_in udp_new_addr_in={0};
	//socklen_t udp_new_addr_len = sizeof(sockaddr_in);
	address_t::storage_t udp_new_addr_in={0};
	socklen_t udp_new_addr_len = sizeof(address_t::storage_t);

	if ((len = recvfrom(local_listen_fd, data, max_data_len+1, 0,
			(struct sockaddr *) &udp_new_addr_in, &udp_new_addr_len)) < 0) {
		mylog(log_error,"recv_from error,this shouldnt happen,err=%s,but we can try to continue\n",strerror(errno));
		return;
		//myexit(1);
	};

	address_t new_addr;
	new_addr.from_sockaddr((struct sockaddr *) &udp_new_addr_in,udp_new_addr_len);

	if(len==max_data_len+1)
	{
		mylog(log_warn,"huge packet, data_len > %d,dropped\n",max_data_len);
		return;
	}

	if(de_cook(data,len)<0)
	{
		mylog(log_warn,"de_cook(data,len)failed \n");
		return;

	}

	char header=0;
	if(get_header(header,data,len)!=0)
	{
		mylog(log_warn,"get_header() failed\n");
		return;
	}

	if(udp_dest.inner.fd_addr.addr==new_addr)
	{
		if(header==header_keep_alive)
		{
			mylog(log_trace,"got keep_alive packet\n");
			return;
		}

		if(header!=header_new_connect&& header!=header_normal)
		{
			mylog(log_warn,"invalid header\n");
			return;
		}
	}
	else
	{
		if(header==header_keep_alive)
		{
			mylog(log_debug,"got keep_alive packet from unexpected client\n");
			return;
		}

		if(header==header_new_connect)
		{
			mylog(log_info,"new connection from %s\n", new_addr.get_str());
			udp_dest.inner.fd_addr.addr=new_addr;
			//udp_dest.inner.fd_ip_port.ip_port.ip=udp_new_addr_in.sin_addr.s_addr;
			//udp_dest.inner.fd_ip_port.ip_port.port=ntohs(udp_new_addr_in.sin_port);
			conn_info.fec_decode_manager.clear();
			conn_info.fec_encode_manager.clear_data();
			memset(&conn_info.stat,0,sizeof(conn_info.stat));

		}
		else if(header==header_normal)
		{
			mylog(log_debug,"rejected connection from %s\n", new_addr.get_str());


			len=1;
			data[0]=header_reject;
			do_cook(data,len);


			dest_t tmp_dest;
			tmp_dest.type=type_fd_addr;

			tmp_dest.inner.fd_addr.fd=local_listen_fd;
			tmp_dest.inner.fd_addr.addr=new_addr;

			delay_manager.add(0,tmp_dest,data,len);;
			return;
		}
		else
		{
			mylog(log_warn,"invalid header\n");
		}
	}

	mylog(log_trace,"Received packet from %s,len: %d\n", new_addr.get_str(),len);

	from_fec_to_normal2(conn_info,tun_dest,data,len);

}
static void tun_fd_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	char data[buf_len];
	int len;

	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);
	int tun_fd=watcher->fd;

	len=read(tun_fd,data,max_data_len+1);

	if(len==max_data_len+1)
	{
		mylog(log_warn,"huge packet, data_len > %d,dropped\n",max_data_len);
		return;
	}

	if(len<0)
	{
		mylog(log_warn,"read from tun_fd return %d,errno=%s\n",len,strerror(errno));
		return;
	}

	do_mssfix(data,len);

	mylog(log_trace,"Received packet from tun,len: %d\n",len);

	if(udp_dest.inner.fd_addr.addr.is_vaild()==0)
	{
		mylog(log_debug,"received packet from tun,but there is no client yet,dropped packet\n");
		return;
	}

	from_normal_to_fec2(conn_info,udp_dest,data,len,header_normal);
}

static void delay_manager_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	//do nothing
}

static void fec_encode_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	mylog(log_trace,"fec_encode_cb() called\n");

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	assert(udp_dest.inner.fd_addr.addr.is_vaild()!=0);
	///mylog(log_trace,"events[idx].data.u64 == conn_info.fec_encode_manager.get_timer_fd64()\n");
	///uint64_t fd64=events[idx].data.u64;
	///uint64_t value;
	///if(!fd_manager.exist(fd64))   //fd64 has been closed
	///{
	///	mylog(log_trace,"!fd_manager.exist(fd64)");
	///	continue;
	///}
	///if((ret=read(fd_manager.to_fd(fd64), &value, 8))!=8)
	///{
	///	mylog(log_trace,"(ret=read(fd_manager.to_fd(fd64), &value, 8))!=8,ret=%d\n",ret);
	///	continue;
	///}
	///if(value==0)
	///{
	///	mylog(log_debug,"value==0\n");
	///	continue;
	///}
	///assert(value==1);

	from_normal_to_fec2(conn_info,udp_dest,0,0,header_normal);
}

static void fifo_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	assert(!(revents&EV_ERROR));
	int fifo_fd=watcher->fd;

	char buf[buf_len];
	int len=read (fifo_fd, buf, sizeof (buf));
	if(len<0)
	{
		mylog(log_warn,"fifo read failed len=%d,errno=%s\n",len,strerror(errno));
		return;
	}
	buf[len]=0;
	handle_command(buf);
}

static void conn_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	mylog(log_trace,"conn_timer_cb() called\n");

	//uint64_t value;
	//read(conn_info.timer.get_timer_fd(), &value, 8);

	if(udp_dest.inner.fd_addr.addr.is_vaild()==0)
	{
		return;
	}
	conn_info.stat.report_as_server(udp_dest.inner.fd_addr.addr);
	do_keep_alive(udp_dest);

}

static void prepare_cb(struct ev_loop *loop, struct ev_prepare *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	delay_manager.check();
}

int tun_dev_server_event_loop()
{
	int i,j,k,ret;
	int tun_fd;
	int local_listen_fd;

    conn_info_t *conn_info_p=new conn_info_t;
    conn_info_t &conn_info=*conn_info_p;  //huge size of conn_info,do not allocate on stack

	tun_fd=get_tun_fd(tun_dev);
	assert(tun_fd>0);

	assert(new_listen_socket2(local_listen_fd,local_addr)==0);
	assert(set_tun(tun_dev,htonl((ntohl(sub_net_uint32)&0xFFFFFF00)|1),htonl((ntohl(sub_net_uint32)&0xFFFFFF00 )|2),tun_mtu)==0);

	udp_dest.cook=1;
	udp_dest.type=type_fd_addr;

	udp_dest.inner.fd_addr.fd=local_listen_fd;
	udp_dest.inner.fd_addr.addr.clear();


	tun_dest.type=type_write_fd;
	tun_dest.inner.fd=tun_fd;

	///epoll_fd = epoll_create1(0);
	///assert(epoll_fd>0);

	///const int max_events = 4096;
	///struct epoll_event ev, events[max_events];
	///if (epoll_fd < 0) {
	///	mylog(log_fatal,"epoll return %d\n", epoll_fd);
	///	myexit(-1);
	///}

	struct ev_loop * loop= ev_default_loop(0);
	assert(loop != NULL);
    conn_info.loop=loop;

	///ev.events = EPOLLIN;
	///ev.data.u64 = local_listen_fd;
	///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, local_listen_fd, &ev);
	///if (ret!=0) {
	///	mylog(log_fatal,"add  udp_listen_fd error\n");
	///	myexit(-1);
	///}

	struct ev_io local_listen_watcher;
	local_listen_watcher.data=&conn_info;
    ev_io_init(&local_listen_watcher, local_listen_cb, local_listen_fd, EV_READ);
    ev_io_start(loop, &local_listen_watcher);

	///ev.events = EPOLLIN;
	///ev.data.u64 = tun_fd;
	///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tun_fd, &ev);
	///if (ret!=0) {
	///	mylog(log_fatal,"add  tun_fd error\n");
	///	myexit(-1);
	///}

	struct ev_io tun_fd_watcher;
	tun_fd_watcher.data=&conn_info;

	ev_io_init(&tun_fd_watcher, tun_fd_cb, tun_fd, EV_READ);
	ev_io_start(loop, &tun_fd_watcher);


	///ev.events = EPOLLIN;
	///ev.data.u64 = delay_manager.get_timer_fd();

	///mylog(log_debug,"delay_manager.get_timer_fd()=%d\n",delay_manager.get_timer_fd());
	///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, delay_manager.get_timer_fd(), &ev);
	///if (ret!= 0) {
	///	mylog(log_fatal,"add delay_manager.get_timer_fd() error\n");
	///	myexit(-1);
	///}

    delay_manager.set_loop_and_cb(loop,delay_manager_cb);


	///u64_t tmp_timer_fd64=conn_info.fec_encode_manager.get_timer_fd64();
	///ev.events = EPOLLIN;
	///ev.data.u64 = tmp_timer_fd64;

	///mylog(log_debug,"conn_info.fec_encode_manager.get_timer_fd64()=%llu\n",conn_info.fec_encode_manager.get_timer_fd64());
	///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd_manager.to_fd(tmp_timer_fd64), &ev);
	///if (ret!= 0) {
	///	mylog(log_fatal,"add fec_encode_manager.get_timer_fd64() error\n");
	///	myexit(-1);
	///}

    conn_info.fec_encode_manager.set_data(&conn_info);
    conn_info.fec_encode_manager.set_loop_and_cb(loop,fec_encode_cb);

	///conn_info.timer.add_fd_to_epoll(epoll_fd);
	///conn_info.timer.set_timer_repeat_us(timer_interval*1000);

    conn_info.timer.data=&conn_info;
    ev_init(&conn_info.timer,conn_timer_cb);
    ev_timer_set(&conn_info.timer, 0, timer_interval/1000.0 );
    ev_timer_start(loop,&conn_info.timer);


    struct ev_io fifo_watcher;
	int fifo_fd=-1;

	if(fifo_file[0]!=0)
	{
		fifo_fd=create_fifo(fifo_file);

	    ev_io_init(&fifo_watcher, fifo_cb, fifo_fd, EV_READ);
	    ev_io_start(loop, &fifo_watcher);

		mylog(log_info,"fifo_file=%s\n",fifo_file);
	}

	ev_prepare prepare_watcher;
	ev_init(&prepare_watcher,prepare_cb);
	ev_prepare_start(loop,&prepare_watcher);

	mylog(log_info,"now listening at %s\n",local_addr.get_str());

	ev_run(loop, 0);

	mylog(log_warn,"ev_run returned\n");
	myexit(0);

	return 0;
}
