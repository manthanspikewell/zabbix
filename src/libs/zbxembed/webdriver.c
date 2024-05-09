/*
** Zabbix
** Copyright (C) 2001-2024 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the envied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

#include "webdriver.h"
#include "zbxcommon.h"
#include "zbxstr.h"

#ifdef HAVE_LIBCURL

#define WEBDRIVER_INVALID_SESSIONID_ERROR	"invalid session id"

static size_t	curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t		r_size = size * nmemb;
	zbx_webdriver_t	*wd = (zbx_webdriver_t *)userdata;

	zbx_str_memcpy_alloc(&wd->data, &wd->data_alloc, &wd->data_offset, (const char *)ptr, r_size);

	return r_size;
}

static size_t	curl_header_cb(void *ptr, size_t size, size_t nmemb, void *userdata)
{
	size_t		r_size = size * nmemb;
	zbx_webdriver_t	*wd = (zbx_webdriver_t *)userdata;

	zbx_strncpy_alloc(&wd->headers_in, &wd->headers_in_alloc, &wd->headers_in_offset, (const char *)ptr, r_size);

	return r_size;
}

/******************************************************************************
 *                                                                            *
 * Purpose: utility function for checking curl attribute setting errors       *
 *                                                                            *
 ******************************************************************************/
static int	webdriver_curl_check_error(CURLcode err, CURLoption opt, char **error)
{
	if (CURLE_OK == err)
		return SUCCEED;

	*error = zbx_dsprintf(NULL, "cannot set cURL option %d: %s.", opt, curl_easy_strerror(err));

	return FAIL;
}

#define CURL_SETOPT(handle, option, value, error)	\
	webdriver_curl_check_error(curl_easy_setopt(handle, option, value), option, error)

/******************************************************************************
 *                                                                            *
 * Purpose: get value returned by webdriver {"value":<value>}                 *
 *                                                                            *
 * Parameters: response - [IN] webdriver response                             *
 *             jp       - [OUT] value contents                                *
 *                                simple - start/end points at <value> start  *
 *                                object/array - start/end points at the      *
 *                                              object/array start/end        *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCCEED - value was returned successfully                    *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
static int	webdriver_get_value(const char *response, struct zbx_json_parse *jp, char **error)
{
	struct zbx_json_parse	jp_resp;

	if (SUCCEED != zbx_json_open(response, &jp_resp))
	{
		*error = zbx_dsprintf(NULL, "cannot open webdriver response: %s", zbx_json_strerror());

		return FAIL;
	}

	if (SUCCEED != zbx_json_brackets_by_name(&jp_resp, "value", jp))
	{
		if (NULL == (jp->start = jp->end = zbx_json_pair_by_name(&jp_resp, "value")))
		{
			*error = zbx_dsprintf(NULL, "cannot parse webdriver response: %s", zbx_json_strerror());

			return FAIL;
		}
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: free webdriver error object                                       *
 *                                                                            *
 ******************************************************************************/
static void	webdriver_free_error(zbx_wd_error_t *err)
{
	zbx_free(err->error);
	zbx_free(err->message);
	zbx_free(err);
}

/******************************************************************************
 *                                                                            *
 * Purpose: create webdriver error object                                     *
 *                                                                            *
 * Return value: webdriver error object                                       *
 *                                                                            *
 ******************************************************************************/
static zbx_wd_error_t	*webdriver_create_error(int http_code, const char *error, const char *message)
{
	zbx_wd_error_t	*err;

	err = (zbx_wd_error_t *)zbx_malloc(NULL, sizeof(zbx_wd_error_t));
	err->http_code = http_code;
	err->error = zbx_strdup(NULL, error);
	err->message = zbx_strdup(NULL, message);

	return err;
}

/******************************************************************************
 *                                                                            *
 * Purpose: discard webdriver errors                                          *
 *                                                                            *
 ******************************************************************************/
void	webdriver_discard_error(zbx_webdriver_t *wd)
{
	if (NULL != wd->error)
	{
		webdriver_free_error(wd->error);
		wd->error = NULL;
	}

	zbx_free(wd->last_error_message);
}

