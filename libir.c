/*
 * libir.c
 *
 *
 * author: BeJo Li
 * mail: bejo.mob@gmail.com
 */

#include <ir.h>

UINT8 checksum(UINT8 *buf, int len)
{
    UINT8 chk = 0;
    while (len-- >= 0) {
        chk += buf[len];
        chk = chk&0xff;
    }
    return chk;
}

int ir_serial_init(char *port, int *fd)
{

    DBG("IR init... with %s\n", port);
    struct termios tio;
    memset(&tio,0,sizeof(tio));

    tio.c_cflag=CS8|CREAD|CLOCAL;
    tio.c_cc[VMIN]=4;
    tio.c_cc[VTIME]=0;

    *fd = open(port, O_RDWR| O_NONBLOCK);
    tcflush(*fd, TCIFLUSH);
    if (tcsetattr(*fd, TCSANOW, &tio) == -1) {
        return IR_FAIL;
    }

    return IR_OK;
}

UINT8 *alloc_code(int size)
{
    return  (UINT8 *)malloc(sizeof(UINT8)*size);
}

void ir_print_data(UINT8 *code, int count)
{
#if 1
    int i = 0;
    DBG("Ready to print len=%d\n", count);
    for (i = 0; i < count; i++) {
        DBG("[%d] = %02x\n", i, code[i]);
    }
#endif
}


UINT8 *ir_open(int *ir_fd)
{
    CMD_HDR *hdr;
    static UINT8 *code = NULL;

    ir_serial_init(IR_SERIAL_TTY, ir_fd);
    code = alloc_code(IR_CMD_BUF);
    bzero(code, sizeof(IR_CMD_BUF));

    hdr = (CMD_HDR *)code;

    hdr->sync_key[0] = SYNC_KEY0;
    hdr->sync_key[1] = SYNC_KEY1;

    return code;
}    

int reply_exec(UINT8 *recv, CMD_INFO *info, int len, int more)
{
    struct IR_RECV {
        UINT8 data[IR_RECV_BUF];
        int len;
    };
    static struct IR_RECV ir_recv;

    UINT8 type = info->type;
    CMD_REPLY *reply = NULL;
    reply = (CMD_REPLY *)ir_recv.data;
    int i = 0;
    FILE *fd = NULL;

    // init ir receive buffer
    if (more == 0) {
        bzero(ir_recv.data, sizeof(ir_recv.data));
        ir_recv.len = 0;
    }

    DBG("Get data len:%d, ir_recv.len=%d\n", len, ir_recv.len);
    ir_print_data(recv, len);
    //copy recv to static buffer and buffer len check
    if (ir_recv.len+len > IR_RECV_BUF) {
        IR_PRINT("ERROR... Receiving buffer is %d and not enough\n", IR_RECV_BUF);
        return IR_FAIL;
    }
    memcpy(ir_recv.data+ir_recv.len, recv, len);
    ir_recv.len += len;
    // Check is it end?
    if ((ir_recv.len-1) != reply->len) {
        DBG("PENDING... Data is fragment\n"
                "There are still data in queue\n"
                "Receive data len %d is different from Header %d, ir_recv.len=%d\n", len-1, reply->len, ir_recv.len);
        ir_print_data(ir_recv.data, ir_recv.len);
        return IR_EXCEPT;
    }    

    // Error handle
    if (reply->status == ERR) {
        IR_PRINT("ERROR... Get error data\n");
        return IR_FAIL;
    }
    // reply checksum is the last bits
    if (ir_recv.data[reply->len] != checksum(ir_recv.data, reply->len)) {
        IR_PRINT("ERROR... Receive checksum error, rev:%x calc:%x\n", ir_recv.data[reply->len], checksum(ir_recv.data, len-1));
        return IR_FAIL;
    }
    DBG("Reveive checksum is %x\n", checksum(ir_recv.data, len-1));
 
    if (type != reply->cmd) {
        IR_PRINT("ERROR... Send and Get cmd type is not match\n");
        IR_PRINT("Send Type=%x, Recv Type=%x\n", type, reply->cmd);
        return IR_FAIL;
    }

    //Show data in console
    switch(type) {
        // Need the second Ack
        case LEARN_IR_SHORT_DATA:
            if (reply->len != 0x03) { // It has payload with learning result
                DBG("LEARN_IR_SHORT_DATA: isLooseLearn=%d\n", info->isLooseLearn);
                //Calc: 
                //carrier Low  = Byte 08 - Byte 25 - Byte 36 - Byte 40
                //carrier High = Byte 09 - Byte 17 - Byte 19 - Byte 41
                UINT8 low = 0xff&(reply->data[8]-reply->data[25]-reply->data[36]-reply->data[40]);
                UINT8 high = 0xff&(reply->data[9]-reply->data[17]-reply->data[19]-reply->data[41]);
                DBG("Flash or Carrier:%02x %02x\n", high, low);
                if (info->isLooseLearn == 0x0) {
                    if (high == 0x0 && low == 0x0) {
                        IR_PRINT("ERROR... Get error data due to Loose Learn using\n");
                        return IR_FAIL;
                    }
                }
                fd = fopen(IR_LEARN_DATA_LOG, "w+");
                IR_PRINT("Data:");
                    for (i = 0; i <= (reply->len-3); i++) { // Just print payload: return is not with CRC bytes
                    IR_PRINT("%02x", (UINT8)reply->data[i]);
                    if (fd > 0) {
                        fprintf(fd, "%02x", reply->data[i]);
                    }
                }

                IR_PRINT("\n");
                fprintf(fd, "\n");
                if (fd > 0) {
                    fclose(fd);
                }
                break;
            }
            DBG("Enter busy state and wait ACK2 for result\n");
            return  IR_LEARN_ACK2;
        case TX_LEARN_IR_DATA:
        case TX_IR_DATA:
            DBG("Enter busy state\n");
            return  IR_ACK2;
        case IR_CMD_ACK2:
            IR_PRINT("Data: Finish\n");
            break;
        default:
            IR_PRINT("Data:");
            for (i = 0; reply->data[i] != ir_recv.data[reply->len]; i++) {
                IR_PRINT("%x", reply->data[i]);
            }
            IR_PRINT("\n");
    }

// Clear frame
    bzero(ir_recv.data, sizeof(ir_recv.data));
    ir_recv.len = 0;

    return IR_OK;
}

