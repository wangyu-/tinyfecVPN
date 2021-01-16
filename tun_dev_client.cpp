#include "tun_dev.h"

static int got_feed_back=0;

static dest_t udp_dest;
static dest_t tun_dest;


static void remote_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	char data[buf_len];
	int len;

	fd64_t fd64=watcher->u64;
	int fd=fd_manager.to_fd(fd64);

	len=recv(fd,data,max_data_len+1,0);

	if(len==max_data_len+1)
	{
		mylog(log_warn,"huge packet, data_len > %d,dropped\n",max_data_len);
		return;
	}

	if(len<0)
	{
		mylog(log_warn,"recv return %d,errno=%s\n",len,strerror(errno));
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
		mylog(log_warn,"get_header failed\n");
		return;
	}

	if(header==header_keep_alive)
	{
		mylog(log_trace,"got keep_alive packet\n");
		return;
	}

	if(header==header_reject)
	{
		if(keep_reconnect==0)
		{
			mylog(log_fatal,"server restarted or switched to handle another client,exited. maybe you need --keep-reconnect\n");
			myexit(-1);
		}
		else
		{
			if(got_feed_back==1)
				mylog(log_warn,"server restarted or switched to handle another client,but keep-reconnect enabled,trying to reconnect\n");
			got_feed_back=0;
		}
		return;
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
		return;
	}

	mylog(log_trace,"Received packet from udp,len: %d\n",len);

	from_fec_to_normal2(conn_info,tun_dest,data,len);


}

static void tun_fd_cb(struct ev_loop *loop, struct ev_io *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	char data[buf_len];
	int len;
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

	char header=(got_feed_back==0?header_new_connect:header_normal);
	from_normal_to_fec2(conn_info,udp_dest,data,len,header);
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

	///int ret;
	///fd64_t fd64=watcher->u64;
	///mylog(log_trace,"events[idx].data.u64 == conn_info.fec_encode_manager.get_timer_fd64()\n");

	///uint64_t value;
	///if(!fd_manager.exist(fd64))   //fd64 has been closed
	///{
	///	mylog(log_trace,"!fd_manager.exist(fd64)");
	///	return;
	///}
	///if((ret=read(fd_manager.to_fd(fd64), &value, 8))!=8)
	///{
	///	mylog(log_trace,"(ret=read(fd_manager.to_fd(fd64), &value, 8))!=8,ret=%d\n",ret);
	///	return;
	///}
	///if(value==0)
	///{
	///	mylog(log_debug,"value==0\n");
	///	return;
	///}
	///assert(value==1);

	char header=(got_feed_back==0?header_new_connect:header_normal);
	from_normal_to_fec2(conn_info,udp_dest,0,0,header);
}

static void conn_timer_cb(struct ev_loop *loop, struct ev_timer *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	conn_info_t & conn_info= *((conn_info_t*)watcher->data);

	///uint64_t value;
	///read(conn_info.timer.get_timer_fd(), &value, 8);
	///mylog(log_trace,"events[idx].data.u64==(u64_t)conn_info.timer.get_timer_fd()\n");

	mylog(log_trace,"conn_timer_cb() called\n");
	conn_info.stat.report_as_client();
	if(got_feed_back) do_keep_alive(udp_dest);
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

static void prepare_cb(struct ev_loop *loop, struct ev_prepare *watcher, int revents)
{
	assert(!(revents&EV_ERROR));

	delay_manager.check();
}

int tun_dev_client_event_loop()
{
	//char data[buf_len];
	//int len;
	int i,j,k,ret;
	int tun_fd;

	int remote_fd;
	fd64_t remote_fd64;

    conn_info_t *conn_info_p=new conn_info_t;
    conn_info_t &conn_info=*conn_info_p;  //huge size of conn_info,do not allocate on stack

	tun_fd=get_tun_fd(tun_dev);
	assert(tun_fd>0);

	assert(new_connected_socket2(remote_fd,remote_addr,out_addr,out_interface)==0);
	remote_fd64=fd_manager.create(remote_fd);

	assert(set_tun(tun_dev,htonl((ntohl(sub_net_uint32)&0xFFFFFF00)|2),htonl((ntohl(sub_net_uint32)&0xFFFFFF00 )|1),tun_mtu)==0);

	tun_dest.type=type_write_fd;
	tun_dest.inner.fd=tun_fd;

	udp_dest.cook=1;
	udp_dest.type=type_fd64;
	udp_dest.inner.fd64=remote_fd64;

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
	///ev.data.u64 = remote_fd64;
	///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, remote_fd, &ev);
	///if (ret!=0) {
	///	mylog(log_fatal,"add  remote_fd64 error\n");
	///	myexit(-1);
	///}

	struct ev_io remote_watcher;
	remote_watcher.data=&conn_info;
	remote_watcher.u64=remote_fd64;

	ev_io_init(&remote_watcher, remote_cb, remote_fd, EV_READ);
	ev_io_start(loop, &remote_watcher);


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


	//conn_info.timer.add_fd_to_epoll(epoll_fd);
	// conn_info.timer.data=&conn_info;

    conn_info.timer.data=&conn_info;
    ev_init(&conn_info.timer,conn_timer_cb);
    ev_timer_set(&conn_info.timer, 0, timer_interval/1000.0 );
    ev_timer_start(loop,&conn_info.timer);


    struct ev_io fifo_watcher;

	int fifo_fd=-1;

	if(fifo_file[0]!=0)
	{
		fifo_fd=create_fifo(fifo_file);
		///ev.events = EPOLLIN;
		///ev.data.u64 = fifo_fd;

		///ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fifo_fd, &ev);
		///if (ret!= 0) {
		///	mylog(log_fatal,"add fifo_fd to epoll error %s\n",strerror(errno));
		///	myexit(-1);
		///}
	    ev_io_init(&fifo_watcher, fifo_cb, fifo_fd, EV_READ);
	    ev_io_start(loop, &fifo_watcher);

		mylog(log_info,"fifo_file=%s\n",fifo_file);
	}


	ev_prepare prepare_watcher;
	ev_init(&prepare_watcher,prepare_cb);
	ev_prepare_start(loop,&prepare_watcher);


	ev_run(loop, 0);

	mylog(log_warn,"ev_run returned\n");
	myexit(0);


	return 0;
}
