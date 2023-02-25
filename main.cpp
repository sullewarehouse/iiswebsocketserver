
//
// main.cpp
// 
// Author:
//     Brian Sullender
//     SULLE WAREHOUSE LLC
// 
// Description:
//     Example using the IIS WebSocket server interface.
//

#define _WINSOCKAPI_

// Header needed for our std::list client list
#include <list>

// The actual WebSocket server
#include "iiswebsocket.h"

// Set this to false to stop debugging
static const bool DEBUG_WEB_SOCKET_SERVER = false;

// Debug class
class DebugHandler
{
private:
	// The path of the client debugging file
	WCHAR FilePath[0x1000];
	// Handle to the debugging file
	HANDLE hDebugFile;
public:
	// Initialize the class and create the debugging text file
	HANDLE Initialize(const WCHAR* path, GUID guid)
	{
		// Set default
		this->hDebugFile = NULL;
		// copy the full path
		wcscpy_s(this->FilePath, 0x1000, path);
		// append the unique id
		LPOLESTR pwszGuid = nullptr;
		if (StringFromCLSID(guid, &pwszGuid) != S_OK) {
			return NULL;
		}
		wcscat_s(this->FilePath, 0x1000, pwszGuid);
		// Free resources
		CoTaskMemFree(pwszGuid);
		// append file type
		wcscat_s(this->FilePath, 0x1000, L".txt");
		// Create the debugging text file
		this->hDebugFile = CreateFile(this->FilePath, GENERIC_WRITE, FILE_SHARE_READ, NULL,
			CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		// Return the file handle
		return this->hDebugFile;
	}
	BOOL Out(const CHAR* string)
	{
		DWORD dwBytesWritten;
		return WriteFile(this->hDebugFile, string, strlen(string), &dwBytesWritten, NULL);
	}
	BOOL Close()
	{
		if (this->hDebugFile) {
			return CloseHandle(this->hDebugFile);
		}
		return FALSE;
	}
};

// Create a pointer for the global server interface.
static IHttpServer* g_pHttpServer = NULL;

// Client connection struct, each connection gets one
struct CLIENT_CONNECTION
{
	GUID guid;
	IHttpContext* pHttpContext;
	IHttpModuleContextContainer* pModuleContextContainer;
	WebSocketServer* pWebSocketServer;
	DebugHandler debugger;
};

// The list of client connections
static std::list<CLIENT_CONNECTION*> client_list;

// The client list mutex lock
static HANDLE client_list_mutex;

// The number of milliseconds to wait for the client list lock
#define CLIENT_LIST_WAIT_TIME 3000

// Add a client connection to the list
bool add_client(CLIENT_CONNECTION* client)
{
	DWORD dwWaitResult;

	// Get the client list lock
	dwWaitResult = WaitForSingleObject(client_list_mutex, CLIENT_LIST_WAIT_TIME);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// Add client to list
		client_list.push_back(client);

		// Release the client list lock
		ReleaseMutex(client_list_mutex);

		// Success
		return true;
	}

	// Failed
	return false;
}

// Remove client by guid
bool remove_client_by_id(GUID guid)
{
	bool success;
	DWORD dwWaitResult;

	success = false;

	// Get the client list lock
	dwWaitResult = WaitForSingleObject(client_list_mutex, CLIENT_LIST_WAIT_TIME);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// Loop each client
		for (std::list<CLIENT_CONNECTION*>::iterator it = client_list.begin(); it != client_list.end(); ++it)
		{
			// This the desired client?
			if ((*it)->guid == guid)
			{
				client_list.erase(it);
				success = true;
				break;
			}
		}

		// Release the client list lock
		ReleaseMutex(client_list_mutex);
	}

	return success;
}

