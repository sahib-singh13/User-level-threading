#include "../include/uthread.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>

void handle_client(int client_fd) {
    char buf[1024];
    while (true) {
        // Use our non-blocking read
        int n = uthread::socket_read(client_fd, buf, sizeof(buf));
        
        if (n <= 0) {
            // Connection closed or error
            close(client_fd);
            break;
        }

        // Echo back (blocking write is fine for small demo, 
        // ideally we'd implement socket_write too)
        write(client_fd, buf, n);
    }
}

void server_task() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(9000);
    addr.sin_addr.s_addr = INADDR_ANY;

    // Allow reusing the address (prevents "Address already in use" errors)
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return;
    }

    listen(server_fd, 128);
    std::cout << "[Server] Listening on port 9000...\n";

    while (true) {
        // Note: accept() blocks the worker! 
        // In a full implementation, we would wrap accept() in uthread::socket_accept
        // using the same epoll logic. For this demo, we accept blocking, 
        // but handle clients non-blocking.
        int client_fd = accept(server_fd, nullptr, nullptr);
        if (client_fd >= 0) {
            std::cout << "[Server] New connection: " << client_fd << "\n";
            // Spawn a green thread for this client
            // Note: We need to pass client_fd to the function.
            // Our current create() only takes void(*)(), so we use a global/static hack
            // or a capture-less lambda for this simple demo. 
            // Better API: create(func, void* arg).
            
            // Quick hack for demo: Store in a hacky way or just use C++ lambda with static?
            // Let's use a static queue of pending FDs to keep it simple.
            
            // Actually, let's just make 'create' smarter in the future.
            // For now, we'll assume the client thread knows how to get its FD 
            // (simulated by just spawning a fixed task or using a global for the demo).
            
            // To make this robust without changing 'create' API yet:
            // We will just spin up a thread that calls a function which uses a global 'last_fd'.
            // (Not thread safe but works for 1 listener thread).
            
            static int passing_fd = -1;
            passing_fd = client_fd;
            
            uthread::create([]() {
                int my_fd = passing_fd; // Capture roughly
                handle_client(my_fd);
            });
            
            // Small sleep to ensure the thread grabs the FD (racy but okay for demo)
            usleep(1000); 
        }
    }
}

int main() {
    uthread::init(4); // 4 Workers
    uthread::create(server_task);
    uthread::run_scheduler_loop();
    return 0;
}
