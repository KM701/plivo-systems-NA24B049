/* BASELINE RECEIVER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender, via the relay (optional)
 *
 * This baseline forwards whatever arrives straight to the player: lost
 * frames stay lost, late frames stay late, duplicates are re-sent
 * harmlessly. All yours to fix — jitter buffer, reordering, recovery.
 *
 * Env vars available: T0, DURATION_S, DELAY_MS. Harness kills the process
 * at run end; a forever-loop is fine.
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
//     in_addr.sin_port = htons(47002);
//     in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//     if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
//         perror("bind 47002");
//         return 1;
//     }

//     int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     struct sockaddr_in player = {0};
//     player.sin_family = AF_INET;
//     player.sin_port = htons(47020);
//     player.sin_addr.s_addr = inet_addr("127.0.0.1");

//     unsigned char buf[2048];
//     for (;;) {
//         ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
//         if (n <= 0) continue;
//         /* jitter buffer / reorder / recovery logic goes here */
//         sendto(out_fd, buf, (size_t)n, 0, (struct sockaddr *)&player,
//                sizeof player);
//     }
//     return 0;
// }


/* BASELINE RECEIVER (C) — naive on purpose. Rewrite it (C, C++, Go, or Rust).
 *
 * Ports (all 127.0.0.1):
 *   bind 47002  <- media from your sender, via the hostile relay
 *   send 47020  -> harness player. MUST be: 4-byte big-endian seq +
 *                  160-byte payload. Frame i counts only if it arrives
 *                  BEFORE its deadline t0 + DELAY_MS + i*20ms.
 *   send 47003  -> feedback to your sender, via the relay (optional)
 *
 * This baseline forwards whatever arrives straight to the player: lost
 * frames stay lost, late frames stay late, duplicates are re-sent
 * harmlessly. All yours to fix — jitter buffer, reordering, recovery.
 *
 * Env vars available: T0, DURATION_S, DELAY_MS. Harness kills the process
 * at run end; a forever-loop is fine.
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>

#define PAYLOAD_SIZE 160
#define HARNESS_PACKET_SIZE 164
#define MAX_FRAMES 10000 

typedef struct {
    unsigned char data[PAYLOAD_SIZE];
    int valid;
    unsigned char fec_data[PAYLOAD_SIZE];
    int fec_valid;
} Frame;

Frame jitter_buffer[MAX_FRAMES];
pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

int out_fd;
struct sockaddr_in player;
long long t0_clock = 0;
int delay_ms = 40; 

long long current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void* playback_thread_func(void* arg) {
    uint32_t current_seq = 0;
    
    while (1) {
        long long now = current_time_ms();
        long long deadline;
        
        pthread_mutex_lock(&buffer_mutex);
        deadline = t0_clock + delay_ms + (current_seq * 20);
        pthread_mutex_unlock(&buffer_mutex);

        // Wake up 2ms BEFORE the absolute deadline to beat OS scheduling lag
        long long sleep_time = deadline - now - 2; 
        if (sleep_time > 0) {
            usleep(sleep_time * 1000); 
        }

        unsigned char outbound[HARNESS_PACKET_SIZE];
        uint32_t network_seq = htonl(current_seq);
        memcpy(outbound, &network_seq, 4);

        pthread_mutex_lock(&buffer_mutex);
        if (current_seq < MAX_FRAMES && jitter_buffer[current_seq].valid) {
            memcpy(outbound + 4, jitter_buffer[current_seq].data, PAYLOAD_SIZE);
        } else {
            memset(outbound + 4, 0, PAYLOAD_SIZE); 
        }
        pthread_mutex_unlock(&buffer_mutex);

        sendto(out_fd, outbound, HARNESS_PACKET_SIZE, 0, (struct sockaddr *)&player, sizeof(player));
        current_seq++;
    }
    return NULL;
}

int main(void) {
    char* t0_env = getenv("T0");
    if (t0_env) {
        t0_clock = (long long)(atof(t0_env) * 1000.0);
    } else {
        t0_clock = current_time_ms(); 
    }
    
    char* delay_env = getenv("DELAY_MS");
    if (delay_env) delay_ms = atoi(delay_env);

    memset(jitter_buffer, 0, sizeof(jitter_buffer));

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr);

    out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&player, 0, sizeof(player));
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    pthread_t playout_worker;
    pthread_create(&playout_worker, NULL, playback_thread_func, NULL);

    unsigned char buf[HARNESS_PACKET_SIZE];
    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n < HARNESS_PACKET_SIZE) continue;
        
        uint32_t raw_seq;
        memcpy(&raw_seq, buf, 4);
        uint32_t seq_in = ntohl(raw_seq);

        pthread_mutex_lock(&buffer_mutex);
        
        if (seq_in & 0x80000000) {
            uint32_t base_seq = seq_in & 0x7FFFFFFF;
            if (base_seq < MAX_FRAMES) {
                memcpy(jitter_buffer[base_seq].fec_data, buf + 4, PAYLOAD_SIZE);
                jitter_buffer[base_seq].fec_valid = 1;
            }
        } else {
            if (seq_in < MAX_FRAMES) {
                memcpy(jitter_buffer[seq_in].data, buf + 4, PAYLOAD_SIZE);
                jitter_buffer[seq_in].valid = 1;
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
                jitter_buffer[even_seq].valid = 1;
            } else if (jitter_buffer[even_seq].valid && !jitter_buffer[odd_seq].valid) { 
                for (int i = 0; i < PAYLOAD_SIZE; i++) {
                    jitter_buffer[odd_seq].data[i] = jitter_buffer[even_seq].data[i] ^ jitter_buffer[odd_seq].fec_data[i];
                }
                jitter_buffer[odd_seq].valid = 1;
            }
        }
        
        pthread_mutex_unlock(&buffer_mutex);
    }
    return 0;
}