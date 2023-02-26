# WebSocketServer.Receive

**Receive(pBuffer, dwBufferLength, pdwBytesReceived, pBufferType)**

Receives data from the WebSocket client. This function blocks until data is received.

***pBuffer***  
A pointer to the destination buffer.

***dwBufferLength***  
The length of the destination buffer in bytes.

***pdwBytesReceived***  
A pointer to a variable to receive the number of bytes read.

***pBufferType***  
Pointer to a varible of type **`IIS_WEB_SOCKET_BUFFER_TYPE`** to receive the type of data received.

**Return Value**  
**`S_OK`** on success, otherwise an error code.