/******************************************************************************
 *                                                                            *
 * Purpose: get error returned by webdriver                                   *
 *             {"error":<error>, "message":<message>}                         *
 *                                                                            *
 * Parameters: wd   - [IN] webdriver object                                   *
 *             jp   - [IN] webdriver response                                 *
 *             into - [OUT] error message                                     *
 *                                                                            *
 * Return value: SUCCEED - error was found                                    *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 * Comments: If error was found in response the webdriver error object will   *
 *           be created from curl http status code, error and message tag     *
 *           contents. The message will be returned as error message in info  *
 *           parameter.                                                       *
 *                                                                            *
 ******************************************************************************/
static int	webdriver_get_error(zbx_webdriver_t *wd, const struct zbx_json_parse *jp, char **info)
{
	size_t		error_alloc = 0, info_alloc = 0;
	char		*error = NULL;
	long		http_code;

	if (ZBX_JSON_TYPE_OBJECT != zbx_json_valuetype(jp->start))
		return FAIL;

	if (SUCCEED != zbx_json_value_by_name_dyn(jp, "error", &error, &error_alloc, NULL))
		return FAIL;

	/* return more informative message as error text */
	if (SUCCEED != zbx_json_value_by_name_dyn(jp, "message", info, &info_alloc, NULL))
		*info = zbx_strdup(NULL, "");

	if (CURLE_OK != curl_easy_getinfo(wd->handle, CURLINFO_RESPONSE_CODE, &http_code))
		http_code = 0;

	webdriver_discard_error(wd);
	wd->error = webdriver_create_error((int)http_code, error, *info);

	zbx_free(error);

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: performs webdriver query                                          *
 *                                                                            *
 * Parameters: wd   - [IN] webdriver object                                   *
 *             method   - [IN] HTTP method                                    *
 *             command - [IN] webdriver command (optional, can be NULL)       *
 *             data    - [IN] data to post (optional, can be NULL)            *
 *             jp      - [OUT] returned value (optional, can be NULL)         *
 *             error   - [OUT] error message                                  *
 *                                                                            *
 * Return value: SUCCEED - query was performed successfully                   *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 * Comments: The webdriver response is inspected for error value and if found *
 *           then error is returned.                                          *
 *                                                                            *
 ******************************************************************************/
static int	webdriver_session_query(zbx_webdriver_t *wd, const char *method, const char *command, const char *data,
		struct zbx_json_parse *jp, char **error)
{
	char			*url = NULL;
	size_t			url_alloc = 0, url_offset = 0;
	CURLcode		err;
	int			ret = FAIL;
	struct zbx_json_parse	jp_value;

	webdriver_discard_error(wd);

	zbx_snprintf_alloc(&url, &url_alloc, &url_offset, "%s/session", wd->endpoint);

	if (NULL != wd->session)
	{
		zbx_chrcpy_alloc(&url, &url_alloc, &url_offset, '/');
		zbx_strcpy_alloc(&url, &url_alloc, &url_offset, wd->session);
	}

	if (NULL != command)
	{
		if (NULL == wd->session && 0 != strcmp(method, "POST"))
		{
			*error = zbx_strdup(NULL, "webdriver session has not been opened");
			goto out;
		}

		zbx_chrcpy_alloc(&url, &url_alloc, &url_offset, '/');
		zbx_strcpy_alloc(&url, &url_alloc, &url_offset, command);
	}

	if (SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_URL, url, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_CUSTOMREQUEST, method, error))
	{
		goto out;
	}

	if (0 == strcmp(method, "POST"))
	{
		if (SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_POSTFIELDS, ZBX_NULL2EMPTY_STR(data), error))
			goto out;
	}
	else
	{
		if (SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_POST, 0L, error))
			goto out;
	}

	wd->data_offset = 0;

	if (CURLE_OK != (err = curl_easy_perform(wd->handle)))
	{
		*error = zbx_dsprintf(NULL, "cannot perform request %s session/%s: %s",
				method, ZBX_NULL2EMPTY_STR(command), curl_easy_strerror(err));
		goto out;
	}

	if (NULL == jp)
		jp = &jp_value;

	if (SUCCEED != webdriver_get_value(wd->data, jp, error) || SUCCEED == webdriver_get_error(wd, jp, error))
		goto out;

	ret = SUCCEED;
