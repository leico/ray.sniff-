# ray.sniff-

convert our web field

[![IMAGE ALT TEXT HERE](http://img.youtube.com/vi/EsE90BRTuCc/0.jpg)](http://www.youtube.com/watch?v=EsE90BRTuCc)

# Project Page
(http://cycling74.com/project/ray-sniff-record-our-web-field/)

# Concept 

At first, I thought that I want to create a system that compose a music automatically when dive into the Web.
Now, I am interested in sounds of network data, inspired Alva noto’s work “unitxt”. so I create ray.sniff~.

> after the first ten tracks that can be regarded as the core recordings of unitxt,
> there are 15 more tracks generated from converting pure data of programs, jpgs or other digital files into sound material. 
> these tracks could be considered as source code ‘solos’ to be played on top of the first ten tracks or to be regarded as sonified concept recordings.
> Raster-noton – Alva Noto/unitxt(http://www.raster-noton.net/main.php?action=products&dat=146)

ray.sniff~ is a max external object that is converting Web data to signal data. I create it using libpcap(http://www.tcpdump.org/).
When we access to any sites or upload any files, those data make sounds. ray.sniff~ enable us to convert innumerable data to sound.
If you dive to the Web with ray.sniff~, you have a special experience. You can dive to find exciting sounds deeper than ever.
ray.sniff~ invites you to new relationship with the Web as amazing sound source.

Time has passed, and most Web traffic is now encrypted. Diving into the Web with ray.sniff~, we can hear little more than signals from a numbers station: something is clearly being said, but we can no longer understand it.

Yet raw data still flows through local networks — OSC, mDNS, ARP, Art-Net, and more. There, a wealth of signals remains, waiting to be heard.

The network soundscape is still out there.

# Requirements

* macOS 11 or later (Universal Binary: Apple Silicon and Intel)
* Max 8.2 or later
* Permission to capture packets (see below)

Legacy Intel-only builds for older systems are archived in [Releases](https://github.com/leico/ray.sniff-/releases/tag/legacy-intel).

# Capture permission

Capturing packets on macOS requires access to the BPF devices (`/dev/bpf*`), which only root can open by default.
ray.sniff~ does not change system settings by itself. Choose one of the following:

1. **Install ChmodBPF (recommended)**

   ```sh
   brew install --cask wireshark-chmodbpf
   ```

   This installs a launch daemon (from the Wireshark project) that grants the `access_bpf` group access to `/dev/bpf*` at every boot, and adds your user to that group. Log out and log in again (or restart) after installing.
   Note that every program you run, not only Max, will then be able to capture network traffic.

2. **Run Max with root privileges**

   ```sh
   sudo "/Applications/Max.app/Contents/MacOS/Max"
   ```

   Files and preferences created in this session may become owned by root, so this is not recommended for daily use.

ray.sniff~ never opens a capture when it is created, so patches always load. If permission is missing when capturing starts, it prints these instructions to the Max console.

# Usage

```
[ray.sniff~ <bitdepth> @device <name> @filter <expression ...> @queuesize <KB>]
```

ray.sniff~ captures **only while DSP is on and it has received a positive int**. Send `0` to stop.
The left outlet outputs packet bytes as a signal; the right outlet is a dumpout.
See the help patch for a working example.

| Argument | Description |
|---|---|
| `bitdepth` | `8`, `16`, `24` or `32`: bits of packet data per sample. If missing or invalid, an error is printed and `16` is used |

| Attribute | Description |
|---|---|
| `@bitdepth` | Same as the argument. Can be changed at any time |
| `@device` | Network interface such as `en0`. Empty (default): chosen automatically. Can be changed while capturing; if the new device can't be opened, the current one keeps running |
| `@filter` | [pcap filter expression](https://www.tcpdump.org/manpages/pcap-filter.7.html), e.g. `@filter tcp port 443`. Can be changed while capturing; an invalid expression is rejected and the current filter stays |
| `@queuesize` | Size of the queue between the capture and the audio, in KB. Rounded up to a power of two, 128 to 1048576 (default 1024). Can be changed only while DSP is off and the capture is closed |

| Message | Description |
|---|---|
| `int` | Positive: capture while DSP is on. `0` or negative: stop |
| `clear` | Empty the queue |
| `stats` | Output counters from the right outlet (see below) |
| `resetstats` | Set the counters back to 0 |
| `size` | Deprecated. Outputs `size <unread bytes>`; use `stats` (`queue size`) |

## How packets become sound

Each sample takes `bitdepth / 8` bytes of packet data, first byte lowest (little-endian), scaled to -1..1. When the queue runs out, the rest of the audio block is silent.

The audio reads `bitdepth / 8 × sample rate` bytes per second (16 bit at 48 kHz: 96 KB/s). When the traffic is faster, the queue fills up and **new packets are dropped** until there is room again. Use a higher bitdepth, a larger `@queuesize`, or a `@filter` to change the balance.

## stats

`stats` outputs one message per counter from the right outlet, so they can be split with `[route pcap queue]`:

```
pcap dropped_packets <n>    dropped by the kernel: the capture could not keep up
queue locked <0|1>          1 if the queue memory is locked in RAM (mlock)
queue size <bytes>          unread bytes in the queue now
queue written_packets <n>   packets put in the queue
queue written_bytes <n>
queue dropped_packets <n>   packets dropped because the queue was full
queue dropped_bytes <n>
```

Counters are totals since the object was created or since `resetstats`.

## Old patches

The old positional arguments `<bitdepth> [device] [ip netmask] [filter ...]` are still read: `device` becomes `@device` and `filter ...` becomes `@filter`. `ip` and `netmask` are ignored. Attributes win over positional arguments.

# Install

Put this repository (or a link to it) into your Max Packages folder and build it:

```sh
ln -s /path/to/ray.sniff- "$HOME/Documents/Max 9/Packages/ray.sniff"
```

Max finds the external in `externals/` and the help patch in `help/`. Restart Max after rebuilding.

# Build

```sh
git clone --recursive https://github.com/leico/ray.sniff-.git
cd ray.sniff-
cmake -B build
cmake --build build
```

The external is written to `externals/ray.sniff~.mxo` (Universal Binary, ad-hoc signed). It links against the libpcap bundled with macOS.

## Tests

```sh
cmake -B build-test -DRAYSNIFF_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-test -j && ctest --test-dir build-test --output-on-failure
```

A test build makes only the tests, not the external. Tests tagged `[live]` capture on `lo0` and are skipped without BPF permission (`sudo ./build-test/tests/raysniff_tests "[live]"`).
Design notes (in Japanese) are in [docs/design.md](docs/design.md).

# Known limitations

* macOS only
* Not every packet becomes sound: packets are dropped when the traffic is faster than the audio reads it (see stats)
