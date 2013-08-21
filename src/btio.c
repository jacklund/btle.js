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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <bluetooth/bluetooth.h>
#include <bluetooth/l2cap.h>
#include <bluetooth/rfcomm.h>
#include <bluetooth/sco.h>

#include "btio.h"

#ifndef BT_FLUSHABLE
#define BT_FLUSHABLE	8
#endif

#define DEFAULT_DEFER_TIMEOUT 30

static BtIOType bt_io_get_type(uv_poll_t *handle)
{
	int sk = handle->io_watcher.fd;
	int domain, proto, err;
	socklen_t len;

	domain = 0;
	len = sizeof(domain);
	err = getsockopt(sk, SOL_SOCKET, SO_DOMAIN, &domain, &len);
	if (err < 0) {
		return BT_IO_INVALID;
	}

	if (domain != AF_BLUETOOTH) {
		return BT_IO_INVALID;
	}

	proto = 0;
	len = sizeof(proto);
	err = getsockopt(sk, SOL_SOCKET, SO_PROTOCOL, &proto, &len);
	if (err < 0) {
		return BT_IO_INVALID;
	}

	switch (proto) {
	case BTPROTO_RFCOMM:
		return BT_IO_RFCOMM;
	case BTPROTO_SCO:
		return BT_IO_SCO;
	case BTPROTO_L2CAP:
		return BT_IO_L2CAP;
	default:
		return BT_IO_INVALID;
	}
}

static int l2cap_bind(int sock, const bdaddr_t *src, uint8_t src_type,
				uint16_t psm, uint16_t cid)
{
	struct sockaddr_l2 addr;

	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, src);

	if (cid)
		addr.l2_cid = htobs(cid);
	else
		addr.l2_psm = htobs(psm);

	addr.l2_bdaddr_type = src_type;

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		int error = -errno;
		return error;
	}

	return 0;
}

static int l2cap_connect(uv_poll_t *handle, uv_poll_cb connect_cb, void* data, const bdaddr_t *dst, uint8_t dst_type,
						uint16_t psm, uint16_t cid)
{
	int err;
	struct sockaddr_l2 addr;

	printf("dst = %x\n", *dst);
	memset(&addr, 0, sizeof(addr));
	addr.l2_family = AF_BLUETOOTH;
	bacpy(&addr.l2_bdaddr, dst);
	if (cid)
		addr.l2_cid = htobs(cid);
	else
		addr.l2_psm = htobs(psm);

	addr.l2_bdaddr_type = dst_type;

	handle->data = data;
	err = connect(handle->io_watcher.fd, (struct sockaddr *) &addr, sizeof(addr));
	if (err < 0 && !(errno == EAGAIN || errno == EINPROGRESS))
		return -errno;

	uv_poll_start(handle, UV_WRITABLE, connect_cb);

	return 0;
}

static int l2cap_set_master(int sock, int master)
{
	int flags;
	socklen_t len;

	len = sizeof(flags);
	if (getsockopt(sock, SOL_L2CAP, L2CAP_LM, &flags, &len) < 0)
		return -errno;

	if (master) {
		if (flags & L2CAP_LM_MASTER)
			return 0;
		flags |= L2CAP_LM_MASTER;
	} else {
		if (!(flags & L2CAP_LM_MASTER))
			return 0;
		flags &= ~L2CAP_LM_MASTER;
	}

	if (setsockopt(sock, SOL_L2CAP, L2CAP_LM, &flags, sizeof(flags)) < 0)
		return -errno;

	return 0;
}

static int rfcomm_set_master(int sock, int master)
{
	int flags;
	socklen_t len;

	len = sizeof(flags);
	if (getsockopt(sock, SOL_RFCOMM, RFCOMM_LM, &flags, &len) < 0)
		return -errno;

	if (master) {
		if (flags & RFCOMM_LM_MASTER)
			return 0;
		flags |= RFCOMM_LM_MASTER;
	} else {
		if (!(flags & RFCOMM_LM_MASTER))
			return 0;
		flags &= ~RFCOMM_LM_MASTER;
	}

	if (setsockopt(sock, SOL_RFCOMM, RFCOMM_LM, &flags, sizeof(flags)) < 0)
		return -errno;

	return 0;
}