out:
	zbx_free(url);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get performance entries from performance object                   *
 *                                                                            *
 * Parameters: wd      - [IN] webdriver object                                *
 *             jp      - [OUT] performance entries                            *
 *             error   - [OUT] error message                                  *
 *                                                                            *
 * Return value: SUCCEED - query was performed successfully                   *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
static int	webdriver_get_perf_entries(zbx_webdriver_t *wd, struct zbx_json_parse *jp, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;
	const char	*script = "return window.performance.getEntries().filter("
						"(value, index, array)=>{"
							"return value.entryType!=='long-animation-frame'"
						"}"
					");";

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "script", script,  ZBX_JSON_TYPE_STRING);
	zbx_json_addarray(&json, "args");
	zbx_json_close(&json);

	if (SUCCEED != webdriver_session_query(wd, "POST", "execute/sync", json.buffer, jp, error))
		goto out;

	ret = SUCCEED;
out:
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: perform post navigation operations                                *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 * Comments: Currently only performance entries are gathered                  *
 *                                                                            *
 ******************************************************************************/
static void	webdriver_post_navigate_event(zbx_webdriver_t *wd)
{
	struct zbx_json_parse	jp;
	char			*error = NULL;

	if (SUCCEED != webdriver_get_perf_entries(wd, &jp, &error))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "cannot get performance entries: %s", error);
		zbx_free(error);

		goto out;
	}

	wd_perf_collect(&wd->perf, wd->bookmark, &jp);
out:
	zbx_free(wd->bookmark);
}

/******************************************************************************
 *                                                                            *
 * Purpose: close webdriver session                                           *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 ******************************************************************************/
static void	webdriver_close_session(zbx_webdriver_t *wd)
{
	char	*error = NULL;

	if (SUCCEED != webdriver_session_query(wd, "DELETE", NULL, NULL, NULL, &error))
	{
		zabbix_log(LOG_LEVEL_DEBUG, "cannot close webdriver session: %s", error);
		zbx_free(error);
	}
	else
		zabbix_log(LOG_LEVEL_DEBUG, "closed webdriver session %s", wd->session);
}

/******************************************************************************
 *                                                                            *
 * Purpose: create webdriver object                                           *
 *                                                                            *
 * Parameters: browser  - [IN] browser type                                   *
 *             endpoint - [IN] webdriver url                                  *
 *             sourceip - [IN] source ip (optional, can be NULL)              *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: created webdriver object or NULL in the case of error        *
 *                                                                            *
 ******************************************************************************/
zbx_webdriver_t	*webdriver_create(const char *endpoint, const char *sourceip, char **error)
{
	int		ret = FAIL;
	zbx_webdriver_t	*wd;

	wd = (zbx_webdriver_t *)zbx_malloc(NULL, sizeof(zbx_webdriver_t));
	memset(wd, 0, sizeof(zbx_webdriver_t));

	wd->endpoint = zbx_strdup(NULL, endpoint);

	if (NULL == (wd->handle = curl_easy_init()))
	{
		*error = zbx_dsprintf(NULL, "cannot initialize cURL library");
		goto out;
	}

	wd->headers = curl_slist_append(wd->headers, "Content-type: application/json; charset=utf-8");
	if (SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_HTTPHEADER, wd->headers, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_COOKIEFILE, "", error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_FOLLOWLOCATION, 1L, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_WRITEFUNCTION, curl_write_cb, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_WRITEDATA, wd, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_SSL_VERIFYPEER, 0L, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_SSL_VERIFYHOST, 0L, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_HEADERFUNCTION, curl_header_cb, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_HEADERDATA, wd, error) ||
			SUCCEED != CURL_SETOPT(wd->handle, CURLOPT_INTERFACE, sourceip, error))
	{
		goto out;
	}

	wd->refcount = 1;
	wd->screen_width = 1920;
	wd->screen_height = 1080;

	wd_perf_init(&wd->perf);

	ret = SUCCEED;
