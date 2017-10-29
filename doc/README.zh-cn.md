# tinyFecVPN

集成了加速器功能的轻量级VPN，可以加速全流量(TCP/UDP/ICMP)。

![image](/images/tinyFecVPN.PNG)

TinyFecVPN使用了和UDPSpeeder相同的lib，用FEC来对抗网络的丢包，改善你的网络在高延迟高丢包环境下的表现。TinyFecVPN和UDPspeeder功能类似，只不过TinyFecVPN工作方式是VPN，UDPspeeder工作方式是UDP tunnel. 

#### 效果
测试环境是一个有100ms RTT 和10%丢包的网络(借用了UDPspeeder的测试结果)。

![](/images/en/ping_compare.PNG)

![](/images/en/scp_compare.PNG)

# 原理简介

主要原理是通过冗余数据来对抗网络的丢包，发送冗余数据的方式支持FEC(Forward Error Correction)和多倍发包，其中FEC算法是Reed-Solomon。

原理图：

![](/images/FEC.PNG)

细节请看UDPspeeder的repo，这里不再重复：

https://github.com/wangyu-/UDPspeeder/

# 简明操作说明

### 环境要求

Linux主机，可以是桌面版，可以是android手机/平板，可以是openwrt路由器，也可以是树莓派。

在windows和mac上配合虚拟机可以稳定使用（tinyFecVPN跑在Linux里，其他应用照常跑在window里，桥接模式测试可用），可以使用[这个](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20170918.0/lede-17.01.2-x86_virtual_machine_image_with_udp2raw_pre_installed.zip)虚拟机镜像，大小只有7.5mb，免去在虚拟机里装系统的麻烦；虚拟机自带ssh server，可以scp拷贝文件，可以ssh进去，可以复制粘贴，root密码123456。

android需要通过terminal运行。

需要root或者cap_net_admin权限（因为需要创建tun设备）。

###### 注意
在使用虚拟机时，建议手动指定桥接到哪个网卡，不要设置成自动。否则可能会桥接到错误的网卡。

# 简明操作说明

### 安装

下载编译好的二进制文件，解压到本地和服务器的任意目录。

https://github.com/wangyu-/tinyFecVPN/releases

### 运行

假设你有一个server，ip为44.55.66.77，有一个服务监听tcp/udp 0.0.0.0:7777。

```
# 在server端运行:
./tinyvpn -s -l0.0.0.0:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

# 在client端运行：
./tinyvpn -c r44.55.66.77:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

```

现在，只要在客户端使用10.22.22.1:7777就可以连上你的服务了,来回的流量都会被加速。去ping 10.22.22.1也会得到回复。

###### 备注:

`-f20:10` 表示对每20个原始数据发送10个冗余包。`-f20:10` 和`-f 20:10`都是可以的，空格可以省略，对于所有的单字节option都是如此。对于双字节option，例如`--mode 0`和`--mtu 1200`，空格不可以省略。

`-k` 指定一个字符串，server/client间所有收发的包都会被异或，改变协议特征，防止UDPspeeder的协议被运营商针对。


# 性能测试(侧重吞吐量)

server 在 vulr 日本，CPU2.4GHz,内存 512mb。client 在搬瓦工美国，CPU 2.0GHZ,内存 96mb。在网路间额外模拟了10%的丢包，用于加重FEC的负担。

### 测试命令

```
在server端：
./tinyvpn_amd64 -s -l 0.0.0.0:5533 --mode 0
iperf3 -s
在client端：
./tinyvpn_amd64 -c -r 45.76.100.53:5533 --mode 0
iperf3 -c 10.22.22.1 -P10
```

### 测试结果

![image](/images/performance2.PNG)


# 进阶操作说明

### 命令选项
```
tinyFecVPN
git version: becd952db3    build date: Oct 28 2017 07:36:09
repository: https://github.com/wangyu-/tinyFecVPN/

usage:
    run as client: ./this_program -c -r server_ip:server_port  [options]
    run as server: ./this_program -s -l server_listen_ip:server_port  [options]

common options, must be same on both sides:
    -k,--key              <string>        key for simple xor encryption. if not set, xor is disabled
main options:
    --sub-net             <number>        specify sub-net, for example: 192.168.1.0 , default: 10.22.22.0
    --tun-dev             <number>        sepcify tun device name, for example: tun10, default: a random name such as tun987
    -f,--fec              x:y             forward error correction, send y redundant packets for every x packets
    --timeout             <number>        how long could a packet be held in queue before doing fec, unit: ms, default: 8ms
    --mode                <number>        fec-mode,available values: 0, 1; 0 cost less bandwidth, 1 cost less latency(default)
    --report              <number>        turn on send/recv report, and set a period for reporting, unit: s
advanced options:
    --mtu                 <number>        mtu. for mode 0, the program will split packet to segment smaller than mtu_value.
                                          for mode 1, no packet will be split, the program just check if the mtu is exceed.
                                          default value: 1250
    -j,--jitter           <number>        simulated jitter. randomly delay first packet for 0~<number> ms, default value: 0.
                                          do not use if you dont know what it means.
    -i,--interval         <number>        scatter each fec group to a interval of <number> ms, to protect burst packet loss.
                                          default value: 0. do not use if you dont know what it means.
    --random-drop         <number>        simulate packet loss, unit: 0.01%. default value: 0
    --disable-obscure     <number>        disable obscure, to save a bit bandwidth and cpu
developer options:
    --fifo                <string>        use a fifo(named pipe) for sending commands to the running program, so that you
                                          can change fec encode parameters dynamically, check readme.md in repository for
                                          supported commands.
    -j ,--jitter          jmin:jmax       similiar to -j above, but create jitter randomly between jmin and jmax
    -i,--interval         imin:imax       similiar to -i above, but scatter randomly between imin and imax
    -q,--queue-len        <number>        max fec queue len, only for mode 0
    --decode-buf          <number>        size of buffer of fec decoder,u nit: packet, default: 2000
    --fix-latency         <number>        try to stabilize latency, only for mode 0
    --delay-capacity      <number>        max number of delayed packets
    --disable-fec         <number>        completely disable fec, turn the program into a normal udp tunnel
    --sock-buf            <number>        buf size for socket, >=10 and <=10240, unit: kbyte, default: 1024
log and help options:
    --log-level           <number>        0: never    1: fatal   2: error   3: warn
                                          4: info (default)      5: debug   6: trace
    --log-position                        enable file name, function name, line number in log
    --disable-color                       disable log color
    -h,--help                             print this help message
```
### 跟UDPspeeder共用的选项

