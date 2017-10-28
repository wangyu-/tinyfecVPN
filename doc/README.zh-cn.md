# tinyFecVPN

集成了加速器功能的一个轻量级VPN

![image](/images/tinyFecVPN.PNG)

TinyFecVPN使用UDPSpeeder作为lib，用FEC来对抗网络的丢包，可以改善你的网络连接(TCP/UDP/ICMP)在高延迟高丢包环境下的表现。TinyFecVPN和UDPspeeder功能类似，只不过TinyFecVPN工作方式是VPN，UDPspeeder工作方式是

#### 效果
测试环境是一个有100ms RTT 和10%丢包的网络(借用了UDPspeeder的测试结果)。

![](/images/en/ping_compare.PNG)

![](/images/en/scp_compare.PNG)

# 简明操作说明


### 环境要求

Linux主机，可以是桌面版，可以是android手机/平板，可以是openwrt路由器，也可以是树莓派。

在windows和mac上配合虚拟机可以稳定使用（tinyFecVPN跑在Linux里，其他应用照常跑在window里，桥接模式测试可用），可以使用[这个](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20170918.0/lede-17.01.2-x86_virtual_machine_image_with_udp2raw_pre_installed.zip)虚拟机镜像，大小只有7.5mb，免去在虚拟机里装系统的麻烦；虚拟机自带ssh server，可以scp拷贝文件，可以ssh进去，可以复制粘贴，root密码123456。

android需要通过terminal运行。


###### 注意
在使用虚拟机时，建议手动指定桥接到哪个网卡，不要设置成自动。否则可能会桥接到错误的网卡。

#### 原理简介

主要原理是通过冗余数据来对抗网络的丢包，发送冗余数据的方式支持FEC(Forward Error Correction)和多倍发包，其中FEC算法是Reed-Solomon。

细节请看UDPspeeder的repo，这里不再重复：

https://github.com/wangyu-/UDPspeeder/

# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyFecVPN/releases

下载编译好的二进制文件，解压到本地和服务器的任意目录。

### Running

Assume your server ip is 44.55.66.77, you have a service listening on udp/tcp port 0.0.0.0:7777.

```
# Run at server side:
./tinyvpn -s -l0.0.0.0:4096 -f20:10 -k "passwd"

# Run at client side
./tinyvpn -c r44.55.66.77:4096 -f20:10 -k "passwd"
```

Now,use 10.0.0.1:7777 to connect to your service,all traffic is speeded-up by FEC.

##### Note

`-f20:10` means sending 10 redundant packets for every 20 original packets.

`-k` enables simple XOR encryption

# Advanced Topic

### Usage
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
    --sub-net             <number>        specify sub-net, for example: 192.168.1.0 , default: 10.112.0.0
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
### FEC Options

The program supports all options of UDPspeeder,check UDPspeeder repo for details:

https://github.com/wangyu-/UDPspeeder

### Addtional Options

##### `--tun-dev`

Specify a tun device name to use. Example: --tun-dev tun100.

If not set,tinyFecVPN will randomly chose a name,such as tun987.

##### `--sub-net`

Specify the sub-net of VPN. Example: --sub-net 10.10.10.0, in this way,server IP will be 10.10.10.1,client IP will be 10.10.10.2.

The last number of option should be zero, for exmaple 10.10.10.123 is invalild, and will be corrected automatically to 10.10.10.0.

### Restriction

There is currently an intended restriction at server side.You cant use tinyFecVPN to access a third server directly. So,as a connection speed-up tool,when used alone,it only allows you to speed-up your connection to your server.You cant use it to bypass network firewalls directly.

To bypass this restriction,you have to disable it by modifying source code,and re-compile by yourself.
