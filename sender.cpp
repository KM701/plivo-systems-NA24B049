#include <iostream>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

constexpr int PAYLOAD_SIZE = 160;
constexpr int HARNESS_PACKET_SIZE = 164;

int main() {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    // Force OS to release the port instantly
    int opt = 1;
    setsockopt(in_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in in_addr{};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr)) < 0) {
        std::cerr << "Sender bind failed on 47010\n";
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    sockaddr_in relay{};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t buf[HARNESS_PACKET_SIZE];
    uint8_t prev_payload[PAYLOAD_SIZE]{0};

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof(buf), 0, nullptr, nullptr);
        if (n < HARNESS_PACKET_SIZE) continue;

        sendto(out_fd, buf, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&relay, sizeof(relay));

        uint32_t raw_seq;
        std::memcpy(&raw_seq, buf, 4);
        uint32_t seq = ntohl(raw_seq);

        if (seq % 2 == 1) {
            uint8_t fec_packet[HARNESS_PACKET_SIZE];
            uint32_t fec_seq = htonl(seq | 0x80000000);
            std::memcpy(fec_packet, &fec_seq, 4);
            
            for (int i = 0; i < PAYLOAD_SIZE; i++) {
                fec_packet[4 + i] = prev_payload[i] ^ buf[4 + i];
            }
            
            sendto(out_fd, fec_packet, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&relay, sizeof(relay));
        }

        std::memcpy(prev_payload, buf + 4, PAYLOAD_SIZE);
    }
    return 0;
}