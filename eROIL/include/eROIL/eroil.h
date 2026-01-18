#ifndef EROIL_HEADER
#define EROIL_HEADER

bool init_manager(int id);
void* open_send(int label, void* buf, int size);
bool close_send(void* handle);
void* open_recv(int label, void* buf, int size, void* sem);
bool close_recv(void* handle);
void send_label(void* handle, void* buf, int buf_size, int buf_offset);
#endif