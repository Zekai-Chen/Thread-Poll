#include <iostream>

#include "BbServer/BbServer.h"

using namespace std;

extern Config g_config;

int main(int argc, char ** argv)
{
    BbServer server;

    server.ParseCommandLine(argc, argv);

    if (server.InitServer()) {
        server.InitSignalServer();
        server.InitDaemonServer();
        cout << "INIT SERVER SUCCESS!" << endl;
        server.Run();
    }

    return 0;
}
