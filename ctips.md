c tips

- function(int *port);  
OK: int port; then function(&port)   
NOT OK; int *port;then, function(port)  
- strstr shold consider when strstr!=NULL && strstr==NULL(not found)

- local program variable when string declare as char port[100]; NOT AS char* port;

- tenative arg: const char** argv means it is not filled in this fuction!!