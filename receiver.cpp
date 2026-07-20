// #include <iostream>
// #include <cstring>
// #include <cstdlib>
// #include <string>
// #include <arpa/inet.h>
// #include <sys/socket.h>
// #include <unistd.h>
// #include <thread>
// #include <mutex>
// #include <chrono>
// #include <vector>

// constexpr int PAYLOAD_SIZE = 160;
// constexpr int HARNESS_PACKET_SIZE = 164;
// constexpr int MAX_FRAMES = 10000;

// struct Frame {
//     uint8_t data[PAYLOAD_SIZE]{0};
//     bool valid = false;
//     uint8_t fec_data[PAYLOAD_SIZE]{0};
//     bool fec_valid = false;
// };

// std::vector<Frame> jitter_buffer(MAX_FRAMES);
// std::mutex buffer_mutex;

// int out_fd;
// sockaddr_in player{};
// long long t0_clock = 0;
// int delay_ms = 40; 

// long long current_time_ms() {
//     auto now = std::chrono::system_clock::now();
//     return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
// }

// void playback_thread_func() {
//     uint32_t current_seq = 0;
    
//     while (true) {
//         long long now = current_time_ms();
//         long long deadline;
        
//         {
//             std::lock_guard<std::mutex> lock(buffer_mutex);
//             deadline = t0_clock + delay_ms + (current_seq * 20);
//         }

//         long long sleep_time = deadline - now - 2; 
//         if (sleep_time > 0) {
//             std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
//         }

//         uint8_t outbound[HARNESS_PACKET_SIZE];
//         uint32_t network_seq = htonl(current_seq);
//         std::memcpy(outbound, &network_seq, 4);

//         {
//             std::lock_guard<std::mutex> lock(buffer_mutex);
//             if (current_seq < MAX_FRAMES && jitter_buffer[current_seq].valid) {
//                 std::memcpy(outbound + 4, jitter_buffer[current_seq].data, PAYLOAD_SIZE);
//             } else {
//                 std::memset(outbound + 4, 0, PAYLOAD_SIZE); 
//             }
//         }

//         sendto(out_fd, outbound, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&player, sizeof(player));
//         current_seq++;
//     }
// }

// int main() {
//     if (const char* t0_env = std::getenv("T0")) {
//         t0_clock = static_cast<long long>(std::stod(t0_env) * 1000.0);
//     } else {
//         t0_clock = current_time_ms(); 
//     }
    
//     if (const char* delay_env = std::getenv("DELAY_MS")) {
//         delay_ms = std::stoi(delay_env);
//     }

//     int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
//     // Force OS to release the port instantly
//     int opt = 1;
//     setsockopt(in_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

//     sockaddr_in in_addr{};
//     in_addr.sin_family = AF_INET;
//     in_addr.sin_port = htons(47002);
//     in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//     bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

//     out_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     player.sin_family = AF_INET;
//     player.sin_port = htons(47020);
//     player.sin_addr.s_addr = inet_addr("127.0.0.1");

//     std::thread playout_worker(playback_thread_func);
//     playout_worker.detach();

//     uint8_t buf[HARNESS_PACKET_SIZE];
//     for (;;) {
//         ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, nullptr, nullptr);
//         if (n < HARNESS_PACKET_SIZE) continue;
        
//         uint32_t raw_seq;
//         std::memcpy(&raw_seq, buf, 4);
//         uint32_t seq_in = ntohl(raw_seq);

//         std::lock_guard<std::mutex> lock(buffer_mutex);
        
//         if (seq_in & 0x80000000) {
//             uint32_t base_seq = seq_in & 0x7FFFFFFF;
//             if (base_seq < MAX_FRAMES) {
//                 std::memcpy(jitter_buffer[base_seq].fec_data, buf + 4, PAYLOAD_SIZE);
//                 jitter_buffer[base_seq].fec_valid = true;
//             }
//         } else {
//             if (seq_in < MAX_FRAMES) {
//                 std::memcpy(jitter_buffer[seq_in].data, buf + 4, PAYLOAD_SIZE);
//                 jitter_buffer[seq_in].valid = true;
//             }
//         }

//         uint32_t check_seq = (seq_in & 0x80000000) ? (seq_in & 0x7FFFFFFF) : seq_in;
//         uint32_t odd_seq = (check_seq % 2 == 1) ? check_seq : (check_seq + 1);
//         uint32_t even_seq = odd_seq - 1;

//         if (odd_seq < MAX_FRAMES && jitter_buffer[odd_seq].fec_valid) {
//             if (jitter_buffer[odd_seq].valid && !jitter_buffer[even_seq].valid) { 
//                 for (int i = 0; i < PAYLOAD_SIZE; i++) {
//                     jitter_buffer[even_seq].data[i] = jitter_buffer[odd_seq].data[i] ^ jitter_buffer[odd_seq].fec_data[i];
//                 }
//                 jitter_buffer[even_seq].valid = true;
//             } else if (jitter_buffer[even_seq].valid && !jitter_buffer[odd_seq].valid) { 
//                 for (int i = 0; i < PAYLOAD_SIZE; i++) {
//                     jitter_buffer[odd_seq].data[i] = jitter_buffer[even_seq].data[i] ^ jitter_buffer[odd_seq].fec_data[i];
//                 }
//                 jitter_buffer[odd_seq].valid = true;
//             }
//         }
//     }
//     return 0;
// }



