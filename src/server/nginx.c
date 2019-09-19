/***************************************************************************
 *
 * Copyright (C) 2018-2019 - ZmartZone Holding BV - www.zmartzone.eu
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * @Author: Hans Zandbelt - hans.zandbelt@zmartzone.eu
 *
 **************************************************************************/

#include "oauth2/nginx.h"

#include <oauth2/http.h>
#include <oauth2/mem.h>
#include <oauth2/oauth2.h>

#include <ngx_log.h>

// yep, this is tightly aligned with the (sequence of...) the log levels in
// lmo/log.h, but is faaast

// clang-format off
/*
static int log_level_nginx2oauth[] = {
    OAUTH2_LOG_ERROR,
	OAUTH2_LOG_ERROR,
	OAUTH2_LOG_ERROR,
	OAUTH2_LOG_ERROR,
	OAUTH2_LOG_ERROR,
	OAUTH2_LOG_WARN,
	OAUTH2_LOG_NOTICE,
	OAUTH2_LOG_INFO,
	OAUTH2_LOG_DEBUG
};
*/

// TODO: TRACE2 to STDERR??
static int log_level_log2nginx[] = {
    NGX_LOG_ERR,
	NGX_LOG_WARN,
	NGX_LOG_NOTICE,
	NGX_LOG_INFO,
    NGX_LOG_DEBUG,
	NGX_LOG_DEBUG,
	NGX_LOG_STDERR
};
// clang-format on

void oauth2_nginx_log(oauth2_log_sink_t *sink, const char *filename,
		      unsigned long line, const char *function,
		      oauth2_log_level_t level, const char *msg)
{
	// TODO: ngx_err_t?
	ngx_log_error_core(log_level_log2nginx[level],
			   (ngx_log_t *)oauth2_log_sink_ctx_get(sink), 0,
			   "# %s: %s", function, msg);
}

char *oauth2_nginx_http_scheme(ngx_http_request_t *r)
{
	int len = r->schema_end - r->schema_start;
	return (len > 0) ? oauth2_strndup((const char *)r->schema_start, len)
			 : NULL;
}

static char *oauth2_ngx_get_host_name(ngx_http_request_t *r)
{
	int len = r->host_end - r->host_start;
	return (len > 0) ? oauth2_strndup((const char *)r->host_start, len)
			 : NULL;
}

static oauth2_uint_t oauth2_ngx_get_port(ngx_http_request_t *r)
{
	oauth2_uint_t port = 0;
	char *v = NULL;
	int len = r->port_end - r->port_start;
	if (len > 0) {
		v = oauth2_strndup((const char *)r->port_start, len);
		port = oauth2_parse_uint(NULL, v, 0);
		oauth2_mem_free(v);
	}
	return port;
}

char *oauth2_ngx_get_path(ngx_http_request_t *r)
{
	return (r->uri.len > 0)
		   ? oauth2_strndup((const char *)r->uri.data, r->uri.len)
		   : NULL;
}

oauth2_http_method_t oauth2_ngx_get_method(ngx_http_request_t *r)
{
	oauth2_http_method_t rv = OAUTH2_HTTP_METHOD_UNKNOWN;
	char *v = (r->method_name.len > 0)
		      ? oauth2_strndup((const char *)r->method_name.data,
				       r->method_name.len)
		      : NULL;

	if (v == NULL)
		goto end;

	if (strcmp(v, "GET") == 0)
		rv = OAUTH2_HTTP_METHOD_GET;
	else if (strcmp(v, "POST") == 0)
		rv = OAUTH2_HTTP_METHOD_POST;
	else if (strcmp(v, "PUT") == 0)
		rv = OAUTH2_HTTP_METHOD_PUT;
	else if (strcmp(v, "DELETE") == 0)
		rv = OAUTH2_HTTP_METHOD_DELETE;
	else if (strcmp(v, "CONNECT") == 0)
		rv = OAUTH2_HTTP_METHOD_CONNECT;
	else if (strcmp(v, "OPTIONS") == 0)
		rv = OAUTH2_HTTP_METHOD_OPTIONS;

end:

	if (v)
		oauth2_mem_free(v);

	return rv;
}

