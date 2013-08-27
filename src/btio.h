/*
 *
 *  BlueZ - Bluetooth protocol stack for Linux
 *
 *  Copyright (C) 2009-2010  Marcel Holtmann <marcel@holtmann.org>
 *  Copyright (C) 2009-2010  Nokia Corporation
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/*
 * This code was originally part of the BlueZ protocol stack (hence the comments above)
 * but has been heavily modified to remove dependencies on glib, and make it a true low-level
 * protocol stack.
 */
#ifndef BT_IO_H
#define BT_IO_H

#include <stdbool.h>
#include <uv.h>
#include <bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Attribute Protocol Opcodes */
#define ATT_OP_ERROR			0x01
#define ATT_OP_MTU_REQ			0x02
#define ATT_OP_MTU_RESP			0x03
#define ATT_OP_FIND_INFO_REQ		0x04
#define ATT_OP_FIND_INFO_RESP		0x05
#define ATT_OP_FIND_BY_TYPE_REQ		0x06
#define ATT_OP_FIND_BY_TYPE_RESP	0x07
#define ATT_OP_READ_BY_TYPE_REQ		0x08
#define ATT_OP_READ_BY_TYPE_RESP	0x09
#define ATT_OP_READ_REQ			0x0A
#define ATT_OP_READ_RESP		0x0B
#define ATT_OP_READ_BLOB_REQ		0x0C
#define ATT_OP_READ_BLOB_RESP		0x0D
#define ATT_OP_READ_MULTI_REQ		0x0E
#define ATT_OP_READ_MULTI_RESP		0x0F
#define ATT_OP_READ_BY_GROUP_REQ	0x10
#define ATT_OP_READ_BY_GROUP_RESP	0x11
#define ATT_OP_WRITE_REQ		0x12
#define ATT_OP_WRITE_RESP		0x13
#define ATT_OP_WRITE_CMD		0x52
#define ATT_OP_PREP_WRITE_REQ		0x16
#define ATT_OP_PREP_WRITE_RESP		0x17
#define ATT_OP_EXEC_WRITE_REQ		0x18
#define ATT_OP_EXEC_WRITE_RESP		0x19
#define ATT_OP_HANDLE_NOTIFY		0x1B
#define ATT_OP_HANDLE_IND		0x1D
#define ATT_OP_HANDLE_CNF		0x1E
#define ATT_OP_SIGNED_WRITE_CMD		0xD2

/* Error codes for Error response PDU */
#define ATT_ECODE_INVALID_HANDLE		0x01
#define ATT_ECODE_READ_NOT_PERM			0x02
#define ATT_ECODE_WRITE_NOT_PERM		0x03
#define ATT_ECODE_INVALID_PDU			0x04
#define ATT_ECODE_AUTHENTICATION		0x05
#define ATT_ECODE_REQ_NOT_SUPP			0x06
#define ATT_ECODE_INVALID_OFFSET		0x07
#define ATT_ECODE_AUTHORIZATION			0x08
#define ATT_ECODE_PREP_QUEUE_FULL		0x09
#define ATT_ECODE_ATTR_NOT_FOUND		0x0A
#define ATT_ECODE_ATTR_NOT_LONG			0x0B
#define ATT_ECODE_INSUFF_ENCR_KEY_SIZE		0x0C
#define ATT_ECODE_INVAL_ATTR_VALUE_LEN		0x0D
#define ATT_ECODE_UNLIKELY			0x0E
#define ATT_ECODE_INSUFF_ENC			0x0F
#define ATT_ECODE_UNSUPP_GRP_TYPE		0x10
#define ATT_ECODE_INSUFF_RESOURCES		0x11
/* Application error */
#define ATT_ECODE_IO				0x80
#define ATT_ECODE_TIMEOUT			0x81
#define ATT_ECODE_ABORTED			0x82

