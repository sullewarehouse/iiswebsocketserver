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
  - [PerformHandshake](docs/WebSocketServer/PerformHandshake.md)
  - [Receive](docs/WebSocketServer/Receive.md)
  - [Send](docs/WebSocketServer/Send.md)
  - [IsConnected](docs/WebSocketServer/IsConnected.md)
  - [Free](docs/WebSocketServer/Free.md)
- Variables
  - [WebSocketFrame](docs/WebSocketServer/WebSocketFrame.md)
  - [MaxPayloadLength](docs/WebSocketServer/MaxPayloadLength.md)
  - [Stream](docs/WebSocketServer/Stream.md)
  - [ErrorCode](docs/WebSocketServer/ErrorCode.md)
  - [ErrorDescription](docs/WebSocketServer/ErrorDescription.md)
  - [ErrorBufferLength](docs/WebSocketServer/ErrorBufferLength.md)

## Installing an IIS native module

1. Add your module to IIS
   - Click on the name of your server (not a site) in the left Connections panel
   - Goto **Modules**, in the right **Actions** panel, select **Configure Native Modules...**
   - In the pop-up window click **Register...** (this button will not appear if you clicked on a site)
   - Enter the name you want associated with the module
   - Enter/Select the path of your module (.dll) file, click OK
   - Your module will now be in the list of modules to enable
   - Ensure your module is selected and click OK
2. Setup the Application Pool
   - Goto **Application Pools** in the left Connections panel
   - Click on **Add Application Pool...** in the right Actions panel
   - Enter a name you want associated with the module
   - Under **.NET CLR version** select **No Managed Code**
   - Under **Managed pipeline mode** select **Classic**
   - Click OK, your application will now appear in the Application Pools list.
   - Select your application and click on **Advanced Settings...** in the right Actions panel
   - If Your module is 32-Bit then select **true** under **Enable 32-Bit Applications**
   - To give your module admin rights, select **LocalSystem** under **Identity**
   - Click OK when you are done setting **Advanced Settings**
3. Add the Application to your site
   - Right click on the site you wish to add the module to, select **Add Application...**
   - Under **Alias** enter the path you want to use for the URL
   - Select the Application Pool created for the module
   - Select the folder your module is in
   - Click OK

## WebSocket resources

- [WebSocket client using WinHTTP functions](https://github.com/sullewarehouse/WinHttpWebSocketClient)
