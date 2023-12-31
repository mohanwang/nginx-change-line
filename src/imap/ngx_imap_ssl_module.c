
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_imap.h>


#define NGX_DEFLAUT_CERTIFICATE      "cert.pem"
#define NGX_DEFLAUT_CERTIFICATE_KEY  "cert.pem"
#define NGX_DEFLAUT_CIPHERS  "ALL:!ADH:RC4+RSA:+HIGH:+MEDIUM:+LOW:+SSLv2:+EXP"


static void *ngx_imap_ssl_create_conf(ngx_conf_t *cf);
static char *ngx_imap_ssl_merge_conf(ngx_conf_t *cf, void *parent, void *child);
static char *ngx_imap_ssl_session_cache(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

#if !defined (SSL_OP_CIPHER_SERVER_PREFERENCE)

static char *ngx_imap_ssl_nosupported(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static char  ngx_imap_ssl_openssl097[] = "OpenSSL 0.9.7 and higher";

#endif


static ngx_conf_enum_t  ngx_http_starttls_state[] = {
    { ngx_string("off"), NGX_IMAP_STARTTLS_OFF },
    { ngx_string("on"), NGX_IMAP_STARTTLS_ON },
    { ngx_string("only"), NGX_IMAP_STARTTLS_ONLY },
    { ngx_null_string, 0 }
};



static ngx_conf_bitmask_t  ngx_imap_ssl_protocols[] = {
    { ngx_string("SSLv2"), NGX_SSL_SSLv2 },
    { ngx_string("SSLv3"), NGX_SSL_SSLv3 },
    { ngx_string("TLSv1"), NGX_SSL_TLSv1 },
    { ngx_null_string, 0 }
};


static ngx_command_t  ngx_imap_ssl_commands[] = {

    { ngx_string("ssl"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_FLAG,
      ngx_conf_set_flag_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, enable),
      NULL },

    { ngx_string("starttls"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_enum_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, starttls),
      ngx_http_starttls_state },

    { ngx_string("ssl_certificate"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, certificate),
      NULL },

    { ngx_string("ssl_certificate_key"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, certificate_key),
      NULL },

    { ngx_string("ssl_protocols"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_1MORE,
      ngx_conf_set_bitmask_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, protocols),
      &ngx_imap_ssl_protocols },

    { ngx_string("ssl_ciphers"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_str_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, ciphers),
      NULL },

    { ngx_string("ssl_prefer_server_ciphers"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_FLAG,
#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE
      ngx_conf_set_flag_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, prefer_server_ciphers),
      NULL },
#else
      ngx_imap_ssl_nosupported, 0, 0, ngx_imap_ssl_openssl097 },
#endif

    { ngx_string("ssl_session_cache"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE12,
      ngx_imap_ssl_session_cache,
      NGX_IMAP_SRV_CONF_OFFSET,
      0,
      NULL },

    { ngx_string("ssl_session_timeout"),
      NGX_IMAP_MAIN_CONF|NGX_IMAP_SRV_CONF|NGX_CONF_TAKE1,
      ngx_conf_set_sec_slot,
      NGX_IMAP_SRV_CONF_OFFSET,
      offsetof(ngx_imap_ssl_conf_t, session_timeout),
      NULL },

      ngx_null_command
};


static ngx_imap_module_t  ngx_imap_ssl_module_ctx = {
    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    ngx_imap_ssl_create_conf,              /* create server configuration */
    ngx_imap_ssl_merge_conf                /* merge server configuration */
};


ngx_module_t  ngx_imap_ssl_module = {
    NGX_MODULE_V1,
    &ngx_imap_ssl_module_ctx,              /* module context */
    ngx_imap_ssl_commands,                 /* module directives */
    NGX_IMAP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_str_t ngx_imap_ssl_sess_id_ctx = ngx_string("IMAP");


static void *
ngx_imap_ssl_create_conf(ngx_conf_t *cf)
{
    ngx_imap_ssl_conf_t  *scf;

    scf = ngx_pcalloc(cf->pool, sizeof(ngx_imap_ssl_conf_t));
    if (scf == NULL) {
        return NGX_CONF_ERROR;
    }

    /*
     * set by ngx_pcalloc():
     *
     *     scf->protocols = 0;
     *     scf->certificate.len = 0;
     *     scf->certificate.data = NULL;
     *     scf->certificate_key.len = 0;
     *     scf->certificate_key.data = NULL;
     *     scf->ciphers.len = 0;
     *     scf->ciphers.data = NULL;
     *     scf->shm_zone = NULL;
     */

    scf->enable = NGX_CONF_UNSET;
    scf->starttls = NGX_CONF_UNSET;
    scf->prefer_server_ciphers = NGX_CONF_UNSET;
    scf->builtin_session_cache = NGX_CONF_UNSET;
    scf->session_timeout = NGX_CONF_UNSET;

    return scf;
}


static char *
ngx_imap_ssl_merge_conf(ngx_conf_t *cf, void *parent, void *child)
{
    ngx_imap_ssl_conf_t *prev = parent;
    ngx_imap_ssl_conf_t *conf = child;

    ngx_pool_cleanup_t  *cln;

    ngx_conf_merge_value(conf->enable, prev->enable, 0);
    ngx_conf_merge_value(conf->starttls, prev->starttls, NGX_IMAP_STARTTLS_OFF);

    if (conf->enable == 0 && conf->starttls == NGX_IMAP_STARTTLS_OFF) {
        return NGX_CONF_OK;
    }

    ngx_conf_merge_value(conf->session_timeout,
                         prev->session_timeout, 300);

    ngx_conf_merge_value(conf->prefer_server_ciphers,
                         prev->prefer_server_ciphers, 0);

    ngx_conf_merge_bitmask_value(conf->protocols, prev->protocols,
                         (NGX_CONF_BITMASK_SET
                          |NGX_SSL_SSLv2|NGX_SSL_SSLv3|NGX_SSL_TLSv1));

    ngx_conf_merge_str_value(conf->certificate, prev->certificate,
                             NGX_DEFLAUT_CERTIFICATE);

    ngx_conf_merge_str_value(conf->certificate_key, prev->certificate_key,
                             NGX_DEFLAUT_CERTIFICATE_KEY);

    ngx_conf_merge_str_value(conf->ciphers, prev->ciphers, NGX_DEFLAUT_CIPHERS);


    conf->ssl.log = cf->log;

    if (ngx_ssl_create(&conf->ssl, conf->protocols, NULL) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    cln = ngx_pool_cleanup_add(cf->pool, 0);
    if (cln == NULL) {
        return NGX_CONF_ERROR;
    }

    cln->handler = ngx_ssl_cleanup_ctx;
    cln->data = &conf->ssl;

    if (ngx_ssl_certificate(cf, &conf->ssl, &conf->certificate,
                            &conf->certificate_key)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    if (conf->ciphers.len) {
        if (SSL_CTX_set_cipher_list(conf->ssl.ctx,
                                   (const char *) conf->ciphers.data)
            == 0)
        {
            ngx_ssl_error(NGX_LOG_EMERG, cf->log, 0,
                          "SSL_CTX_set_cipher_list(\"%V\") failed",
                          &conf->ciphers);
        }
    }

#ifdef SSL_OP_CIPHER_SERVER_PREFERENCE

    if (conf->prefer_server_ciphers) {
        SSL_CTX_set_options(conf->ssl.ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
    }

#endif

    if (ngx_ssl_generate_rsa512_key(&conf->ssl) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ngx_conf_merge_value(conf->builtin_session_cache,
                         prev->builtin_session_cache,
                         NGX_SSL_DFLT_BUILTIN_SCACHE);

    if (conf->shm_zone == NULL) {
        conf->shm_zone = prev->shm_zone;
    }

    if (ngx_ssl_session_cache(&conf->ssl, &ngx_imap_ssl_sess_id_ctx,
                              conf->builtin_session_cache,
                              conf->shm_zone, conf->session_timeout)
        != NGX_OK)
    {
        return NGX_CONF_ERROR;
    }

    return NGX_CONF_OK;
}


static char *
ngx_imap_ssl_session_cache(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_imap_ssl_conf_t  *scf = conf;

    size_t       len;
    ngx_str_t   *value, name, size;
    ngx_int_t    n;
    ngx_uint_t   i, j;

    value = cf->args->elts;

    for (i = 1; i < cf->args->nelts; i++) {

        if (ngx_strcmp(value[i].data, "builtin") == 0) {
            scf->builtin_session_cache = NGX_SSL_DFLT_BUILTIN_SCACHE;
            continue;
        }

        if (value[i].len > sizeof("builtin:") - 1
            && ngx_strncmp(value[i].data, "builtin:", sizeof("builtin:") - 1)
               == 0)
        {
            n = ngx_atoi(value[i].data + sizeof("builtin:") - 1,
                         value[i].len - (sizeof("builtin:") - 1));

            if (n == NGX_ERROR) {
                goto invalid;
            }

            scf->builtin_session_cache = n;

            continue;
        }

        if (value[i].len > sizeof("shared:") - 1
            && ngx_strncmp(value[i].data, "shared:", sizeof("shared:") - 1)
               == 0)
        {
            len = 0;

            for (j = sizeof("shared:") - 1; j < value[i].len; j++) {
                if (value[i].data[j] == ':') {
                    break;
                }

                len++;
            }

            if (len == 0) {
                goto invalid;
            }

            name.len = len;
            name.data = value[i].data + sizeof("shared:") - 1;

            size.len = value[i].len - j - 1;
            size.data = name.data + len + 1;

            n = ngx_parse_size(&size);

            if (n == NGX_ERROR) {
                goto invalid;
            }

            if (n < (ngx_int_t) (8 * ngx_pagesize)) {
                ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                                   "session cache \"%V\" is too small",
                                   &value[i]);

                return NGX_CONF_ERROR;
            }

            scf->shm_zone = ngx_shared_memory_add(cf, &name, n,
                                                   &ngx_imap_ssl_module);
            if (scf->shm_zone == NULL) {
                return NGX_CONF_ERROR;
            }

            continue;
        }

        goto invalid;
    }

    if (scf->shm_zone && scf->builtin_session_cache == NGX_CONF_UNSET) {
        scf->builtin_session_cache = NGX_SSL_NO_BUILTIN_SCACHE;
    }

    return NGX_CONF_OK;

invalid:

    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "invalid session cache \"%V\"", &value[i]);

    return NGX_CONF_ERROR;
}


#if !defined (SSL_OP_CIPHER_SERVER_PREFERENCE)

static char *
ngx_imap_ssl_nosupported(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                       "\"%V\" directive is available only in %s,",
                       &cmd->name, cmd->post);

    return NGX_CONF_ERROR;
}

#endif
