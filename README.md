
# netsnoop

A network snoop tool, support detect multi-client network (unicast/multicast) bandwith,delay,jitter,loss which is not supported by iperf3 currently.

## Usage

In server side run:

```sh
netsnoop -s
```

In All clients run:

```sh
# Note: you must specif a NON-loopback ip to make multicast valid.
netsnoop -c <server_ip>
```

When all clients has connected, you can send commands like belows in server side:

```python
# detect use default params.
echo # detect network delay and packet loss rate.
send # detect network bandwith and packet loss rate.
send multicast true # detect multicast bandwith.

# you can also specif params as you like.

# send 10 packets, send one packet every 200 milliseconds, every packet contains 1472 bytes data.
echo count 10 interval 200 size 1472

# send 200 packets, send one packet every 10 milliseconds, every packet contains 1472 bytes data.
# you can add 'multicast true' to detect multicast performance.
# you can add 'wait 500' to make client wait 500 milliseconds until stop receive data.
send count 200 interal 10 size 1472
```
