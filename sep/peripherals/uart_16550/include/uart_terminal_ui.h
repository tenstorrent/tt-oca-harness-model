#pragma once

#include <systemc.h>

#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Linux socket headers
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "terminal_if.h"
#include "thread_safe_queue_channel.h"

// Define socket types for clarity
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

// UART Terminal UI module
// Acts as a bridge between SystemC UART ports and a TCP socket
class uart_terminal_ui : public sc_module, public terminal_if {
   public:
    // Terminal interface communication
    sc_port<terminal_if> uart_port;     // Port to send data to UART
    sc_export<terminal_if> uart_export; // Export for receiving data from UART

    sc_time m_byte_time;        // Time to transmit/receive one byte


    // Constructor
    SC_HAS_PROCESS(uart_terminal_ui);
    uart_terminal_ui(sc_module_name name, int port = 8888,
                     bool auto_spawn_client = false);

    // Destructor
    ~uart_terminal_ui();
    
    // Terminal interface implementation
    void uart_to_terminal(uint8_t data) override;
    void terminal_to_uart(uint8_t data) override;

   private:
    // Thread-safe queue channel for data from client (external thread) to UART (SystemC thread)
    thread_safe_queue_channel rx_channel;

    void rx_data_handler();  // SystemC thread to process RX data
    // Socket handling
    void socket_server_thread();  // Thread for socket server operations
    void handle_client(SOCKET client_socket);  // Handle a connected client
    void spawn_client_process();  // Spawn the Python client process

    // Thread-safe queues for data exchange between SystemC and socket threads
    std::queue<uint8_t> tx_queue;  // Data from UART to socket

    // Synchronization primitives
    std::mutex tx_mutex;
    std::condition_variable tx_cv;

    // Socket variables
    int server_port;
    SOCKET server_socket;
    std::atomic<bool> socket_running;
    std::thread server_thread;

    // Client handling
    SOCKET client_socket;
    std::atomic<bool> client_connected;
    bool
        auto_spawn_client;  // Whether to automatically spawn the client process
    pid_t client_pid;       // Process ID of spawned client

    // Helper methods
    bool initialize_socket();
    void cleanup_socket();
    bool send_data_to_client(uint8_t data);
    void process_client_data(uint8_t data);
    void closesocket(SOCKET client_sock);

};

