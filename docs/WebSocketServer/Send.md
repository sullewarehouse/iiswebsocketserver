# WebSocketServer.Send

**Send(bufferType, pBuffer, dwLength)**

Sends data to the WebSocket client. This function blocks until data is sent.

***bufferType***  
The type of data being sent. This can be 1 of the following:
- **`IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_PING_BUFFER_TYPE`**
- **`IIS_WEB_SOCKET_PONG_BUFFER_TYPE`**

***pBuffer***  
Pointer to the data to send.

***dwLength***  
The number of bytes to send.

**Return Value**  
**`S_OK`** on success, otherwise an error code.
