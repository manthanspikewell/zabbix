/*
** Zabbix
** Copyright (C) 2001-2023 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "pg_service.h"
#include "pg_cache.h"
#include "zbxpgservice.h"
#include "zbxnix.h"
#include "zbxserialize.h"
#include "zbxthreads.h"

/******************************************************************************
 *                                                                            *
 * Purpose: move hosts between proxy groups in cache                          *
 *                                                                            *
 * Parameter: pgs     - [IN] proxy group service                              *
 *            message - [IN] IPC message with host relocation data            *
 *                                                                            *
 ******************************************************************************/
static void	pg_update_host_pgroup(zbx_pg_service_t *pgs, zbx_ipc_message_t *message)
{
	unsigned char	*ptr = message->data;
	zbx_uint64_t	hostid, srcid, dstid;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	pg_cache_lock(pgs->cache);

	pg_cache_update_groups(pgs->cache);

	while (ptr - message->data < message->size)
	{
		zbx_pg_group_t	*group;

		ptr += zbx_deserialize_value(ptr, &hostid);
		ptr += zbx_deserialize_value(ptr, &srcid);
		ptr += zbx_deserialize_value(ptr, &dstid);

		if (0 != srcid)
		{
			if (NULL != (group = (zbx_pg_group_t *)zbx_hashset_search(&pgs->cache->groups, &srcid)))
				pg_cache_group_remove_host(pgs->cache, group, hostid);
		}

		if (0 != dstid)
		{
			if (NULL != (group = (zbx_pg_group_t *)zbx_hashset_search(&pgs->cache->groups, &dstid)))
				pg_cache_group_add_host(pgs->cache, group, hostid);
		}
	}

	pg_cache_unlock(pgs->cache);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}

#define ZBX_PROXY_SYNC_NONE	0
#define ZBX_PROXY_SYNC_FULL	1
#define ZBX_PROXY_SYNC_PARTIAL	2

/******************************************************************************
 *                                                                            *
 * Purpose: get proxy configuration sync data                                 *
 *                                                                            *
 * Parameter: pgs     - [IN] proxy group service                              *
 *            message - [IN] IPC message with host relocation data            *
 *                                                                            *
 ******************************************************************************/
static void	pg_get_proxy_sync_data(zbx_pg_service_t *pgs, zbx_ipc_client_t *client, zbx_ipc_message_t *message)
{
	unsigned char	*ptr = message->data, *data, mode;
	zbx_uint64_t	proxyid, hostmap_revision;
	int		now;
	zbx_uint32_t	data_len;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	ptr += zbx_deserialize_value(ptr, &proxyid);
	(void)zbx_deserialize_value(ptr, &hostmap_revision);

	now = time(NULL);

	pg_cache_lock(pgs->cache);

	zbx_pg_proxy_t	*proxy;

	data_len = sizeof(unsigned char) + sizeof(zbx_uint64_t);

	if (NULL != (proxy = (zbx_pg_proxy_t *)zbx_hashset_search(&pgs->cache->proxies, &proxyid)))
	{
		if (SEC_PER_DAY <= now - proxy->sync_time)
		{
			zbx_vector_pg_host_clear(&proxy->deleted_group_hosts);
			mode = ZBX_PROXY_SYNC_FULL;
		}
		else
		{
			for (int i = 0; i < proxy->deleted_group_hosts.values_num;)
			{
				if (proxy->deleted_group_hosts.values[i].revision <= hostmap_revision)
					zbx_vector_pg_host_remove_noorder(&proxy->deleted_group_hosts, i);
				else
					i++;
			}

			data_len += sizeof(int) + proxy->deleted_group_hosts.values_num * sizeof(zbx_uint64_t);
			mode = ZBX_PROXY_SYNC_PARTIAL;
		}

		if (proxy->remote_hostmap_revision != hostmap_revision)
			proxy->sync_time = now;
	}
	else
		mode = ZBX_PROXY_SYNC_NONE;

	ptr = data =(unsigned char *)zbx_malloc(NULL, data_len);
	ptr += zbx_serialize_value(ptr, mode);
	ptr += zbx_serialize_value(ptr, pgs->cache->hpmap_revision);

	if (ZBX_PROXY_SYNC_PARTIAL == mode)
	{
		ptr += zbx_serialize_value(ptr, proxy->deleted_group_hosts.values_num);

		for (int i = 0; i < proxy->deleted_group_hosts.values_num; i++)
			ptr += zbx_serialize_value(ptr, proxy->deleted_group_hosts.values[i]);
	}

	pg_cache_unlock(pgs->cache);

	zbx_ipc_client_send(client, ZBX_IPC_PGM_PROXY_SYNC_DATA, data, data_len);
	zbx_free(data);

	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);
}


/******************************************************************************
 *                                                                            *
 * Purpose: proxy group service thread entry                                  *
 *                                                                            *
 ******************************************************************************/
static void	*pg_service_entry(void *data)
{
	zbx_pg_service_t	*pgs = (zbx_pg_service_t *)data;
	zbx_timespec_t		timeout = {1, 0};
	zbx_ipc_client_t	*client;
	zbx_ipc_message_t	*message;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	while (ZBX_IS_RUNNING())
	{
		(void)zbx_ipc_service_recv(&pgs->service, &timeout, &client, &message);

		if (NULL != message)
		{
			switch (message->code)
			{
				case ZBX_IPC_PGM_HOST_PGROUP_UPDATE:
					pg_update_host_pgroup(pgs, message);
					break;
				case ZBX_IPC_PGM_GET_PROXY_SYNC_DATA:
					pg_get_proxy_sync_data(pgs, client, message);
					break;
				case ZBX_IPC_PGM_STOP:
					goto out;
			}

			zbx_ipc_message_free(message);
		}

		if (NULL != client)
			zbx_ipc_client_release(client);
	}
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s()", __func__);

	return NULL;
}

/******************************************************************************
 *                                                                            *
 * Purpose: initialize proxy group service                                    *
 *                                                                            *
 ******************************************************************************/
int	pg_service_init(zbx_pg_service_t *pgs, zbx_pg_cache_t *cache, char **error)
{
	int	ret = FAIL;

	zabbix_log(LOG_LEVEL_DEBUG, "In %s()", __func__);

	if (FAIL == zbx_ipc_service_start(&pgs->service, ZBX_IPC_SERVICE_PG_MANAGER, error))
		goto out;

	pthread_attr_t	attr;
	int		err;

	zbx_pthread_init_attr(&attr);
	if (0 != (err = pthread_create(&pgs->thread, &attr, pg_service_entry, (void *)pgs)))
	{
		*error = zbx_dsprintf(NULL, "cannot create thread: %s", zbx_strerror(err));
		goto out;
	}

	pgs->cache = cache;

	ret = SUCCEED;
out:
	zabbix_log(LOG_LEVEL_DEBUG, "End of %s():%s", __func__, zbx_result_string(ret));

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: destroy proxy group service                                       *
 *                                                                            *
 ******************************************************************************/
void	pg_service_destroy(zbx_pg_service_t *pgs)
{
	zbx_ipc_socket_t	sock;
	char			*error = NULL;

	if (FAIL == zbx_ipc_socket_open(&sock, ZBX_IPC_SERVICE_PG_MANAGER, 0, &error))
	{
		zabbix_log(LOG_LEVEL_ERR, "cannot connect to to proxy group manager service: %s", error);
		zbx_free(error);
		return;
	}

	zbx_ipc_socket_write(&sock, ZBX_IPC_PGM_STOP, NULL, 0);
	zbx_ipc_socket_close(&sock);

	void	*retval;

	pthread_join(pgs->thread, &retval);
}
