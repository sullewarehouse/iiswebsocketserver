# WebSocketServer.PerformHandshake

**PerformHandshake(pHttpContext)**

Performs a WebSocket upgrade handshake with the connected client.

***pHttpContext***  
A **`IHttpContext*`** pointer passed to the IIS native module in the `OnBeginRequest` function.

**Return Value**  
**`S_OK`** on success, otherwise an error code.
