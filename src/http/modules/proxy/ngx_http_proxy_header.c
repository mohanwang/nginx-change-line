
/*
 * Copyright (C) Igor Sysoev
 */


#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>
#include <ngx_http_proxy_handler.h>


static int ngx_http_proxy_rewrite_location_header(ngx_http_proxy_ctx_t *p,
                                                  ngx_table_elt_t *loc);

int ngx_http_proxy_copy_header(ngx_http_proxy_ctx_t *p,
                               ngx_http_proxy_headers_in_t *headers_in)
{
    ngx_uint_t           i;
    ngx_list_part_t     *part;
    ngx_table_elt_t     *ho, *h;
    ngx_http_request_t  *r;

    r = p->request;

    part = &headers_in->headers.part;
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

        /* ignore some headers */

        if (&h[i] == headers_in->connection) {
            continue;
        }

        if (&h[i] == headers_in->x_pad) {
            continue;
        }

        if (p->accel) {
            if (&h[i] == headers_in->date
                || &h[i] == headers_in->accept_ranges) {
                continue;
            }

            if (&h[i] == headers_in->x_accel_expires
                && !p->lcf->pass_x_accel_expires)
            {
                continue;
            }

            if (&h[i] == headers_in->server && !p->lcf->pass_server) {
                continue;
            }

            if (&h[i] == headers_in->location) {
                if (ngx_http_proxy_rewrite_location_header(p, &h[i])
                                                                  == NGX_ERROR)
                {
                    return NGX_ERROR;
                }

                continue;
            }
        }


        /* "Content-Type" is handled specially */

        if (&h[i] == headers_in->content_type) {
            r->headers_out.content_type = &h[i];
            r->headers_out.content_type->key.len = 0;
            continue;
        }


        /* copy some header pointers and set up r->headers_out */

        ho = ngx_list_push(&r->headers_out.headers);
        if (ho == NULL) {
            return NGX_ERROR;
        }

        *ho = h[i];

        if (&h[i] == headers_in->expires) {
            r->headers_out.expires = ho;
            continue;
        }

        if (&h[i] == headers_in->cache_control) {
            r->headers_out.cache_control = ho;
            continue;
        }

        if (&h[i] == headers_in->etag) {
            r->headers_out.etag = ho;
            continue;
        }

#if (NGX_HTTP_GZIP)
        if (&h[i] == headers_in->content_encoding) {
            r->headers_out.content_encoding = ho;
            continue;
        }
#endif

        if (&h[i] == headers_in->last_modified) {
            r->headers_out.last_modified = ho;
            /* TODO: update r->headers_out.last_modified_time */
            continue;
        }

        /*
         * ngx_http_header_filter() passes the following headers as is
         * and does not handle them specially if they are set:
         *     r->headers_out.server,
         *     r->headers_out.date,
         *     r->headers_out.content_length
         */

        if (&h[i] == headers_in->server) {
            r->headers_out.server = ho;
            continue;
        }

        if (&h[i] == headers_in->date) {
            r->headers_out.date = ho;
            continue;
        }

        if (&h[i] == headers_in->content_length) {
            r->headers_out.content_length = ho;
            r->headers_out.content_length_n = ngx_atoi(ho->value.data,
                                                       ho->value.len);
            continue;
        }
    }

    return NGX_OK;
}


static int ngx_http_proxy_rewrite_location_header(ngx_http_proxy_ctx_t *p,
                                                  ngx_table_elt_t *loc)
{
    u_char                          *last;
    ngx_table_elt_t                 *location;
    ngx_http_request_t              *r;
    ngx_http_proxy_upstream_conf_t  *uc;

    r = p->request;
    uc = p->lcf->upstream;

    location = ngx_list_push(&r->headers_out.headers);
    if (location == NULL) {
        return NGX_ERROR;
    }

    if (p->lcf->preserve_host
        || uc->url.len > loc->value.len
        || ngx_rstrncmp(loc->value.data, uc->url.data, uc->url.len) != 0)
    {

       /*
        * we do not set r->headers_out.location here to avoid the handling
        * the local redirects without a host name by ngx_http_header_filter()
        */

        *location = *loc;
        return NGX_OK;
    }

    /* TODO: proxy_reverse */

    r->headers_out.location = location;

    location->key.len = 0;
    location->key.data = NULL;

    location->value.len = uc->location->len
                                          + (loc->value.len - uc->url.len) + 1;
    location->value.data = ngx_palloc(r->pool, location->value.len);
    if (location->value.data == NULL) {
        return NGX_ERROR;
    }

    last = ngx_cpymem(location->value.data,
                      uc->location->data, uc->location->len);

    ngx_cpystrn(last, loc->value.data + uc->url.len,
                loc->value.len - uc->url.len + 1);

    return NGX_OK;
}
