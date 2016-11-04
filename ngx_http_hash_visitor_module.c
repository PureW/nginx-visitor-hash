
/*
 * Copyright (C) Anders Bennehag
 * Built from ngx_http_empty_gif_module.c
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static char *ngx_http_hash_visitor(ngx_conf_t *cf, ngx_command_t *cmd,
                                   void *conf);

static ngx_command_t  ngx_http_hash_visitor_commands[] = {

    { ngx_string("hash_visitor"),
      NGX_HTTP_LOC_CONF|NGX_CONF_NOARGS,
      ngx_http_hash_visitor,
      0,
      0,
      NULL },

      ngx_null_command
};




static ngx_http_module_t  ngx_http_hash_visitor_module_ctx = {
    NULL,                          /* preconfiguration */
    NULL,                          /* postconfiguration */

    NULL,                          /* create main configuration */
    NULL,                          /* init main configuration */

    NULL,                          /* create server configuration */
    NULL,                          /* merge server configuration */

    NULL,                          /* create location configuration */
    NULL                           /* merge location configuration */
};


ngx_module_t  ngx_http_hash_visitor_module = {
    NGX_MODULE_V1,
    &ngx_http_hash_visitor_module_ctx, /* module context */
    ngx_http_hash_visitor_commands,   /* module directives */
    NGX_HTTP_MODULE,               /* module type */
    NULL,                          /* init master */
    NULL,                          /* init module */
    NULL,                          /* init process */
    NULL,                          /* init thread */
    NULL,                          /* exit thread */
    NULL,                          /* exit process */
    NULL,                          /* exit master */
    NGX_MODULE_V1_PADDING
};

static ngx_str_t  unknwn = ngx_string("unknown value");

/** Read headers_in and generate a hash from headers of interest */
static int
ngx_http_hash_visitor_calc(ngx_http_headers_in_t *headers_in, 
                           unsigned char* buf, 
                           size_t buf_len)
{
    unsigned char* end;

    ngx_str_t* host  = headers_in->host ? &headers_in->host->value : &unknwn;
    ngx_str_t* agent = headers_in->user_agent ? &headers_in->user_agent->value : &unknwn;
#if (NGX_HTTP_X_FORWARDED_FOR)
    ngx_str_t* x_fwd = &unknwn;
    // TODO: Transform forwarded_for (ngx_array_t) to ngx_str_t
    /*if (headers_in->forwarded_for) {*/
        /*headers_in->forwarded_for->value*/
    /*}*/
#endif
#if (NGX_HTTP_REALIP)
    ngx_str_t* rl_ip = headers_in->real_ip ? &headers_in->real_ip->value : &unknwn;
#endif

    end = ngx_snprintf(buf, buf_len, 
        "Headers used for hash:\n"
        "host: \t%V\n"
        "user_agent: \t%V\n"
        /*": \t%V\n"*/
#if (NGX_HTTP_X_FORWARDED_FOR)
        "x_forwarded_for: \t%V\n"
#endif
#if (NGX_HTTP_REALIP)
        "x_real_ip: \t%V\n"
#endif
        , host
        , agent
#if (NGX_HTTP_X_FORWARDED_FOR)
        , x_fwd
#endif
#if (NGX_HTTP_REALIP)
        , rl_ip
#endif
        );
    return end - buf;
    
    /**user_agent;*/
/*#if (NGX_HTTP_X_FORWARDED_FOR)*/
    /*ngx_array_t x_forwarded_for;*/
/*#endif*/
/*#if (NGX_HTTP_REALIP)*/
    /*ngx_table_elt_t *x_real_ip;*/
/*#endif*/

}

static ngx_int_t
ngx_http_hash_visitor_handler(ngx_http_request_t *r)
{
    ngx_int_t     rc;
    ngx_buf_t    *b;
    ngx_chain_t   out;
    int body_size;

    r->headers_out.status = NGX_HTTP_OK;
    // TODO: Should we do content-length? Or send header early?
    /*r->headers_out.content_length_n = ngx_http_sample_text.len;*/

    rc = ngx_http_send_header(r);

    if (rc == NGX_ERROR || rc > NGX_OK || r->header_only) {
        return rc;
    }

    b = ngx_calloc_buf(r->pool);
    if (b == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }

    out.buf = b;
    out.next = NULL;

    // TODO: Figure out size of buf at runtime
    //       Only handles buf spanning a single nginx-buf for simplicity
    size_t buf_len = 1024;
    unsigned char *buf = ngx_palloc(r->pool, buf_len);
    if (buf == NULL) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    body_size = ngx_http_hash_visitor_calc(&r->headers_in, buf, buf_len);
    if (body_size <= 0) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0, 
                  "body_size %d bytes", body_size);

    b->start = b->pos = buf;
    b->end = b->last = buf + body_size;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    return ngx_http_output_filter(r, &out);

    /** Simpler version, ngx_http_send_response handles most work */
    /*ngx_http_complex_value_t  cv;*/

    /*if (!(r->method & (NGX_HTTP_GET|NGX_HTTP_HEAD))) {*/
        /*return NGX_HTTP_NOT_ALLOWED;*/
    /*}*/

    /*ngx_memzero(&cv, sizeof(ngx_http_complex_value_t));*/

    /*cv.value.len = sizeof(ngx_hash_visitor);*/
    /*cv.value.data = ngx_hash_visitor;*/
    /*r->headers_out.last_modified_time = 23349600;*/

    /*return ngx_http_send_response(r, NGX_HTTP_OK, &ngx_http_gif_type, &cv);*/
}


static char *
ngx_http_hash_visitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_hash_visitor_handler;

    return NGX_CONF_OK;
}
