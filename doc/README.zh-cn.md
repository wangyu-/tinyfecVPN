# tinyfecVPN

工作在VPN方式的双边网络加速工具，可以加速全流量(TCP/UDP/ICMP)。

![image](/images/tinyFecVPNcn2.PNG)

假设你的本地主机到某个服务器的丢包很高，你只需要用tinyfecVPN建立一条VPN连接，然后透过此VPN来访问server，你的网路质量会得到显著改善。通过合理设置参数，可以轻易把网络丢包率降低到万分之一以下。除了可以降低丢包，还可以显著改善TCP的响应速度，提高TCP的单线程传输速度。

tinyfecVPN使用了和UDPSpeeder相同的lib，功能和UDPspeeder类似，只不过tinyfecVPN工作方式是VPN，UDPspeeder工作方式是UDP tunnel. 


##### 提示

UDPspeeder的repo:

https://github.com/wangyu-/UDPspeeder

#### 效果
测试环境是一个有100ms RTT 和双向10%丢包的网络。

![](https://raw.githubusercontent.com/wangyu-/UDPspeeder/master/images/cn/ping_compare_cn.PNG)

![](https://github.com/wangyu-/UDPspeeder/blob/master/images/cn/scp_compare.PNG)

#### 关键词
双边加速、全流量加速、开源加速器、游戏加速、网游加速器

# 原理简介

主要原理是通过冗余数据来对抗网络的丢包，发送冗余数据的方式支持FEC(Forward Error Correction)和多倍发包，其中FEC算法是Reed-Solomon。

原理图：

![](/images/FEC.PNG)

细节请看UDPspeeder的repo，这里不再重复：

https://github.com/wangyu-/UDPspeeder/


# 简明操作说明

### 环境要求

Linux主机，可以是桌面版，<del>可以是android手机/平板</del>，可以是openwrt路由器，也可以是树莓派。(android暂时有问题)

在windows和mac上配合虚拟机可以稳定使用（tinyfecVPN跑在Linux里，其他应用照常跑在window里，桥接模式测试可用），可以使用[这个](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20171108.0/lede-17.01.2-x86_virtual_machine_image.zip)虚拟机镜像，大小只有7.5mb，免去在虚拟机里装系统的麻烦；虚拟机自带ssh server，可以scp拷贝文件，可以ssh进去，可以复制粘贴，root密码123456。

android需要通过terminal运行。

需要root或者cap_net_admin权限（因为需要创建tun设备）。

###### 注意
在使用虚拟机时，建议手动指定桥接到哪个网卡，不要设置成自动。否则可能会桥接到错误的网卡。

# 简明操作说明

### 安装

下载编译好的二进制文件，解压到本地和服务器的任意目录。

https://github.com/wangyu-/tinyfecVPN/releases

### 运行

假设你有一个server，ip为44.55.66.77，有一个服务监听tcp/udp 0.0.0.0:7777。

```
# 在server端运行:
./tinyvpn -s -l0.0.0.0:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

# 在client端运行：
./tinyvpn -c -r44.55.66.77:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

```

现在，只要在客户端使用10.22.22.1:7777就可以连上你的服务了,来回的流量都会被加速。执行ping 10.22.22.1也会得到回复。

###### 备注:

`-f20:10`表示对每20个原始数据发送10个冗余包。`-f20:10` 和`-f 20:10`都是可以的，空格可以省略，对于所有的单字节option都是如此。对于双字节option，例如后面会提到的`--mode 0`，空格不可以省略。

`-k` 指定一个字符串，开启简单的异或加密

##### 提示

对于某些运营商，tinyfecVPN跟udp2raw配合可以达到更好的速度，udp2raw负责把UDP伪装成TCP，来绕过运营商的UDP限速。

udp2raw的repo:

https://github.com/wangyu-/udp2raw-tunnel

# 进阶操作说明

### 命令选项
```
tinyfecVPN
git version: b03df1b586    build date: Oct 31 2017 19:46:50
repository: https://github.com/wangyu-/tinyfecVPN/

usage:
    run as client: ./this_program -c -r server_ip:server_port  [options]
    run as server: ./this_program -s -l server_listen_ip:server_port  [options]

common options, must be same on both sides:
    -k,--key              <string>        key for simple xor encryption. if not set, xor is disabled
main options:
    --sub-net             <number>        specify sub-net, for example: 192.168.1.0 , default: 10.22.22.0
    --tun-dev             <number>        sepcify tun device name, for example: tun10, default: a random name such as tun987
    -f,--fec              x:y             forward error correction, send y redundant packets for every x packets
    --timeout             <number>        how long could a packet be held in queue before doing fec, unit: ms, default: 8ms
    --mode                <number>        fec-mode,available values: 0, 1; 0 cost less bandwidth, 1 cost less latency;default: 0)
    --report              <number>        turn on send/recv report, and set a period for reporting, unit: s
    --keep-reconnect                      re-connect after lost connection,only for client.
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
    --tun-mtu             <number >       mtu of the tun interface,most time you shouldnt change this
    --disable-mssfix      <number >       disable mssfix for tcp connection
    --fifo                <string>        use a fifo(named pipe) for sending commands to the running program, so that you
                                          can change fec encode parameters dynamically, check readme.md in repository for
                                          supported commands.
    -j ,--jitter          jmin:jmax       similiar to -j above, but create jitter randomly between jmin and jmax
    -i,--interval         imin:imax       similiar to -i above, but scatter randomly between imin and imax
    -q,--queue-len        <number>        max fec queue len, only for mode 0
    --decode-buf          <number>        size of buffer of fec decoder,u nit: packet, default: 2000
    --fix-latency                         try to stabilize latency, only for mode 0
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

tinyfecVPN支持UDPspeeder的所有选项，具体请看UDPspeeder的repo：

https://github.com/wangyu-/UDPspeeder/blob/master/doc/README.zh-cn.md#命令选项

### tinyfecVPN的新增选项

##### `--tun-dev`

指定tun设备的名字. 例如: --tun-dev tun100.

如果不指定,tinyfecVPN会创建一个随机名字的tun设备，比如tun987.

##### `--sub-net`

指定VPN的子网，格式为xxx.xxx.xxx.0。 例如: 对于--sub-net 10.10.10.0, server的IP会被设置成10.10.10.1,client的IP会被设置成10.10.10.2 .

子网中的最后一个数字应该是0, 比如10.10.10.123是不符合规范的, 会被程序自动纠正成10.10.10.0.

##### `--keep-reconnect`

只对client有效

tinyfecVPN server只接受一个client的连接，后连接的client会把新的挤掉。

如果开启了--keep-reconnect，client在连接断开或者被挤掉以后会尝试重新获得连接。

# 使用经验

https://github.com/wangyu-/tinyfecVPN/wiki/使用经验

# 性能测试(侧重吞吐量)

server 在 vulr 日本，CPU2.4GHz,内存 512mb。client 在搬瓦工美国，CPU 2.0GHZ,内存 96mb。在网路间额外模拟了10%的丢包，用于加重FEC的负担。

### 测试命令

```
在server端：
./tinyvpn_amd64 -s -l 0.0.0.0:5533 --mode 0
iperf3 -s
在client端：
./tinyvpn_amd64 -c -r 44.55.66.77:5533 --mode 0
iperf3 -c 10.22.22.1 -P10
```

### 测试结果

![image](/images/performance2.PNG)

# Wiki

Wiki上有更丰富的内容：

https://github.com/wangyu-/tinyfecVPN/wiki

# 限制

考虑到国内的实情，为了降低风险，[releases](https://github.com/wangyu-/tinyfecVPN/releases)中的binaries存在一点人为限制：只允许透过tinyfecVPN访问server上的服务，不能(直接)访问第三方服务器；也就是只能当加速器用，不能(直接)用来科学上网。

即使开启了ipforward和MASQUERADE也不行，代码里有额外处理，直接透过tinyfecVPN访问第三方服务器的包会被丢掉，效果如图：

![image](/images/restriction.PNG)

如果你需要去掉这个限制，可以自己编译(makefile里有相应的target)。 只需要自己编译server端，因为限制只存在于server端。

