
/*
 * Copyright (C) Igor Sysoev
 */


#ifndef _NGX_HTTP_LOG_MODULE_H_INCLUDED_
#define _NGX_HTTP_LOG_MODULE_H_INCLUDED_


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct ngx_http_log_op_s  ngx_http_log_op_t;

typedef u_char *(*ngx_http_log_op_run_pt) (ngx_http_request_t *r, u_char *buf,
    ngx_http_log_op_t *op);

typedef size_t (*ngx_http_log_op_getlen_pt) (ngx_http_request_t *r,
    uintptr_t data);

typedef ngx_int_t (*ngx_http_log_op_compile_pt) (ngx_conf_t *cf,
    ngx_http_log_op_t *op, ngx_str_t *value);


struct ngx_http_log_op_s {
    size_t                      len;
    ngx_http_log_op_getlen_pt   getlen;
    ngx_http_log_op_run_pt      run;
    uintptr_t                   data;
};


typedef struct {
    ngx_str_t                   name;
    ngx_array_t                *ops;        /* array of ngx_http_log_op_t */
} ngx_http_log_fmt_t;


typedef struct {
    ngx_str_t                   name;
    size_t                      len;
    ngx_http_log_op_compile_pt  compile;
    ngx_http_log_op_getlen_pt   getlen;
    ngx_http_log_op_run_pt      run;
} ngx_http_log_op_name_t;


typedef struct {
    ngx_array_t                 formats;    /* array of ngx_http_log_fmt_t */
} ngx_http_log_main_conf_t;


typedef struct {
    ngx_open_file_t            *file;
    ngx_array_t                *ops;        /* array of ngx_http_log_op_t */
} ngx_http_log_t;


typedef struct {
    ngx_array_t                *logs;       /* array of ngx_http_log_t */
    ngx_uint_t                  off;        /* unsigned  off:1 */
} ngx_http_log_loc_conf_t;


extern ngx_http_log_op_name_t ngx_http_log_fmt_ops[];


#endif /* _NGX_HTTP_LOG_MODULE_H_INCLUDED_ */
