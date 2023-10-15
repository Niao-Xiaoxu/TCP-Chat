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
	if (argc < 2) {
		std::cerr << "Usage: chat [server|client]\n";
		return 1;
	}
	
	bool is_server = strcmp(argv[1], "server") == 0;
	if (!is_server && strcmp(argv[1], "client") != 0) {
		std::cerr << "Invalid argument: " << argv[1] << "\n";
		std::cerr << "Usage: chat [server|client]\n";
		return 1;
	}
	
	WSADATA wsa_data;
	int result = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (result != 0) {
		std::cerr << "WSAStartup failed: " << result << "\n";
		return 1;
	}
	
	SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == INVALID_SOCKET) {
		std::cerr << "Failed to create socket: " << WSAGetLastError() << "\n";
		WSACleanup();
		return 1;
	}
	
	if (is_server) {
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = htons(5555);
		
		result = bind(sock, (sockaddr*)&addr, sizeof(addr));
		if (result == SOCKET_ERROR) {
			std::cerr << "Failed to bind socket: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		result = listen(sock, SOMAXCONN);
		if (result == SOCKET_ERROR) {
			std::cerr << "Failed to listen on socket: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::cout << "Waiting for client to connect...\n";
		
		sockaddr_in client_addr;
		int client_addr_len = sizeof(client_addr);
		SOCKET client_sock = accept(sock, (sockaddr*)&client_addr, &client_addr_len);
		if (client_sock == INVALID_SOCKET) {
			std::cerr << "Failed to accept client connection: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		char client_ip[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET_ADDRSTRLEN);
		std::cout << "Client connected from " << client_ip << ":" << ntohs(client_addr.sin_port) << "\n";
		
		std::thread recvThread([&]() {
			while (true) {
				char buffer[1024];
				int bytes_received = recv(client_sock, buffer, sizeof(buffer), 0);
				if (bytes_received == SOCKET_ERROR) {
					std::cerr << "Error receiving data: " << WSAGetLastError() << "\n";
					closesocket(client_sock);
					closesocket(sock);
					WSACleanup();
					return;
				} else if (bytes_received == 0) {
					std::cout << "Client disconnected\n";
					closesocket(client_sock);
					closesocket(sock);
					WSACleanup();
					return;
				}
				
				buffer[bytes_received] = '\0';
				std::cout << "Received from client: " << buffer << "\n";
				Log("Received from client: " + std::string(buffer));
			}
		});
		
		while (true) {
			std::string message;
			std::getline(std::cin, message);
			
			int bytes_sent = send(client_sock, message.c_str(), message.size(), 0);
			if (bytes_sent == SOCKET_ERROR) {
				std::cerr << "Error sending data: " << WSAGetLastError() << "\n";
				closesocket(client_sock);
				closesocket(sock);
				WSACleanup();
				return 1;
			}
			
			if (message == "quit") {
				std::cout << "Disconnected from client\n";
				closesocket(client_sock);
				closesocket(sock);
				WSACleanup();
				break;
			}
		}
		
		recvThread.join();
	} else {
		if (argc < 3) {
			std::cerr << "Usage: chat client [ip_address]\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::string server_ip = argv[2];
		
		sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		inet_pton(AF_INET, server_ip.c_str(), &(server_addr.sin_addr));
		server_addr.sin_port = htons(5555);
		
		result = connect(sock, (sockaddr*)&server_addr, sizeof(server_addr));
		if (result == SOCKET_ERROR) {
			std::cerr << "Failed to connect to server: " << WSAGetLastError() << "\n";
			closesocket(sock);
			WSACleanup();
			return 1;
		}
		
		std::cout << "Connected to server at " << server_ip << ":5555\n";
		
		std::thread recvThread([&]() {
			while (true) {
				char buffer[1024];
				int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
				if (bytes_received == SOCKET_ERROR) {
					std::cerr << "Error receiving data: " << WSAGetLastError() << "\n";
					closesocket(sock);
					WSACleanup();
					return;
				} else if (bytes_received == 0) {
					std::cout << "Disconnected from server\n";
					closesocket(sock);
					WSACleanup();
					return;
				}
				
				buffer[bytes_received] = '\0';
				std::cout << "Received from server: " << buffer << "\n";
				Log("Received from server: " + std::string(buffer));
			}
		});
		
		while (true) {
			std::string message;
			std::getline(std::cin, message);
			
			int bytes_sent = send(sock, message.c_str(), message.size(), 0);
			if (bytes_sent == SOCKET_ERROR) {
				std::cerr << "Error sending data: " << WSAGetLastError() << "\n";
				closesocket(sock);
				WSACleanup();
				return 1;
			}
			
			if (message == "quit") {
				std::cout << "Disconnected from server\n";
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