static int l2cap_set_lm(int sock, int level)
{
	int lm_map[] = {
		0,
		L2CAP_LM_AUTH,
		L2CAP_LM_AUTH | L2CAP_LM_ENCRYPT,
		L2CAP_LM_AUTH | L2CAP_LM_ENCRYPT | L2CAP_LM_SECURE,
	}, opt = lm_map[level];

	if (setsockopt(sock, SOL_L2CAP, L2CAP_LM, &opt, sizeof(opt)) < 0)
		return -errno;

	return 0;
}

static int rfcomm_set_lm(int sock, int level)
{
	int lm_map[] = {
		0,
		RFCOMM_LM_AUTH,
		RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT,
		RFCOMM_LM_AUTH | RFCOMM_LM_ENCRYPT | RFCOMM_LM_SECURE,
	}, opt = lm_map[level];

	if (setsockopt(sock, SOL_RFCOMM, RFCOMM_LM, &opt, sizeof(opt)) < 0)
		return -errno;

	return 0;
}

static bool set_sec_level(int sock, BtIOType type, int level)
{
	struct bt_security sec;
	int ret;

	if (level < BT_SECURITY_LOW || level > BT_SECURITY_HIGH) {
		return false;
	}

	memset(&sec, 0, sizeof(sec));
	sec.level = level;

	if (setsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &sec,
							sizeof(sec)) == 0)
		return true;

	if (errno != ENOPROTOOPT) {
		return false;
	}

	if (type == BT_IO_L2CAP)
		ret = l2cap_set_lm(sock, level);
	else
		ret = rfcomm_set_lm(sock, level);

	if (ret < 0) {
		return false;
	}

	return true;
}

static int l2cap_get_lm(int sock, int *sec_level)
{
	int opt;
	socklen_t len;

	len = sizeof(opt);
	if (getsockopt(sock, SOL_L2CAP, L2CAP_LM, &opt, &len) < 0)
		return -errno;

	*sec_level = 0;

	if (opt & L2CAP_LM_AUTH)
		*sec_level = BT_SECURITY_LOW;
	if (opt & L2CAP_LM_ENCRYPT)
		*sec_level = BT_SECURITY_MEDIUM;
	if (opt & L2CAP_LM_SECURE)
		*sec_level = BT_SECURITY_HIGH;

	return 0;
}

static int rfcomm_get_lm(int sock, int *sec_level)
{
	int opt;
	socklen_t len;

	len = sizeof(opt);
	if (getsockopt(sock, SOL_RFCOMM, RFCOMM_LM, &opt, &len) < 0)
		return -errno;

	*sec_level = 0;

	if (opt & RFCOMM_LM_AUTH)
		*sec_level = BT_SECURITY_LOW;
	if (opt & RFCOMM_LM_ENCRYPT)
		*sec_level = BT_SECURITY_MEDIUM;
	if (opt & RFCOMM_LM_SECURE)
		*sec_level = BT_SECURITY_HIGH;

	return 0;
}

static bool get_sec_level(int sock, BtIOType type, int *level)
{
	struct bt_security sec;
	socklen_t len;
	int ret;

	memset(&sec, 0, sizeof(sec));
	len = sizeof(sec);
	if (getsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &sec, &len) == 0) {
		*level = sec.level;
		return true;
	}

	if (errno != ENOPROTOOPT) {
		return false;
	}

	if (type == BT_IO_L2CAP)
		ret = l2cap_get_lm(sock, level);
	else
		ret = rfcomm_get_lm(sock, level);

	if (ret < 0) {
		return false;
	}

	return true;
}

static int l2cap_set_flushable(int sock, bool flushable)
{
	int f;

	f = flushable;
	if (setsockopt(sock, SOL_BLUETOOTH, BT_FLUSHABLE, &f, sizeof(f)) < 0)
		return -errno;

	return 0;
}

static int set_priority(int sock, uint32_t prio)
{
	if (setsockopt(sock, SOL_SOCKET, SO_PRIORITY, &prio, sizeof(prio)) < 0)
		return -errno;

	return 0;
}