out:
	if (SUCCEED != ret)
	{
		webdriver_destroy(wd);
		wd = NULL;
	}

	return wd;
}

/******************************************************************************
 *                                                                            *
 * Purpose: destroy webdriver object                                          *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 ******************************************************************************/
void	webdriver_destroy(zbx_webdriver_t *wd)
{
	zabbix_log(LOG_LEVEL_DEBUG, "webdriver_destroy()");

	wd_perf_destroy(&wd->perf);

	if (NULL != wd->session)
	{
		webdriver_close_session(wd);
		zbx_free(wd->session);
	}

	if (NULL != wd->handle)
		curl_easy_cleanup(wd->handle);

	if (NULL != wd->headers)
		curl_slist_free_all(wd->headers);

	zbx_free(wd->endpoint);
	zbx_free(wd->data);
	zbx_free(wd->headers_in);

	zbx_free(wd->last_error_message);

	if (NULL != wd->error)
		webdriver_free_error(wd->error);

	zbx_free(wd);
}

/******************************************************************************
 *                                                                            *
 * Purpose: release webdriver object (decrement reference count and destroy   *
 *          if necessary)                                                     *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 ******************************************************************************/
void	webdriver_release(zbx_webdriver_t *wd)
{
	zabbix_log(LOG_LEVEL_DEBUG, "webdriver_release()");

	if (0 == --wd->refcount)
		webdriver_destroy(wd);
}

/******************************************************************************
 *                                                                            *
 * Purpose: increment webdriver object reference count                        *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 ******************************************************************************/
zbx_webdriver_t	*webdriver_addref(zbx_webdriver_t *wd)
{
	wd->refcount++;

	return wd;
}

