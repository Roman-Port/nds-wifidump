#include "defines.h"

#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>

#include "../zlib/deflate.h"

static int enable_compression_scheme(http_request_t* request, const char* name) {
    //Get the header
    char* supported;
    if (!http_get_header(request, "Accept-Encoding", &supported))
        return 0;
    
    //Search through types
    if (strstr(supported, name) != 0) {
        //Supported! Set type
        http_send_header(request, "content-encoding", name);
        return 1;
    }

    //Not supported
    return 0;
}

#define CHUNK_SIZE 1024

static u8 compression_buffer[CHUNK_SIZE];

static void transmit_deflate(http_request_t* request, const u8* buffer, int length) {
    //Prepare compression
    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    deflateInit(&strm, Z_DEFAULT_COMPRESSION);

    //Compress all data
    int chunk;
    int flush;
    while (length > 0) {
        //Determine bytes available
        chunk = length > CHUNK_SIZE ? CHUNK_SIZE : length;

        //Configure input
        strm.avail_in = chunk;
        flush = length <= CHUNK_SIZE ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = buffer;

        //Advance
        buffer += chunk;
        length -= chunk;

        //Do compression on this chunk
        do {
            //Set up output
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = compression_buffer;

            //Compress
            deflate(&strm, flush);

            //Write compressed data
            http_write(request, compression_buffer, CHUNK_SIZE - strm.avail_out);
        } while (strm.avail_out == 0);
    }

    //Clean up
    deflateEnd(&strm);
}

void send_compressed_data(http_request_t* request, const u8* buffer, int length) {
    //Set length
    http_send_header_int(request, "content-length", length);
    
    //Try different compression methods
    if (enable_compression_scheme(request, "deflate")) {
        transmit_deflate(request, buffer, length);
    } else {
        //No supported methods. Fallback to standard
        http_write(request, buffer, length);
    }
}