# tinyFecVPN

A Lightweight High-Performance VPN with Build-in Forward Error Correction Support.

![image](/images/tinyFecVPN.PNG)

TinyFecVPN Improves your Network Quality on a High-Latency Lossy Link by using Forward Error Correction. It uses same lib as UDPspeeder, supports all FEC features of UDPspeeder. TinyFecVPN works at VPN mode,while UDPspeeder works at UDP tunnel mode.

[简体中文](/doc/README.zh-cn.md)(内容更丰富)

UDPspeeder's repo:

https://github.com/wangyu-/UDPspeeder

# Efficacy
Tested on a link with 100ms roundtrip and 10% packet loss at both direction(borrowed UDPspeeder's result)

### Ping Packet Loss
![](/images/en/ping_compare.PNG)

### SCP Copy Speed
![](/images/en/scp_compare.PNG)

# Supported Platforms
Linux host (including desktop Linux,<del>Android phone/tablet</del>, OpenWRT router, or Raspberry PI).

For Windows and MacOS You can run TinyFecVPN inside [this](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20170918.0/lede-17.01.2-x86_virtual_machine_image_with_udp2raw_pre_installed.zip) 7.5mb virtual machine image.

# How doest it work

TinyFecVPN uses FEC(Forward Error Correction) to reduce packet loss rate, at the cost of addtional bandwidth. The algorithm for FEC is called Reed-Solomon.

![](/images/FEC.PNG)

For more details,check:

https://github.com/wangyu-/UDPspeeder/

# Performance Test(throughput)

Server is Vulr VPS in japan，CPU: single core 2.4GHz,ram: 512mb. Client is Bandwagonhost VPS in USA，CPU: single core 2.0GHZ,ram: 96mb。

### Test command

```
Server side：
./tinyvpn_amd64 -s -l 0.0.0.0:5533 --mode 0
iperf3 -s

Client side：
./tinyvpn_amd64 -c -r 44.55.66.77:5533 --mode 0
iperf3 -c 10.22.22.1 -P10
```

### Test result

![image](/images/performance2.PNG)


# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyFecVPN/releases

### Running

Assume your server ip is 44.55.66.77, you have a service listening on udp/tcp port 0.0.0.0:7777. 

```
# Run at server side:
./tinyvpn -s -l0.0.0.0:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

# Run at client side
./tinyvpn -c r44.55.66.77:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0
```

Now,use 10.22.22.1:7777 to connect to your service,all traffic is speeded-up by FEC. If you ping 10.22.22.1, you will get ping reply.

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
    --sub-net             <number>        specify sub-net, for example: 192.168.1.0 , default: 10.22.22.0
    --tun-dev             <number>        sepcify tun device name, for example: tun10, default: a random name such as tun987
    -f,--fec              x:y             forward error correction, send y redundant packets for every x packets
    --timeout             <number>        how long could a packet be held in queue before doing fec, unit: ms, default: 8ms
    --mode                <number>        fec-mode,available values: 0, 1; 0 cost less bandwidth, 1 cost less latency(default)
    --report              <number>        turn on send/recv report, and set a period for reporting, unit: s
    --re-connect                          re-connect after lost connection,only for client.

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

##### `--keep-reconnect`

Only works at client side.

TinyFecVPN server only handles one client at same time,the connection of a new client will kick old client,after being kicked,old client will just exit by default.

If `--keep-reconnect` is enabled , the client will try to get connection back after being kicked.

### Restriction

There is currently an intended restriction at server side.You cant use tinyFecVPN to access a third server directly. So,as a connection speed-up tool,when used alone,it only allows you to speed-up your connection to your server.You cant use it to bypass network firewalls directly.

To bypass this restriction,you have to disable it by modifying source code,and re-compile by yourself.