/******************************************************************************
 *                                                                            *
 * Purpose: open webdriver session                                            *
 *                                                                            *
 * Parameters: wd    - [IN] webdriver object                                  *
 *             capabilities - [IN] browser capabilities (JSON formatted)      *
 *             error - [OUT] error message                                    *
 *                                                                            *
 * Return value: SUCEED - session was opened successfully                     *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_open_session(zbx_webdriver_t *wd, const char *capabilities, char **error)
{
#define WEBDRIVER_DEFAULT_SCREEN_WIDTH	1920
#define WEBDRIVER_DEFAULT_SCREEN_HEIGHT	1080

	int			ret = FAIL;
	struct zbx_json_parse	jp;
	size_t			session_alloc = 0;

	if (SUCCEED != webdriver_session_query(wd, "POST", NULL, capabilities, &jp, error))
		goto out;

	if (SUCCEED != zbx_json_value_by_name_dyn(&jp, "sessionId", &wd->session, &session_alloc, NULL))
	{
		*error = zbx_dsprintf(NULL, "cannot read sessionId: %s", zbx_json_strerror());
		goto out;
	}

	if (SUCCEED != webdriver_set_screen_size(wd, WEBDRIVER_DEFAULT_SCREEN_WIDTH, WEBDRIVER_DEFAULT_SCREEN_HEIGHT,
			error))
	{
		goto out;
	}

	zabbix_log(LOG_LEVEL_DEBUG, "opened webdriver session with sessionid:%s", wd->session);

	wd->create_time = zbx_time();

	ret = SUCCEED;
out:
	return ret;

#undef WEBDRIVER_DEFAULT_SCREEN_WIDTH
#undef WEBDRIVER_DEFAULT_SCREEN_HEIGHT
}

/******************************************************************************
 *                                                                            *
 * Purpose: navigate to url                                                   *
 *                                                                            *
 * Parameters: wd    - [IN] webdriver object                                  *
 *             url   - [IN] target url                                        *
 *             error - [OUT] error message                                    *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_url(zbx_webdriver_t *wd, const char *url, char **error)
{
	struct zbx_json	json;
	int		ret;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "url", url, ZBX_JSON_TYPE_STRING);

	if (SUCCEED == (ret = webdriver_session_query(wd, "POST", "url", json.buffer, NULL, error)))
		webdriver_post_navigate_event(wd);

	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get current url                                                   *
 *                                                                            *
 * Parameters: wd    - [IN] webdriver object                                  *
 *             url   - [OUT] current url                                      *
 *             error - [OUT] error message                                    *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_url(zbx_webdriver_t *wd, char **url, char **error)
{
	struct zbx_json_parse	jp;
	size_t			url_alloc = 0;

	if (SUCCEED != webdriver_session_query(wd, "GET", "url", NULL, &jp, error))
		return FAIL;

	if (NULL == zbx_json_decodevalue_dyn(jp.start, url, &url_alloc, NULL))
	{
		*error = zbx_dsprintf(NULL, "cannot read url: %s", zbx_json_strerror());

		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: find single element                                               *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             strategy - [IN] xpath, css selector, ... (see webdriver doc)   *
 *             selector - [IN] (see webdriver doc)                            *
 *             element  - [OUT] element id                                    *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_find_element(zbx_webdriver_t *wd, const char *strategy, const char *selector, char **element,
		char **error)
{
	struct zbx_json		json;
	int			ret = FAIL;
	struct zbx_json_parse	jp;
	size_t			element_alloc = 0;
	char			buffer[MAX_STRING_LEN];
	const char		*value;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "using", strategy, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&json, "value", selector, ZBX_JSON_TYPE_STRING);

	if (SUCCEED != webdriver_session_query(wd, "POST", "element", json.buffer, &jp, error))
	{
		/* throw exception in the case of connection errors */
		if (404 != wd->error->http_code || 0 == strcmp(wd->error->error, WEBDRIVER_INVALID_SESSIONID_ERROR))
			goto out;

		/* otherwise log the error and return NULL element */
		zabbix_log(LOG_LEVEL_DEBUG, "cannot find element with strategy:'%s' and selector:'%s': %s",
				strategy, selector, *error);

		webdriver_free_error(wd->error);
		wd->error = NULL;
		zbx_free(*error);
		*element = NULL;
	}
	else if (NULL == (value = zbx_json_pair_next(&jp, NULL, buffer, sizeof(buffer))) ||
			NULL == zbx_json_decodevalue_dyn(value, element, &element_alloc, NULL))
	{
		*element = NULL;
		zabbix_log(LOG_LEVEL_DEBUG, "cannot read element: %s", zbx_json_strerror());

		goto out;
	}

	ret = SUCCEED;
out:
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: find multiple elements                                            *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             strategy - [IN] xpath, css selector, ... (see webdriver doc)   *
 *             selector - [IN] (see webdriver doc)                            *
 *             elements - [OUT] vector of element identifiers                 *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_find_elements(zbx_webdriver_t *wd, const char *strategy, const char *selector,
		zbx_vector_str_t *elements, char **error)
{
	struct zbx_json		json;
	int			ret = FAIL;
	struct zbx_json_parse	jp;
	char			buffer[MAX_STRING_LEN];

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "using", strategy, ZBX_JSON_TYPE_STRING);
	zbx_json_addstring(&json, "value", selector, ZBX_JSON_TYPE_STRING);

	if (SUCCEED != webdriver_session_query(wd, "POST", "elements", json.buffer, &jp, error))
	{
		/* throw exception in the case of connection errors */
		if (0 == wd->data_offset)
			goto out;

		/* otherwise log the error and return NULL element */
		zabbix_log(LOG_LEVEL_DEBUG, "cannot find element with strategy:'%s' and selector:'%s': %s",
				strategy, selector, *error);

		zbx_free(*error);
	}
	else
	{
		struct zbx_json_parse	jp_elements;

		if (SUCCEED == zbx_json_brackets_open(jp.start, &jp_elements))
		{
			const char	*p = NULL;

			while (NULL != (p = zbx_json_next(&jp_elements, p)))
			{
				struct zbx_json_parse	jp_element;
				const char		*value;
				char			*element = NULL;
				size_t			element_alloc = 0;

				if (SUCCEED != zbx_json_brackets_open(p, &jp_element))
					continue;

				if (NULL == (value = zbx_json_pair_next(&jp_element, NULL, buffer, sizeof(buffer))))
					continue;

				if (NULL != zbx_json_decodevalue_dyn(value, &element, &element_alloc, NULL))
					zbx_vector_str_append(elements, element);
			}
		}
	}

	ret = SUCCEED;
