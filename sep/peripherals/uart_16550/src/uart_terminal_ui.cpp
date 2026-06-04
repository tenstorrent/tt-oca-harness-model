#include "uart_terminal_ui.h"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include "uart_log_adapter.h"


/**
 * @brief Construct a new uart_terminal_ui module.
 *
 * Initializes the SystemC threads, networking, and optionally spawns the
 * terminal client process.
 *
 * @param name SystemC module instance name.
 * @param port TCP port the server listens on for client connections.
 * @param auto_spawn_client If true, automatically launches the Python UART client.
 */
uart_terminal_ui::uart_terminal_ui(sc_module_name name, int port,
                                   bool auto_spawn_client)
    : sc_module(name),
      m_byte_time(1, SC_US),
      rx_channel("rx_channel"),
      server_port(port),
      server_socket(INVALID_SOCKET),
      socket_running(false),
      client_socket(INVALID_SOCKET),
      client_connected(false),
      auto_spawn_client(auto_spawn_client),
      client_pid(-1)
    {
    // Register SystemC processes
    
    // Bind export to this module
    uart_export(*this);

    // Register RX data handler thread
    SC_THREAD(rx_data_handler);

    // Initialize socket and start server thread
    if (initialize_socket()) {
        socket_running = true;
        server_thread =
            std::thread(&uart_terminal_ui::socket_server_thread, this);
        UART_DEBUG("UART Terminal UI initialized successfully");

        // Spawn client process if auto_spawn_client is enabled
        if (auto_spawn_client) {
            UART_DEBUG("Auto-spawning client process");
            spawn_client_process();
        }
    } else {
        UART_DEBUG("Failed to initialize UART Terminal UI socket");
    }
}

/**
 * @brief Destroy the uart_terminal_ui module.
 *
 * Stops the socket server thread, disconnects any client, and cleans up
 * socket resources.
 */
uart_terminal_ui::~uart_terminal_ui() {
    // Stop socket server thread
    socket_running = false;

    closesocket(client_socket);
    // Cleanup socket resources
    cleanup_socket();

    // Wait for server thread to finish
    if (server_thread.joinable()) {
        server_thread.join();
    }

    UART_DEBUG("UART Terminal UI destroyed");
}


/**
 * @brief SystemC thread that monitors TX activity and forwards bytes to the socket.
 *
 * On each `tx_valid` event, reads `tx_data`, queues it for the socket server
 * thread, and notifies the condition variable. If `auto_spawn_client` is false,
 * it attempts to spawn the client once.
 */
/**
 * @brief Terminal interface implementation: UART to Terminal
 * 
 * This method is called by the UART IP core when it needs to transmit
 * a byte to the terminal. It replaces the tx_monitor_thread.
 * 
 * @param data Byte to be transmitted to the terminal
 */
void uart_terminal_ui::uart_to_terminal(uint8_t data) {
    
    if(auto_spawn_client == false){
        auto_spawn_client = true;
        UART_DEBUG("Auto-spawning client process");
        spawn_client_process();
    }

    // Add data to tx_queue for socket thread to send
    {
        std::lock_guard<std::mutex> lock(tx_mutex);
        tx_queue.push(data);
    }

    // Notify socket thread that data is available
    tx_cv.notify_one();

    // Debug output
    std::stringstream ss;
    ss << "TX: 0x" << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(data) << " ('"
       << (isprint(data) ? static_cast<char>(data) : '.') << "')";
    UART_DEBUG(ss.str());
}

/**
 * @brief Terminal interface implementation: Terminal to UART
 * 
 * This method is called by the terminal when it receives data from
 * the client. This is a placeholder since the terminal uses
 * uart_port to send data, not receive via this method.
 * 
 * @param data Byte received (not used in Terminal UI)
 */
void uart_terminal_ui::terminal_to_uart(uint8_t data) {
    // This should not be called on Terminal side
    // Terminal sends via uart_port->terminal_to_uart()
    UART_DEBUG("Warning: terminal_to_uart called on Terminal UI (should not happen)");
}
/**
 * @brief Background thread that runs the TCP server.
 *
 * Accepts incoming client connections and, when connected, drains the TX queue
 * and sends bytes to the client. Operates until `socket_running` is cleared.
 */
