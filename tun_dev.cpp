/*
 * tun.cpp
 *
 *  Created on: Oct 26, 2017
 *      Author: root
 */


#include "tun_dev.h"

my_time_t last_keep_alive_time=0;

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

	if (persist_tun == 1) {
		if (ioctl(tun_fd, TUNSETPERSIST, 1) != 0) {
			mylog(log_warn,"failed to set tun persistent");
		}
	}
	return tun_fd;
}

int set_tun(char *if_name,u32_t local_ip,u32_t remote_ip,int mtu)
{
	if(manual_set_tun) return 0;

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

#ifndef NOLIMIT
		if(program_mode==server_mode)
		{
			char * tmp_data=out_arr[i];
			int tmp_len=out_len[i];
			iphdr *  iph;
			iph = (struct iphdr *) tmp_data;
			if(tmp_len>=int(sizeof(iphdr))&&iph->version==4)
			{
				u32_t dest_ip=iph->daddr;
				//printf("%s\n",my_ntoa(dest_ip));
				if(  ( ntohl(sub_net_uint32)&0xFFFFFF00 ) !=  ( ntohl (dest_ip) &0xFFFFFF00) )
				{
					string sub=my_ntoa(dest_ip);
					string dst=my_ntoa( htonl( ntohl (sub_net_uint32) &0xFFFFFF00)   );
					mylog(log_warn,"[restriction]packet's dest ip [%s] not in subnet [%s],dropped, maybe you need to compile an un-restricted server\n", sub.c_str(), dst.c_str());
					continue;
				}
			}
		}
#endif
		delay_send(out_delay[i],dest,out_arr[i],out_len[i]);

	}

	return 0;
}
int do_mssfix(char * s,int len)//currently only for ipv4
{
	if(mssfix==0)
	{
		return 0;
	}

	if(len<int(sizeof(iphdr)))
	{
		mylog(log_debug,"packet from tun len=%d <20\n",len);
		return -1;
	}
	iphdr *  iph;
	iph = (struct iphdr *) s;
	if(iph->version!=4)
	{
		//mylog(log_trace,"not ipv4");
		return 0;
	}

	if(iph->protocol!=IPPROTO_TCP)
	{
		//mylog(log_trace,"not tcp");
		return 0;
	}

    int ip_len=ntohs(iph->tot_len);
    int ip_hdr_len=iph->ihl*4;

    if(len<ip_hdr_len)
    {
    	mylog(log_debug,"len<ip_hdr_len,%d %d\n",len,ip_hdr_len);
    	return -1;
    }
    if(len<ip_len)
    {
    	mylog(log_debug,"len<ip_len,%d %d\n",len,ip_len);
    	return -1;
    }
    if(ip_hdr_len>ip_len)
    {
    	mylog(log_debug,"ip_hdr_len<ip_len,%d %d\n",ip_hdr_len,ip_len);
    	return -1;
    }

    if( ( ntohs(iph->frag_off) &(short)(0x1FFF) ) !=0 )
    {
    	//not first segment

    	//printf("line=%d %x  %x \n",__LINE__,(u32_t)ntohs(iph->frag_off),u32_t( ntohs(iph->frag_off) &0xFFF8));
    	return 0;
    }
    if( ( ntohs(iph->frag_off) &(short)(0x80FF) )  !=0 )
    {
    	//not whole segment
      	//printf("line=%d   \n",__LINE__);
    	return 0;
    }

    char * tcp_begin=s+ip_hdr_len;
    int tcp_len=ip_len-ip_hdr_len;

    if(tcp_len<20)
    {
    	mylog(log_debug,"tcp_len<20,%d\n",tcp_len);
    	return -1;
    }

    tcphdr * tcph=(struct tcphdr*)tcp_begin;

    if(int(tcph->syn)==0)  //fast fail
    {
    	mylog(log_trace,"tcph->syn==0\n");
    	return 0;
    }

    int tcp_hdr_len = tcph->doff*4;

    if(tcp_len<tcp_hdr_len)
    {
    	mylog(log_debug,"tcp_len <tcp_hdr_len, %d %d\n",tcp_len,tcp_hdr_len);
    	return -1;
    }

    /*
    if(tcp_hdr_len==20)
    {
    	//printf("line=%d\n",__LINE__);
    	mylog(log_trace,"no tcp option\n");
    	return 0;
    }*/

    char *ptr=tcp_begin+20;
    char *option_end=tcp_begin+tcp_hdr_len;
    while(ptr<option_end)
    {
    	if(*ptr==0)
    	{
    		return  0;
    	}
    	else if(*ptr==1)
    	{
    		ptr++;
    	}
    	else if(*ptr==2)
    	{
    		if(ptr+1>=option_end)
    		{
    			mylog(log_debug,"invaild option ptr+1==option_end,for mss\n");
    			return -1;
    		}
    		if(*(ptr+1)!=4)
    		{
    			mylog(log_debug,"invaild mss len\n");
    			return -1;
    		}
    		if(ptr+3>=option_end)
    		{
    			mylog(log_debug,"ptr+4>option_end for mss\n");
    			return -1;
    		}
    		int mss= read_u16(ptr+2);//uint8_t(ptr[2])*256+uint8_t(ptr[3]);
    		int new_mss=mss;
    		if(new_mss>::mssfix-40-10) //minus extra 10 for safe
    		{
    			new_mss=::mssfix-40-10;
    		}
    		write_u16(ptr+2,(unsigned short)new_mss);

    	    pseudo_header psh;

    	    psh.source_address =iph->saddr;
    	    psh.dest_address = iph->daddr;
    	    psh.placeholder = 0;
    	    psh.protocol = iph->protocol;
    	    psh.tcp_length = htons(tcp_len);


    	    tcph->check=0;
    	    tcph->check=tcp_csum(psh,(unsigned short *)tcph,tcp_len);

    		mylog(log_trace,"mss=%d  syn=%d ack=%d, changed mss to %d \n",mss,(int)tcph->syn,(int)tcph->ack,new_mss);

    		//printf("test=%x\n",u32_t(1));
    		//printf("frag=%x\n",u32_t( ntohs(iph->frag_off) ));

    		return 0;
    	}
    	else
    	{

    		if(ptr+1>=option_end)
    		{
    			mylog(log_debug,"invaild option ptr+1==option_end\n");
    			return -1;
    		}
    		else
    		{
    			int len=(unsigned char)*(ptr+1);
    			if(len<=1)
    			{
    				mylog(log_debug,"invaild option len %d\n",len);
    				return -1;
    			}
    			ptr+=len;
    		}
    	}
    }

	return 0;
}
int do_keep_alive(dest_t & dest)
{
	if(get_current_time()-last_keep_alive_time>u64_t(keep_alive_interval))
	{
		last_keep_alive_time=get_current_time();
		char data[buf_len];int len;
		data[0]=header_keep_alive;
		len=1;

		assert(dest.cook==1);
		//do_cook(data,len);

		delay_send(0,dest,data,len);
	}
	return 0;
}

