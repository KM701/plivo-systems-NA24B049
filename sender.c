/* BASELINE SENDER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver (YOUR wire format)
 *   bind 47004  <- feedback from your receiver, via the relay (optional)
 *
 * This baseline forwards each frame once, unchanged, and ignores feedback.
 * No redundancy, no retransmission. It cannot pass. That is the point.
 *
 * Env vars available if you want them: T0 (epoch seconds, float),
 * DURATION_S, DELAY_MS. The harness kills this process when the run ends,
 * so a forever-loop is fine.
 *
 * build: make        run: python3 run.py --delay_ms 60
 */
// #include <arpa/inet.h>
// #include <stdio.h>
// #include <string.h>
// #include <sys/socket.h>
// #include <unistd.h>

// int main(void) {
//     int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     struct sockaddr_in in_addr = {0};
//     in_addr.sin_family = AF_INET;
//     in_addr.sin_port = htons(47010);
//     in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//     if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
//         perror("bind 47010");
//         return 1;
//     }

//     int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     struct sockaddr_in relay = {0};
//     relay.sin_family = AF_INET;
//     relay.sin_port = htons(47001);
//     relay.sin_addr.s_addr = inet_addr("127.0.0.1");

//     unsigned char buf[2048];
//     for (;;) {
//         ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
//         if (n <= 0) continue;
//         /* your protocol design goes here; baseline = send once, as-is */
//         sendto(out_fd, buf, (size_t)n, 0, (struct sockaddr *)&relay,
//                sizeof relay);
//     }
//     return 0;
// }



/* BASELINE SENDER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47010  <- harness source delivers frame i here at t0 + i*20ms
 *                  (format: 4-byte big-endian seq + 160-byte payload)
 *   send 47001  -> relay uplink toward the receiver (YOUR wire format)
 *   bind 47004  <- feedback from your receiver, via the relay (optional)
 *
 * This baseline forwards each frame once, unchanged, and ignores feedback.
 * No redundancy, no retransmission. It cannot pass. That is the point.
 *
 * Env vars available if you want them: T0 (epoch seconds, float),
 * DURATION_S, DELAY_MS. The harness kills this process when the run ends,
 * so a forever-loop is fine.
 *
 * build: make        run: python3 run.py --delay_ms 60
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PAYLOAD_SIZE 160
#define HARNESS_PACKET_SIZE 164

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) return 1;

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[HARNESS_PACKET_SIZE];
    unsigned char prev_payload[PAYLOAD_SIZE];
    memset(prev_payload, 0, PAYLOAD_SIZE);

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < HARNESS_PACKET_SIZE) continue;

        // 1. Always send the original incoming packet straight through
        sendto(out_fd, buf, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&relay, sizeof relay);

        uint32_t raw_seq;
        memcpy(&raw_seq, buf, 4);
        uint32_t seq = ntohl(raw_seq);

        // 2. For every pair (even/odd), generate an XOR FEC packet
        if (seq % 2 == 1) {
            unsigned char fec_packet[HARNESS_PACKET_SIZE];
            
            // Mark this as an FEC packet by flipping the highest bit of the sequence number
            uint32_t fec_seq = htonl(seq | 0x80000000);
            memcpy(fec_packet, &fec_seq, 4);
            
            // XOR the current payload with the previous payload
            for (int i = 0; i < PAYLOAD_SIZE; i++) {
                fec_packet[4 + i] = prev_payload[i] ^ buf[4 + i];
            }
            
            sendto(out_fd, fec_packet, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&relay, sizeof relay);
        }

        // Cache this payload for the next cycle
        memcpy(prev_payload, buf + 4, PAYLOAD_SIZE);
    }
    return 0;
}