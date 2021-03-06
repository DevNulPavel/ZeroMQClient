//  Hello World client
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <zmq.h>

//  Receive ZeroMQ string from socket and convert into C string
//  Chops string at 255 chars, if it's longer
static char* s_recv (void *socket) {
    char buffer [256];
    int size = zmq_recv (socket, buffer, 255, 0);
    if (size == -1)
        return NULL;
    if (size > 255)
        size = 255;
    buffer [size] = 0;
    return strdup (buffer);
}

static void s_send (void* socket, const char* message) {
    size_t length = strlen(message);    // TODO: нужно ли вычислять размер?
    zmq_send(socket, message, length, 0);
}

int simpleClient(){
    printf ("Connecting to hello world server…\n");
    void *context = zmq_ctx_new ();
    void *requester = zmq_socket (context, ZMQ_REQ);
    zmq_connect (requester, "tcp://localhost:5555");
    
    int request_nbr;
    for (request_nbr = 0; request_nbr < 1000000; request_nbr++) {
        char buffer [10];
        printf ("Sending Hello %d…\n", request_nbr);
        zmq_send (requester, "Hello", 5, 0);
        zmq_recv (requester, buffer, 10, 0);
        printf ("Received World %d\n", request_nbr);
    }
    zmq_close (requester);
    zmq_ctx_destroy (context);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

int pushClient(){
    //  Socket to talk to server
    printf ("Collecting updates from weather server…\n");
    void *context = zmq_ctx_new ();
    void *subscriber = zmq_socket (context, ZMQ_SUB);
    int rc = zmq_connect (subscriber, "tcp://localhost:5555");
    assert(rc == 0);
    
    //  Subscribe to zipcode, default is NYC, 10001
    const char* filter = "10001 ";
    rc = zmq_setsockopt (subscriber, ZMQ_SUBSCRIBE,
                         filter, strlen(filter));
    assert (rc == 0);
    
    //  Process 100 updates
    int update_nbr;
    long total_temp = 0;
    for (update_nbr = 0; update_nbr < 100; update_nbr++) {
        char* string = s_recv (subscriber);
        
        int zipcode, temperature, relhumidity;
        sscanf (string, "%d %d %d", &zipcode, &temperature, &relhumidity);
        total_temp += temperature;
        
        free(string);
    }
    printf ("Average temperature for zipcode '%s' was %dF\n", filter, (int) (total_temp / update_nbr));
    
    zmq_close (subscriber);
    zmq_ctx_destroy (context);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

int synchronizedSubscriber(void) {
    void* context = zmq_ctx_new ();
    
    // подключаемся к публикациям
    void* subscriber = zmq_socket (context, ZMQ_SUB);
    zmq_connect(subscriber, "tcp://localhost:5561");
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, "", 0);
    
    //  0MQ is so fast, we need to wait a while…
    sleep (1);
    
    //  Second, synchronize with publisher
    void *syncclient = zmq_socket (context, ZMQ_REQ);
    zmq_connect(syncclient, "tcp://localhost:5562");
    
    //  - send a synchronization request
    s_send(syncclient, "");
    
    //  - wait for synchronization reply
    char *string = s_recv (syncclient);
    free (string);
    
    //  Third, get our updates and report how many we got
    int update_nbr = 0;
    while (1){
        char* string = s_recv (subscriber);
        if (strcmp(string, "END") == 0) {
            free (string);
            break;
        }
        free (string);
        update_nbr++;
    }
    printf ("Received %d updates\n", update_nbr);
    
    zmq_close (subscriber);
    zmq_close (syncclient);
    zmq_ctx_destroy (context);
    return 0;
}

//////////////////////////////////////////////////////////////////////////////

int main(void) {
    return synchronizedSubscriber();
}
