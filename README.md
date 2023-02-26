# iiswebsocketserver
Windows IIS native WebSocket server module.

You do not need to compile anything to use this WebSocket server in your IIS module, just include **`iiswebsocket.cpp`** and **`iiswebsocket.h`** in your IIS module.
Everything is contained in the **`IISWebSocketServer`** namespace.

Check **`main.cpp`** for a detailed example that echos messages back to the client.

## Functions

- [PrintLastError](docs/PrintLastError.md)

## WebSocketServer Class

**IISWebSocketServer::WebSocketServer**

Members:
- Functions
  - [Initialize](docs/WebSocketServer/Initialize.md)
  - [Initialize](docs/WebSocketServer/PerformHandshake.md)
  - [Receive](docs/WebSocketServer/Receive.md)
  - [Send](docs/WebSocketServer/Send.md)
  - [Close](docs/WebSocketServer/IsConnected.md)
  - [Free](docs/WebSocketServer/Free.md)
- Variables
  - [WebSocketFrame](docs/WebSocketServer/WebSocketFrame.md)
  - [MaxPayloadLength](docs/WebSocketServer/MaxPayloadLength.md)
  - [Stream](docs/WebSocketServer/Stream.md)
  - [ErrorCode](docs/WebSocketServer/ErrorCode.md)
  - [ErrorDescription](docs/WebSocketServer/ErrorDescription.md)
  - [ErrorBufferLength](docs/WebSocketServer/ErrorBufferLength.md)

## WebSocket resources

- [WebSocket client using WinHTTP functions](https://github.com/sullewarehouse/WinHttpWebSocketClient)
