#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <pthread.h>

#define MAX_PAYLOAD_SIZE 1024 * 10

static struct lws *client_wsi = NULL;
static int force_exit = 0;
static char input[MAX_PAYLOAD_SIZE];

struct thread_data {
    struct lws_context *context;
};

static int callback_client(struct lws *wsi, enum lws_callback_reasons reason,
                           void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_CLIENT_ESTABLISHED:
        printf("Connection established\n");
        force_exit = 1;
        break;

    case LWS_CALLBACK_CLIENT_RECEIVE:
        printf("Received data: %s\n", (char *)in);
        break;

    case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
        printf("Connection error\n");
        break;

    case LWS_CALLBACK_CLIENT_WRITEABLE:
        printf("Writeable data: %s\n", (char *)in);

    default:
        printf("Unknown\n");
        break;
    }

    return 0;
}

static struct lws_protocols protocols[] =
{
    {"ws", callback_client, 0, MAX_PAYLOAD_SIZE},
    {NULL, NULL, 0, 0}
};

void* lws_service_thread(void *arg)
{
    struct thread_data* data = (struct thread_data*)arg;

    while(1)
    {
        lws_service(data->context, 50);
    }

    pthread_exit(NULL);
}

int main(int argc, char *argv[])
{
    pthread_t thread;
    struct lws_context *context;
    struct lws_context_creation_info info;
    const char *server_ip;
    int server_port;
    struct thread_data data;

    if (argc != 3)
    {
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    server_ip = argv[1];
    server_port = atoi(argv[2]);

    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = 0;

    context = lws_create_context(&info);
    if (!context)
    {
        fprintf(stderr, "Error creating libwebsocket context\n");
        return 1;
    }

    struct lws_client_connect_info connect_info = {0};
    connect_info.context = context;
    connect_info.address = server_ip;
    connect_info.port = server_port;
    connect_info.path = "/";
    connect_info.host = connect_info.address;
    connect_info.origin = connect_info.address;
    connect_info.protocol = "ws";
    connect_info.pwsi = &client_wsi;

    struct lws *wsi = lws_client_connect_via_info(&connect_info);

    if (!wsi)
    {
        fprintf(stderr, "Failed to connect to server\n");
        lws_context_destroy(context);
        return 1;
    }

    data.context = context;

    if (pthread_create(&thread, NULL, lws_service_thread, (void *)&data) != 0)
    {
        lws_context_destroy(context);

        return 0;
    }

    while (1)
    {
        fgets(input, MAX_PAYLOAD_SIZE, stdin);

        if (0 == strcmp("quit", input))
        {
            break;
        }

        lws_write(wsi, (unsigned char *)input, strlen(input), LWS_WRITE_TEXT);
    }

    pthread_join(thread, NULL);

    lws_context_destroy(context);

    return 0;
}