out:
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: send keys to element                                              *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             element  - [IN] target element identifier                      *
 *             keys     - [IN] keys sto send                                  *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_send_keys_to_element(zbx_webdriver_t *wd, const char *element, const char *keys, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;
	char		*command = NULL;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "text", keys, ZBX_JSON_TYPE_STRING);

	command = zbx_dsprintf(NULL, "element/%s/value", element);

	if (SUCCEED == (ret = webdriver_session_query(wd, "POST", command, json.buffer, NULL, error)))
		webdriver_post_navigate_event(wd);

	zbx_free(command);
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: click element                                                     *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             element  - [IN] target element identifier                      *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_click_element(zbx_webdriver_t *wd, const char *element, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;
	char		*command = NULL;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "id", element,  ZBX_JSON_TYPE_STRING);

	command = zbx_dsprintf(NULL, "element/%s/click", element);

	if (SUCCEED == (ret = webdriver_session_query(wd, "POST", command, json.buffer, NULL, error)))
		webdriver_post_navigate_event(wd);

	zbx_free(command);
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: clear element                                                     *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             element  - [IN] target element identifier                      *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_clear_element(zbx_webdriver_t *wd, const char *element, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;
	char		*command = NULL;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "id", element,  ZBX_JSON_TYPE_STRING);

	command = zbx_dsprintf(NULL, "element/%s/clear", element);

	if (SUCCEED == (ret = webdriver_session_query(wd, "POST", command, json.buffer, NULL, error)))
		webdriver_post_navigate_event(wd);

	zbx_free(command);
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get element information (attributes, properties, text)            *
 *                                                                            *
 * Parameters: wd       - [IN] webdriver object                               *
 *             element  - [IN] target element identifier                      *
 *             info     - [IN] information to get (attribute, property, text) *
 *             name     - [IN] attribute/property name, NULL for text         *
 *             value    - [OUT] attribute/property/text value                 *
 *             error    - [OUT] error message                                 *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_element_info(zbx_webdriver_t *wd, const char *element, const char *info, const char *name,
		char **value, char **error)
{
	int			ret = FAIL;
	char			*command = NULL;
	size_t			command_alloc = 0, command_offset = 0, value_alloc = 0;
	struct zbx_json_parse	jp;

	zbx_snprintf_alloc(&command, &command_alloc, &command_offset, "element/%s/%s", element, info);
	if (NULL != name)
		zbx_snprintf_alloc(&command, &command_alloc, &command_offset, "/%s", name);

	if (SUCCEED != webdriver_session_query(wd, "GET", command, NULL, &jp, error))
		goto out;

	if (NULL == zbx_json_decodevalue_dyn(jp.start, value, &value_alloc, NULL))
	{
		*error = zbx_dsprintf(NULL, "cannot parse %s value: %s", info, zbx_json_strerror());

		goto out;
	}

	ret = SUCCEED;
out:
	zbx_free(command);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: set webdriver timeouts                                            *
 *                                                                            *
 * Parameters: wd                - [IN] webdriver object                      *
 *             script_timeout    - [IN] script timeout (ms) or -1             *
 *             page_load_timeout - [IN] page load timeout (ms) or -1          *
 *             implicit_timeot   - [IN] implicit timeout (ms) or -1           *
 *             error             - [OUT] error message                        *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 * Comments: When timeout parameter is set to -1 the timeout value is not     *
 *           changed.                                                         *
 *                                                                            *
 ******************************************************************************/
int	webdriver_set_timeouts(zbx_webdriver_t *wd, int script_timeout, int page_load_timeout, int implicit_timeot,
		char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;

	zbx_json_init(&json, 128);

	if (-1 != script_timeout)
		zbx_json_addint64(&json, "script", script_timeout);

	if (-1 != page_load_timeout)
		zbx_json_addint64(&json, "pageLoad", page_load_timeout);

	if (-1 != implicit_timeot)
		zbx_json_addint64(&json, "implicit", implicit_timeot);

	ret = webdriver_session_query(wd, "POST", "timeouts", json.buffer, NULL, error);

	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get cookies                                                       *
 *                                                                            *
 * Parameters: wd      - [IN] webdriver object                                *
 *             cookies - [OUT] array of cookie objects in JSON format         *
 *             error   - [OUT] error message                                  *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_cookies(zbx_webdriver_t *wd, char **cookies, char **error)
{
	struct zbx_json_parse	jp;

	if (SUCCEED != webdriver_session_query(wd, "GET", "cookie", NULL, &jp, error))
		return FAIL;

	size_t	len = (size_t)(jp.end - jp.start + 1);

	*cookies = zbx_malloc(NULL, len + 1);
	memcpy(*cookies, jp.start, len);
	(*cookies)[len] = '\0';

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: add cookie                                                        *
 *                                                                            *
 * Parameters: wd     - [IN] webdriver object                                 *
 *             cookie - [IN] cookie object in JSON format                     *
 *             error  - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_add_cookie(zbx_webdriver_t *wd, const char *cookie, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;

	zbx_json_init(&json, 128);
	zbx_json_addraw(&json, "cookie", cookie);

	ret = webdriver_session_query(wd, "POST", "cookie", json.buffer, NULL, error);

	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: capture screen                                                    *
 *                                                                            *
 * Parameters: wd         - [IN] webdriver object                             *
 *             screenshot - [OUT] base64 encoded captured screenshot          *
 *             error      - [OUT] error message                               *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_screenshot(zbx_webdriver_t *wd, char **screenshot, char **error)
{
	struct zbx_json_parse	jp;
	size_t			screenshot_alloc = 0;

	if (SUCCEED != webdriver_session_query(wd, "GET", "screenshot", NULL, &jp, error))
		return FAIL;

	if (NULL == zbx_json_decodevalue_dyn(jp.start, screenshot, &screenshot_alloc, NULL))
	{
		*error = zbx_dsprintf(NULL,  "cannot extract screenshot from json: %s", zbx_json_strerror());

		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: set screen size                                                   *
 *                                                                            *
 * Parameters: wd     - [IN] webdriver object                                 *
 *             width  - [IN] screen width                                     *
 *             height - [IN] screen height                                    *
 *             error  - [OUT] error message                                   *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_set_screen_size(zbx_webdriver_t *wd, int width, int height, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;

	zbx_json_init(&json, 128);
	zbx_json_addint64(&json, "width", width);
	zbx_json_addint64(&json, "height", height);

	ret = webdriver_session_query(wd, "POST", "window/rect", json.buffer, NULL, error);

	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: execute script on webdriver                                       *
 *                                                                            *
 * Parameters: wd      - [IN] webdriver object                                *
 *             script  - [IN] script to execute                               *
 *             error   - [OUT] error message                                  *
 *                                                                            *
 * Return value: SUCCEED - query was performed successfully                   *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
int	webdriver_execute_script(zbx_webdriver_t *wd, const char *script, char **error)
{
	struct zbx_json	json;
	int		ret = FAIL;

	zbx_json_init(&json, 128);
	zbx_json_addstring(&json, "script", script,  ZBX_JSON_TYPE_STRING);
	zbx_json_addarray(&json, "args");
	zbx_json_close(&json);

	if (SUCCEED != webdriver_session_query(wd, "POST", "execute/sync", json.buffer, NULL, error))
		goto out;

	ret = SUCCEED;
out:
	zbx_json_free(&json);

	return ret;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get page source                                                   *
 *                                                                            *
 * Parameters: wd      - [IN] webdriver object                                *
 *             sopurce - [OUT] page source                                    *
 *             error   - [OUT] error message                                  *
 *                                                                            *
 * Return value: SUCCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_page_source(zbx_webdriver_t *wd, char **source, char **error)
{
	struct zbx_json_parse	jp;
	size_t			source_alloc = 0;

	if (SUCCEED != webdriver_session_query(wd, "GET", "source", NULL, &jp, error))
		return FAIL;

	if (NULL == zbx_json_decodevalue_dyn(jp.start, source, &source_alloc, NULL))
	{
		*error = zbx_dsprintf(NULL,  "cannot extract page source from json: %s", zbx_json_strerror());

		return FAIL;
	}

	return SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: check if webdriver has cached an error                            *
 *                                                                            *
 * Parameters: wd - [IN] webdriver object                                     *
 *                                                                            *
 * Return value: SUCCEED - webdriver has cached an error                      *
 *               FAIL    - otherwise                                          *
 *                                                                            *
 ******************************************************************************/
int	webdriver_has_error(zbx_webdriver_t *wd)
{
	return NULL == wd->last_error_message ? FAIL : SUCCEED;
}

/******************************************************************************
 *                                                                            *
 * Purpose: set custom error message                                          *
 *                                                                            *
 * Parameters: wd      - [IN] webdriver object                                *
 *             message - [IN] error message                                   *
 *                                                                            *
 * Comments: The error message must be preallocated by caller and will be     *
 *           freed when webdriver custom error message is freed.              *
 *                                                                            *
 ******************************************************************************/
void	webdriver_set_error(zbx_webdriver_t *wd, char *message)
{
	if (NULL != wd->error)
		webdriver_free_error(wd->error);

	zbx_free(wd->last_error_message);
	wd->last_error_message = message;
}

/******************************************************************************
 *                                                                            *
 * Purpose: get alert text                                                    *
 *                                                                            *
 * Parameters: wd    - [IN] webdriver object                                  *
 *             text  - [OUT] alert text or null if there are no alerts        *
 *             error - [OUT] error message                                    *
 *                                                                            *
 * Return value: SUCEED - operation was performed successfully                *
 *               FAIL   - otherwise                                           *
 *                                                                            *
 ******************************************************************************/
int	webdriver_get_alert(zbx_webdriver_t *wd, char **text, char **error)
{
	int			ret = FAIL;
	struct zbx_json_parse	jp;
	size_t			text_alloc = 0;

	if (SUCCEED != webdriver_session_query(wd, "GET", "alert/text", NULL, &jp, error))
	{
		/* throw exception in the case of connection errors */
		if (404 != wd->error->http_code || 0 == strcmp(wd->error->error, WEBDRIVER_INVALID_SESSIONID_ERROR))
			goto out;

		/* otherwise log the error and return NULL alert */
		zabbix_log(LOG_LEVEL_DEBUG, "cannot get alert text: %s", error);

		webdriver_free_error(wd->error);
		wd->error = NULL;
		zbx_free(*error);
		*text = NULL;
	}
	else if (NULL == zbx_json_decodevalue_dyn(jp.start, text, &text_alloc, NULL))
	{
		*text = NULL;
		zabbix_log(LOG_LEVEL_DEBUG, "cannot read alert text: %s", zbx_json_strerror());

		goto out;
	}

	ret = SUCCEED;
out:
	return ret;
}

int	webdriver_accept_alert(zbx_webdriver_t *wd, char **error)
{
	return webdriver_session_query(wd, "POST", "alert/accept", "{}", NULL, error);
}

int	webdriver_dismiss_alert(zbx_webdriver_t *wd, char **error)
{
	return webdriver_session_query(wd, "POST", "alert/dismiss", "{}", NULL, error);
}

#endif

