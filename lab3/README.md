## Lab3 - Qos Implement with DPDK

- 学号：516030910141
- 姓名：谢添翼
- 邮箱：lsyhxty@sjtu.edu.cn

### 参数设置

Lab的要求是利用srTCM和WRED算法来实现meter和dropper，其中包含4个数据流，要求4个流的带宽比满足8：4：2：1，且0号流的带宽应为1.28Gbps左右。

由于测试中是每1000000ns发送一堆包，1.28Gbps相当于每秒通过0.16*(1024^3)byte大小的包，因而每一个time period大约发送172000byte大小的包。在实现中，考虑所有的绿色和黄色包都能成功发送，而所有的红色包都被丢弃。并利用ebs和cbs来实现限速要求，为了设置比例方便，在flow0中将ebs和cbs设置为88000byte大小即可实现限速要求，而cir可设为大于计算出来的结果值，因为这里的实现是通过令牌桶大小来实现的。其他流的参数根据比例设置即可

srTCM算法参数设置如下：

| flow id | cir | cbs | ebs |
| ------ | ------ | ------ | ------ |
| 0 | 1760000000 | 88000 | 88000 |
| 1 | 880000000 | 44000 | 44000 |
| 2 | 440000000 | 22000 | 22000 |
| 3 | 220000000 | 11000 | 11000 |

由于测试中每次发送包的个数为500-1500个，WRED算法参数设置如下：

| color | wq_log2 | min_th | max_th | maxp_inv |
| ------ | ------ | ------ | ------ | ----- |
| GREEN | 9 | 999 | 1000 | 10 |
| YELLOW | 9 | 999 | 1000 | 10 |
| RED | 9 | 0 | 1 | 10 |

其中wq_log2和maxp_inv是根据官方文档推荐设置的。

### DPDK API 使用

- rte_meter_srtcm_config:用设置好的参数初始化每个srTCM算法的流对象
- rte_meter_srtcm_color_blind_check：使用色盲模式给packet标色
- rte_red_config_init：初始化red算法的参数结构
- rte_red_rt_data_init：初始化red算法的颜色对象
- rte_red_enqueue：应用red算法决定该包是否要发出
- rte_panic:用于报错提醒