static bool get_key_size(int sock, int *size)
{
	struct bt_security sec;
	socklen_t len;

	memset(&sec, 0, sizeof(sec));
	len = sizeof(sec);
	if (getsockopt(sock, SOL_BLUETOOTH, BT_SECURITY, &sec, &len) == 0) {
		*size = sec.key_size;
		return true;
	}

	return false;
}

static bool l2cap_set(int sock, int sec_level, uint16_t imtu,
				uint16_t omtu, uint8_t mode, int master,
				int flushable, uint32_t priority)
{
	if (imtu || omtu || mode) {
		struct l2cap_options l2o;
		socklen_t len;

		memset(&l2o, 0, sizeof(l2o));
		len = sizeof(l2o);
		if (getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &l2o,
								&len) < 0) {
			return false;
		}

		if (imtu)
			l2o.imtu = imtu;
		if (omtu)
			l2o.omtu = omtu;
		if (mode)
			l2o.mode = mode;

		if (setsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &l2o,
							sizeof(l2o)) < 0) {
			return false;
		}
	}

	if (master >= 0 && l2cap_set_master(sock, master) < 0) {
		return false;
	}

	if (flushable >= 0 && l2cap_set_flushable(sock, flushable) < 0) {
		return false;
	}

	if (priority > 0 && set_priority(sock, priority) < 0) {
		return false;
	}

	if (sec_level && !set_sec_level(sock, BT_IO_L2CAP, sec_level))
		return false;

	return true;
}

static int rfcomm_bind(int sock, const bdaddr_t *src, uint8_t channel)
{
	struct sockaddr_rc addr;

	memset(&addr, 0, sizeof(addr));
	addr.rc_family = AF_BLUETOOTH;
	bacpy(&addr.rc_bdaddr, src);
	addr.rc_channel = channel;

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		int error = -errno;
		return error;
	}

	return 0;
}

static int rfcomm_connect(uv_poll_t *handle, uv_poll_cb connect_cb, void* data, const bdaddr_t *dst, uint8_t channel)
{
	int err;
	struct sockaddr_rc addr;

	memset(&addr, 0, sizeof(addr));
	addr.rc_family = AF_BLUETOOTH;
	bacpy(&addr.rc_bdaddr, dst);
	addr.rc_channel = channel;

  handle->data = data;

	err = connect(handle->io_watcher.fd, (struct sockaddr *) &addr, sizeof(addr));
	if (err < 0 && !(errno == EAGAIN || errno == EINPROGRESS))
		return -errno;

  uv_poll_start(handle, UV_WRITABLE, connect_cb);

	return 0;
}

static bool rfcomm_set(int sock, int sec_level, int master)
{
	if (sec_level && !set_sec_level(sock, BT_IO_RFCOMM, sec_level))
		return false;

	if (master >= 0 && rfcomm_set_master(sock, master) < 0) {
		return false;
	}

	return true;
}

static int sco_bind(int sock, const bdaddr_t *src)
{
	struct sockaddr_sco addr;

	memset(&addr, 0, sizeof(addr));
	addr.sco_family = AF_BLUETOOTH;
	bacpy(&addr.sco_bdaddr, src);

	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		int error = -errno;
		return error;
	}

	return 0;
}

static int sco_connect(uv_poll_t *handle, uv_poll_cb connect_cb, void* data, const bdaddr_t *dst)
{
	struct sockaddr_sco addr;
	int err;

	memset(&addr, 0, sizeof(addr));
	addr.sco_family = AF_BLUETOOTH;
	bacpy(&addr.sco_bdaddr, dst);

  handle->data = data;

	err = connect(handle->io_watcher.fd, (struct sockaddr *) &addr, sizeof(addr));
	if (err < 0 && !(errno == EAGAIN || errno == EINPROGRESS))
		return -errno;

  uv_poll_start(handle, UV_WRITABLE, connect_cb);

	return 0;
}

