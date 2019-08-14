/*
 * FILE: rdt_sender.cc
 * DESCRIPTION: Reliable data transfer sender.
 * NOTE: This implementation assumes there is no packet loss, corruption, or 
 *       reordering.  You will need to enhance it to deal with all these 
 *       situations.  In this implementation, the packet format is laid out as 
 *       the following:
 *       
 *      Send packet:
 *       |<- 2 byte ->|<- 4 byte ->|<- 1 byte ->|<-    the rest    ->|
 *       | check sum  |   pkt seq  |payload size|      payload       |
 * 
 *       |<- 2 byte ->|<- 4 byte ->|<- 1 byte ->|<- 4 byte ->|<-    the rest    ->|
 *       | check sum  |   pkt seq  |payload size|  msg size  |<-    payload     ->|
 *
 *      Ack packet:
 *       |<- 2 byte ->|<- 4 byte ->|<-   the rest   ->|
 *       | check sum  |   pkt seq  |     nothing      |
 * 
 *       The first byte of each packet indicates the size of the payload
 *       (excluding this single-byte header)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rdt_struct.h"
#include "rdt_sender.h"

#define WINDOW_SIZE 20
#define TIME_OUT 0.2
#define MSG_BUF_SIZE 10000
#define HEAD_SIZE 7
#define MAX_PAYLOAD_SIZE (RDT_PKTSIZE - HEAD_SIZE)

static struct message *msg_buf;
static struct packet *window;
static int pkt_map_msg[WINDOW_SIZE];
static int pkt_map_cursor[WINDOW_SIZE];

static int window_head_pkt_num;
static int pkt_to_send_num;

static int current_msg_num;
static int next_msg_num;

static int msg_cursor;

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

/* sender initialization, called once at the very beginning */
void Sender_Init()
{
    //fprintf(stdout, "At %.2fs: sender initializing ...\n", GetSimulationTime());
    msg_buf = (struct message *)malloc(sizeof(struct message) * MSG_BUF_SIZE);
    window = (struct packet *)malloc(sizeof(struct packet) * WINDOW_SIZE);

    memset(msg_buf, 0, sizeof(struct message) * MSG_BUF_SIZE);
    memset(window, 0, sizeof(struct packet) * WINDOW_SIZE);

    window_head_pkt_num = 0;
    pkt_to_send_num = 0;

    current_msg_num = 0;
    next_msg_num = 0;

    msg_cursor = 0;
}

/* sender finalization, called once at the very end.
   you may find that you don't need it, in which case you can leave it blank.
   in certain cases, you might want to take this opportunity to release some 
   memory you allocated in Sender_init(). */

void Sender_Final()
{
    //fprintf(stdout, "At %.2fs: sender finalizing ...\n", GetSimulationTime());
    free(msg_buf);
    free(window);
}

static void Set_pkt(struct packet *pkt, int seq_num, char size, char *data)
{
    //fprintf(stdout, "S: packet seq num: %d, size: %d\n", seq_num, size);
    ASSERT(size != 0);
    ASSERT(data != NULL);
    memcpy(pkt->data + 2, &seq_num, sizeof(int));
    memcpy(pkt->data + 6, &size, sizeof(char));
    memcpy(pkt->data + HEAD_SIZE, data, size);
    short sum = checksum(pkt);
    memcpy(pkt->data, &sum, sizeof(short));
}