void uart_terminal_ui::socket_server_thread() {
    UART_DEBUG("Socket server thread started");

    while (socket_running) {
        if (client_connected) {
            // Process data from tx_queue and send to client
            std::unique_lock<std::mutex> lock(tx_mutex);
            if (tx_cv.wait_for(lock, std::chrono::milliseconds(100),
                               [this] { return !tx_queue.empty(); })) {
                while (!tx_queue.empty()) {
                    uint8_t data = tx_queue.front();
                    tx_queue.pop();
                    UART_DEBUG("Sending data to client");
                    // Send data to client
                    if (!send_data_to_client(data)) {
                        UART_DEBUG("Failed to send data to client");
                        // Failed to send data, client might be disconnected
                        client_connected = false;
                        break;
                    }
                }
            }
            lock.unlock();
        } else {
            // Accept new client connections
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            // Set server socket to non-blocking mode for accept
            int flags = fcntl(server_socket, F_GETFL, 0);
            fcntl(server_socket, F_SETFL, flags | O_NONBLOCK);

            // Try to accept a connection
            SOCKET new_client = accept(
                server_socket, (struct sockaddr*)&client_addr, &client_len);

            if (new_client != INVALID_SOCKET) {
                // Set client socket to blocking mode
                UART_TRACE("Client connected: " << new_client);
                flags = fcntl(new_client, F_GETFL, 0);
                fcntl(new_client, F_SETFL, flags & ~O_NONBLOCK);

                // Handle the client in a separate thread
                client_socket = new_client;
                client_connected = true;

                // Log client connection
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip,
                          INET_ADDRSTRLEN);
                std::stringstream ss;
                ss << "Client connected from " << client_ip << ":"
                   << ntohs(client_addr.sin_port);
                UART_DEBUG(ss.str());

                // Start handling the client
                std::thread client_thread(&uart_terminal_ui::handle_client,
                                          this, new_client);
                client_thread.detach();
            }

            // Sleep to avoid busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    UART_DEBUG("Socket server thread stopped");
}

/**
 * @brief Handle a connected client socket.
 *
 * Receives data from the client, byte-by-byte, and forwards it into the module
 * via `process_client_data()`. Closes the socket when the client disconnects or
 * an error occurs.
 *
 * @param client_socket The connected client socket handle.
 */
void uart_terminal_ui::handle_client(SOCKET client_socket) {
    UART_DEBUG("Client handler thread started");

    // Buffer for receiving data
    uint8_t buffer[1024];
    int bytes_received;

    while (client_connected && socket_running) {
        // Receive data from client
        bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);

        if (bytes_received > 0) {
            // Process received data
            for (int i = 0; i < bytes_received; i++) {
                process_client_data(buffer[i]);
            }
        } else if (bytes_received == 0) {
            // Connection closed by client
            UART_DEBUG("Client disconnected");
            client_connected = false;
            break;
        } else {
            // Error in recv
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::stringstream ss;
                ss << "Error receiving data from client: " << strerror(errno);
                UART_DEBUG(ss.str());
                client_connected = false;
                break;
            }
        }
    }

    // Close client socket
    closesocket(client_socket);
    UART_DEBUG("Client handler thread stopped");
}

/**
 * @brief Initialize the TCP server socket.
 *
 * Creates the server socket, sets socket options, binds to the configured
 * port, and starts listening for incoming connections.
 *
 * @return true on successful initialization, false otherwise.
 */
bool uart_terminal_ui::initialize_socket() {
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        UART_ERROR("Error creating socket: " + std::string(strerror(errno)));
        return false;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt,
                   sizeof(opt)) < 0) {
        UART_ERROR("Error setting socket options: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }

    // Also set SO_REUSEPORT for better port reuse handling
    // This allows immediate port reuse even if previous process is in TIME_WAIT
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEPORT, (const char*)&opt,
                   sizeof(opt)) < 0) {
        UART_ERROR("Error setting SO_REUSEPORT: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }

    // Bind socket
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_socket, (struct sockaddr*)&server_addr,
             sizeof(server_addr)) == SOCKET_ERROR) {
        UART_ERROR("Error binding socket: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }

    // Listen for connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        UART_ERROR("Error listening on socket: " + std::string(strerror(errno)));
        close(server_socket);
        return false;
    }

    std::stringstream ss;
    ss << "Server listening on port " << server_port;
    UART_DEBUG(ss.str());

    return true;
}

/**
 * @brief Clean up server/client socket resources.
 *
 * Closes any open client socket and the server socket, resetting their
 * descriptors to INVALID_SOCKET.
 */
void uart_terminal_ui::cleanup_socket() {
    // Close client socket if connected
    if (client_socket != INVALID_SOCKET) {
        close(client_socket);
        client_socket = INVALID_SOCKET;
    }

    // Close server socket
    if (server_socket != INVALID_SOCKET) {
        close(server_socket);
        server_socket = INVALID_SOCKET;
    }
}

/**
 * @brief Send a single byte to the connected client.
 *
 * Writes one byte to the client socket if a client is connected.
 *
 * @param data Byte to send to the client.
 * @return true if the byte was sent, false if not connected or on error.
 */