int wait_reply(void *data)
{
    fd_set readfs;
    UINT8 recv[IR_RECV_BUF];
    int len = 0;
    struct timeval tv;
    int ret = 0, fragment = 0, ret_exec = 0, loop = 0;
    CMD_INFO *info = (CMD_INFO *)data;
    
    DBG("Read fd = %x\n", info->fd);
    DBG("Command type = %x\n", info->type);

    do {
 
        FD_ZERO(&readfs);
        FD_SET(info->fd, &readfs);
        //timeout 3s
        tv.tv_sec = 3;
        tv.tv_usec = 0;

        ret = select(info->fd+1, &readfs, NULL, NULL, &tv);
        if (-1 == ret) {
            IR_PRINT("Select I/O error \n");
            return IR_FAIL;
        }

        if (ret) {
            DBG("Select: Get data !!! and ret = %x\n", ret);
            if(FD_ISSET(info->fd, &readfs)) {
                bzero(recv, sizeof(recv));
                len = read(info->fd, &recv, sizeof(recv));
                ret_exec = reply_exec(recv, info, len, fragment);
                switch(ret_exec) {
                    case IR_EXCEPT:
                        fragment = 1;
                        loop = 1;
                        break;
                    case IR_LEARN_ACK2:
                        info->type = LEARN_IR_SHORT_DATA;
                        loop = 1;
                        break;
                    case IR_ACK2:
                        info->type = IR_CMD_ACK2;
                        loop = 1;
                        break;
                    default:
                        loop = 0;
                        fragment = 0;
                }
            }
        } else {
//            loop = 0;
            IR_PRINT("IR receive timeout\n");
        }
    } while (loop);

    close(info->fd);
    return  IR_OK;
}

