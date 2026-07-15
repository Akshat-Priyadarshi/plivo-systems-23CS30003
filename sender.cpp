#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

struct HarnessPacket {
    uint32_t seq;
    uint8_t payload[160];
} __attribute__((packed));

int main() {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    bind(in_fd, (struct sockaddr *)&in_addr, sizeof(in_addr));

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    uint8_t prev_payload[160] = {0};
    uint32_t prev_seq = 0;
    bool has_prev = false;

    while (true) {
        HarnessPacket in_pkt;
        ssize_t n = recvfrom(in_fd, &in_pkt, sizeof(in_pkt), 0, nullptr, nullptr);
        if (n != sizeof(HarnessPacket)) continue;

        uint32_t seq = ntohl(in_pkt.seq);

        // 1. Send Data Packet (Type 0) - 165 bytes
        uint8_t out_buf[165];
        out_buf[0] = 0; 
        std::memcpy(out_buf + 1, &in_pkt.seq, 4);
        std::memcpy(out_buf + 5, in_pkt.payload, 160);
        sendto(out_fd, out_buf, 165, 0, (struct sockaddr *)&relay, sizeof(relay));

        // 2. Send FEC Parity Packet (Type 1) - Every 2nd frame
        if (has_prev && (seq == prev_seq + 1) && (seq % 2 == 1)) {
            uint8_t fec_buf[165];
            fec_buf[0] = 1; 
            uint32_t base_seq_net = htonl(prev_seq);
            std::memcpy(fec_buf + 1, &base_seq_net, 4);
            
            for(int i = 0; i < 160; i++) {
                fec_buf[5 + i] = prev_payload[i] ^ in_pkt.payload[i];
            }
            sendto(out_fd, fec_buf, 165, 0, (struct sockaddr *)&relay, sizeof(relay));
            has_prev = false;
        } else {
            std::memcpy(prev_payload, in_pkt.payload, 160);
            prev_seq = seq;
            has_prev = true;
        }
    }
    return 0;
}