#include <iostream>
#include <cstring>
#include <cstdlib>
#include <string>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>

constexpr int PAYLOAD_SIZE = 160;
constexpr int HARNESS_PACKET_SIZE = 164;
constexpr int MAX_FRAMES = 10000;

struct Frame {
    uint8_t data[PAYLOAD_SIZE]{0};
    bool valid = false;
    uint8_t fec_data[PAYLOAD_SIZE]{0};
    bool fec_valid = false;
};

std::vector<Frame> jitter_buffer(MAX_FRAMES);
std::mutex buffer_mutex;

int out_fd;
sockaddr_in player{};
long long t0_clock_ms = 0;
int delay_ms = 40; 

long long current_time_ms() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
}

void playback_thread_func() {
    uint32_t current_seq = 0;
    
    while (true) {
        long long target_time_ms;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            // Calculate exact epoch millisecond deadline
            target_time_ms = t0_clock_ms + delay_ms + (current_seq * 20);
        }

        // OPTIMIZATION 1: Absolute Time Sleeping
        // Converts the target ms into a native C++ time_point to eliminate loop drift
        auto deadline_tp = std::chrono::time_point<std::chrono::system_clock>(std::chrono::milliseconds(target_time_ms));
        
        // Sleep until exactly 1ms before the absolute deadline
        std::this_thread::sleep_until(deadline_tp - std::chrono::milliseconds(1));

        uint8_t outbound[HARNESS_PACKET_SIZE];
        uint32_t network_seq = htonl(current_seq);
        std::memcpy(outbound, &network_seq, 4);

        {
            std::lock_guard<std::mutex> lock(buffer_mutex);
            if (current_seq < MAX_FRAMES && jitter_buffer[current_seq].valid) {
                std::memcpy(outbound + 4, jitter_buffer[current_seq].data, PAYLOAD_SIZE);
            } else {
                std::memset(outbound + 4, 0, PAYLOAD_SIZE); 
            }
        }

        sendto(out_fd, outbound, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&player, sizeof(player));
        current_seq++;
    }
}

int main() {
    if (const char* t0_env = std::getenv("T0")) {
        t0_clock_ms = static_cast<long long>(std::stod(t0_env) * 1000.0);
    } else {
        t0_clock_ms = current_time_ms(); 
    }
    
    if (const char* delay_env = std::getenv("DELAY_MS")) {
        delay_ms = std::stoi(delay_env);
    }

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    int opt = 1;
    setsockopt(in_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // OPTIMIZATION 2: Maximize UDP Receive Buffer
    // Prevents kernel-level packet drops during network bursts
    int rcv_buf_size = 1024 * 1024; // 1 MB buffer
    setsockopt(in_fd, SOL_SOCKET, SO_RCVBUF, &rcv_buf_size, sizeof(rcv_buf_size));

    sockaddr_in in_addr{};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

    out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::thread playout_worker(playback_thread_func);
    playout_worker.detach();

    uint8_t buf[HARNESS_PACKET_SIZE];
    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < HARNESS_PACKET_SIZE) continue;
        
        uint32_t raw_seq;
        std::memcpy(&raw_seq, buf, 4);
        uint32_t seq_in = ntohl(raw_seq);

        std::lock_guard<std::mutex> lock(buffer_mutex);
        
        if (seq_in & 0x80000000) {
            uint32_t base_seq = seq_in & 0x7FFFFFFF;
            if (base_seq < MAX_FRAMES) {
                std::memcpy(jitter_buffer[base_seq].fec_data, buf + 4, PAYLOAD_SIZE);
                jitter_buffer[base_seq].fec_valid = true;
            }
        } else {
            if (seq_in < MAX_FRAMES) {
                std::memcpy(jitter_buffer[seq_in].data, buf + 4, PAYLOAD_SIZE);
                jitter_buffer[seq_in].valid = true;
            }
        }

        uint32_t check_seq = (seq_in & 0x80000000) ? (seq_in & 0x7FFFFFFF) : seq_in;
        uint32_t odd_seq = (check_seq % 2 == 1) ? check_seq : (check_seq + 1);
        uint32_t even_seq = odd_seq - 1;

        if (odd_seq < MAX_FRAMES && jitter_buffer[odd_seq].fec_valid) {
            if (jitter_buffer[odd_seq].valid && !jitter_buffer[even_seq].valid) { 
                for (int i = 0; i < PAYLOAD_SIZE; i++) {
                    jitter_buffer[even_seq].data[i] = jitter_buffer[odd_seq].data[i] ^ jitter_buffer[odd_seq].fec_data[i];
                }
                jitter_buffer[even_seq].valid = true;
            } else if (jitter_buffer[even_seq].valid && !jitter_buffer[odd_seq].valid) { 
                for (int i = 0; i < PAYLOAD_SIZE; i++) {
                    jitter_buffer[odd_seq].data[i] = jitter_buffer[even_seq].data[i] ^ jitter_buffer[odd_seq].fec_data[i];
                }
                jitter_buffer[odd_seq].valid = true;
            }
        }
    }
    return 0;
}