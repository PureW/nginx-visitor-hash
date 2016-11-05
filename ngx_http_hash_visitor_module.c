
/*
 * Copyright (C) Anders Bennehag
 * Built from ngx_http_empty_gif_module.c
 */

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static const ngx_str_t  unknwn = ngx_string("unknown value");
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

/** Hash a ngx_str_t */
static ngx_uint_t
hash_str(ngx_uint_t key, const ngx_str_t* s)
{
    for (unsigned i=0; i<s->len; ++i) {
        key = ngx_hash(key, s->data[i]);
    }
    return key;
}

/** Macro for fetching headers with ngx_table_elt and hashing */
#define DECLARE_HEADER_HASH_ITEM_TABLE_ELT(hdrname) \
    const ngx_str_t* hdrname = hdrs_in->hdrname ? &hdrs_in->hdrname->value : &unknwn; \
    hash = hash_str(hash, hdrname);

/** Macro for fetching headers with ngx_str_t and hashing */
#define DECLARE_HEADER_HASH_ITEM_STR(hdrname) \
    ngx_str_t* hdrname = &hdrs_in->hdrname; \
    hash = hash_str(hash, hdrname);

/** Read headers_in and generate a hash from headers of interest */
static int
ngx_http_hash_visitor_calc(ngx_http_headers_in_t *hdrs_in,
                           unsigned char* buf,
                           size_t buf_len)
{
    unsigned char* end;
    ngx_uint_t hash = 0;

    // Fetch and hash headers using DECLARE_HEADER_HASH_ITEM_... macro
    DECLARE_HEADER_HASH_ITEM_TABLE_ELT(host)
    DECLARE_HEADER_HASH_ITEM_TABLE_ELT(user_agent)
    DECLARE_HEADER_HASH_ITEM_STR(user)
#if (NGX_HTTP_REALIP)
    DECLARE_HEADER_HASH_ITEM(real_ip)
#endif
#if (NGX_HTTP_X_FORWARDED_FOR)
    // Handle x_forwarded_for manually since it is not a ngx_str_t
    // but a ngx_array_t
    // TODO: Transform forwarded_for (ngx_array_t) to ngx_str_t.
    //       Implementation below is unnecessarily complex
    const ngx_str_t *x_fwd = &unknwn;
    /*ngx_array_t* fwd_arr = &hdrs_in->x_forwarded_for;*/
    // // Create comma-separated string from fwd_arr.
    // // Allocate string for nelts*size bytes + 2*(nelds-1) bytes
    // // for ", " (comma + space) for pretty print.
    /*size_t num_bytes = fwd_arr->nelts * fwd_arr->size \*/
                       /*+ 2*(fwd_arr->nelts-1);*/
    /*unsigned char* x_fwd_buf[num_bytes];*/
    /*ngx_str_t x_fwd = ngx_string(x_fwd_buf);*/

    /*size_t idx = 0;*/
    /*for (ngx_uint_t el=0;el<fwd_arr->nelts;++el) {*/
        /*for (size_t i=0; i<fwd_arr->size; ++i) {*/
            /*x_fwd_buf[idx] = fwd_arr->elts[idx];*/
            /*idx += 1;*/
        /*}*/
        /*x_fwd_buf[idx] = ',';*/
        /*idx += 1;*/
        /*x_fwd_buf[idx] = ' ';*/
        /*idx += 1;*/
    /*}*/
#else
    const ngx_str_t *x_fwd = &unknwn;
#endif
    hash = hash_str(hash, x_fwd);

    // Now format body-string, specifying variables used in hash and hash
    // itself.
    end = ngx_snprintf(buf, buf_len,
        "Headers used for hash:\n"
        "host: \t\"%V\"\n"
        "user_agent: \t\"%V\"\n"
        "user: \t\"%V\"\n"
        "x_forwarded_for: \t\"%V\"\n"
#if (NGX_HTTP_REALIP)
        "x_real_ip: \t\"%V\"\n"
#endif
        "\n"
        "Hash becomes %d\n"
        , host
        , user_agent
        , user
        , x_fwd
#if (NGX_HTTP_REALIP)
        , rl_ip
#endif
        , hash
        );
    return end - buf;
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
    /*r->headers_out.content_length_n = ... */

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
    body_size = ngx_http_hash_visitor_calc(&r->headers_in,
                                           buf,
                                           buf_len);
    if (body_size <= 0) {
        return NGX_HTTP_INTERNAL_SERVER_ERROR;
    }
    ngx_log_error(NGX_LOG_INFO, r->connection->log, 0,
                  "body_size %d bytes\n", body_size);

    b->start = b->pos = buf;
    b->end = b->last = buf + body_size;
    b->memory = 1;
    b->last_buf = 1;
    b->last_in_chain = 1;

    return ngx_http_output_filter(r, &out);
}


static char *
ngx_http_hash_visitor(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_http_core_loc_conf_t  *clcf;

    clcf = ngx_http_conf_get_module_loc_conf(cf, ngx_http_core_module);
    clcf->handler = ngx_http_hash_visitor_handler;

    return NGX_CONF_OK;
}