/* slide window and send packets in window */
static void slide_window_send()
{
    message cur_msg = msg_buf[current_msg_num % MSG_BUF_SIZE];
    //fprintf(stdout, "S: current msg num:%d, size:%d\n", current_msg_num, cur_msg.size);
    ASSERT(cur_msg.size > 0);
    while (pkt_to_send_num < window_head_pkt_num + WINDOW_SIZE &&
           current_msg_num < next_msg_num)
    {
        struct packet pkt;
        pkt_map_msg[pkt_to_send_num % WINDOW_SIZE] = current_msg_num;
        pkt_map_cursor[pkt_to_send_num % WINDOW_SIZE] = msg_cursor;
        if (cur_msg.size - msg_cursor > MAX_PAYLOAD_SIZE)
        {
            Set_pkt(&pkt, pkt_to_send_num, MAX_PAYLOAD_SIZE, cur_msg.data + msg_cursor);
            msg_cursor += MAX_PAYLOAD_SIZE;
        }
        else
        {
            Set_pkt(&pkt, pkt_to_send_num, cur_msg.size - msg_cursor, cur_msg.data + msg_cursor);
            msg_cursor = 0;
            current_msg_num++;
            if (current_msg_num < next_msg_num)
            {
                cur_msg = msg_buf[current_msg_num % MSG_BUF_SIZE];
                //fprintf(stdout, "S: current msg num:%d, size:%d\n", current_msg_num, cur_msg.size);
                ASSERT(cur_msg.size > 0);
            }
        }
        Sender_ToLowerLayer(&pkt);
        pkt_to_send_num++;
    }

    //fprintf(stdout, "S: slide_window_send finish!\n");
}

/* event handler, called when a message is passed from the upper layer at the 
   sender */
void Sender_FromUpperLayer(struct message *msg)
{
    if (next_msg_num - current_msg_num > MSG_BUF_SIZE)
    {
        //fprintf(stdout, "Message buffer is too small to hold waiting messages\n");
        return;
    }
    int index = next_msg_num % MSG_BUF_SIZE;

    msg_buf[index].size = msg->size + 4;
    //fprintf(stdout, "S: new message size:%d\n", msg_buf[index].size);

    char *data = (char *)malloc(msg_buf[index].size);
    memcpy(data, &(msg->size), sizeof(int));
    memcpy(data + sizeof(int), msg->data, msg->size);
    msg_buf[index].data = data;

    next_msg_num++;
    slide_window_send();

    if (!Sender_isTimerSet())
    {
        Sender_StartTimer(TIME_OUT);
    }
}

/* event handler, called when a packet is passed from the lower layer at the 
   sender */
void Sender_FromLowerLayer(struct packet *pkt)
{
    short ack_checksum;
    memcpy(&ack_checksum, pkt->data, sizeof(short));

    if (ack_checksum != checksum(pkt))
    {
        //fprintf(stdout, "S: Ack packet error!\n");
        return;
    }

    int ack_pkt_num;
    memcpy(&ack_pkt_num, pkt->data + sizeof(short), sizeof(int));
    //fprintf(stdout, "S: ack pkt num: %d\n", ack_pkt_num);
    if (ack_pkt_num >= window_head_pkt_num &&
        ack_pkt_num < pkt_to_send_num)
    {
        window_head_pkt_num = ack_pkt_num + 1;
        Sender_StartTimer(TIME_OUT);
        if (current_msg_num < next_msg_num)
        {
            slide_window_send();
        }

        // means that no message to send
        if (ack_pkt_num == pkt_to_send_num - 1)
        {
            Sender_StopTimer();
        }
    }
}

/* event handler, called when the timer expires */
void Sender_Timeout()
{
    //fprintf(stdout, "S: Time out!\n");
    Sender_StartTimer(TIME_OUT);

    for (int i = 0; i < pkt_to_send_num - window_head_pkt_num; i++)
    {
        //fprintf(stdout, "S: seq_num:%d, cur_msg_num:%d, msg_cursor:%d\n", i + window_head_pkt_num, pkt_map_msg[(i + window_head_pkt_num) % WINDOW_SIZE], pkt_map_cursor[(i + window_head_pkt_num) % WINDOW_SIZE]);
    }
    pkt_to_send_num = window_head_pkt_num;
    current_msg_num = pkt_map_msg[window_head_pkt_num % WINDOW_SIZE];
    msg_cursor = pkt_map_cursor[window_head_pkt_num % WINDOW_SIZE];
    slide_window_send();
}