static bool sco_set(int sock, uint16_t mtu)
{
	struct sco_options sco_opt;
	socklen_t len;

	if (!mtu)
		return true;

	len = sizeof(sco_opt);
	memset(&sco_opt, 0, len);
	if (getsockopt(sock, SOL_SCO, SCO_OPTIONS, &sco_opt, &len) < 0) {
		return false;
	}

	sco_opt.mtu = mtu;
	if (setsockopt(sock, SOL_SCO, SCO_OPTIONS, &sco_opt,
						sizeof(sco_opt)) < 0) {
		return false;
	}

	return true;
}

static bool get_peers(int sock, struct sockaddr *src, struct sockaddr *dst,
				socklen_t len)
{
	socklen_t olen;

	memset(src, 0, len);
	olen = len;
	if (getsockname(sock, src, &olen) < 0) {
		return false;
	}

	memset(dst, 0, len);
	olen = len;
	if (getpeername(sock, dst, &olen) < 0) {
		return false;
	}

	return true;
}

static int l2cap_get_info(int sock, uint16_t *handle, uint8_t *dev_class)
{
	struct l2cap_conninfo info;
	socklen_t len;

	len = sizeof(info);
	if (getsockopt(sock, SOL_L2CAP, L2CAP_CONNINFO, &info, &len) < 0)
		return -errno;

	if (handle)
		*handle = info.hci_handle;

	if (dev_class)
		memcpy(dev_class, info.dev_class, 3);

	return 0;
}

static int l2cap_get_flushable(int sock, bool *flushable)
{
	int f;
	socklen_t len;

	f = 0;
	len = sizeof(f);
	if (getsockopt(sock, SOL_BLUETOOTH, BT_FLUSHABLE, &f, &len) < 0)
		return -errno;

	if (f)
		*flushable = true;
	else
		*flushable = false;

	return 0;
}

static int get_priority(int sock, uint32_t *prio)
{
	socklen_t len;

	len = sizeof(*prio);
	if (getsockopt(sock, SOL_SOCKET, SO_PRIORITY, prio, &len) < 0)
		return -errno;

	return 0;
}

