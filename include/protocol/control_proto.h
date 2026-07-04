#ifndef PROTOCOL_CONTROL_PROTO_H
#define PROTOCOL_CONTROL_PROTO_H

typedef void (*control_proto_callback_t)(const char *prefix, const char *message);

int control_proto_start_listen(int port, control_proto_callback_t cb);
void control_proto_stop_listen(void);

#endif // PROTOCOL_CONTROL_PROTO_H