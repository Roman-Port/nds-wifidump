#pragma once

#include <stdint.h>
#include <stddef.h>
#include <nds.h>

#define MAX_HTTP_HEADERS 32

typedef struct {

    char key[64];
    char value[256];

} http_header_t;

typedef struct {

    int sock;

    char method[16];
    char path[64];
    char version[16];

    int header_count;
    http_header_t headers[MAX_HTTP_HEADERS];

    int has_sent_response_headers;

} http_request_t;

typedef struct {

    const char* name;
    int size;

} gbasav_type_t;

void serve_http_server();
void handle_http_request(http_request_t* request);
void http_write(http_request_t* request, const void* data, size_t len); //write body
void http_write_text(http_request_t* request, const char* text); //write body
void http_send_header(http_request_t* request, const char* key, const char* value);
void http_send_header_int(http_request_t* request, const char* key, int value);
int http_get_header(http_request_t* request, const char* key, char** value);

void send_compressed_data(http_request_t* request, const u8* buffer, int length);

int query_gba_rom_size();
void safe_string(const char* input, int length, char* output, int allowEarlyEnd);

/* GBA SAVE */

extern const gbasav_type_t gbasav_types[6];

int gbasav_identify();
void gbasav_read(u8 *dst, u32 src, u32 len, int type);