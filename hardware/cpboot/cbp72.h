/* CP Binary offsets */
#define CBL_SIZE 0x4000
#define FSM_FILE_MODE	00700
#define MAX_FSMDATA_SIZE	0x40000
#define FSM_DATA_PATH		"/mnt/vendor/efs/fsm_data.bin"

/* CBP72 Flashless booting command */
#define CBP72_IMG_DL_READY	0xA100
#define CBP72_IMG_DL_START_REQ	0x9200
#define CBP72_IMG_DL_START_RESP	0xA301
#define CBP72_IMG_DL_REQ	0x9400
#define CBP72_IMG_DL_RESP	0xA501
#define CBP72_IMG_DL_DONE	0x9600
#define CBP72_IMG_DL_DONE_RESP	0xA701

#define MAX_PAYLOAD_SIZE 8168
#define START_INDEX	0x7F
#define HEADER	7 /*sizeof(dpram_dl_header)*/
#define END	3 /* dest + 3 7E*/

/* CBP72 RAMDUMP */
#define CBP72_DUMP_BUFF_SIZE	16128
#define CBP72_IMG_UL_START_REQ	0x9200
#define CBP72_IMG_UL_START_RESP	0xA301
#define CBP72_IMG_UL_READY	0x9400
#define CBP72_IMG_UL_SEND_REQ	0xA500
#define CBP72_IMG_UL_SEND_RESP	0x9601
#define CBP72_IMG_UL_DONE_REQ	0xA700
#define CBP72_IMG_UL_DONE_RESP	0x9801

enum dpram_img_type {
	IMG_CBL,	/*primary signed image*/
	IMG_MAIN,
	IMG_FSM,
	IMG_MAX_IDX,
};

struct cp_imgmap {
	char name[12];		/*Binary name*/
	unsigned bin_offset;	/*Binary offset*/
	unsigned mem_offset;	/*Memory offset*/
	unsigned size;		/*size*/
	unsigned reserved[2];	/*reserved*/
};

struct via_args {
	struct boot_args *cbd_args;
	struct cp_imgmap *img_tab;
	int bin_fd;
	int boot_fd;
	int link_fd;
};

struct image_buf {
	unsigned int length;
	unsigned char *buf;
};

struct dpram_boot_frame {
	unsigned req; 	/* AP to CP Message */
	unsigned res; 	/* CP to AP Response */
	ssize_t len; 		/* request size*/
	unsigned offset; 	/* offset to write */
	char data[MAX_PAYLOAD_SIZE + HEADER + END];
};
struct dpram_dl_header {
	unsigned char start_index;
	unsigned short nframes;
	unsigned short curframe;
	unsigned short len;
} __packed;

struct Msg_header {
	unsigned char Sync;
	unsigned char PacketLen;
	unsigned char MsgId ;
} __packed;

struct Header_Msg {
	struct Msg_header Header; /*header info *img->buf*/
	unsigned int LoadAddr;
	unsigned int ExecuteAddr;
} __packed;

struct Checksum_Msg {
	struct Msg_header Header;
	unsigned int Checksum;
} __packed;

typedef enum {
	BOOT_UARTMSG_ACK = 0,
	BOOT_UARTMSG_NACK,
	BOOT_UARTMSG_HANDSHAKE,
	BOOT_UARTMSG_HEADER,
	BOOT_UARTMSG_DATA,
	BOOT_UARTMSG_CHECKSUM
} BootSwFromUart_MsgId;

struct dpram_dump_arg {
	char *buff;
	int buff_size;
	unsigned req;
	unsigned resp;
	unsigned cmd; /*if (cmd = 1) for cmd */
};
