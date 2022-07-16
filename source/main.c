#include "defines.h"

#include <nds.h>
#include <dswifi9.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include <stdio.h>
#include <string.h>

int main(void) {
    //Setup the sub screen for printing
	consoleDemoInit();
    iprintf("\n\nDownload Play Wi-Fi Dump\n    by RomanPort\n\n");

    //Activate slots
    sysSetBusOwners(true, true);
	sysSetCartOwner(true);

    //Connect to Wi-Fi
    iprintf("Connecting to Wi-Fi...\n");
	if (!Wifi_InitDefault(WFC_CONNECT)) {
        iprintf("Failed to connect! Check your Wi-Fi settings.\n");
        return 0;
	}

    //Serve
    serve_http_server();

	return 0;
}

static char request_buffer[1024];

static void prepare_download(http_request_t* request, const char* fileName, const char* fileExt, const void* data, int size) {
    //Format disposition header for filename
    char disposition[128];
    sprintf(disposition, "attachment; filename=\"%s.%s\"", fileName, fileExt);

    //Set headers
    http_send_header(request, "content-type", "application/octet-stream");
    http_send_header(request, "content-disposition", disposition);

    //Send over in chunks
    send_compressed_data(request, (const u8*)data, size);
}

static void handle_page_root(http_request_t* request) {
    //Request sizes
    int gbaRomSize = query_gba_rom_size();
    int gbaSavType = gbaRomSize == 0 ? 0 : gbasav_identify();

    //Send
    http_write_text(request, "<html><head><title>Wi-Fi Dump</title><style>table, th, td { border: 1px solid black; } th, td { text-align:left; padding:5px; }</style></head><body><h1>Wi-Fi Dumper by RomanPort</h1><p>The following sources are available.</p>");
    http_write_text(request, "<table><tr><th>Name</th><th>Type</th><th>Size</th><th>Actions</th></tr>");
    if (gbaRomSize > 0) {
        char name[13];
        safe_string(((struct sGBAHeader*)GBAROM)->title, 12, name, 1);
        sprintf(request_buffer, "<tr><td>%s</td><td>GBA ROM</td><td>%i kB</td><td><a href=\"/gba/rom/download\" target=\"_blank\">Download</a></td></tr>", name, gbaRomSize / 1024);
        http_write_text(request, request_buffer);
    }
    if (gbaSavType > 0) {
        char name[13];
        safe_string(((struct sGBAHeader*)GBAROM)->title, 12, name, 1);
        sprintf(request_buffer, "<tr><td>%s</td><td>GBA SAVE - %s</td><td>%i kB</td><td><a href=\"/gba/sav/download\" target=\"_blank\">Download</a> - <a href=\"/gba/sav/upload\" target=\"_blank\">Upload</a></td></tr>", name, gbasav_types[gbaSavType].name, gbasav_types[gbaSavType].size / 1024);
        http_write_text(request, request_buffer);
    }
    http_write_text(request, "</table></body></html>");
}

static void handle_page_gba_sav_download(http_request_t* request) {
    //Query the type
    int type = gbasav_identify();
    int size = gbasav_types[type].size;

    //Get GBA Game ID from the header
    char gamecode[5];
    safe_string(((struct sGBAHeader*)GBAROM)->gamecode, 4, gamecode, 0);

    //Allocate buffer for this type and read the save file into it
    void* buffer = malloc(size);
    gbasav_read((u8*)buffer, 0, size, type);
    
    //Send
    prepare_download(request, gamecode, "sav", buffer, size);

    //Cleanup
    free(buffer);
}

static void handle_page_gba_rom_download(http_request_t* request) {
    //Query the size
    int size = query_gba_rom_size();

    //Get GBA Game ID from the header
    char gamecode[5];
    safe_string(((struct sGBAHeader*)GBAROM)->gamecode, 4, gamecode, 0);

    //Send
    prepare_download(request, gamecode, "gba", GBAROM, size);
}

void handle_http_request(http_request_t* request) {
    if (strcmp(request->path, "/") == 0)
        handle_page_root(request);
    else if (strcmp(request->path, "/gba/rom/download") == 0)
        handle_page_gba_rom_download(request);
    else if (strcmp(request->path, "/gba/sav/download") == 0)
        handle_page_gba_sav_download(request);
    else
        http_write_text(request, "<html><head><title>Not Found</title></head><body><h1>Not Found</h1><p>This page is invalid.</p></body></html>");
}