int ir_pre_xmit(UINT8 type, UINT8 *data, int len)
{
    int ir_fd = 0;
    CMD_HDR *hdr;
    UINT8 *code = NULL;
    pthread_t ptid;
    CMD_INFO info;
    
    code = ir_open(&ir_fd);
    DBG("Get ir fd = %x\n", ir_fd);
    info.fd = ir_fd;
    info.type = type;

    if (pthread_create(&ptid, NULL, (void *)wait_reply, (void *)&info) != 0) {
        IR_PRINT("Create rev thread fail...\n");
        return IR_FAIL;
    }

    hdr = (CMD_HDR *)code;
    hdr->cmd_id = type;
    switch(type) {
        case TX_LEARN_IR_DATA:
        case TX_IR_DATA:
            DBG("IR TX payload len = %d\n", len);
            memcpy(hdr->data, data, len);
            hdr->len = sizeof(CMD_HDR)+len;
            break;
        case LEARN_IR_SHORT_DATA:
            info.isLooseLearn = data[0];
        default:
            hdr->len = sizeof(CMD_HDR);
    }

    *(code+hdr->len) = checksum(code, hdr->len);
    ir_print_data(code, 10);
   
    // +1 for checksum bit
    write(ir_fd, code, hdr->len+1);
    free(code);
    
    pthread_join(ptid, NULL);
    
    return IR_OK;

}

int wakeup(int argc, char **argv)
{
    DBG("Try IR wake up\n");
    ir_pre_xmit(WAKE_UP, NULL, 0);
    return IR_OK;
}

int chip_revision(int argc, char **argv)
{
    DBG("Try IR get Chip revision\n");
    ir_pre_xmit(GET_REVISION, NULL, 0);
    return IR_OK;
}

int chip_id(int argc, char **argv)
{
    DBG("Try IR get Chip ID\n");
    ir_pre_xmit(GET_ID, NULL, 0);
    return IR_OK;
}

int chip_sign(int argc, char **argv)
{
    DBG("Try IR get Chip Signature\n");
    ir_pre_xmit(GET_IR_SIGNATURE, NULL, 0);    
    return IR_OK;
}

void data_cp(UINT8 *data, char *in, int len)
{
    int i = 0, j = 0;
    char bits[3] = {0, 0, '\0'};
    for(i = 0, j = 0; i < len; i = i+2, j++) {
        bits[0] = in[i];
        bits[1] = in[i+1];
        data[j] = strtol(bits, NULL, 16);
    }
    
}

int ir_tx(int argc, char **argv)
{
    DBG("Try IR TX\n");
    UINT8 data[250];
    int len = 0;

    if (argc != 3||strlen(argv[2])%2) {
        IR_PRINT("Usage: %s tx <data in HEX>\n"
                "Example: %s tx 00112233\n", argv[0], argv[0]);
        return IR_FAIL;
    }

    len = strlen(argv[2]);
    DBG("tx user-define data len is %d\n", len);

    bzero(data, sizeof(data));
    TX_DATA *tx_pl = (TX_DATA *)data;
    tx_pl->tx_type = SINGLE_TX;
    tx_pl->custom_code[0] = CUSTOM_CODE0;
    tx_pl->custom_code[1] = CUSTOM_CODE1;
    tx_pl->custom_code[2] = CUSTOM_CODE2;
    tx_pl->custom_code[3] = CUSTOM_CODE3;

    data_cp(tx_pl->data, argv[2], len);
    
    ir_pre_xmit(TX_IR_DATA, data, (len/2)+sizeof(TX_DATA));

    return IR_OK;
}

