#ifndef _WEBSOCKET_PROTOCOL_H_
#define _WEBSOCKET_PROTOCOL_H_


#include "protocol.h"

#include <web_socket.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#define WEBSOCKET_PROTOCOL_SERVER_HELLO_EVENT (1 << 0)

class WebsocketProtocol : public Protocol {
public:
    WebsocketProtocol();
    ~WebsocketProtocol();

    bool Start() override;
    bool SendAudio(std::unique_ptr<AudioStreamPacket> packet) override;
    bool OpenAudioChannel() override;
    void CloseAudioChannel(bool send_goodbye = true) override;
    bool IsAudioChannelOpened() const override;

private:
    class TransportTracker;

    EventGroupHandle_t event_group_handle_;
    std::unique_ptr<TransportTracker> transports_;
    std::unique_ptr<WebSocket> websocket_;
    int version_ = 1;

    void ParseServerHello(const cJSON* root);
    bool Connect(const std::string& url, const std::string& token);
    bool CreateWebsocket(const std::string& token);
    void DiscardFailedWebsocket();
    bool SendText(const std::string& text) override;
    std::string GetHelloMessage();
};

#endif