static bool l2cap_get(int sock, BtIOOption opt1, va_list args)
{
	BtIOOption opt = opt1;
	struct sockaddr_l2 src, dst;
	struct l2cap_options l2o;
	int flags;
	uint8_t dev_class[3];
	uint16_t handle;
	socklen_t len;
	bool flushable = false;
	uint32_t priority;

	len = sizeof(l2o);
	memset(&l2o, 0, len);
	if (getsockopt(sock, SOL_L2CAP, L2CAP_OPTIONS, &l2o, &len) < 0) {
		return false;
	}

	if (!get_peers(sock, (struct sockaddr *) &src,
				(struct sockaddr *) &dst, sizeof(src)))
		return false;

	while (opt != BT_IO_OPT_INVALID) {
		switch (opt) {
		case BT_IO_OPT_SOURCE:
			ba2str(&src.l2_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_SOURCE_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &src.l2_bdaddr);
			break;
		case BT_IO_OPT_DEST:
			ba2str(&dst.l2_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_DEST_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &dst.l2_bdaddr);
			break;
		case BT_IO_OPT_DEST_TYPE:
			return false;
		case BT_IO_OPT_DEFER_TIMEOUT:
			len = sizeof(int);
			if (getsockopt(sock, SOL_BLUETOOTH, BT_DEFER_SETUP,
					va_arg(args, int *), &len) < 0) {
				return false;
			}
			break;
		case BT_IO_OPT_SEC_LEVEL:
			if (!get_sec_level(sock, BT_IO_L2CAP,
						va_arg(args, int *)))
				return false;
			break;
		case BT_IO_OPT_KEY_SIZE:
			if (!get_key_size(sock, va_arg(args, int *)))
				return false;
			break;
		case BT_IO_OPT_PSM:
			*(va_arg(args, uint16_t *)) = src.l2_psm ?
					btohs(src.l2_psm) : btohs(dst.l2_psm);
			break;
		case BT_IO_OPT_CID:
			*(va_arg(args, uint16_t *)) = src.l2_cid ?
					btohs(src.l2_cid) : btohs(dst.l2_cid);
			break;
		case BT_IO_OPT_OMTU:
			*(va_arg(args, uint16_t *)) = l2o.omtu;
			break;
		case BT_IO_OPT_IMTU:
			*(va_arg(args, uint16_t *)) = l2o.imtu;
			break;
		case BT_IO_OPT_MASTER:
			len = sizeof(flags);
			if (getsockopt(sock, SOL_L2CAP, L2CAP_LM, &flags, &len) < 0) {
				return false;
			}
			*(va_arg(args, bool *)) =
				(flags & L2CAP_LM_MASTER) ? true : false;
			break;
		case BT_IO_OPT_HANDLE:
			if (l2cap_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			*(va_arg(args, uint16_t *)) = handle;
			break;
		case BT_IO_OPT_CLASS:
			if (l2cap_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			memcpy(va_arg(args, uint8_t *), dev_class, 3);
			break;
		case BT_IO_OPT_MODE:
			*(va_arg(args, uint8_t *)) = l2o.mode;
			break;
		case BT_IO_OPT_FLUSHABLE:
			if (l2cap_get_flushable(sock, &flushable) < 0) {
				return false;
			}
			*(va_arg(args, bool *)) = flushable;
			break;
		case BT_IO_OPT_PRIORITY:
			if (get_priority(sock, &priority) < 0) {
				return false;
			}
			*(va_arg(args, uint32_t *)) = priority;
			break;
		default:
			return false;
		}

		opt = va_arg(args, int);
	}

	return true;
}

static int rfcomm_get_info(int sock, uint16_t *handle, uint8_t *dev_class)
{
	struct rfcomm_conninfo info;
	socklen_t len;

	len = sizeof(info);
	if (getsockopt(sock, SOL_RFCOMM, RFCOMM_CONNINFO, &info, &len) < 0)
		return -errno;

	if (handle)
		*handle = info.hci_handle;

	if (dev_class)
		memcpy(dev_class, info.dev_class, 3);

	return 0;
}

static bool rfcomm_get(int sock, BtIOOption opt1, va_list args)
{
	BtIOOption opt = opt1;
	struct sockaddr_rc src, dst;
	int flags;
	socklen_t len;
	uint8_t dev_class[3];
	uint16_t handle;

	if (!get_peers(sock, (struct sockaddr *) &src,
				(struct sockaddr *) &dst, sizeof(src)))
		return false;

	while (opt != BT_IO_OPT_INVALID) {
		switch (opt) {
		case BT_IO_OPT_SOURCE:
			ba2str(&src.rc_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_SOURCE_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &src.rc_bdaddr);
			break;
		case BT_IO_OPT_DEST:
			ba2str(&dst.rc_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_DEST_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &dst.rc_bdaddr);
			break;
		case BT_IO_OPT_DEFER_TIMEOUT:
			len = sizeof(int);
			if (getsockopt(sock, SOL_BLUETOOTH, BT_DEFER_SETUP,
					va_arg(args, int *), &len) < 0) {
				return false;
			}
			break;
		case BT_IO_OPT_SEC_LEVEL:
			if (!get_sec_level(sock, BT_IO_RFCOMM,
						va_arg(args, int *)))
				return false;
			break;
		case BT_IO_OPT_CHANNEL:
			*(va_arg(args, uint8_t *)) = src.rc_channel ?
					src.rc_channel : dst.rc_channel;
			break;
		case BT_IO_OPT_SOURCE_CHANNEL:
			*(va_arg(args, uint8_t *)) = src.rc_channel;
			break;
		case BT_IO_OPT_DEST_CHANNEL:
			*(va_arg(args, uint8_t *)) = dst.rc_channel;
			break;
		case BT_IO_OPT_MASTER:
			len = sizeof(flags);
			if (getsockopt(sock, SOL_RFCOMM, RFCOMM_LM, &flags,
								&len) < 0) {
				return false;
			}
			*(va_arg(args, bool *)) =
				(flags & RFCOMM_LM_MASTER) ? true : false;
			break;
		case BT_IO_OPT_HANDLE:
			if (rfcomm_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			*(va_arg(args, uint16_t *)) = handle;
			break;
		case BT_IO_OPT_CLASS:
			if (rfcomm_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			memcpy(va_arg(args, uint8_t *), dev_class, 3);
			break;
		default:
			return false;
		}

		opt = va_arg(args, int);
	}

	return true;
}

static int sco_get_info(int sock, uint16_t *handle, uint8_t *dev_class)
{
	struct sco_conninfo info;
	socklen_t len;

	len = sizeof(info);
	if (getsockopt(sock, SOL_SCO, SCO_CONNINFO, &info, &len) < 0)
		return -errno;

	if (handle)
		*handle = info.hci_handle;

	if (dev_class)
		memcpy(dev_class, info.dev_class, 3);

	return 0;
}

static bool sco_get(int sock, BtIOOption opt1, va_list args)
{
	BtIOOption opt = opt1;
	struct sockaddr_sco src, dst;
	struct sco_options sco_opt;
	socklen_t len;
	uint8_t dev_class[3];
	uint16_t handle;

	len = sizeof(sco_opt);
	memset(&sco_opt, 0, len);
	if (getsockopt(sock, SOL_SCO, SCO_OPTIONS, &sco_opt, &len) < 0) {
		return false;
	}

	if (!get_peers(sock, (struct sockaddr *) &src,
				(struct sockaddr *) &dst, sizeof(src)))
		return false;

	while (opt != BT_IO_OPT_INVALID) {
		switch (opt) {
		case BT_IO_OPT_SOURCE:
			ba2str(&src.sco_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_SOURCE_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &src.sco_bdaddr);
			break;
		case BT_IO_OPT_DEST:
			ba2str(&dst.sco_bdaddr, va_arg(args, char *));
			break;
		case BT_IO_OPT_DEST_BDADDR:
			bacpy(va_arg(args, bdaddr_t *), &dst.sco_bdaddr);
			break;
		case BT_IO_OPT_MTU:
		case BT_IO_OPT_IMTU:
		case BT_IO_OPT_OMTU:
			*(va_arg(args, uint16_t *)) = sco_opt.mtu;
			break;
		case BT_IO_OPT_HANDLE:
			if (sco_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			*(va_arg(args, uint16_t *)) = handle;
			break;
		case BT_IO_OPT_CLASS:
			if (sco_get_info(sock, &handle, dev_class) < 0) {
				return false;
			}
			memcpy(va_arg(args, uint8_t *), dev_class, 3);
			break;
		default:
			return false;
		}

		opt = va_arg(args, int);
	}

	return true;
}

static bool get_valist(uv_poll_t *handle, BtIOType type, BtIOOption opt1, va_list args)
{
	int sock;

	sock = handle->io_watcher.fd;

	switch (type) {
	case BT_IO_L2CAP:
		return l2cap_get(sock, opt1, args);
	case BT_IO_RFCOMM:
		return rfcomm_get(sock, opt1, args);
	case BT_IO_SCO:
		return sco_get(sock, opt1, args);
	default:
		return false;
	}
}

bool bt_io_accept(uv_poll_t *handle, uv_poll_cb connect_cb)
{
	int sock;
	char c;
	struct pollfd pfd;

	sock = handle->io_watcher.fd;

	memset(&pfd, 0, sizeof(pfd));
	pfd.fd = sock;
	pfd.events = POLLOUT;

	if (poll(&pfd, 1, 0) < 0) {
		return false;
	}

	if (!(pfd.revents & POLLOUT)) {
		if (read(sock, &c, 1) < 0) {
			return false;
		}
	}

	return true;
}

bool bt_io_set(uv_poll_t *handle, struct set_opts *opts)
{
	int sock;
	BtIOType type;

	type = bt_io_get_type(handle);
	if (type == BT_IO_INVALID)
		return false;

	sock = handle->io_watcher.fd;

	switch (type) {
	case BT_IO_L2CAP:
		return l2cap_set(sock, opts->sec_level, opts->imtu, opts->omtu,
				opts->mode, opts->master, opts->flushable,
				opts->priority);
	case BT_IO_RFCOMM:
		return rfcomm_set(sock, opts->sec_level, opts->master);
	case BT_IO_SCO:
		return sco_set(sock, opts->mtu);
	default:
		return false;
	}

}

bool bt_io_get(uv_poll_t *handle, BtIOOption opt1, ...)
{
	va_list args;
	bool ret;
	BtIOType type;

	type = bt_io_get_type(handle);
	if (type == BT_IO_INVALID)
		return false;

	va_start(args, opt1);
	ret = get_valist(handle, type, opt1, args);
	va_end(args);

	return ret;
}

static uv_poll_t *create_handle(bool server, struct set_opts *opts)
{
	int sock, r;
	uv_poll_t *handle = NULL;

	switch (opts->type) {
	case BT_IO_L2CAP:
		sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_L2CAP);
		if (sock < 0) {
			return NULL;
		}
		if (l2cap_bind(sock, &opts->src, opts->src_type,
				server ? opts->psm : 0, opts->cid) < 0)
			goto failed;
		if (!l2cap_set(sock, opts->sec_level, opts->imtu, opts->omtu,
				opts->mode, opts->master, opts->flushable,
				opts->priority))
			goto failed;
		break;
	case BT_IO_RFCOMM:
		sock = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM);
		if (sock < 0) {
			return NULL;
		}
		if (rfcomm_bind(sock, &opts->src,
					server ? opts->channel : 0) < 0)
			goto failed;
		if (!rfcomm_set(sock, opts->sec_level, opts->master))
			goto failed;
		break;
	case BT_IO_SCO:
		sock = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BTPROTO_SCO);
		if (sock < 0) {
			return NULL;
		}
		if (sco_bind(sock, &opts->src) < 0)
			goto failed;
		if (!sco_set(sock, opts->mtu))
			goto failed;
		break;
	default:
		return NULL;
	}

	r = fcntl(sock, F_GETFL);
	if (r == -1) {
		//TODO: Handle error
	}

	r = fcntl(sock, F_SETFL, r | O_NONBLOCK);
	if (r == -1) {
		//TODO: Handle error
	}

	handle = malloc(sizeof(uv_poll_t));
	memset(handle, 0, sizeof(uv_poll_t));
	uv_poll_init_socket(uv_default_loop(), handle, sock);

	return handle;

failed:
	close(sock);

	return NULL;
}

uv_poll_t *bt_io_connect(uv_poll_cb connect_cb, void* data, struct set_opts* opts) 
{
	uv_poll_t *handle;
	int err;

	printf("set_opts, type = %d, src_type = %d, dst_type = %d, sec_level = %d, channel = %d, psm = %d, cid = %d\n mtu = %d, imtu = %d, omtu = %d, mode = %d, flushable = %d\n", opts->type, opts->src_type, opts->dst_type, opts->sec_level, opts->channel, opts->psm, opts->cid, opts->mtu, opts->imtu, opts->omtu, opts->mode, opts->flushable);

	handle = create_handle(false, opts);
	printf("Handle = %p\n", handle);
	if (handle == NULL)
		return NULL;

	switch (opts->type) {
	case BT_IO_L2CAP:
		err = l2cap_connect(handle, connect_cb, data, &opts->dst, opts->dst_type,
							opts->psm, opts->cid);
		break;
	case BT_IO_RFCOMM:
		err = rfcomm_connect(handle, connect_cb, data, &opts->dst, opts->channel);
		break;
	case BT_IO_SCO:
		err = sco_connect(handle, connect_cb, data, &opts->dst);
		break;
	default:
		return NULL;
	}

	printf("err = %d\n", err);
	if (err < 0) {
		uv_close((uv_handle_t *) handle, NULL);
		return NULL;
	}

	return handle;
}

uv_poll_t *bt_io_listen(uv_connection_cb connect_cb, void* data, struct set_opts* opts)
{
	uv_poll_t *handle;

	handle = create_handle(true, opts);
	if (handle == NULL)
		return NULL;

  handle->data = data;

	if (uv_listen((uv_stream_t *) handle, 5, connect_cb) < 0) {
		uv_close((uv_handle_t *) handle, NULL);
		return NULL;
	}

	return handle;
}
