#define CONNECT_MIN 0
#define CONNECT_MAX 1
#define CONNECT_ALL 2
void connect(string a, string b, string filter_a="", string filter_b="", int mode=CONNECT_MAX);
void disconnect(string a, string b, string filter_a="", string filter_b="", int mode=CONNECT_MAX);