// Remove client by pointer
bool remove_client(CLIENT_CONNECTION* client)
{
	bool success;
	DWORD dwWaitResult;

	success = false;

	// Get the client list lock
	dwWaitResult = WaitForSingleObject(client_list_mutex, CLIENT_LIST_WAIT_TIME);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// Loop each client
		for (std::list<CLIENT_CONNECTION*>::iterator it = client_list.begin(); it != client_list.end(); ++it)
		{
			// This the desired client?
			if (*it == client)
			{
				client_list.erase(it);
				success = true;
				break;
			}
		}

		// Release the client list lock
		ReleaseMutex(client_list_mutex);
	}

	return success;
}

size_t get_client_count()
{
	size_t count;
	DWORD dwWaitResult;

	count = 0;

	// Get the client list lock
	dwWaitResult = WaitForSingleObject(client_list_mutex, CLIENT_LIST_WAIT_TIME);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// Get client count
		count = client_list.size();

		// Release the client list lock
		ReleaseMutex(client_list_mutex);
	}

	return count;
}

// WebSocket communication thread
DWORD WINAPI RunWork(void* parameter)
{
	DWORD errorCode;
	CHAR* pInBuffer;
	CHAR* pNewBuffer;
	CHAR* pOutBuffer;
	CHAR localBuffer[8];

	// Set pointers
	pInBuffer = NULL;
	pNewBuffer = NULL;
	pOutBuffer = NULL;

	// Get the connection class
	CLIENT_CONNECTION* pClientConnection = (CLIENT_CONNECTION*)parameter;

	// Add our client to the client list
	add_client(pClientConnection);

	// Get the WebSocket class
	WebSocketServer* pWebSocketServer = pClientConnection->pWebSocketServer;

	// Set a max payload length, if a frame payload is over this length the connection is closed
	pWebSocketServer->MaxPayloadLength = 0x100;

	// Max bytes a message can be, if a message is larger than this length then the connection is closed
	DWORD dwMaxTotalBytesReceived = 0x1000;

	// Receive variables
	DWORD dwBytesReceived = 0;
	IIS_WEB_SOCKET_BUFFER_TYPE bufferType;
	DWORD dwTotalBytesReceived;

	// Debug buffer when debugging is enabled
	CHAR* DebugBuffer = NULL;

	// Allocate the debug buffer
	if (DEBUG_WEB_SOCKET_SERVER) {
		DebugBuffer = (CHAR*)malloc(0x1000);
		if (DebugBuffer == NULL) {
			goto exit;
		}
	}

	if (DEBUG_WEB_SOCKET_SERVER) {
		pClientConnection->debugger.Out("Entering WebSocket loop...\n\n");
	}

	// Run loop while connected, get messages and process them
	while (pWebSocketServer->IsConnected())
	{
		// Reset dwBytesRead and dwTotalBytesReceived
		dwBytesReceived = 0;
		dwTotalBytesReceived = 0;

		do
		{
			// Receive bytes
			errorCode = pWebSocketServer->Receive(localBuffer, 8, &dwBytesReceived, &bufferType);
			if (errorCode != S_OK)
			{
				if (DEBUG_WEB_SOCKET_SERVER) {
					pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
					pClientConnection->debugger.Out("WebSocketServer:Receive() failed\n");
				}
				goto exit;
			}

			// Calculate the total bytes received
			dwTotalBytesReceived += dwBytesReceived;

			// Check if we have gone over the specified limit
			if (dwTotalBytesReceived > dwMaxTotalBytesReceived) {
				goto exit;
			}

			if (dwTotalBytesReceived != 0)
			{
				// Realloc the destination memory with room for a terminating NULL character
				pNewBuffer = (CHAR*)realloc(pInBuffer, dwTotalBytesReceived + 1);
				if (pNewBuffer == NULL) {
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out("realloc() for WebSocketServer::Receive() failed\n");
					}
					goto exit;
				}
				pInBuffer = pNewBuffer;

				// Copy the new data
				memcpy(pInBuffer + (dwTotalBytesReceived - dwBytesReceived), localBuffer, dwBytesReceived);

				if (DEBUG_WEB_SOCKET_SERVER)
				{
					sprintf_s(DebugBuffer, 0x1000, "Incoming: FIN:%d Opcode:0x%X Mask(BOOLEAN):%d Payload-Len:0x%llX Frame-Len:0x%X%s",
						pWebSocketServer->WebSocketFrame.FIN,
						pWebSocketServer->WebSocketFrame.Opcode,
						pWebSocketServer->WebSocketFrame.bMask,
						pWebSocketServer->WebSocketFrame.PayloadLength,
						pWebSocketServer->WebSocketFrame.FrameSize,
						"\n");
					pClientConnection->debugger.Out(DebugBuffer);

					pClientConnection->debugger.Out("RAW FRAME DATA: ");
					for (unsigned int i = 0; i < pWebSocketServer->WebSocketFrame.FrameSize; i++)
					{
						sprintf_s(DebugBuffer, 0x1000, "0x%X ", (ULONG)(UCHAR)pWebSocketServer->Stream.pFrameBuffer[i]);
						pClientConnection->debugger.Out(DebugBuffer);
					}
					pClientConnection->debugger.Out("\n");
				}
			}

			// Loop while buffer type is a fragment
		} while ((bufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE) || (bufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_BINARY_FRAGMENT_BUFFER_TYPE));

		//
		// **** We have received a full message at this point, determine the type and act! ****
		//

		if (bufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE)
		{
			if (DEBUG_WEB_SOCKET_SERVER) {
				pClientConnection->debugger.Out("Received (CLOSE BUFFER TYPE)\n");
			}

			// Finish the CLOSE 
			errorCode = pWebSocketServer->Send(IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE, pInBuffer, dwTotalBytesReceived);
			if (errorCode != S_OK)
			{
				if (DEBUG_WEB_SOCKET_SERVER) {
					pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
					pClientConnection->debugger.Out("WebSocketServer::Send() 'CLOSE FRAME' failed\n");
				}
			}
			break;
		}
		else if (bufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PING_BUFFER_TYPE)
		{
			if (DEBUG_WEB_SOCKET_SERVER) {
				pClientConnection->debugger.Out("Received: (PING)\n");
			}
			errorCode = pWebSocketServer->Send(IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_PONG_BUFFER_TYPE, pInBuffer, dwTotalBytesReceived);
			if (errorCode != S_OK)
			{
				if (DEBUG_WEB_SOCKET_SERVER) {
					pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
					pClientConnection->debugger.Out("WebSocketServer::Send() 'PING' failed\n\n");
				}
				break;
			}
		}
		else if ((bufferType == IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE) && (dwTotalBytesReceived != 0))
		{
			// Compiler thinks we are causing a buffer overrun, so we disbale the warning, then re-enable it
#pragma warning( push )
			// Silly compiler, overruns are for kids :)
#pragma warning( disable : 6386 )
			// Set terminating NULL character
			pInBuffer[dwTotalBytesReceived] = NULL;
#pragma warning( pop )

			if (DEBUG_WEB_SOCKET_SERVER) {
				pClientConnection->debugger.Out("Received: ");
				pClientConnection->debugger.Out(pInBuffer);
				pClientConnection->debugger.Out("\n");
			}

			// Check if user sent a command message
			if (_stricmp(pInBuffer, "send-exit") == 0)
			{
				pClientConnection->debugger.Out("User requested server connection close\n");

				// Close data
				IIS_WEB_SOCKET_SERVER_CLOSE_DATA closeData(IIS_WEB_SOCKET_CLOSE_STATUS::IIS_WEB_SOCKET_SUCCESS_CLOSE_STATUS, "User requested");

				// Send the CLOSE frame with the reason
				errorCode = pWebSocketServer->Send(IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_CLOSE_BUFFER_TYPE, &closeData, closeData.length());
				if (errorCode != S_OK)
				{
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
						pClientConnection->debugger.Out("WebSocketServer::Send(CLOSE) failed\n");
					}
					break;
				}

				// Client will send a CLOSE frame back
				errorCode = pWebSocketServer->Receive(&closeData, sizeof(IIS_WEB_SOCKET_SERVER_CLOSE_DATA), &dwBytesReceived, &bufferType);
				if (errorCode != S_OK)
				{
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
						pClientConnection->debugger.Out("WebSocketServer::Receive(CLOSE) failed\n");
					}
					break;
				}

				// Exit the the loop
				break;
			}
			else if (_stricmp(pInBuffer, "send-connection-count") == 0)
			{
				// Allocate the out message buffer
				pOutBuffer = (CHAR*)malloc(256);
				if (pOutBuffer == NULL) {
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out("malloc() for OutBuffer failed\n");
					}
					break;
				}

				// Convert client count to string
				sprintf_s(pOutBuffer, 256, "%zu", get_client_count());

				// Send the client count message
				errorCode = pWebSocketServer->Send(IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, pOutBuffer, strlen(pOutBuffer));
				if (errorCode != S_OK)
				{
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
						pClientConnection->debugger.Out("WebSocketServer::Send() failed\n");
					}
					break;
				}

				// Free out buffer
				free(pOutBuffer);
				pOutBuffer = NULL;
			}
			else
			{
				// Just echo the client message

				DWORD EchoBufferSize = dwTotalBytesReceived + 14;

				// Allocate the out message buffer
				pOutBuffer = (CHAR*)malloc(EchoBufferSize);
				if (pOutBuffer == NULL) {
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out("malloc() for OutBuffer failed\n");
					}
					break;
				}

				// Create the echo message
				strcpy_s(pOutBuffer, EchoBufferSize, "echo: ");
				strcat_s(pOutBuffer, EchoBufferSize, pInBuffer);

				// Send the echo message
				errorCode = pWebSocketServer->Send(IIS_WEB_SOCKET_BUFFER_TYPE::IIS_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, pOutBuffer, strlen(pOutBuffer));
				if (errorCode != S_OK)
				{
					if (DEBUG_WEB_SOCKET_SERVER) {
						pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
						pClientConnection->debugger.Out("WebSocketServer::Send() failed\n");
					}
					break;
				}

				if (DEBUG_WEB_SOCKET_SERVER) {
					pClientConnection->debugger.Out("Sent: ");
					pClientConnection->debugger.Out(pOutBuffer);
					pClientConnection->debugger.Out("\n\n");
				}

				// Free out buffer
				free(pOutBuffer);
				pOutBuffer = NULL;
			}
		}
	}

