Checkpoint Report for CS 219
============================

**Team**: Yuanzhi Gao (704145326), Zhaowei Tan (504777867), Zheng Xing (504013807), Zhiyi Zhang (104705108)

**Project**: Cloud-Assisted Augmented Reality (AR) on Android

Progress
--------

**Source Code**:
* Server:
* Socket:
* Android:

### Realized video stream uploading/downloading between android endpoint and remote server without compression ###
We have implemented the video stream transfer between two endpoints using UDP.

Snapshot of server's receiving UDP packets. The UDP payload is 1100 Bytes.

![Image of Server Receiving](serverReceiving.png)

### Designed the video frame transfer protocol based on UDP ###
Given application should be aware of the boundary of each frame and also should be able to check whether the frame is integrated, the pure UDP is not sufficient.
Therefore we designed a video transfer protocol in which we add:
  * Packet type:
    + Initialization meta data
    + Video Frame Data
    + Sensor Data
  * Frame identifier
  * Frame segmentation identifier

```
Language: C/C++
Function: Packet encoding/decoding

void
Example()
{
  example;
}
```

### Realized AR logic on both server side and android side ###
AR running on Android endpoint.

![Image of AR running on Android](arAndroid.png)

AR running on Linux.

![Image of AR running on Linux](arLinux.png)

### Realized Android sensor information collection and uploading to remote server ###
The sensor information includes:
  * A
  * B
  * C

Core function on Android endpoint

```
Language: Java/Android
Function: Collecting sensor info

void
Example()
{
  example;
}
```

Technical Issues and Our Plans
------------------------------

### Single video frame grabbed directly from Camera is too large ###
We found that a single Android video frame is about **100 KB**.
Considering the MTU = 1500B, it usually needs around **100 UDP packets** to transfer one singe frame, which is not viable at all.
Cloud-Assisted AR requires low latency and the packet number should be small enough.
Also, the increase of UDP packet number also causes higher packet lost possibility for each frame.
Because of the vulnerability that AR processing requires format-strict and size-strict frame data, packet lost means the frame would finally be dropped.

**Solution**
Cloud-Assisted AR needs relatively high reliability and thus we need to do compression to improve the performance.

### Server AR process should take Android endpoint's upload as input ###
Currently we have achieved AR logic on both sever side and Android endpoint.
However it's not easy to combine these two parts together.
Server (MACOS) and Android endpoint are using totally different parameters regarding the video frame.
They use totally different modules of the AR library to process input video.

**Solution**
Adapt the server side to use the same pre-installed parameter as the android side.
We needs to dive into the library source and figure out how each parameter works.

Future work
-----------

* Video stream compression
* Server AR: getting input from Android endpoint
* Android endpoint: drawing the frame downloaded from remote server

Timeline
--------
### **Before Week 9** ###

Solve technical issues mentioned in the report.

### **Week 9** ###

Performance evaluation and improvement

* Record the RTT of video stream packets without compression
* Record the RTT change after utilizing compression

### **Week 10** ###

Presentation and project report