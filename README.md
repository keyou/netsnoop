
# netsnoop

[![Build Status](https://travis-ci.com/keyou/netsnoop.svg?token=wDoYyAQNx1yEgEQp2UUi&branch=master)](https://travis-ci.com/keyou/netsnoop)

A network snoop tool, support detect multi-client (which is not supported by iperf3 now) network (unicast/multicast) bandwith,delay,jitter,loss.

## Usage

```sh
$ ./netsnoop -h
usage:
  netsnoop -s <local ip> 4000         (start server)
  netsnoop -c <server ip> 4000        (start client)
  --------
  command:
  ping count 10                       (test delay)
  send count 1000                     (test unicast)
  send count 1000 multicast true      (test multicast)
  send speed 500 time 3000            (test unicast)
  
  version: v0.1.85 (Aug 28 2019 15:02:50)
```

In server side run:

```sh
netsnoop -s
```

In all clients run:

```sh
# Note: you must specif a NON-loopback ip to make multicast valid.
netsnoop -c <server_ip>
```

When all clients has connected, you can send commands like belows in server side:

```python
# detect use default params.
ping # detect network delay and packet loss rate.
send # detect network bandwith and packet loss rate.
send multicast true # detect multicast bandwith.

# you can also specif params as you like.

# send 10 packets, send one packet every 200 milliseconds, every packet contains 1472 bytes data.
ping count 10 interval 200 size 1472

# send 200 packets, send one packet every 10 milliseconds, every packet contains 1472 bytes data.
# you can add 'multicast true' to detect multicast performance.
# you can add 'wait 500' to make client wait 500 milliseconds until stop receive data.
send count 200 interal 10 size 1472

# send in 500KB/s speed, 3000 milliseconds.
send speed 500 time 3000
```

## Subcommands

`ping` Format:

```python
ping [count <num>] [interval <milliseconds>] [size <num>] [wait <milliseconds>]
```

`send` Format:

```python
send [count <num>] [interval <milliseconds>] [size <num>] [wait <milliseconds>] \
     [speed <KB/s>] [time <milliseconds>] [timeout <milliseconds>]
```

## Advanced Usage

You can use script file with netsnoop to run multiple commands automatically:

```sh
# check script.snoop file contents.
$ cat script.snoop
peers 2
ping count 10
send count 100 interval 10 wait 500
sleep 5
send multicast true speed 100 time 3000

# run netsnoop as a server with a script file
$ netsnoop -s < script.snoop
```

The `script.snoop` file is a text file:

- `peers 2`: wait until 2 peers connect;
- `sleep 5`: wait 5 seconds;

## For developers

Welcome to expand more subcommands. This tool is not fully complete, although the code is friendly with unicast, it is not friendly with multicast now. Welcome to reflector the multicast code.

TODO:

- Support 'recv' command which is revese of 'send'.
- Support NAT environment.
- Optimize multicast code.
- Optimize command line arguements resolve code.
- Refine documents about netsnoop_select, netsnoop_multicast.

### Compile

#### Linux

In Linux alike system just run `make`.

And you can run `make package` to compile and pack the binary to zip archive.
It will compile linux and win32 binaries(mingw needed.).

#### Windows

In windows system you should install mingw or msys2 first, then run `make`.
