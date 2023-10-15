#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "ws2_32.lib")

const std::string log_file = "chat_log.txt";

void Log(const std::string& message) {
	std::ofstream file;
	file.open(log_file.c_str(), std::ios::app);
	if (file.is_open()) {
		file << message << std::endl;
		file.close();
	}
}

int main(int argc, char* argv[]) {
	if (argc < 3) {
		std::cerr << "使用方法: chat [server|client] [username]\n";
		return 1;
	}
	
	// 判断是服务器模式还是客户端模式
	bool is_server = strcmp(argv[1], "server") == 0;
	if (!is_server && strcmp(argv[1], "client") != 0) {
		std::cerr << "无效参数: " << argv[1] << "\n";
		std::cerr << "使用方法: chat [server|client] [username]\n";
		return 1;
	}
	
	const std::string username(argv[2]);
	
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		std::cerr << "WSAStartup 失败: " << result << "\n";
		return 1;
	}
	
	SOCKET sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		std::cerr << "创建套接字失败: " << WSAGetLastError() << "\n";
		WSACleanup();
		return 1;
	}
	
	// 支持IPv6和IPv4
	int dual_stack = 0;
	result = setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&dual_stack), sizeof(dual_stack));
	if (result != 0) {
		std::cerr << "设置套接字选项失败: " << WSAGetLastError() << "\n";
		closesocket(sock);
		WSACleanup();
		return 1;
	}
	
	if (is_server) {
		sockaddr_in6 addr;
		addr.sin6_family = AF_INET6;
		addr.sin6_addr = in6addr_any;
		addr.sin6_port = htons(5555);
		
		result = bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
		if (result == SOCKET_ERROR) {
			std::cerr << "绑定套接字失败: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		result = listen(sock, SOMAXCONN);
		if (result == SOCKET_ERROR) {
			std::cerr << "监听套接字失败: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::cout << "等待客户端连接...\n";
		
		sockaddr_in6 client_addr;
		int client_addr_len = sizeof(client_addr);
		SOCKET client_sock = accept(sock, reinterpret_cast<sockaddr*>(&client_addr), &client_addr_len);
		if (client_sock == INVALID_SOCKET) {
			std::cerr << "接受客户端连接失败: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		char client_ip[INET6_ADDRSTRLEN];
		inet_ntop(AF_INET6, &(client_addr.sin6_addr), client_ip, INET6_ADDRSTRLEN);
		std::cout << "客户端已连接，来自 " << client_ip << ":" << ntohs(client_addr.sin6_port) << "\n";
		
		std::thread recvThread([&]() {
			while (true) {
				char buffer[1024];
				int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
				if (bytes_received == SOCKET_ERROR) {
					std::cerr << "接收数据时发生错误: " << WSAGetLastError() << "\n";
					closesocket(client_sock);
					closesocket(sock);
					WSACleanup();
					return;
				}
				else if (bytes_received == 0) {
					std::cout << "客户端已断开连接\n";
					closesocket(client_sock);
					closesocket(sock);
					WSACleanup();
					return;
				}
				
				buffer[bytes_received] = '\0';
				std::cout << "[" << username << "] 从客户端收到消息: " << buffer << "\n";
				Log("[" + username + "] 从客户端收到消息: " + std::string(buffer));
			}
		});
		
		while (true) {
			std::string message;
			std::getline(std::cin, message);
			
			message = "[" + username + "] " + message; // 在消息前面加上用户名
			
			int bytes_sent = send(client_sock, message.c_str(), message.size(), 0);
			if (bytes_sent == SOCKET_ERROR) {
				std::cerr << "发送数据时发生错误: " << WSAGetLastError() << "\n";
				closesocket(client_sock);
				closesocket(sock);
				WSACleanup();
				return 1;
			}
			
			if (message == "[" + username + "] quit") {
				std::cout << "与客户端断开连接\n";
				closesocket(client_sock);
				closesocket(sock);
				WSACleanup();
				break;
			}
		}
		
		recvThread.join();
	}
	else {
		if (argc < 4) {
			std::cerr << "使用方法: chat client [ip_address] [username]\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::string server_ip = argv[2];
		
		sockaddr_in6 server_addr;
		server_addr.sin6_family = AF_INET6;
		inet_pton(AF_INET6, server_ip.c_str(), &(server_addr.sin6_addr));
		server_addr.sin6_port = htons(5555);
		
		result = connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
		if (result == SOCKET_ERROR) {
			std::cerr << "连接服务器失败: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::cout << "已连接到服务器 " << server_ip << ":5555\n";
		
		std::thread recvThread([&]() {
			while (true) {
				char buffer[1024];
				int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
				if (bytes_received == SOCKET_ERROR) {
					std::cerr << "接收数据时发生错误: " << WSAGetLastError() << "\n";
					closesocket(sock);
					WSACleanup();
					return;
				}
				else if (bytes_received == 0) {
					std::cout << "与服务器断开连接\n";
					closesocket(sock);
					WSACleanup();
					return;
				}
				
				buffer[bytes_received] = '\0';
				std::cout << "[" << username << "] 从服务器收到消息: " << buffer << "\n";
				Log("[" + username + "] 从服务器收到消息: " + std::string(buffer));
			}
		});
		
		while (true) {
			std::string message;
			std::getline(std::cin, message);
			
			message = "[" + username + "] " + message; // 在消息前面加上用户名
			
			int bytes_sent = send(sock, message.c_str(), message.size(), 0);
			if (bytes_sent == SOCKET_ERROR) {
				std::cerr << "发送数据时发生错误: " << WSAGetLastError() << "\n";
				closesocket(sock);
				WSACleanup();
				return 1;
			}
			
			if (message == "[" + username + "] quit") {
				std::cout << "与服务器断开连接\n";
				closesocket(sock);
				WSACleanup();
				break;
			}
		}
		
		recvThread.join();
	}
	
	WSACleanup();
	
	return 0;
}