/* Characteristic Property bit field */
#define ATT_CHAR_PROPER_BROADCAST		0x01
#define ATT_CHAR_PROPER_READ			0x02
#define ATT_CHAR_PROPER_WRITE_WITHOUT_RESP	0x04
#define ATT_CHAR_PROPER_WRITE			0x08
#define ATT_CHAR_PROPER_NOTIFY			0x10
#define ATT_CHAR_PROPER_INDICATE		0x20
#define ATT_CHAR_PROPER_AUTH			0x40
#define ATT_CHAR_PROPER_EXT_PROPER		0x80

#define ATT_MAX_VALUE_LEN			512
#define ATT_DEFAULT_L2CAP_MTU			48
#define ATT_DEFAULT_LE_MTU			23

#define ATT_CID					4
#define ATT_PSM					31

/* Flags for Execute Write Request Operation */
#define ATT_CANCEL_ALL_PREP_WRITES              0x00
#define ATT_WRITE_ALL_PREP_WRITES               0x01

/* Find Information Response Formats */
#define ATT_FIND_INFO_RESP_FMT_16BIT		0x01
#define ATT_FIND_INFO_RESP_FMT_128BIT		0x02

typedef enum {
	BT_IO_OPT_INVALID = 0,
	BT_IO_OPT_SOURCE,
	BT_IO_OPT_SOURCE_BDADDR,
	BT_IO_OPT_SOURCE_TYPE,
	BT_IO_OPT_DEST,
	BT_IO_OPT_DEST_BDADDR,
	BT_IO_OPT_DEST_TYPE,
	BT_IO_OPT_DEFER_TIMEOUT,
	BT_IO_OPT_SEC_LEVEL,
	BT_IO_OPT_KEY_SIZE,
	BT_IO_OPT_CHANNEL,
	BT_IO_OPT_SOURCE_CHANNEL,
	BT_IO_OPT_DEST_CHANNEL,
	BT_IO_OPT_PSM,
	BT_IO_OPT_CID,
	BT_IO_OPT_MTU,
	BT_IO_OPT_OMTU,
	BT_IO_OPT_IMTU,
	BT_IO_OPT_MASTER,
	BT_IO_OPT_HANDLE,
	BT_IO_OPT_CLASS,
	BT_IO_OPT_MODE,
	BT_IO_OPT_FLUSHABLE,
	BT_IO_OPT_PRIORITY,
} BtIOOption;

typedef enum {
	BT_IO_SEC_SDP = 0,
	BT_IO_SEC_LOW,
	BT_IO_SEC_MEDIUM,
	BT_IO_SEC_HIGH,
} BtIOSecLevel;

typedef enum {
	BT_IO_MODE_BASIC = 0,
	BT_IO_MODE_RETRANS,
	BT_IO_MODE_FLOWCTL,
	BT_IO_MODE_ERTM,
	BT_IO_MODE_STREAMING
} BtIOMode;

typedef enum {
	BT_IO_L2CAP,
	BT_IO_RFCOMM,
	BT_IO_SCO,
	BT_IO_INVALID,
} BtIOType;

struct set_opts {
	bdaddr_t src;
	bdaddr_t dst;
	BtIOType type;
	uint8_t src_type;
	uint8_t dst_type;
	int defer;
	int sec_level;
	uint8_t channel;
	uint16_t psm;
	uint16_t cid;
	uint16_t mtu;
	uint16_t imtu;
	uint16_t omtu;
	int master;
	uint8_t mode;
	int flushable;
	uint32_t priority;
};

static inline void att_put_u16(uint16_t src, void *dst)
{
	bt_put_unaligned(htobs(src), (uint16_t *) dst);
}

bool bt_io_accept(int sock);

bool bt_io_set(int sock, struct set_opts *opts);

bool bt_io_get(int sock, BtIOOption opt1, ...);

int bt_io_connect(struct set_opts* opts);

int bt_io_listen(struct set_opts* opts);

#ifdef __cplusplus
}
#endif

#endif
