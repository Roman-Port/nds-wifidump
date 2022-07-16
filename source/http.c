#include "defines.h"

#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <errno.h>

#include <stdio.h>
#include <string.h>

static char http_header_buffer[256];
static http_request_t request;

static void tcp_send_chunked(int sock, const u8* buffer, int len) {
    int sent;
    while (len > 0) {
        //Send as much as possible
        sent = send(sock, buffer, len, 0);

        //Check for errors
        if (sent == -1) {
            iprintf("WARN: Failed to send TCP packet of size %i with error %s.\n", sent, strerror(errno));
        }

        //Offset
        buffer += sent;
        len -= sent;
    }
}

static int read_header(int sock) {
    int len = 0;
    while (len < sizeof(http_header_buffer) - 1) {
        //Get byte
        if (recv(sock, &http_header_buffer[len], 1, 0) != 1)
            break;
        
        //Check if we've reached the end
        if (len >= 1 && http_header_buffer[len - 1] == '\r' && http_header_buffer[len] == '\n') {
            http_header_buffer[len - 1] = 0;
            return len - 1;
        }

        //Advance
        len++;
    }
    return -1;
}

static int seek_split_until(const char* buffer, int* pos, char token, char* outputBuffer, int outputBufferLen) {
    int output = 0;
    while (output < (outputBufferLen - 1)) {
        //Check if this is the split
        if (buffer[(*pos)] == token) {
            (*pos)++;
            outputBuffer[output] = 0;
            return output;
        }

        //Check if this is the end of the input buffer
        if (buffer[*pos] == 0)
            break;

        //Copy
        outputBuffer[output++] = buffer[(*pos)++];
    }
    return -1;
}

static void handle_request(int sock) {
    //Set
    request.sock = sock;
    request.has_sent_response_headers = 0;

    //Read HTTP main header
    if (read_header(sock) == -1) {
        iprintf("WARN: HTTP request sent no main header.\n");
        return;
    }

    //Parse HTTP header
    int pos = 0;
    if (seek_split_until(http_header_buffer, &pos, ' ', request.method, sizeof(request.method)) == -1 ||
        seek_split_until(http_header_buffer, &pos, ' ', request.path, sizeof(request.path)) == -1 ||
        seek_split_until(http_header_buffer, &pos, 0, request.version, sizeof(request.version)) == -1) {
        iprintf("WARN: HTTP main header is invalid.\n");
        return;
    }

    //Read headers
    request.header_count = 0;
    while (1) {
        //Read the header
        int read = read_header(sock);
        if (read == -1) {
            iprintf("WARN: HTTP request ended early.\n");
            return;
        }

        //Check if this is the end of headers
        if (read == 0)
            break;
        
        //Split
        pos = 0;
        char junk[3];
        if (seek_split_until(http_header_buffer, &pos, ':', request.headers[request.header_count].key, sizeof(request.headers[request.header_count].key)) == -1 ||
            seek_split_until(http_header_buffer, &pos, ' ', junk, sizeof(junk)) == -1 ||
            seek_split_until(http_header_buffer, &pos, 0, request.headers[request.header_count].value, sizeof(request.headers[request.header_count].value)) == -1) {
            iprintf("WARN: HTTP request header is invalid.\n");
            return;
        }

        //Advance and make sure we haven't hit the limit
        if (request.header_count++ == MAX_HTTP_HEADERS) {
            iprintf("WARN: HTTP request sent too many headers!\n");
            return;
        }
    }

    //Send response headers
    const char* response = "HTTP/1.0 200 OK\r\nServer: Nintendo DS\r\nConnection: close\r\n";
    send(request.sock, response, strlen(response), 0);

    //Handle
    handle_http_request(&request);
}

void serve_http_server() {
    //Create TCP listener socket
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == -1) {
        iprintf("Failed to create TCP listener socket.\n");
        return;
    }

    //Bind to address
    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(80);
    if ((bind(server, (struct sockaddr*)&servaddr, sizeof(servaddr))) != 0) {
        iprintf("Failed to bind to socket.\n");
        return;
    }

    //Begin listening
    if (listen(server, 1) != 0) {
        iprintf("Failed to begin listening.\n");
        return;
    }

    //Fetch own IP address
    u32 ip = Wifi_GetIP();
    iprintf("Ready: http://%li.%li.%li.%li/\n", (ip ) & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, (ip >> 24) & 0xFF);

    //Now, enter loop of accepting client requests
    while (1) {
        //Print
        iprintf("Awaiting request...\n");

        //Accept incoming socket
        int sock;
        struct sockaddr_in client;
        int clientLen;
        sock = accept(server, (struct sockaddr*)&client, &clientLen);
        if (sock < 0)
            break;
        
        //Process
        handle_request(sock);

        //Close connection
        shutdown(sock, 0);
        closesocket(sock);
    }
}

void http_write(http_request_t* request, const void* data, size_t len) {
    //Check if we need to finalize
    if (request->has_sent_response_headers == 0) {
        send(request->sock, "\r\n", 2, 0);
        request->has_sent_response_headers = 1;
    }

    //Send
    tcp_send_chunked(request->sock, data, len);
}

void http_write_text(http_request_t* request, const char* text) {
    http_write(request, text, strlen(text));
}

void http_send_header(http_request_t* request, const char* key, const char* value) {
    //Validate
    if (request->has_sent_response_headers != 0) {
        iprintf("WARN: HTTP response headers have already been set! Attempted to set new ones.\n");
        return;
    }

    //Format
    char buffer[512];
    sprintf(buffer, "%s: %s\r\n", key, value);

    //Send
    int len = strlen(buffer);
    if (send(request->sock, buffer, len, 0) != len) {
        iprintf("WARN: Failed to send HTTP header!\n");
    }
}

void http_send_header_int(http_request_t* request, const char* key, int value) {
    //Format
    char buffer[16];
    sprintf(buffer, "%i", value);

    //Send
    http_send_header(request, key, buffer);
}

int http_get_header(http_request_t* request, const char* key, char** value) {
    //Search
    for (int i = 0; i < request->header_count; i++) {
        //Check if match
        if (strcasecmp(request->headers[i].key, key) == 0) {
            *value = request->headers[i].value;
            return 1;
        }
    }

    return 0;
}