TinyFecVPN支持UDPspeeder的所有选项，具体请看UDPspeeder的repo：

https://github.com/wangyu-/UDPspeeder

### tinyFecVPN的新增选项

##### `--tun-dev`

指定tun设备的名字. 例如: --tun-dev tun100.

如果不指定,tinyFecVPN会创建一个随机名字的tun设备，比如tun987.

##### `--sub-net`

指定VPN的子网。 例如: 对于--sub-net 10.10.10.0, server的IP会被设置成10.10.10.1,client的IP会被设置成10.10.10.2 .

子网中的最后一个数字应该是0, 比如10.10.10.123是不符合规范的, 会被程序自动纠正成10.10.10.0.

# 使用经验
### 不能正常连通

绝大多数情况，都是因为配置了不规范的iptables造成的。不能正常连通，请清空两端的iptables后重试。清空后记得用iptable-save检查，确保确实是清空了的。

还有一部分情况是因为你要访问的服务没有bind在0.0.0.0，请用netstat -nlp检查服务器的bind情况。

### 透过tinyFecVPN免改iptables加速网络

因为iptables很多人都不会配，即使是对熟练的人也容易出错。这里推荐一种免iptables的方法，基本上可以应对任何情况，推荐给新手用。如果你可以熟练配置iptables和路由规则，可以跳过这节。

##### 假设tinyFecVPN client 运行在本地的linux上，现在VPS上有个服务监听在TCP和UDP的0.0.0.0:443，我怎么在本地linux上访问到这个服务？(假设tinyFecVPN server分配的ip是 10.22.22.1)

直接访问10.22.22.1:443即可。

##### 假设tinyFecVPN client运行在路由器/虚拟机里，假设tinyFecVPN Server运行在VPS上，现在VPS上有个服务监听在TCP和UDP的0.0.0.0:443，我怎么在本地windows上访问到这个服务？

假设tinyFecVPN server分配的ip是 10.22.22.1，路由器/虚拟机的ip是192.168.1.105

在路由器/虚拟机中运行如下命令(socat在我提供的虚拟机里已经安装好了)：

```
socat UDP-LISTEN:443,fork,reuseaddr UDP:10.22.22.1:443
socat TCP-LISTEN:443,fork,reuseaddr TCP:10.22.22.1:443
```

然后你只需要在本地windows访问192.168.1.105:443就相当于访问VPS上的443端口了。

##### 假设tinyFecVPN client 运行在本地的linux上,假设 tinyFecVPN Server运行在VPS A上。现在另一台VPS B(假设ip是123.123.123.123)上面有个服务监听在123.123.123.123:443，我怎么在本地的linux上，透过tinyFecVPN访问到这个服务？

在VPS A上运行：

```
socat UDP-LISTEN:443,fork,reuseaddr UDP:123.123.123.123:443
socat TCP-LISTEN:443,fork,reuseaddr TCP:123.123.123.123:443
```

然后，VPS B上的443端口就被映射到10.22.22.1:443了。这样，在linux上访问10.22.22.1:443就相当于访问123.123.123.123:443了。

##### 假设tinyFecVPN client运行在路由器/虚拟机里，假设 tinyFecVPN Server运行在VPS A上。现在另一台VPS B(假设ip是123.123.123.123)上面有个服务监听在123.123.123.123:443，我怎么在本地的windows上，透过tinyFecVPN访问到这个服务？

结合前两种情况,就可以了。既在路由器/虚拟机中运行socat，又在VPS中运行socat，就可以把这个端口映射到本地了。

# 限制

目前，server端的代码里有一个人为限制，作为一个加速器，tinyFecVPN只允许(直接)访问server上的服务，不能(直接)用来科学上网。即使你开启了ipforward和 MASQUERADE也不行，代码里有额外处理，直接透过tinyFecVPN访问第三方服务器的包会被丢掉，效果如图：

![image](/images/restriction.PNG)

绕过这个限制的方法有：1. 在server搭个代理，比如socks5，透过tinyFecVPN访问这个代理，用代理访问第三方服务器。  2. 自己找到相关限制的代码，修改代码，编译一个自用的无限制版（不要传播）。