exit:

	if (DEBUG_WEB_SOCKET_SERVER) {
		pClientConnection->debugger.Out("Worker thread exiting!");
	}

	// Remove client from our connection list
	remove_client(pClientConnection);

	// Free resources

	if (pInBuffer) {
		free(pInBuffer);
	}
	if (pOutBuffer) {
		free(pOutBuffer);
	}

	pWebSocketServer->Free();

	pClientConnection->debugger.Close();

	// Tell IIS we are finished with the connection and resources
	pClientConnection->pHttpContext->IndicateCompletion(RQ_NOTIFICATION_FINISH_REQUEST);

	// Free connection classes
	free(pClientConnection);
	free(pWebSocketServer);

	// Exit thread
	return 0;
}

// Create the module class.
class WebSocketEchoModule : public CHttpModule
{
public:
	REQUEST_NOTIFICATION_STATUS OnBeginRequest(IN IHttpContext* pHttpContext, IN IHttpEventProvider* pProvider)
	{
		UNREFERENCED_PARAMETER(pProvider);

		// Returned error code
		HRESULT errorCode;

		// Connection classes
		CLIENT_CONNECTION* pClientConnection;
		WebSocketServer* pWebSocketServer;

		// Set pointers
		pClientConnection = NULL;
		pWebSocketServer = NULL;

		// Allocate our client connection struct
		pClientConnection = (CLIENT_CONNECTION*)malloc(sizeof(CLIENT_CONNECTION));
		if (pClientConnection == NULL) {
			goto exit;
		}

		// Create client id
		if (CoCreateGuid(&pClientConnection->guid) != S_OK) {
			goto exit;
		}

		// Set basic connection info
		pClientConnection->pHttpContext = pHttpContext;
		pClientConnection->pModuleContextContainer = pHttpContext->GetModuleContextContainer();

		// Setup debugging
		if (DEBUG_WEB_SOCKET_SERVER) {
			pClientConnection->debugger.Initialize(L"C:\\inetpub\\modules\\echo\\echo", pClientConnection->guid);
			pClientConnection->debugger.Out("Echo WebSocket Server - New Connection\n");
		}

		// Allocate our WebSocket server class
		pWebSocketServer = (WebSocketServer*)malloc(sizeof(WebSocketServer));
		if (pWebSocketServer == NULL) {
			goto exit;
		}

		// Set the clients websocket class
		pClientConnection->pWebSocketServer = pWebSocketServer;

		// Initialize our WebSocket server
		if (pWebSocketServer->Initialize() != S_OK) {
			goto exit;
		}

		if (DEBUG_WEB_SOCKET_SERVER) {
			pClientConnection->debugger.Out("WebSocket Server Initialized\n");
		}

		// Attempt the WebSocket handshake
		errorCode = pWebSocketServer->PerformHandshake(pHttpContext);
		if (errorCode == S_OK)
		{
			if (DEBUG_WEB_SOCKET_SERVER) {
				pClientConnection->debugger.Out("WebSocket Handshake was successful!\n");
			}

			// Enable Full Duplex
			IHttpContext3* pHttpContext3;
			HttpGetExtendedInterface(g_pHttpServer, pHttpContext, &pHttpContext3);
			pHttpContext3->EnableFullDuplex();

			// Create the WebSocket worker thread
			CreateThread(NULL, 0, RunWork, pClientConnection, 0, 0);

			// Tell IIS to keep the connection pending...
			return RQ_NOTIFICATION_PENDING;
		}

		if (DEBUG_WEB_SOCKET_SERVER) {
			pClientConnection->debugger.Out("WebSocket Handshake Failed!\n");
			pClientConnection->debugger.Out(pWebSocketServer->ErrorDescription);
			pClientConnection->debugger.Out("\nExiting Application\n\n");
		}

	exit:

		// Free resources

		if (pClientConnection) {
			pClientConnection->debugger.Close();
			free(pClientConnection);
		}

		if (pWebSocketServer) {
			pWebSocketServer->Free();
			free(pWebSocketServer);
		}

		// Return processing to the pipeline.
		return RQ_NOTIFICATION_CONTINUE;
	}
};