char *oauth2_ngx_get_query(ngx_http_request_t *r)
{
	return ((r->args_start) && (r->args.len > 0))
		   ? oauth2_strndup((const char *)r->args_start, r->args.len)
		   : NULL;
}

oauth2_nginx_request_context_t *
oauth2_nginx_request_context_init(ngx_http_request_t *r)
{
	// ngx_http_core_srv_conf_t *cscf;
	oauth2_nginx_request_context_t *ctx = NULL;
	oauth2_log_sink_t *log_sink_nginx = NULL;

	// cscf = ngx_http_get_module_srv_conf(r, ngx_http_core_module);

	// TODO: memory allocation failure checks...?
	ctx = oauth2_mem_alloc(sizeof(oauth2_nginx_request_context_t));

	ctx->r = r;

	// TODO: get the log level from NGINX...
	oauth2_log_level_t level = OAUTH2_LOG_TRACE1;
	log_sink_nginx =
	    oauth2_log_sink_create(level, oauth2_nginx_log, r->connection->log);
	ctx->log = oauth2_log_init(level, log_sink_nginx);

	ngx_list_part_t *part;
	ngx_table_elt_t *h;
	ngx_uint_t i;
	char *name = NULL, *value = NULL;

	ctx->request = oauth2_http_request_init(ctx->log);

	// TODO: optimize/macroize...
	// value = oauth2_nginx_http_scheme(r);
	// oauth2_http_request_scheme_set(ctx->log, ctx->request, name);
	// oauth2_mem_free(value);

	value = oauth2_ngx_get_host_name(r);
	oauth2_http_request_hostname_set(ctx->log, ctx->request, value);
	oauth2_mem_free(value);

	oauth2_http_request_port_set(ctx->log, ctx->request,
				     oauth2_ngx_get_port(r));

	// value = oauth2_ngx_get_path(r);
	// oauth2_http_request_path_set(ctx->log, ctx->request, value);
	// oauth2_mem_free(value);

	//	oauth2_http_request_method_set(ctx->log, ctx->request,
	//				       oauth2_ngx_get_method(r));

	// value = oauth2_ngx_get_query(r);
	// oauth2_http_request_query_set(ctx->log, ctx->request, value);
	// oauth2_mem_free(value);

	part = &r->headers_in.headers.part;
	h = part->elts;
	for (i = 0; /* void */; i++) {
		if (i >= part->nelts) {
			if (part->next == NULL) {
				break;
			}
			part = part->next;
			h = part->elts;
			i = 0;
		}
		name =
		    oauth2_strndup((const char *)h[i].key.data, h[i].key.len);
		value = oauth2_strndup((const char *)h[i].value.data,
				       h[i].value.len);
		// TODO: avoid duplicate copy
		oauth2_http_request_header_add(ctx->log, ctx->request, name,
					       value);
		oauth2_mem_free(name);
		oauth2_mem_free(value);
	}

	oauth2_debug(ctx->log, "created NGINX request context: %p", ctx);

	return ctx;
}

void oauth2_nginx_request_context_free(void *rec)
{
	oauth2_nginx_request_context_t *ctx =
	    (oauth2_nginx_request_context_t *)rec;
	if (ctx) {
		oauth2_debug(ctx->log, "dispose NGINX request context: %p",
			     ctx);
		if (ctx->request)
			oauth2_http_request_free(ctx->log, ctx->request);
		oauth2_log_free(ctx->log);
		oauth2_mem_free(ctx);
	}
}

ngx_int_t oauth2_nginx_http_response_set(oauth2_log_t *log,
					 oauth2_http_response_t *response,
					 ngx_http_request_t *r)
{
	ngx_int_t nrc = NGX_ERROR;
	// ngx_table_elt_t *h = NULL;

	if ((response == NULL) || (r == NULL))
		goto end;

	// oauth2_http_response_headers_loop(log, response,
	//				  oauth2_nginx_response_header_set, r);

	r->headers_out.status =
	    oauth2_http_response_status_code_get(log, response);

	nrc = ngx_http_send_header(r);

end:

	return nrc;
}

// clang-format off
/*
oauth2_cfg_server_callback_funcs_t oauth2_nginx_server_callback_funcs = {
    _oauth2_nginx_env_get_cb,
	_oauth2_nginx_env_set_cb,
    _oauth2_nginx_read_form_post
};
*/
// clang-format on
