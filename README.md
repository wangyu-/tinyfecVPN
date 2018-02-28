# tinyfecVPN

A Lightweight VPN with Build-in Forward Error Correction Support(or A Network Improving Tool which works at VPN mode). Improves your Network Quality on a High-latency Lossy Link. 

![image](/images/tinyFecVPN3.PNG)

tinyfecVPN uses Forward Error Correction(Reed-Solomon code) to reduce packet loss rate, at the cost of additional bandwidth usage. 

Assume your local network to your server is lossy. Just establish a VPN connection to your server with tinyfecVPN, access your server via this VPN connection, then your connection quality will be significantly improved. With well-tuned parameters , you can easily reduce  IP or UDP/ICMP packet-loss-rate to less than 0.01% . Besides reducing packet-loss-rate, tinyfecVPN can also significantly improve your TCP latency and TCP single-thread download speed.

tinyfecVPN uses same lib as [UDPspeeder](https://github.com/wangyu-/UDPspeeder), supports all FEC features of UDPspeeder. tinyfecVPN works at VPN mode,while UDPspeeder works at UDP tunnel mode.

[tinyfecVPN wiki](https://github.com/wangyu-/tinyfecVPN/wiki)

[简体中文](/doc/README.zh-cn.md)(内容更丰富)

##### Note
UDPspeeder's repo:

https://github.com/wangyu-/UDPspeeder

# Efficacy
Tested on a link with 100ms roundtrip and 10% packet loss at both direction. You can easily reproduce the test result by yourself.

### Ping Packet Loss
![](/images/en/ping_compare2.PNG)

### SCP Copy Speed
![](/images/en/scp_compare2.PNG)

# Supported Platforms
Linux host (including desktop Linux,<del>Android phone/tablet</del>, OpenWRT router, or Raspberry PI).Binaries for `amd64` `x86` `mips_be` `mips_le` `arm` are provided.

For Windows and MacOS, You can run tinyfecVPN inside [this](https://github.com/wangyu-/udp2raw-tunnel/releases/download/20171108.0/lede-17.01.2-x86_virtual_machine_image.zip) 7.5mb virtual machine image.

Need root or at least CAP_NET_ADMIN permission to run, for creating tun device.

# How doest it work

tinyfecVPN uses FEC(Forward Error Correction) to reduce packet loss rate, at the cost of additional bandwidth usage. The algorithm for FEC is called Reed-Solomon.

![](/images/FEC.PNG)

### Reed-Solomon

`
In coding theory, the Reed–Solomon code belongs to the class of non-binary cyclic error-correcting codes. The Reed–Solomon code is based on univariate polynomials over finite fields.
`

`
It is able to detect and correct multiple symbol errors. By adding t check symbols to the data, a Reed–Solomon code can detect any combination of up to t erroneous symbols, or correct up to ⌊t/2⌋ symbols. As an erasure code, it can correct up to t known erasures, or it can detect and correct combinations of errors and erasures. Reed–Solomon codes are also suitable as multiple-burst bit-error correcting codes, since a sequence of b + 1 consecutive bit errors can affect at most two symbols of size b. The choice of t is up to the designer of the code, and may be selected within wide limits.
`

![](/images/en/rs.png)

Check wikipedia for more info, https://en.wikipedia.org/wiki/Reed–Solomon_error_correction


# Getting Started

### Installing

Download binary release from https://github.com/wangyu-/tinyfecVPN/releases

### Running

Assume your server ip is `44.55.66.77`, you have a service listening on udp/tcp port `0.0.0.0:7777`. 

```
# Run at server side:
./tinyvpn -s -l0.0.0.0:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0

# Run at client side
./tinyvpn -c -r44.55.66.77:4096 -f20:10 -k "passwd" --sub-net 10.22.22.0
```

Now, use `10.22.22.1:7777` to connect to your service,all traffic will be improved by FEC. If you ping `10.22.22.1`, you will get ping reply.

##### Note

`-f20:10` means sending 10 redundant packets for every 20 original packets.

`-k` enables simple XOR encryption

To create tun device, you need root or cap_net_admin permission. Its suggested to run tinyfecVPN as non-root, check [this link](https://github.com/wangyu-/tinyfecVPN/wiki/run-tinyfecVPN-as-non-root) for more info.

##### Note2

You can use udp2raw with tinyfecVPN together to get better speed on some ISP with UDP QoS(UDP throttling).

udp2raw's repo：

https://github.com/wangyu-/udp2raw-tunnel

# Advanced Topic

### Usage
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
### FEC Options

The program supports all options of UDPspeeder,check UDPspeeder repo for details:

https://github.com/wangyu-/UDPspeeder

### Addtional Options

##### `--tun-dev`

Specify a tun device name to use. Example: `--tun-dev tun100`.

If not set,tinyfecVPN will randomly chose a name,such as `tun987`.

##### `--sub-net`

Specify the sub-net of VPN. Example: `--sub-net 10.10.10.0`, in this way,server IP will be `10.10.10.1`,client IP will be `10.10.10.2`.

The last number of option should be zero, for exmaple `10.10.10.123` is invalild, and will be corrected automatically to `10.10.10.0`.

##### `--keep-reconnect`

Only works at client side.

tinyfecVPN server only handles one client at same time,the connection of a new client will kick old client,after being kicked,old client will just exit by default.

If `--keep-reconnect` is enabled , the client will try to get connection back after being kicked.

# Performance Test(throughput)

Server is a Vulr VPS in japan，CPU: single core 2.4GHz,ram: 512mb. Client is a Bandwagonhost VPS in USA，CPU: single core 2.0GHZ,ram: 96mb. To put pressure on the FEC algorithm, an additional 10% packet-loss rate was introduced at both direction.

### Test command

```
Server side：
./tinyvpn_amd64 -s -l 0.0.0.0:5533 --mode 0 -f20:10
iperf3 -s

Client side：
./tinyvpn_amd64 -c -r 44.55.66.77:5533 --mode 0 -f20:10
iperf3 -c 10.22.22.1 -P10
```

### Test result

![image](/images/performance2.PNG)

Note: the performance is mainly limited by the RS code lib.

# Other

As a VPN software may contradict with local regulations, I had to introduce an intended restriction in the pre-released binaries: you can only use tinyfecVPN to access your own server.

You can easily get rid of this restriction by compiling the source code by yourself (take a look at the makefile). This restriction exits only at server side, only the server side binary needs to be compiled by yourself.