// Create the module's class factory.
class WebSocketEchoFactory : public IHttpModuleFactory
{
public:
	HRESULT GetHttpModule(OUT CHttpModule** ppModule, IN IModuleAllocator* pAllocator)
	{
		UNREFERENCED_PARAMETER(pAllocator);

		// Create a new instance.
		WebSocketEchoModule* pModule = new WebSocketEchoModule;

		// Test for an error.
		if (!pModule) {
			// Return an error if the factory cannot create the instance.
			return HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY);
		}
		else
		{
			// Return a pointer to the module.
			*ppModule = pModule;
			pModule = NULL;
			// Return a success status.
			return S_OK;
		}
	}

	void Terminate()
	{
		// Remove the class from memory.
		delete this;
	}
};

// Create the module's exported registration function.
HRESULT __stdcall RegisterModule(DWORD dwServerVersion, IHttpModuleRegistrationInfo* pModuleInfo, IHttpServer* pGlobalInfo)
{
	UNREFERENCED_PARAMETER(dwServerVersion);
	UNREFERENCED_PARAMETER(pGlobalInfo);

	// Set global IHttpServer variable
	g_pHttpServer = pGlobalInfo;

	// Create the global client list mutex
	client_list_mutex = CreateMutex(NULL, FALSE, NULL);

	// Set the request notifications and exit.
	return pModuleInfo->SetRequestNotifications(new WebSocketEchoFactory, RQ_BEGIN_REQUEST, 0);
}
