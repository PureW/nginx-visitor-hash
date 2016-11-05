
VisitorHash - Proof-of-concept Nginx-module
==============================

Simple non-proxying handler module for Nginx. Reads a set of headers and computes a checksum from these to build an ID-hash.

Build
=====

Compile Nginx with VisitorHash:
```
git submodule update
make
```

Simple test:
```
make run_local
curl 127.0.0.1:8080/hash --user-agent client3
```
outputs
```
Headers used for hash:
host:   "127.0.0.1:8080"
user_agent:     "client3"
user:   ""
x_forwarded_for:        "unknown value"

Hash becomes -377004896
```

Design
======

The interesting function is `ngx_http_hash_visitor_calc` that reads the headers of interest and hashes them. I wrote a macro to minimize retyping of code for each header-variable. 

The final body-string is `ngx_snprintf`'ed into a buffer. The buffer is dynamically-allocated in the handler-function using the nginx-pool. Only this buffer is heap-allocated which should keep things lean.

In a less performance-oriented environment, a loop and/or hash-table of headers with an iterative construction of the body-string may be more idiomatic. But here the body-string is constructed in one go.

Most of the rest of the c-file comes from the ["empty gif" module:](https://github.com/nginx/nginx/blob/f8a9d528df92c7634088e575e5c3d63a1d4ab8ea/src/http/modules/ngx_http_empty_gif_module.c).

