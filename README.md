# tinyFecVPN

A tiny VPN with Build-in FEC Support.

![image](/images/tinyFecVPN.PNG)

TinyFecVPN use UDPSpeeder as lib.Supports all FEC features of UDPspeeder. It can improve quality of all network traffic(TCP/UDP/ICMP) as a single program.


# Efficacy
Tested on a link with 100ms latency and 10% packet loss at both direction(borrowed UDPspeeder 's result,they behave almost the same)

### Ping Packet Loss
![](/images/en/ping_compare.PNG)

### SCP Copy Speed
![](/images/en/scp_compare.PNG)

# Supported Platforms
Linux host (including desktop Linux,Android phone/tablet, OpenWRT router, or Raspberry PI).

For Windows and MacOS You can run TinyFecVPN inside [this](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20170918.0/lede-17.01.2-x86_virtual_machine_image_with_udp2raw_pre_installed.zip) 7.5mb virtual machine image.

# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyFecVPN/releases

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

##### Restriction

There is currently an intended restriction at client side.You cant use tinyFecVPN to access a third server directly.So,as a connection speed-up tool,when used alone,it only allows you to speed-up your connection to your server.You cant use it to bypass network firewalls directly.

To bypass this restriction,you have to disable it by modifying source code,and re-compile by yourself.
