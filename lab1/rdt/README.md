## RDT protocol

- 学号：516030910141
- 姓名：谢添翼
- 邮箱：lsyhxty@sjtu.edu.cn


本次lab主要以GO-BACK-N为基础来实现，并进行了一定的改进。具体说明如下：

#### 1. 包设计

每个message的第一个包：
```
|<- 2 byte ->|<- 4 byte ->|<- 1 byte ->|<- 4 byte ->|<-    the rest    ->|
| check sum  |   pkt seq  |payload size|  msg size  |      payload       |
```
普通数据包：
```
|<- 2 byte ->|<- 4 byte ->|<- 1 byte ->|<-    the rest    ->|
| check sum  |   pkt seq  |payload size|      payload       |
```
Ack包：
```
|<- 2 byte ->|<- 4 byte ->|<-   the rest   ->|
| check sum  |   pkt seq  |     nothing      |
```
由于一个message的数据大小可能会超过packet包的上限，所以在需要时刻，应把message中的数据划分到多个packet包中发送。每个sender发送的数据包的包头应包含check sum，packet sequence number以及payload size。check sum用于检查包在发送过程中是否损坏，packet sequence number用于表示每个包的顺序以应对乱序情况，payload size用于表示该数据包中payload的大小。

特别地，每个消息的第一个包应额外包含message size来表示message的大小，以告知接受方关于这条消息应接受多少packet。

Ack packet是接收方发送给发送方的，用于告知sender我已收到某一包，packet中包含收到包的packet sequece number

#### 2. 发送方

任意时刻，发送方维持一个大小为WINDOW_SIZE的发送窗口，置于窗口内的包均可发送。当从上层收到一个message请求时，发送方会先将message进行改造处理。根据包设计，在其data的最前方插入四个byte用以表示这条message的大小，然后根据其message sequence number放入message buffer中。若发送窗口还没满，则根据当前需要的发送消息的sequence number从message buffer中取出相应message并将其打包成packet并发送；若发送窗口已满，则暂不处理。

另外，发送方还维护了一个msg_cursor，用于标识当前message的数据哪些部分已经发过了，以便下一次发送该message时，应当从message data的哪一部分取出data并包装发送。

当发送方收到一个合理的ack包时，会向后滑动窗口，移动窗口起始位sequence number至ack sequence number + 1，并查看是否有可发送的包，若有则填入窗口中继续发送。

滑动窗口工作原理如下图所示：

![](https://i.imgur.com/3WnmZHO.png)

#### 3. 接收方

在GO-BACK-N协议中，接受方只接受某个特定sequence num的包，其余均丢弃。在该lab中，对于接受方做了一定改进。接收方也会维护一个大小为WINDOW_SIZE的接收窗口以及expected sequence num用于表示接收窗口起始包的sequence number。

当收到一个包的sequence num大于expected sequence num并未超出窗口时，将其缓存至接收窗口中；若收到一个包的sequence num恰好等于expected num时，发送一个对应的ack packet给接收端，将窗口整体向后移动1，expected sequence num++，再检查此时期待的包是否已经收到过。若发现未收到过，则返回，继续等待之后的包；若发现已经收到过，则重复该过程。

另外，接收方也维护了一个msg_cursor，当该包的sequence num等于expected sequence num且msg_cursor为0时，说明该包是某个message中的第一个包。将该包data的前四个byte取出，这表示该message的大小，从第五个byte起才是该message真正的数据，将这些数据读取并保存在本地。msg_cursor会在每处理完一个expected packet时加上该packet的payload_size，当msg_cursor等于message大小时，说明该message已经收取完毕，将该message传给上层并重新将msg_cursor置0，开始对下一条message的接收。

#### 4. TIME_OUT机制

TIME_OUT机制主要应用于发送端。设计原则如下：

- 每开始新一轮的发包时，重置计时时间再次计时
- 收到一个ack包后发现已经没有需要发送的包了，则关闭计时
- 当发现超时时，将窗口内所有的包重新发送一遍，开始新的计时

在这样的设计下，可能会出现某些包TIME_OUT的时长比实际设计时间长的情况。

#### 5. CHECK_SUM
```
	static short checksum(struct packet *pkt)
	{
    	unsigned int sum = 0;
    	for (int i = 2; i < RDT_PKTSIZE; i++)
    	{
        	sum += pkt->data[i];
    	}

    	while ((sum >> 16) > 0)
    	{
        	sum = (sum >> 16) + (sum & 0xffff);
    	}

    	unsigned short result = sum;

    	return ~result;
	}
```
当收到包发现读取的check_sum和计算出的check_sum不一致时，则说明该包已损坏，丢弃即可。

#### 6. 测试

经测试，在三个模拟参数都为0.3的情况下，将WINDOW_SIZE设置为20，TIME_OUT设置为0.2最佳

```
tianyixie@tianyixie-virtual-machine:~/distributed system/lab1/rdt$ ./rdt_sim 1000 0.1 100 0.3 0.3 0.3 0
## Reliable data transfer simulation with:
	simulation time is 1000.000 seconds
	average message arrival interval is 0.100 seconds
	average message size is 100 bytes
	average out-of-order delivery rate is 30.00%
	average loss rate is 30.00%
	average corrupt rate is 30.00%
	tracing level is 0
Please review these inputs and press <enter> to proceed.


## Simulation completed at time 1000.90s with
	1000283 characters sent
	1000283 characters delivered
	60134 packets passed between the sender and the receiver
## Congratulations! This session is error-free, loss-free, and in order.

```
