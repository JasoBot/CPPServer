#include <iostream>
#include <thread>
#include <winsock2.h>
#include <vector>
#include <string>
#include <unistd.h>
#include "ThreadPool.hpp"
// INADDR_ANY 零地址的宏，实际值为0。0=0.0.0.0 对于0来说，大端小端没有区别，因此不需要转换
/*
零地址可以绑定本地任意一个IP地址，给监听的套接字绑定零地址意味着服务器端会自动的读取网卡的实际IP
网卡的IP是什么，就和这个实际的IP进行绑定，不是绑定的0.
*/

class Clientfd // 存储客户端的信息
{

public:
    int clientSockfd;
    int num; // 这是第几个连接的客户端
    sockaddr_in client_addr;
    Clientfd(int clientfd, int num, sockaddr_in caddr) : clientSockfd(clientfd), num(num),
                                                         client_addr(caddr) {}
    ~Clientfd() = default;
};

class Server
{
public:
    int connectNum;
    WSADATA wsdata;
    int listenfd;
    std::vector<Clientfd> Client;
    sockaddr_in server_addr;
    ThreadPool pool;
    struct ClientData
    {
        int sockfd;
        const char *clientIP;
        int clientPort;
        ClientData(int sockfd, const char *ip, int port)
            : sockfd(sockfd), clientIP(ip), clientPort(port) {}
    };

public:
    Server(int MinThreadnum, int MaxThreadnum, int Taskmax) : pool(MinThreadnum, MaxThreadnum, Taskmax)
    {
        WSAStartup(MAKEWORD(2, 2), &wsdata);
        listenfd = socket(AF_INET, SOCK_STREAM, 0);
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY; // inet_addr("0.0.0.0");
        server_addr.sin_port = htons(8888);
        if (bind(listenfd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cout << " bind error! " << std::endl;
            return;
        }
        if (listen(listenfd, 128) < 0)
        {
            std::cout << " listen error! " << std::endl;
            return;
        }
    }

    static void doClient(void *args);
    static void acceptClient(void* args);
    void startAccept();
    void startClient();

    ~Server()
    {
        closesocket(listenfd);
        WSACleanup();
    }
};



void Server::doClient(void *args)
{
    ClientData *data = (ClientData *)args;
    int clientSockfd = data->sockfd;
    const char *clientIP = data->clientIP;
    int clientPort = data->clientPort;
    printf("The client's IP is %s ,and its port is %d \n", clientIP, clientPort);
    char rBuf[1024];
    while (true)
    {
        memset(rBuf, 0, sizeof(rBuf));
        int recvlen = recv(clientSockfd, rBuf, sizeof(rBuf), 0);

        if (recvlen > 0)
        {
            // 处理接收到的数据
            char sBuf[1024];
            memset(sBuf, 0, sizeof(sBuf));
            std::string respond = "Thank u for ur message: ";
            /*
            关于 strncpy(sBuf + respond.length(), rBuf, recvlen); 这一行，实际上不需要
            从 sBuf + respond.length() - 1 开始复制。原因如下：
            1、std::string 的 length() 方法返回字符串中字符的数量，不包括终止的 null 字符（\0）。
            2、当您使用 strcpy(sBuf, respond.c_str()); 将 respond 复制到 sBuf 时，sBuf 中已经
                包含了 respond 末尾的 null 字符。
            3、使用 strncpy(sBuf + respond.length(), rBuf, recvlen); 将从 respond 末尾的 null
                字符之后开始复制，正好接在 respond 的内容之后。
            */
            strcpy(sBuf, respond.c_str());
            strncpy(sBuf + respond.length(), rBuf, recvlen);
            // 要发送的长度为 strlen(sBuf)+1 因为 strlen() 只返回字符数量，+1将最后的 '\0' 也一并发送
            send(clientSockfd, sBuf, strlen(sBuf) + 1, 0);
        }
        else if (recvlen == 0)
        {
            // 客户端关闭了连接
            std::cout << "Client closed the connection." << std::endl;
            break;
        }
        else
        {
            // 发生错误
            std::cerr << "Recv error: " << WSAGetLastError() << std::endl;
            break;
        }
    }
    closesocket(clientSockfd);
    // for(auto it=Client.begin();it!=Client.end();++it){
    //     if(it->clientSockfd==clientSockfd){
    //         Client.erase(it);
    //     }
    // }
    delete data;
}

void Server::acceptClient(void* args)
{
    Server* mServer=(Server*) args;
    int clientSockfd;
    struct sockaddr_in caddr;
    memset(&caddr, 0, sizeof(caddr));
    int caddr_len;
    caddr_len = sizeof(sockaddr_in);
    while (1)
    {
        std::cout << "Prepare to accept" << std::endl;
        clientSockfd = accept(mServer->listenfd, (sockaddr *)&caddr, &caddr_len);
        std::cout << "Accepted" << std::endl;
        mServer->Client.emplace_back(clientSockfd, mServer->Client.size() + 1, caddr);
        ClientData *data = new ClientData(clientSockfd, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port));
        mServer->pool.addTask(&Server::doClient,data);//add "doClient()"
    }
}





int main()
{
    Server *mServer = new Server(3, 10, 100);
    mServer->pool.addTask(&Server::acceptClient,mServer); //add "acceptClient()"
    while (1)
    {
        sleep(2);
    }
    return 0;
}