bool uart_terminal_ui::send_data_to_client(uint8_t data) {
    if (!client_connected || client_socket == INVALID_SOCKET) {
        return false;
    }

    // Send data to client
    int result = send(client_socket, &data, 1, 0);

    if (result == SOCKET_ERROR) {
        UART_DEBUG("Error sending data to client: " + std::string(strerror(errno)));
        return false;
    }

    return true;
}

/**
 * @brief Process a byte received from the client.
 *
 * Called from the socket thread (external to SystemC) when data is received.
 * Uses the thread-safe channel which internally calls async_request_update()
 * to safely communicate with the SystemC kernel.
 *
 * @param data Byte received from the client.
 */
void uart_terminal_ui::process_client_data(uint8_t data) {
    UART_DEBUG("Inside process_client_data");
    
    // Write to thread-safe channel
    // This is safe to call from external threads
    rx_channel.write(data);
}

/**
 * @brief SystemC thread that processes RX data from the thread-safe channel.
 *
 * Waits for the data_available_event from the channel and processes all
 * queued data by forwarding it to the UART via the terminal interface.
 */
void uart_terminal_ui::rx_data_handler() {
    while (true) {
        // Wait for data to arrive in the channel
        wait(rx_channel.data_available_event());
        
        // Process all available data from the channel
        while (rx_channel.has_data()) {
            uint8_t data = rx_channel.read();
            
            // Forward to UART - safe to call SystemC functions here
            uart_port->terminal_to_uart(data);
            
            // Debug output
            std::stringstream ss;
            ss << "RX: 0x" << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<int>(data) << " ('"
               << (isprint(data) ? static_cast<char>(data) : '.') << "')";
            UART_DEBUG(ss.str());
        }
    }
}


/**
 * @brief Spawn the Python UART terminal client in an gnome-terminal.
 *
 * Forks the current process. In the child, launches `gnome-terminal` to run the Python
 * client script with the appropriate host/port. The parent logs the spawned PID.
 */
void uart_terminal_ui::spawn_client_process() {
    UART_DEBUG("Spawning Python client process");

    // Fork a new process
    client_pid = fork();

    if (client_pid == -1) {
        // Fork failed
        UART_DEBUG("Failed to fork process for client: " +
            std::string(strerror(errno)));
        return;
    }

    if (client_pid == 0) {
        // Child process
        UART_DEBUG("Child process: " << client_pid);
        // Construct the command to run the Python script in gnome-terminal
        std::string port_str = std::to_string(server_port);

        // Get the absolute path to the script by looking for it relative to this source file's location
        // The script is in the same directory as this file
        std::string script_path = std::string(__FILE__);
        size_t pos = script_path.find_last_of("/\\");
        if (pos != std::string::npos) {
            script_path = script_path.substr(0, pos + 1) + "uart_terminal_client.py";
        } else {
            script_path = "uart_terminal_client.py";  // fallback
        }

    // Split pane and run client
    execlp("tmux", "tmux",
            "split-window", "-h",
            "python3",
            script_path.c_str(),
            "--host", "localhost",
            "--port", port_str.c_str(),
            (char*)nullptr);

    // After spawning (parent side), normalize layout
    // Ignoring the return value
    const int tmux_layout_rc = std::system("tmux select-layout even-horizontal");
    (void) tmux_layout_rc;

#if 0
        execlp("gnome-terminal", "gnome-terminal", "--title=Connected to UART Terminal UI at localhost:8888",
        "--",  // Separator for gnome-terminal options
        "python3",
        script_path.c_str(),
        "--host", "localhost",
        "--port", port_str.c_str(),
        (char*)NULL);

        // If execlp returns, it failed
        UART_DEBUG("Failed to execute gnome-terminal: " << strerror(errno));
        exit(EXIT_FAILURE);
#endif
    } else {
        // Parent process
        UART_DEBUG("Parent process: " << client_pid);
    }
}

/**
 * @brief Close a specific client socket and mark client as not auto-spawned.
 *
 * Gracefully shuts down the provided client socket and closes the descriptor.
 * Also disables `auto_spawn_client` so the client won't be respawned
 * immediately.
 *
 * @param client_sock The client socket descriptor to close.
 */
void uart_terminal_ui::closesocket(SOCKET client_sock) {

    if (client_sock != INVALID_SOCKET) {

        auto_spawn_client = false;
        // Gracefully shut down the socket before closing
        shutdown(client_sock, SHUT_RDWR);

        // Close the socket descriptor
        close(client_sock);

        // Log cleanup
        std::stringstream ss;
        ss << "Closed client socket (fd=" << client_sock << ")";
        UART_DEBUG(ss.str());
    }
}
