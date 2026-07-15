#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <map>
#include <poll.h>
#include <sys/time.h>

struct HarnessPacket {
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

struct FecCache {
    uint8_t payload[160];
};

long long current_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec) * 1000LL + (tv.tv_usec) / 1000LL;
}

int main() {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    std::map<uint32_t, HarnessPacket> buffer;
    std::map<uint32_t, FecCache> fec_buffer; 
    
    uint32_t next_playout_seq = 0;
    bool first_frame = true;
    
    double t0 = 0.0;
    int delay_ms = 50; 
    
    const char* t0_env = getenv("T0");
    if (t0_env) t0 = std::atof(t0_env);
    const char* delay_env = getenv("DELAY_MS");
    if (delay_env) delay_ms = std::atoi(delay_env);

    long long t0_ms = (long long)(t0 * 1000.0);
    if (t0_ms == 0) t0_ms = current_time_ms(); 

    struct pollfd pfd = {in_fd, POLLIN, 0};

    while (true) {
        int ret = poll(&pfd, 1, 2); 
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            uint8_t in_buf[165];
            ssize_t n = recvfrom(in_fd, in_buf, sizeof(in_buf), 0, nullptr, nullptr);
            
            if (n == 165) {
                uint8_t type = in_buf[0];
                uint32_t seq;
                std::memcpy(&seq, in_buf + 1, 4);
                seq = ntohl(seq);
                
                if (first_frame) {
                    next_playout_seq = seq;
                    first_frame = false;
                }

                if (type == 0) { 
                    HarnessPacket hp;
                    hp.seq = htonl(seq);
                    std::memcpy(hp.payload, in_buf + 5, 160);
                    buffer[seq] = hp;
                } else if (type == 1) { 
                    FecCache fp;
                    std::memcpy(fp.payload, in_buf + 5, 160);
                    fec_buffer[seq] = fp;
                }
                
                // XOR Recovery Logic
                uint32_t base = (seq % 2 == 0) ? seq : seq - 1;
                if (fec_buffer.find(base) != fec_buffer.end()) {
                    bool has_even = (buffer.find(base) != buffer.end());
                    bool has_odd = (buffer.find(base + 1) != buffer.end());
                    
                    if (has_even && !has_odd) {
                        HarnessPacket rec;
                        rec.seq = htonl(base + 1);
                        for(int i=0; i<160; i++) {
                            rec.payload[i] = buffer[base].payload[i] ^ fec_buffer[base].payload[i];
                        }
                        buffer[base + 1] = rec;
                    } else if (!has_even && has_odd) {
                        HarnessPacket rec;
                        rec.seq = htonl(base);
                        for(int i=0; i<160; i++) {
                            rec.payload[i] = buffer[base + 1].payload[i] ^ fec_buffer[base].payload[i];
                        }
                        buffer[base] = rec;
                    }
                }
            }
        }

        if (!first_frame) {
            // FIX 1: Dispatch immediately to easily beat the deadline
            while (buffer.find(next_playout_seq) != buffer.end()) {
                sendto(out_fd, &buffer[next_playout_seq], 164, 0, (struct sockaddr *)&player, sizeof(player));
                // FIX 2: Do NOT erase here. XOR recovery requires historical payloads!
                next_playout_seq++;
            }

            // If we are stuck, wait until the deadline expires before giving up
            long long now = current_time_ms();
            while (now >= t0_ms + delay_ms + (next_playout_seq * 20)) {
                next_playout_seq++; // Give up on the dropped packet
                
                // Flush again if skipping unblocked us
                while (buffer.find(next_playout_seq) != buffer.end()) {
                    sendto(out_fd, &buffer[next_playout_seq], 164, 0, (struct sockaddr *)&player, sizeof(player));
                    next_playout_seq++;
                }
            }

            // Safely clean up old packets to prevent memory leaks
            if (next_playout_seq > 20) {
                uint32_t old_seq = next_playout_seq - 20;
                buffer.erase(old_seq);
                fec_buffer.erase(old_seq);
            }
        }
    }
    return 0;
}