int ir_loop_sparkle(int argc, char **argv)
{
    DBG("Enter loop IR sparkle...\n");
    int secN = 0, conti = 0;
    UINT8 data[250];
    int len = 0;

    char sample[] = "nothing\n";

    if (argc != 3) {
        IR_PRINT("Usage: %s loopsparkle [0~60s]\n"
                "Example: %s loopsparkle 3\n", argv[0], argv[0]);
        return IR_FAIL;
    }

    secN = atoi(argv[2]);
    if (secN < 0 || secN > 60) {
        IR_PRINT("Time interval is invalid\n Auto set to 0\n");
//Special case: continue mode for reset testing
        if (secN == -1) {
            conti = 1;
            IR_PRINT("\n\n***You will Enter continous TX mode***\n"
                        "Please use <IR reset> to STOP\n\n");
        }
        secN = 0;
    }

    IR_PRINT("Start sparkle IR each %d second\n", secN);
    len = strlen(sample);
    bzero(data, sizeof(data));
    TX_DATA *tx_pl = (TX_DATA *)data;
    if (conti == 1) {
        tx_pl->tx_type = CONTI_TX;
    } else {
        tx_pl->tx_type = SINGLE_TX;
    }
    tx_pl->custom_code[0] = 0x00;
    tx_pl->custom_code[1] = 0x00;
    tx_pl->custom_code[2] = 0x00;
    tx_pl->custom_code[3] = 0x00;

    data_cp(tx_pl->data, sample, len);
    do {
        ir_pre_xmit(TX_IR_DATA, data, (len/2)+sizeof(TX_DATA));
        sleep(secN);
    } while (secN);

    return IR_OK;
}

int ir_stop_tx(int argc, char **argv)
{
    DBG("Try IR STOP\n");
    UINT8 *code = NULL;
    int ir_fd = 0;

    code = ir_open(&ir_fd);
    //Single command TYPE ONLY
    write(ir_fd, 0x0, 1);
    free(code);

    return IR_OK;
}

int ir_reset(int argc, char **argv)
{
    DBG("Try IR Reset MCU\n");

// It is GPIO_20
    system("echo 1 > /proc/GPIO_IR_MCU_RESET");
    sleep(1);
    system("echo 0 > /proc/GPIO_IR_MCU_RESET");
    IR_PRINT("IR MCU Reset done...\n");

    return IR_OK;
}

int ir_short_learn(int argc, char **argv)
{
    UINT8 isLooseLearn = 0x0;

    if (argc == 3 && (atoi(argv[2]) >0)) {
        isLooseLearn = 0x1;
    }

    DBG("Try IR enter short learning state for AV device\n");
    IR_PRINT("Learning IR short data\n"
        "This is for a sort of AV device\n"
        "Please push key for your IR signal durning 15 Sec.\n");

    ir_pre_xmit(LEARN_IR_SHORT_DATA, &isLooseLearn, 0);
    return IR_OK;
}

int ir_learn_tx(int argc, char **argv)
{
    DBG("Try IR TX learning result\n");
    UINT8 data[250];
    int len = 0;

    if (argc != 3||strlen(argv[2])%2) {
        IR_PRINT("Usage: %s learntx <data in HEX>\n"
                "Example: %s learntx 00112233\n", argv[0], argv[0]);
        return IR_FAIL;
    }

    len = strlen(argv[2]);
    DBG("tx learning data len is %d\n", len);

    bzero(data, sizeof(data));
    TX_LEARN_DATA *tx_pl = (TX_LEARN_DATA *)data;
    tx_pl->tx_type = SINGLE_TX;

    data_cp(tx_pl->data, argv[2], len);

    ir_pre_xmit(TX_LEARN_IR_DATA, data, (len/2)+sizeof(TX_LEARN_DATA));

    return IR_OK;
}

int ir_pre_learn_tx(int argc, char **argv)
{
    FILE *fd = NULL;
    char *cmd[3];
    char str[512];

    DBG("Try IR TX pre-record learning in %s\n", IR_LEARN_DATA_LOG);
    if (argc != 2) {
        IR_PRINT("Usage: %s prelearntx \nData is in %s\n", argv[0], IR_LEARN_DATA_LOG);
        return IR_FAIL;
    }
    
    fd = fopen(IR_LEARN_DATA_LOG, "r");
    if (fd == NULL) {
        IR_PRINT("ERROR Data%s open fail\n", IR_LEARN_DATA_LOG);
        return IR_FAIL;
    }

    bzero(str, sizeof(str));
    fscanf(fd, "%s\n", str);
    fclose(fd);
    cmd[0] = strdup("ir");
    cmd[1] = strdup("learntx");
    cmd[2] = strdup(str);

    DBG("Data:%s\nLen:%d\n", cmd[2], strlen(cmd[2]));
    ir_learn_tx(3, cmd);
    
    return IR_OK;
}
