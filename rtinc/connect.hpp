enum class ConnectMode : int {
    Min = 0,
    Max,
    All
};
string client_name();
void connect(string a, string b, string filter_a="", string filter_b="", ConnectMode mode=ConnectMode::Max);
void disconnect(string a, string b, string filter_a="", string filter_b="", ConnectMode mode=ConnectMode::Max);
