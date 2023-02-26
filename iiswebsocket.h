
//
// iiswebsocket.h
// 
// Author:
//     Brian Sullender
//     SULLE WAREHOUSE LLC
// 
// Description:
//     The primary header file for the IIS WebSocket server interface.
//

#ifndef IIS_WEB_SOCKET_SERVER_H
#define IIS_WEB_SOCKET_SERVER_H

#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#include <windows.h>
#include <httpserv.h>
#include <sstream>

// Include header required for generating handshake HTTP headers
#include <websocket.h>
// Add library dependency for <websocket.h> functions
#pragma comment(lib, "Websocket.lib")

// WebSocket server namespace
namespace IISWebSocketServer
{
	// Print a Windows Error Code
	void PrintLastError(DWORD errorCode, CHAR* des, size_t desLen, CHAR* action, bool append = false);

	// Parsed WebSocket frame
	struct WEB_SOCKET_FRAME
	{
		// 0x00 = Continuation frame
		// 0x01 = Text frame
		// 0x02 = Binary frame
		// 0x08 = Connection close
		// 0x09 = Ping
		// 0x0A = Pong
		int Opcode;
		bool FIN;
		unsigned long long PayloadLength;
		bool bMask;
		char MaskingKey[4];
		unsigned int FrameSize;
	};

	// WebSocket buffer type
	typedef enum class _IIS_WEB_SOCKET_BUFFER_TYPE {
		IIS_WEB_SOCKET_BINARY_MESSAGE_BUFFER_TYPE = 0,
		IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE = 1,
		IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE = 2,
		IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE = 3,
		IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE = 4,
		IIS_WEB_SOCKET_PING_BUFFER_TYPE = 5,
		IIS_WEB_SOCKET_PONG_BUFFER_TYPE = 6
	} IIS_WEB_SOCKET_BUFFER_TYPE;

	// WebSocket close status
	typedef enum class _IIS_WEB_SOCKET_CLOSE_STATUS
	{
		IIS_WEB_SOCKET_SUCCESS_CLOSE_STATUS = 1000,
		IIS_WEB_SOCKET_ENDPOINT_TERMINATED_CLOSE_STATUS = 1001,
		IIS_WEB_SOCKET_PROTOCOL_ERROR_CLOSE_STATUS = 1002,
		IIS_WEB_SOCKET_INVALID_DATA_TYPE_CLOSE_STATUS = 1003,
		IIS_WEB_SOCKET_EMPTY_CLOSE_STATUS = 1005,
		IIS_WEB_SOCKET_ABORTED_CLOSE_STATUS = 1006,
		IIS_WEB_SOCKET_INVALID_PAYLOAD_CLOSE_STATUS = 1007,
		IIS_WEB_SOCKET_POLICY_VIOLATION_CLOSE_STATUS = 1008,
		IIS_WEB_SOCKET_MESSAGE_TOO_BIG_CLOSE_STATUS = 1009,
		IIS_WEB_SOCKET_UNSUPPORTED_EXTENSIONS_CLOSE_STATUS = 1010,
		IIS_WEB_SOCKET_SERVER_ERROR_CLOSE_STATUS = 1011,
		IIS_WEB_SOCKET_SECURE_HANDSHAKE_ERROR_CLOSE_STATUS = 1015
	} IIS_WEB_SOCKET_CLOSE_STATUS;

	// WebSocket close data
	__declspec(align(8)) struct IIS_WEB_SOCKET_CLOSE_DATA
	{
		USHORT status;
		CHAR reason[123];
		IIS_WEB_SOCKET_CLOSE_DATA(IIS_WEB_SOCKET_CLOSE_STATUS server_status, CHAR* server_reason)
		{
			status = ((USHORT)((UCHAR*)&server_status)[0] << 8) | (USHORT)((UCHAR*)&server_status)[1];
			strcpy_s(reason, 123, server_reason);
		}
		size_t length() {
			return strlen(reason) + 2;
		}
	};

	// WebSocket receiving stream
	struct WEB_SOCKET_STREAM
	{
		// Set to true when we are queuing a new frame
		BOOL bQueuing;
		// The frame buffer
		CHAR* pFrameBuffer;
		// The total bytes received in the frame buffer
		DWORD dwReceivedSize;
		// Remaining payload to receive
		unsigned long long qwPayloadRemaining;
		// Index of the current payload byte to unmask
		unsigned long long mkI;
	};

	// WebSocket server class
	class WebSocketServer
	{
	private:
		// IIS class pointers
		IHttpContext* pHttpContext;
		IHttpResponse* pHttpResponse;
		IHttpRequest* pHttpRequest;
		IHttpConnection* pHttpConnection;
		// Headers received and sent for the connection
		WEB_SOCKET_HTTP_HEADER* pRequestHeaders;
		ULONG RequestHeadersCount;
		// This is set according to the Send function
		BOOL IsFragment;
	public:
		// The parsed recieved WebSocket frame
		WEB_SOCKET_FRAME WebSocketFrame;
		// The maximum a payload can be in a frame
		unsigned long long MaxPayloadLength;
		// The receiving WebSocket stream
		WEB_SOCKET_STREAM Stream;
		// Error of the called function
		DWORD ErrorCode;
		// The description of the error code
		CHAR* ErrorDescription;
		// The number of characters the ErrorDescription buffer can hold
		size_t ErrorBufferLength;
		// Initialize the WebSocket server class
		DWORD Initialize();
		// Perform a WebSocket handshake with a client
		HRESULT PerformHandshake(IHttpContext* pHttpContext);
		// Receive data from the WebSocket client
		DWORD Receive(void* pBuffer, DWORD dwBufferLength, DWORD* pdwBytesReceived, IIS_WEB_SOCKET_BUFFER_TYPE* pBufferType);
		// Send data to the WebSocket client
		DWORD Send(IIS_WEB_SOCKET_BUFFER_TYPE bufferType, void* pBuffer, DWORD dwLength);
		// Determines whether a WebSocket client is still connected
		BOOL IsConnected();
		// Free resources
		VOID Free();
	};
}

#endif // !IIS_WEB_SOCKET_SERVER_H
